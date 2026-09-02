add_llvm_library(OurSR MODULE BUILDTREE_ONLY
        StrengthReduction.cpp

        PLUGIN_TOOL
        opt
)
