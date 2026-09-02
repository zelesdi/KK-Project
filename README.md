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
git clone https://github.com/zelesdi/KK-Project
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
```

# DSE + DAE

```bash
./bin/clang \
  -S -emit-llvm -O0 \
  -Xclang -disable-O0-optnone \
  DSE_DAE_example.c -o dse_dae_tmp.ll

./bin/opt \
  -load ./lib/OurDSE.so \
  -load ./lib/OurDAE.so \
  -enable-new-pm=0 \
  -simple-dse -simple-dae \
  dse_dae_tmp.ll \
  -S -o dse_dae.ll
```

# IC

```bash
./bin/clang \
  -S -emit-llvm -O0 \
  -Xclang -disable-O0-optnone \
  IC_example.cpp -o ic_tmp.ll

./bin/opt \
  -load ./lib/OurIC.so \
  -enable-new-pm=0 \
  -simple-ic \
  ic_tmp.ll \
  -S -o ic.ll

./bin/opt \
  -passes=mem2reg \
  -S ic_tmp.ll -o ic.ll
```

# SR

```bash
./bin/clang \
  -S -emit-llvm -O0 \
  -Xclang -disable-O0-optnone \
  SR_example.c -o sr_tmp.ll

./bin/opt \
  -load ./lib/OurSR.so \
  -enable-new-pm=0 \
  -simple-sr \
  sr_tmp.ll \
  -S -o sr.ll
```
