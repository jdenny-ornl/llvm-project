# Based on llvm/examples/Bye/CMakeLists.txt, polly/lib/CMakeLists.txt, and
# llvm/cmake/modules/AddLLVM.cmake.

# See ./KernelInfoPlugin.cpp comments for why we hardcode this for now.
set(LLVM_KERNELINFOPLUGIN_LINK_INTO_TOOLS On)

add_llvm_pass_plugin(KernelInfoPlugin
  SUBPROJECT llvm
  KernelInfoPlugin.cpp
  DEPENDS
  intrinsics_gen
  BUILDTREE_ONLY
 )
