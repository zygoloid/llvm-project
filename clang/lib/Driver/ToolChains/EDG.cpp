//===--- EDG.cpp - EDG Tool Implementation ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "EDG.h"
#include "clang/Config/config.h"
#include "clang/Driver/Action.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Job.h"
#include "clang/Driver/ToolChain.h"
#include "clang/Options/Options.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/ArgList.h"
#include "llvm/TargetParser/Triple.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

EDG::EDG(const ToolChain &TC) : Tool("edg", "edg frontend", TC) {}

EDG::~EDG() = default;

void EDG::ConstructJob(Compilation &C, const JobAction &JA,
                       const InputInfo &Output, const InputInfoList &Inputs,
                       const ArgList &Args, const char *LinkingOutput) const {
  assert(Inputs.size() == 1 && "Unable to handle multiple inputs.");
  const ToolChain &TC = getToolChain();
  const Driver &D = TC.getDriver();
  const llvm::Triple &EffectiveTriple = TC.getEffectiveTriple();

#if !CLANG_ENABLE_EDG
  if (!C.getArgs().hasArg(options::OPT__HASH_HASH_HASH))
    D.Diag(diag::err_drv_edg_not_built);
#endif

  ArgStringList CmdArgs;
  CmdArgs.push_back("-cc1edg");

  CmdArgs.push_back("-triple");
  CmdArgs.push_back(Args.MakeArgString(EffectiveTriple.getTriple()));

  // Select the corresponding EDG target configuration.
  if (EffectiveTriple.isOSWindows() &&
      EffectiveTriple.isWindowsMSVCEnvironment()) {
    if (EffectiveTriple.isArch64Bit())
      CmdArgs.push_back("--target=win64");
    else
      CmdArgs.push_back("--target=win32");
  } else {
    switch (EffectiveTriple.getArch()) {
    case llvm::Triple::x86_64:
      CmdArgs.push_back("--target=linux_x86_64");
      break;
    case llvm::Triple::x86:
      CmdArgs.push_back("--target=linux_i686");
      break;
    case llvm::Triple::aarch64:
    case llvm::Triple::aarch64_be:
      CmdArgs.push_back("--target=linux_aarch64");
      break;
    case llvm::Triple::arm:
    case llvm::Triple::armeb:
    case llvm::Triple::thumb:
    case llvm::Triple::thumbeb:
      CmdArgs.push_back("--target=linux_armv7");
      break;
    case llvm::Triple::riscv64:
      CmdArgs.push_back("--target=linux_riscv64");
      break;
    case llvm::Triple::riscv32:
      CmdArgs.push_back("--target=linux_riscv32");
      break;
    default:
      break;
    }
  }

  // Pass --clang before the standard flag so EDG's option parser knows
  // clang_mode is active when processing --c++23 / --c++26.
  CmdArgs.push_back("--clang");

  // Language standard selection.
  const char *StdFlag = "--c++17";
  if (Arg *A = Args.getLastArg(options::OPT_std_EQ)) {
    StringRef Val = A->getValue();
    StdFlag = llvm::StringSwitch<const char *>(Val)
                  .Cases({"c++98", "c++03", "gnu++98", "gnu++03"}, "--c++03")
                  .Cases({"c++11", "c++0x", "gnu++11", "gnu++0x"}, "--c++11")
                  .Cases({"c++14", "c++1y", "gnu++14", "gnu++1y"}, "--c++14")
                  .Cases({"c++17", "c++1z", "gnu++17", "gnu++1z"}, "--c++17")
                  .Cases({"c++20", "c++2a", "gnu++20", "gnu++2a"}, "--c++20")
                  .Cases({"c++23", "c++2b", "gnu++23", "gnu++2b"}, "--c++23")
                  .Cases({"c++26", "c++2c", "gnu++26", "gnu++2c"}, "--c++26")
                  .Default("--c++17");
  }
  CmdArgs.push_back(StdFlag);

  // Signed/unsigned char.
  if (Args.hasFlag(options::OPT_fsigned_char, options::OPT_funsigned_char,
                   isSignedCharDefault(EffectiveTriple)))
    CmdArgs.push_back("--signed_chars");
  else
    CmdArgs.push_back("--unsigned_chars");

  // Exceptions and RTTI.
  if (Args.hasFlag(options::OPT_fexceptions, options::OPT_fno_exceptions,
                   true) &&
      Args.hasFlag(options::OPT_fcxx_exceptions,
                   options::OPT_fno_cxx_exceptions, true))
    CmdArgs.push_back("--exceptions");
  else
    CmdArgs.push_back("--no_exceptions");

  if (Args.hasFlag(options::OPT_frtti, options::OPT_fno_rtti, true))
    CmdArgs.push_back("--rtti");
  else
    CmdArgs.push_back("--no_rtti");

  if (Args.hasArg(options::OPT_w))
    CmdArgs.push_back("--no_warnings");

  // Preprocessor defines, undefines, and user include paths.
  for (const Arg *A : Args.filtered(options::OPT_D, options::OPT_U)) {
    A->claim();
    StringRef Prefix = A->getOption().matches(options::OPT_D) ? "-D" : "-U";
    CmdArgs.push_back(Args.MakeArgString(Twine(Prefix) + A->getValue()));
  }

  for (const Arg *A : Args.filtered(options::OPT_I)) {
    A->claim();
    CmdArgs.push_back(Args.MakeArgString(Twine("-I") + A->getValue()));
  }

  for (const Arg *A : Args.filtered(options::OPT_isystem)) {
    A->claim();
    CmdArgs.push_back(
        Args.MakeArgString(Twine("--sys_include=") + A->getValue()));
  }

  for (const Arg *A : Args.filtered(options::OPT_include)) {
    A->claim();
    CmdArgs.push_back(
        Args.MakeArgString(Twine("--preinclude=") + A->getValue()));
  }

  // System and C++ standard library include paths from the toolchain.
  if (!Args.hasArg(options::OPT_nostdinc)) {
    ArgStringList StdIncArgs;
    if (!Args.hasArg(options::OPT_nostdincxx)) {
      if (Args.hasArg(options::OPT_stdlibxx_isystem))
        TC.AddClangCXXStdlibIsystemArgs(Args, StdIncArgs);
      else
        TC.AddClangCXXStdlibIncludeArgs(Args, StdIncArgs);
    }
    TC.AddClangSystemIncludeArgs(Args, StdIncArgs);
    for (size_t I = 0, E = StdIncArgs.size(); I < E; ++I) {
      StringRef Opt = StdIncArgs[I];
      if ((Opt == "-internal-isystem" || Opt == "-internal-externc-isystem" ||
           Opt == "-c-isystem" || Opt == "-cxx-isystem" || Opt == "-isystem") &&
          I + 1 < E) {
        CmdArgs.push_back(
            Args.MakeArgString(Twine("--sys_include=") + StdIncArgs[++I]));
      } else if (Opt.starts_with("-isystem")) {
        CmdArgs.push_back(Args.MakeArgString(
            Twine("--sys_include=") + Opt.drop_front(strlen("-isystem"))));
      }
    }
  }

  // Action-specific flags and output specification.
  if (isa<PreprocessJobAction>(JA)) {
    if (Args.hasArg(options::OPT_P))
      CmdArgs.push_back("-P");
    else
      CmdArgs.push_back("-E");
    if (Args.hasArg(options::OPT_C) || Args.hasArg(options::OPT_CC))
      CmdArgs.push_back("-C");
    if (Output.isFilename() && StringRef(Output.getFilename()) != "-") {
      CmdArgs.push_back("-o");
      CmdArgs.push_back(Output.getFilename());
    }
  } else if (Output.getType() == types::TY_Nothing) {
    CmdArgs.push_back("--no_code_gen");
  } else if (Output.isFilename()) {
    CmdArgs.push_back(
        Args.MakeArgString(Twine("--gen_c_file_name=") + Output.getFilename()));
  }

  // Forward raw -Xedg arguments.
  Args.AddAllArgValues(CmdArgs, options::OPT_Xedg);

  // Input file.
  const InputInfo &Input = Inputs[0];
  if (Input.isFilename())
    CmdArgs.push_back(Input.getFilename());
  else
    Input.getInputArg().renderAsInput(Args, CmdArgs);

  const char *Exec = D.getDriverProgramPath();
  if (D.CC1Main && !D.CCGenDiagnostics) {
    C.addCommand(std::make_unique<CC1Command>(
        JA, *this, ResponseFileSupport::AtFileUTF8(), Exec, CmdArgs, Inputs,
        Output, D.getPrependArg()));
  } else {
    C.addCommand(std::make_unique<Command>(
        JA, *this, ResponseFileSupport::AtFileUTF8(), Exec, CmdArgs, Inputs,
        Output, D.getPrependArg()));
  }
}
