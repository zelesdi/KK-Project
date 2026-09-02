add_llvm_library(OurIC MODULE BUILDTREE_ONLY
        InstructionCombining.cpp

        PLUGIN_TOOL
        opt
)
