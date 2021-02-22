//===- DeclContextInternals.h - DeclContext Representation ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the data structures used in the implementation
//  of DeclContext.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_DECLCONTEXTINTERNALS_H
#define LLVM_CLANG_AST_DECLCONTEXTINTERNALS_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclarationName.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/PointerUnion.h"
#include <algorithm>
#include <cassert>

namespace clang {

class DependentDiagnostic;

/// An array of decls optimized for the common case of only containing
/// one entry.
class StoredDeclsList {
  using Decls = DeclListNode::Decls;

  /// A collection of declarations, with a flag to indicate if we have
  /// further external declarations.
  using DeclsAndHasExternalTy = llvm::PointerIntPair<Decls, 1, bool>;

  /// The stored data, which will be either a pointer to a NamedDecl,
  /// or a pointer to a list with a flag to indicate if there are further
  /// external declarations.
  DeclsAndHasExternalTy Data;

  void setOnlyValue(NamedDecl *ND) {
    assert(!getAsList() && "Not inline");
    Data.setPointer(ND);
    // Make sure that Data is a plain NamedDecl* so we can use its address
    // at getLookupResult.
    assert(*(NamedDecl **)Data.getPointer().getAddrOfPtr1() == ND &&
           "PointerUnion mangles the NamedDecl pointer!");
  }

  void push_front(NamedDecl *ND) {
    assert(!isNull() && "No need to call push_front");

    ASTContext &C = getASTContext();

    // auto Vec = getLookupResult();
    // (void)Vec;
    // assert(llvm::find(Vec, ND) == Vec.end() && "list still contains decl");

    DeclListNode *Node = C.AllocateDeclListNode(ND);
    Node->Rest = Data.getPointer();
    Data.setPointer(Node);
  }

  template<typename Fn>
  void erase_if(Fn ShouldErase) {
    Decls List = Data.getPointer();
    if (!List)
      return;
    ASTContext &C = getASTContext();
    DeclListNode::Decls NewHead = nullptr;
    DeclListNode::Decls *NewLast = nullptr;
    DeclListNode::Decls *NewTail = &NewHead;
    while (true) {
      if (!ShouldErase(*DeclListNode::iterator(List))) {
        NewLast = NewTail;
        *NewTail = List;
        if (auto *Node = List.dyn_cast<DeclListNode*>()) {
          NewTail = &Node->Rest;
          List = Node->Rest;
        } else {
          break;
        }
      } else if (DeclListNode *N = List.dyn_cast<DeclListNode*>()) {
        List = N->Rest;
        C.DeallocateDeclListNode(N);
      } else {
        if (NewLast) {
          DeclListNode *Node = NewLast->get<DeclListNode*>();
          *NewLast = Node->D;
          C.DeallocateDeclListNode(Node);
        }
        break;
      }
    }
    Data.setPointer(NewHead);

    assert(llvm::find_if(getLookupResult(), ShouldErase) ==
           getLookupResult().end() && "Still exists!");
  }

  void erase(NamedDecl *ND) {
    erase_if([ND](NamedDecl *D){ return D == ND; });
  }

public:
  StoredDeclsList() = default;

  StoredDeclsList(StoredDeclsList &&RHS) : Data(RHS.Data) {
    RHS.Data.setPointer(nullptr);
    RHS.Data.setInt(0);
  }

  void MaybeDeallocList() {
    if (isNull())
      return;
    // If this is a list-form, free the list.
    ASTContext &C = getASTContext();
    Decls List = Data.getPointer();
    while(List.is<DeclListNode*>()) {
      DeclListNode *ToDealloc = List.get<DeclListNode*>();
      List = ToDealloc->Rest;
      C.DeallocateDeclListNode(ToDealloc);
    }
  }

  ~StoredDeclsList() {
    MaybeDeallocList();
  }

  StoredDeclsList &operator=(StoredDeclsList &&RHS) {
    MaybeDeallocList();

    Data = RHS.Data;
    RHS.Data.setPointer(nullptr);
    RHS.Data.setInt(0);
    return *this;
  }

  bool isNull() const { return Data.getPointer().isNull(); }

