#include <stdio.h>

int foo(int a, int b)
{
    int x;

    x = a;
    x = 10;

    return x;
}

int main()
{
    return foo(5, 20);
}