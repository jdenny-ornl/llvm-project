//=- KernelInfoAnalysis.h - Kernel Analysis -----------------------*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the KernelInfo and KernelInfoAnalysis classes used to
// extract function properties from a kernel.
//
// TODO: What other properties from FunctionPropertiesAnalysis do we want?
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_KERNELINFOANALYSIS_H
#define LLVM_ANALYSIS_KERNELINFOANALYSIS_H

#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
class DominatorTree;
class Function;

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
  /// appear in a module compiled for offload.
  bool IsValid = false;

  /// The number of alloca instructions inside the function, the number of those
  /// with allocation sizes that cannot be determined at compile time, and the
  /// sum of the sizes that can be.
  int64_t AllocaCount = 0;
  int64_t AllocaDynCount = 0;
  int64_t AllocaStaticSizeSum = 0;

  /// Call related instructions.
  int64_t DirectCallCount = 0;
  int64_t IndirectCallCount = 0;

  /// Number of direct calls made from this function to other functions
  /// defined in this module.
  int64_t DirectCallsToDefinedFunctions = 0;
};

class KernelInfoAnalysis : public AnalysisInfoMixin<KernelInfoAnalysis> {

public:
  static AnalysisKey Key;

  using Result = const KernelInfo;

  KernelInfo run(Function &F, FunctionAnalysisManager &FAM) {
    return KernelInfo::getKernelInfo(F, FAM);
  }
};

/// Pass for KernelInfoAnalysis.
///
/// It just calls KernelInfoAnalysis, which prints remarks if they are enabled.
///
/// Example usage:
///
/// $ opt -pass-remarks=kernel-info -passes=kernel-info \
///       -disable-output test-openmp-nvptx64-nvidia-cuda.bc
class KernelInfoPass : public PassInfoMixin<KernelInfoPass> {
public:
  explicit KernelInfoPass() {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    AM.getResult<KernelInfoAnalysis>(F);
    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace llvm
#endif // LLVM_ANALYSIS_KERNELINFOANALYSIS_H
