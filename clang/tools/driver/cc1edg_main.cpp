//===-- cc1edg_main.cpp - Clang EDG Frontend Driver -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/CodeGenOptions.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/LangStandard.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Config/config.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/Utils.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace clang;
using namespace llvm;

#if CLANG_ENABLE_EDG
namespace edg {
int edg_main(int argc, char *argv[]);
} // namespace edg

static bool shouldSkipEDGPredefinedMacro(StringRef Name) {
  // Skip macros that EDG defines internally in Clang/GNU mode.
  if (Name.starts_with("__STDC") || Name.starts_with("__clang") ||
      Name.starts_with("__cpp_") || Name.starts_with("__EDG"))
    return true;
  return Name == "__cplusplus" || Name == "_GLIBCXX_USE_FLOAT128" ||
         Name == "__NO_MATH_INLINES" || Name == "__USER_LABEL_PREFIX__" ||
         Name == "__GXX_RTTI" || Name == "__EXCEPTIONS" ||
         Name == "_GNU_SOURCE" || Name == "__SIZE_TYPE__" ||
         Name == "__PTRDIFF_TYPE__" || Name == "__WCHAR_TYPE__" ||
         Name == "__CHAR16_TYPE__" || Name == "__CHAR32_TYPE__" ||
         Name == "__WCHAR_UNSIGNED__" || Name == "__CHAR_UNSIGNED__" ||
         Name == "__DEPRECATED" || Name == "__THROWNL";
}
#endif

extern int cc1edg_main(ArrayRef<const char *> Argv, const char *Argv0,
                       void *MainAddr);

