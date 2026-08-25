add_llvm_library(LLVMStrengthReduction MODULE BUILDTREE_ONLY
        StrengthReduction.cpp

        PLUGIN_TOOL
        opt
)