  ASTContext &getASTContext() {
    assert(!isNull() && "No ASTContext.");
    if (NamedDecl *ND = getAsDecl())
      return ND->getASTContext();
    return getAsList()->D->getASTContext();
  }

  DeclsAndHasExternalTy getAsListAndHasExternal() const { return Data; }

  NamedDecl *getAsDecl() const {
    return getAsListAndHasExternal().getPointer().dyn_cast<NamedDecl *>();
  }

  DeclListNode *getAsList() const {
    return getAsListAndHasExternal().getPointer().dyn_cast<DeclListNode*>();
  }

  bool hasExternalDecls() const {
    return getAsListAndHasExternal().getInt();
  }

  void setHasExternalDecls() {
    Data.setInt(1);
  }

  void remove(NamedDecl *D) {
    assert(!isNull() && "removing from empty list");
    if (NamedDecl *Singleton = getAsDecl()) {
      if (Singleton == D)
        Data.setPointer(nullptr);
      return;
    }

    erase(D);
  }

  /// Remove any declarations which were imported from an external AST source.
  void removeExternalDecls() {
    if (isNull()) {
      // Nothing to do.
    } else if (NamedDecl *Singleton = getAsDecl()) {
      if (Singleton->isFromASTFile())
        *this = StoredDeclsList();
    } else {
      erase_if([](NamedDecl *ND){ return ND->isFromASTFile(); });
      // Don't have any external decls any more.
      Data.setInt(0);
    }
  }

  /// getLookupResult - Return an array of all the decls that this list
  /// represents.
  DeclContext::lookup_result getLookupResult() const {
    return DeclContext::lookup_result(Data.getPointer());
  }

  /// HandleRedeclaration - If this is a redeclaration of an existing decl,
  /// replace the old one with D and return true. Otherwise return false.
  bool HandleRedeclaration(NamedDecl *D, bool IsKnownNewer) {
    // Most decls only have one entry in their list, special case it.
    if (NamedDecl *OldD = getAsDecl()) {
      if (!D->declarationReplaces(OldD, IsKnownNewer))
        return false;
      setOnlyValue(D);
      return true;
    }

    // Determine if this declaration is actually a redeclaration.
    for (DeclListNode* N = getAsList(); N;
         N = N->Rest.dyn_cast<DeclListNode*>()) {
      if (D->declarationReplaces(N->D, IsKnownNewer)) {
        N->D = D;
        return true;
      }
      if (auto *ND = N->Rest.dyn_cast<NamedDecl*>()) // the two element case
        if (D->declarationReplaces(ND, IsKnownNewer)) {
          N->Rest = D;
          return true;
        }
    }

    return false;
  }

  /// AddDecl - Called to add declarations when it is not a redeclaration to
  /// merge it into the appropriate place in our list.
  void AddDecl(NamedDecl *D) {
    if (isNull()) {
      setOnlyValue(D);
      return;
    }

    push_front(D);
  }

  LLVM_DUMP_METHOD void dump() const {
    Decls D = Data.getPointer();
    if (!D) {
      llvm::errs() << "<null>\n";
      return;
    }

    while (true) {
      if (auto *Node = D.dyn_cast<DeclListNode*>()) {
        llvm::errs() << '[' << Node->D << "] -> ";
        D = Node->Rest;
      } else {
        llvm::errs() << '[' << D.get<NamedDecl*>() << "]\n";
        return;
      }
    }
  }
};

class StoredDeclsMap
    : public llvm::SmallDenseMap<DeclarationName, StoredDeclsList, 4> {
  friend class ASTContext; // walks the chain deleting these
  friend class DeclContext;

  llvm::PointerIntPair<StoredDeclsMap*, 1> Previous;
public:
  static void DestroyAll(StoredDeclsMap *Map, bool Dependent);
};

class DependentStoredDeclsMap : public StoredDeclsMap {
  friend class DeclContext; // iterates over diagnostics
  friend class DependentDiagnostic;

  DependentDiagnostic *FirstDiagnostic = nullptr;
public:
  DependentStoredDeclsMap() = default;
};

} // namespace clang

#endif // LLVM_CLANG_AST_DECLCONTEXTINTERNALS_H
