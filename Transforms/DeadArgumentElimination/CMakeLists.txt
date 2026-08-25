add_llvm_library(LLVMDeadArgumentElimination MODULE BUILDTREE_ONLY
        DeadArgumentElimination.cpp

        PLUGIN_TOOL
        opt
)