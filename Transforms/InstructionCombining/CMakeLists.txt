add_llvm_library(LLVMInstructionCombining MODULE BUILDTREE_ONLY
        InstructionCombining.cpp

        PLUGIN_TOOL
        opt
)