cmake_minimum_required(VERSION 3.13)

project(KKProject LANGUAGES CXX)

# Putanja do lokalnog LLVM source foldera.
# Svaki član tima podešava svoju putanju kroz CLion/CMake.
set(LLVM_SOURCE_DIR "" CACHE PATH "Path to llvm-project/llvm")

include_directories(
    ${LLVM_SOURCE_DIR}/include
)

add_library(DeadArgumentElimination STATIC
    Transforms/DeadArgumentElimination/DeadArgumentElimination.cpp
)

add_library(DeadStoreElimination STATIC
    Transforms/DeadStoreElimination/DeadStoreElimination.cpp
)

add_library(InstructionCombining STATIC
    Transforms/InstructionCombining/InstructionCombining.cpp
)

add_library(StrengthReduction STATIC
    Transforms/StrengthReduction/StrengthReduction.cpp
)
