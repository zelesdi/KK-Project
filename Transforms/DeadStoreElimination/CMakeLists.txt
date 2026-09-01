add_llvm_library(OurDSE MODULE BUILDTREE_ONLY
        DeadStoreElimination.cpp

        PLUGIN_TOOL
        opt
)
