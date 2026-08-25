#!/bin/bash

set -e

# Putanja do foldera gde se nalazi ova skripta
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Root folder KK-Project repozitorijuma
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Ako korisnik nije prosledio putanju
if [ $# -ne 1 ]; then
    echo "Usage:"
    echo "  ./setup_llvm.sh /path/to/llvm-project"
    exit 1
fi

LLVM_PROJECT="$1"
LLVM_TRANSFORMS="$LLVM_PROJECT/llvm/lib/Transforms"

# Provera da li postoji LLVM Transforms folder
if [ ! -d "$LLVM_TRANSFORMS" ]; then
    echo "Error: LLVM Transforms directory not found:"
    echo "  $LLVM_TRANSFORMS"
    exit 1
fi

echo "KK-Project:"
echo "  $PROJECT_ROOT"

echo "LLVM Transforms:"
echo "  $LLVM_TRANSFORMS"

echo ""

# Lista svih optimizacija
PASSES=(
    "DeadArgumentElimination"
    "DeadStoreElimination"
    "InstructionCombining"
    "StrengthReduction"
)

# --------------------------------------------------
# 1. Kreiranje symbolic linkova
# --------------------------------------------------

echo "Creating symbolic links..."

for PASS in "${PASSES[@]}"; do

    SOURCE="$PROJECT_ROOT/Transforms/$PASS"
    DESTINATION="$LLVM_TRANSFORMS/$PASS"

    if [ ! -d "$SOURCE" ]; then
        echo "Error: Pass directory not found:"
        echo "  $SOURCE"
        exit 1
    fi

    # Ako već postoji symbolic link
    if [ -L "$DESTINATION" ]; then
        echo "Removing existing symbolic link:"
        echo "  $DESTINATION"

        rm "$DESTINATION"

    # Ako postoji pravi folder, ne diraj ga automatski
    elif [ -e "$DESTINATION" ]; then
        echo "Error: Destination already exists and is not a symbolic link:"
        echo "  $DESTINATION"
        echo ""
        echo "Remove or rename it manually first."
        exit 1
    fi

    ln -s "$SOURCE" "$DESTINATION"

    echo "Linked:"
    echo "  $DESTINATION -> $SOURCE"

done

echo ""

# --------------------------------------------------
# 2. Dodavanje add_subdirectory u LLVM CMakeLists.txt
# --------------------------------------------------

CMAKE_FILE="$LLVM_TRANSFORMS/CMakeLists.txt"

echo "Updating:"
echo "  $CMAKE_FILE"

echo ""

# Marker koji pokazuje gde počinje naš deo
BEGIN_MARKER="# BEGIN KK-PROJECT PASSES"
END_MARKER="# END KK-PROJECT PASSES"

# Ako marker već postoji, obriši stari blok
if grep -q "$BEGIN_MARKER" "$CMAKE_FILE"; then

    echo "Existing KK-Project CMake block found."
    echo "Removing old block..."

    sed -i "/$BEGIN_MARKER/,/$END_MARKER/d" "$CMAKE_FILE"

fi

# Dodaj novi blok na kraj
cat >> "$CMAKE_FILE" << EOF

$BEGIN_MARKER

add_subdirectory(DeadArgumentElimination)
add_subdirectory(DeadStoreElimination)
add_subdirectory(InstructionCombining)
add_subdirectory(StrengthReduction)

$END_MARKER
EOF

echo ""
echo "Setup completed successfully!"
echo ""
echo "Your passes are now available in:"
echo "  $LLVM_TRANSFORMS"
echo ""
echo "Next step:"
echo "  cd $LLVM_PROJECT"
echo "  ./make_llvm.sh"
