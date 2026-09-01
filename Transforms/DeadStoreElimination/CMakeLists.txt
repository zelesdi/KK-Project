add_llvm_library(OurDSE MODULE BUILDTREE_ONLY
    DeadStoreElimination.cpp

    DEPENDS
    intrinsics_gen
    PLUGIN_TOOL
    opt
)