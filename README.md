# KK-Project

LLVM optimization project implementing the following passes:

* Dead Argument Elimination
* Dead Store Elimination
* Instruction Combining
* Strength Reduction

Dead Argument Elimination is designed to run after Dead Store Elimination.

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
cd path/to/llvm-project/build

./bin/clang -S -emit-llvm -O0 -Xclang -disable-O0-optnone example.c -o example.ll

./bin/opt \
  -load ./build/lib/OurDSE.so \
  -load ./build/lib/OurDAE.so \
  -load ./build/lib/LLVMInstructionCombining.so \
  -load ./build/lib/LLVMStrengthReduction.so \
  -simple-dse \
  -simple-dae \
  -instruction-combining \
  -strength-reduction \
  example.ll \
  -o output.ll
```
