# KK-Project

LLVM optimization project implementing the following passes:

* Dead Argument Elimination
* Dead Store Elimination
* Instruction Combining
* Strength Reduction

## Setup

Clone the repository:

```bash
git clone <repository-url>
cd KK-Project
```

Make the setup script executable:

```bash
chmod +x Scripts/setup_llvm.sh
```

Run the setup script and provide the path to your `llvm-project` directory:

```bash
./Scripts/setup_llvm.sh /path/to/llvm-project
```

The script creates symbolic links for all optimization passes inside LLVM's `llvm/lib/Transforms` directory and adds them to its `CMakeLists.txt`.

## Build

Build LLVM using the existing build script:

```bash
cd /path/to/llvm-project
./make_llvm.sh
```

## CLion Setup

To enable LLVM headers and autocomplete in CLion, set the `LLVM_SOURCE_DIR` CMake option to your local LLVM source directory:

```text
-DLLVM_SOURCE_DIR=/path/to/llvm-project/llvm
```

## Run Passes

```bash
./build/bin/opt \
  -load ./build/lib/LLVMDeadArgumentElimination.so \
  -load ./build/lib/LLVMDeadStoreElimination.so \
  -load ./build/lib/LLVMInstructionCombining.so \
  -load ./build/lib/LLVMStrengthReduction.so \
  -dead-argument-elimination \
  -dead-store-elimination \
  -instruction-combining \
  -strength-reduction \
  input.ll \
  -o output.ll
```
