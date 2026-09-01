add_llvm_library(OurDAE MODULE BUILDTREE_ONLY
        DeadArgumentElimination.cpp

        PLUGIN_TOOL
        opt
)