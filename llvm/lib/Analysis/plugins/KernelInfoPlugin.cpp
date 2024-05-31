//===- KernelInfoPlugin.cpp - Kernel Analysis Plugin ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the KernelInfoPlugin, which wraps KernelInfoAnalysis in a
// plugin pass.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/KernelInfoAnalysis.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

/// Plugin version of KernelInfoAnalysis.
///
/// When this plugin is loaded, getKernelInfoPluginPluginInfo in this file
/// automatically inserts it into any LLVM pass list.  This behavior is most
/// helpful when trying to run KernelInfoAnalysis using clang, which, unlike
/// opt, seems to have no way to run a single LLVM pass by itself.
/// TODO: In the future, once KernelInfoAnalysis is otherwise used somewhere
/// among the LLVM passes, this behavior will probably not be desirable, and the
/// value of this plugin generally will be questionable.
///
/// How to load the plugin depends on the cmake variable
/// LLVM_KERNELINFOPLUGIN_LINK_INTO_TOOLS, as defined by add_llvm_pass_plugin in
/// ./CMakeLists.txt:
///
/// - If set to On, then this plugin pass is linked statically, so it's always
///   loaded, whether using clang or opt.
/// - Otherwise, this pass is a dynamically linked plugin, and something like
///   "opt -load-pass-plugin" or "clang -fpass-plugin" must be used to load it.
///
/// TODO: Currently, it's hardcoded to the first mode in ./CMakeLists.txt to
/// facilitate development (avoids extra command-line options).  Ultimately, the
/// second mode seems cleaner (you have to opt in with extra command-line
/// options).  If we decide we want a dynamically linked plugin version of
/// KernelInfoAnalysis that works with forks of LLVM but is not built as part of
/// them, this plugin in the second mode might serve as an example, but we would
/// need to think about how KernelInfoAnalysis would be linked as it's not part
/// of this plugin.
///
/// Example usage, assuming the first mode:
///
/// $ clang -g -Rpass=kernel-info -fopenmp \
///         -fopenmp-targets=nvptx64-nvidia-cuda test.c
///
/// $ opt -pass-remarks=kernel-info -passes='default<O1>' \
///       -disable-output test-openmp-nvptx64-nvidia-cuda.bc

namespace llvm {
class KernelInfoPlugin : public PassInfoMixin<KernelInfoPlugin> {
public:
  explicit KernelInfoPlugin() {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    AM.getResult<KernelInfoAnalysis>(F);
    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};
} // namespace llvm

llvm::PassPluginLibraryInfo getKernelInfoPluginPluginInfo() {
  using namespace llvm;
  return {
      LLVM_PLUGIN_API_VERSION, "KernelInfoPlugin", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        PB.registerVectorizerStartEPCallback(
            [](llvm::FunctionPassManager &PM, llvm::OptimizationLevel Level) {
              PM.addPass(KernelInfoPlugin());
            });
      }};
}

// Used when built as dynamic plugin.
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getKernelInfoPluginPluginInfo();
}