int cc1edg_main(ArrayRef<const char *> Argv, const char *Argv0,
                void *MainAddr) {
#if !CLANG_ENABLE_EDG
  (void)Argv;
  (void)Argv0;
  (void)MainAddr;
  llvm::errs()
      << "error: the EDG frontend is not enabled in this build of Clang; "
         "rebuild with -DCLANG_EDG_SOURCE_DIR=<path> to enable '-fedg'\n";
  return 1;
#else
  (void)MainAddr;
  BumpPtrAllocator Alloc;
  StringSaver Saver(Alloc);

  std::string TripleStr = llvm::sys::getDefaultTargetTriple();
  LangStandard::Kind LangStd = LangStandard::lang_gnucxx17;
  bool Exceptions = true;
  bool RTTI = true;
  std::optional<bool> SignedChar;
  SmallString<128> StdinTempPath;

  SmallVector<const char *, 64> ForwardedArgs;
  for (size_t I = 0, E = Argv.size(); I < E; ++I) {
    StringRef Arg = Argv[I];
    if (Arg == "-triple" && I + 1 < E) {
      TripleStr = Argv[++I];
      continue;
    }
    if (Arg == "-") {
      // EDG's c_gen_be ignores --gen_c_file_name and writes to stdout whenever
      // primary_source_file_name is "-". Materialize stdin to a temporary file
      // so that --gen_c_file_name is honored and source lines can be displayed
      // in diagnostics.
      int TempFD = -1;
      if (std::error_code EC = llvm::sys::fs::createTemporaryFile(
              "clang-edg-stdin", "cpp", TempFD, StdinTempPath)) {
        llvm::errs() << "error: unable to create temporary file for stdin: "
                     << EC.message() << "\n";
        return 1;
      }
      llvm::raw_fd_ostream TempOS(TempFD, /*shouldClose=*/true);
      auto StdinBuf = llvm::MemoryBuffer::getSTDIN();
      if (!StdinBuf) {
        llvm::errs() << "error: unable to read stdin: "
                     << StdinBuf.getError().message() << "\n";
        llvm::sys::fs::remove(StdinTempPath);
        return 1;
      }
      TempOS << (*StdinBuf)->getBuffer();
      TempOS.close();
      ForwardedArgs.push_back(Saver.save(StdinTempPath.str()).data());
      continue;
    }
    if (Arg == "--c++03")
      LangStd = LangStandard::lang_gnucxx98;
    else if (Arg == "--c++11")
      LangStd = LangStandard::lang_gnucxx11;
    else if (Arg == "--c++14")
      LangStd = LangStandard::lang_gnucxx14;
    else if (Arg == "--c++17")
      LangStd = LangStandard::lang_gnucxx17;
    else if (Arg == "--c++20")
      LangStd = LangStandard::lang_gnucxx20;
    else if (Arg == "--c++23")
      LangStd = LangStandard::lang_gnucxx23;
    else if (Arg == "--c++26")
      LangStd = LangStandard::lang_gnucxx26;
    else if (Arg == "--exceptions")
      Exceptions = true;
    else if (Arg == "--no_exceptions")
      Exceptions = false;
    else if (Arg == "--rtti")
      RTTI = true;
    else if (Arg == "--no_rtti")
      RTTI = false;
    else if (Arg == "--signed_chars")
      SignedChar = true;
    else if (Arg == "--unsigned_chars")
      SignedChar = false;

    ForwardedArgs.push_back(Argv[I]);
  }

  SmallVector<char *, 128> EDGArgv;
  EDGArgv.push_back(const_cast<char *>(Saver.save(Argv0 ? Argv0 : "clang").data()));

  // Initialize Clang's target predefined macros and pass them as -D flags to
  // EDG so that system and standard library headers see the expected target
  // macros without requiring an external predefined_macros.txt file.
  clang::TargetOptions TargetOpts;
  TargetOpts.Triple = llvm::Triple::normalize(TripleStr);
  llvm::Triple T(TargetOpts.Triple);

  LangOptions LangOpts;
  std::vector<std::string> Includes;
  LangOptions::setLangDefaults(LangOpts, Language::CXX, T, Includes, LangStd);
  LangOpts.Exceptions = Exceptions;
  LangOpts.CXXExceptions = Exceptions;
  LangOpts.RTTI = RTTI;
  LangOpts.RTTIData = RTTI;
  if (SignedChar)
    LangOpts.CharIsSigned = *SignedChar;
  LangOpts.GNUCVersion = 40201;

  DiagnosticOptions DiagOpts;
  IntrusiveRefCntPtr<DiagnosticsEngine> Diags =
      CompilerInstance::createDiagnostics(*llvm::vfs::getRealFileSystem(),
                                          DiagOpts);
  IntrusiveRefCntPtr<TargetInfo> TI =
      TargetInfo::CreateTargetInfo(*Diags, TargetOpts);
  if (TI) {
    TI->adjust(*Diags, LangOpts, /*AuxTarget=*/nullptr);
    FrontendOptions FEOpts;
    PreprocessorOptions PPOpts;
    CodeGenOptions CGOpts;
    std::string PredefineBuffer;
    llvm::raw_string_ostream Predefines(PredefineBuffer);
    MacroBuilder Builder(Predefines);
    InitializePredefinedMacros(*TI, LangOpts, FEOpts, PPOpts, CGOpts, Builder);

    StringRef Buf = Predefines.str();
    while (!Buf.empty()) {
      auto [Line, Rest] = Buf.split('\n');
      Buf = Rest;
      if (!Line.starts_with("#define "))
        continue;
      StringRef Def = Line.drop_front(strlen("#define "));
      auto [Name, Val] = Def.split(' ');
      if (Name.empty() || shouldSkipEDGPredefinedMacro(Name))
        continue;
      StringRef Saved = Saver.save(Twine("-D") + Name + "=" + Val);
      EDGArgv.push_back(const_cast<char *>(Saved.data()));
    }
  }

  for (const char *Arg : ForwardedArgs) {
    StringRef Saved = Saver.save(Arg);
    EDGArgv.push_back(const_cast<char *>(Saved.data()));
  }
  EDGArgv.push_back(nullptr);

  int Status = edg::edg_main(static_cast<int>(EDGArgv.size() - 1),
                             EDGArgv.data());
  if (!StdinTempPath.empty())
    llvm::sys::fs::remove(StdinTempPath);
  // EDG returns 0 for success, 1 for warnings (RC_WARNING), and >= 2 for
  // errors/catastrophes. Map warning-only exit status to 0 for Clang driver.
  return Status <= 1 ? 0 : 1;
#endif
}
