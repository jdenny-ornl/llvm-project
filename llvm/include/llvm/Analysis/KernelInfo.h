//=- KernelInfo.h - Kernel Analysis -------------------------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the KernelInfo, KernelInfoAnalysis, and KernelInfoPrinter
// classes used to extract function properties from a kernel.
//
// Usage examples:
//
// $ opt -load-pass-plugin=$LLVM_DIR/lib/KernelInfo.so \
//       -pass-remarks=kernel-info -passes=kernel-info \
//       -disable-output test-openmp-nvptx64-nvidia-cuda.bc
//
// $ opt -load-pass-plugin=$LLVM_DIR/lib/KernelInfo.so \
//       -pass-remarks=kernel-info -passes='default<O1>' \
//       -disable-output test-openmp-nvptx64-nvidia-cuda.bc
//
// $ clang -fpass-plugin=$LLVM_DIR/lib/KernelInfo.so -Rpass=kernel-info -g \
//         -fopenmp -fopenmp-targets=nvptx64-nvidia-cuda test.c
//
// When this plugin is loaded, getKernelInfoPluginInfo in KernelInfo.cpp
// automatically inserts it into any LLVM pass list.  This behavior is most
// helpful when trying to run KernelInfoAnalysis using clang, which, unlike opt,
// seems to have no way to run a single LLVM pass by itself.
//
// How to load the plugin depends on the cmake variable
// LLVM_KERNELINFO_LINK_INTO_TOOLS, as defined by add_llvm_pass_plugin in
// ./CMakeLists.txt:
//
// - If set to On, then this plugin pass is linked statically, so it's always
//   loaded, whether using clang or opt.
// - Otherwise, this pass is a dynamically linked plugin, and something like
//   "opt -load-pass-plugin" or "clang -fpass-plugin" must be used to load it,
//   as in the above examples.
//
// opt, clang, etc. from forks of LLVM can sometimes successfully load and use
// this plugin even when this plugin is built as part of upstream LLVM sources.
// However, if a fork has diverged, the plugin might crash or otherwise
// misbehave.  Also, some clang forks have been known to produce altered debug
// metadata that this plugin cannot interpret and thus must ignore, limiting
// the info in the remarks it produces.
// ===---------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_KERNELINFO_H
#define LLVM_ANALYSIS_KERNELINFO_H

#include "llvm/Analysis/OptimizationRemarkEmitter.h"

namespace llvm {
class DominatorTree;
class Function;

/// Data structure holding function info for kernels.
class KernelInfo {
  void updateForBB(const BasicBlock &BB, int64_t Direction,
                   OptimizationRemarkEmitter &ORE);

public:
  static KernelInfo getKernelInfo(Function &F, FunctionAnalysisManager &FAM);

  bool operator==(const KernelInfo &FPI) const {
    return std::memcmp(this, &FPI, sizeof(KernelInfo)) == 0;
  }

  bool operator!=(const KernelInfo &FPI) const { return !(*this == FPI); }

  /// If false, nothing was recorded here because the supplied function didn't
  /// appear in a module compiled for a GPU.
  bool IsValid = false;

  /// The number of alloca instructions inside the function, the number of those
  /// with allocation sizes that cannot be determined at compile time, and the
  /// sum of the sizes that can be.
  ///
  /// With the current implementation for at least some GPU archs,
  /// AllocasDyn > 0 might not be possible, but we report AllocasDyn anyway in
  /// case the implementation changes.
  int64_t Allocas = 0;
  int64_t AllocasDyn = 0;
  int64_t AllocasStaticSizeSum = 0;

  /// Number of direct/indirect calls (anything derived from CallBase).
  int64_t DirectCalls = 0;
  int64_t IndirectCalls = 0;

  /// Number of direct calls made from this function to other functions
  /// defined in this module.
  int64_t DirectCallsToDefinedFunctions = 0;

  /// Number of calls of type InvokeInst.
  int64_t Invokes = 0;
};

/// Analysis class for KernelInfo.
class KernelInfoAnalysis : public AnalysisInfoMixin<KernelInfoAnalysis> {
public:
  static AnalysisKey Key;

  using Result = const KernelInfo;

  KernelInfo run(Function &F, FunctionAnalysisManager &FAM) {
    return KernelInfo::getKernelInfo(F, FAM);
  }
};

/// Printer pass for KernelInfoAnalysis.
///
/// It just calls KernelInfoAnalysis, which prints remarks if they are enabled.
class KernelInfoPrinter : public PassInfoMixin<KernelInfoPrinter> {
public:
  explicit KernelInfoPrinter() {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    AM.getResult<KernelInfoAnalysis>(F);
    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace llvm
#endif // LLVM_ANALYSIS_KERNELINFO_H
