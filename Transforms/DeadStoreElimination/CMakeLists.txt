add_llvm_library(LLVMDeadStoreElimination MODULE BUILDTREE_ONLY
        DeadStoreElimination.cpp

        PLUGIN_TOOL
        opt
)