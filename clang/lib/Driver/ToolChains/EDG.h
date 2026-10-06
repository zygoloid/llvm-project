//===--- EDG.h - EDG Tool Implementation ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_EDG_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_EDG_H

#include "clang/Driver/Tool.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Compiler.h"

namespace clang {
namespace driver {
namespace tools {

/// EDG C++ frontend tool.
class LLVM_LIBRARY_VISIBILITY EDG : public Tool {
public:
  explicit EDG(const ToolChain &TC);
  ~EDG() override;

  bool hasGoodDiagnostics() const override { return true; }
  bool hasIntegratedAssembler() const override { return false; }
  bool hasIntegratedCPP() const override { return true; }
  bool canEmitIR() const override { return false; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

} // end namespace tools
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_EDG_H
