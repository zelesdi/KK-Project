unsigned int test(unsigned int a) {
    unsigned int r = 0;

    // Single power of two -> shl
    r += a * 2;
    r += a * 4;
    r += a * 8;
    r += a * 16;
    r += a * 32;

    // 2^n + 1 -> (a<<n)+a
    r += a * 3;
    r += a * 5;
    r += a * 9;
    r += a * 17;
    r += a * 33;
    r += a * 65;

    // 2^n - 1 -> (a<<n)-a
    r += a * 7;
    r += a * 15;
    r += a * 31;
    r += a * 63;

    // Sum of two powers -> (a<<p)+(a<<q)
    r += a * 6;
    r += a * 10;
    r += a * 12;
    r += a * 18;
    r += a * 20;
    r += a * 24;
    r += a * 34;
    r += a * 36;
    r += a * 40;
    r += a * 48;

    // Difference of two powers -> (a<<p)-(a<<q)
    r += a * 14;
    r += a * 28;
    r += a * 30;

    // --- NEGATIVE CONSTANTS ---
    // Trivial
    r += a * -1;   // -a

    // Single power of two
    r += a * -2;   // -(a<<1)
    r += a * -4;   // -(a<<2)
    r += a * -8;   // -(a<<3)
    r += a * -16;  // -(a<<4)
    r += a * -32;  // -(a<<5)

    // 2^n + 1
    r += a * -3;   // -((a<<1)+a)
    r += a * -5;   // -((a<<2)+a)
    r += a * -9;   // -((a<<3)+a)
    r += a * -17;  // -((a<<4)+a)
    r += a * -33;  // -((a<<5)+a)

    // 2^n - 1
    r += a * -7;   // -((a<<3)-a)
    r += a * -15;  // -((a<<4)-a)
    r += a * -31;  // -((a<<5)-a)
    r += a * -63;  // -((a<<6)-a)

    // Sum of two powers
    r += a * -6;   // -((a<<2)+(a<<1))
    r += a * -10;  // -((a<<3)+(a<<1))
    r += a * -12;  // -((a<<3)+(a<<2))
    r += a * -18;  // -((a<<4)+(a<<1))
    r += a * -20;  // -((a<<4)+(a<<2))
    r += a * -24;  // -((a<<4)+(a<<3))
    r += a * -34;  // -((a<<5)+(a<<1))
    r += a * -36;  // -((a<<5)+(a<<2))
    r += a * -40;  // -((a<<5)+(a<<3))
    r += a * -48;  // -((a<<5)+(a<<4))

    // Difference of two powers
    r += a * -14;  // -((a<<4)-(a<<1))
    r += a * -28;  // -((a<<5)-(a<<2))
    r += a * -30;  // -((a<<5)-(a<<1))

    // udiv -> lshr
    r += a / 2;
    r += a / 4;
    r += a / 8;
    r += a / 16;

    // urem -> and
    r += a % 2;
    r += a % 4;
    r += a % 8;
    r += a % 16;
    r += a % 32;

    return r;
}
