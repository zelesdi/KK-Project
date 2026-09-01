#include <stdio.h>
#include <stdint.h>

int main() {
    int32_t x = 4;

    // 1. Test Rule 1 (OpCanonicalization): C + X -> X + C
    //    Constant is written on the LEFT here on purpose, so we can see
    //    the pass move it to the right in the emitted IR.
    int32_t r1 = 5 + x;

    // 2. Test Rule 2 (SubToAddCanonicalization): X - C -> X + (-C)
    int32_t r2 = x - 5;

    // 3. Test Rule 3 (MultiplyToShiftStrengthReduction): X * C -> X << log2(C)
    //    8 is a power of two, so this should become a shift.
    int32_t r3 = x * 8;

    // 4. Test Rule 4 (NestedShiftFolding): (X << C1) << C2 -> X << (C1+C2)
    int32_t r4 = (x << 2) << 3;

    // 5. Test Rule 5 (ShiftBitwiseReassociation):
    //    (X | C1) << C2 -> (X << C2) | (C1 << C2)
    int32_t r5 = (x | 1) << 3;

    // 6. Test Rule 6 (ConstantAddReassociation): (X + C1) + C2 -> X + (C1+C2)
    int32_t r6 = (x + 3) + 5;

    // Print everything so the compiler is forced to keep the computations
    printf("Rule 1 (5 + x): %d\n", r1);
    printf("Rule 2 (x - 5): %d\n", r2);
    printf("Rule 3 (x * 8): %d\n", r3);
    printf("Rule 4 ((x << 2) << 3): %d\n", r4);
    printf("Rule 5 ((x | 1) << 3): %d\n", r5);
    printf("Rule 6 ((x + 3) + 5): %d\n", r6);

    return 0;
}
