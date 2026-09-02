int unused_argument(int a, int b)
{
    return a;
}

int partially_unused(int a, int b, int c)
{
    int x = a + 10;
    return x;
}

int dead_stores(int a)
{
    int x;

    x = 10;
    x = 20;

    return x;
}

int multiple_dead_stores(int a)
{
    int x = 5;
    int y = 10;

    x = a + 1;
    y = x + 2;

    x = 100;

    return 42;
}

int useful_and_dead_stores(int a)
{
    int x;

    x = 10;
    x = a + 5;
    x = 20;

    return x;
}

int combined(int a, int b, int c)
{
    int x;

    x = a;
    x = 100;

    return b;
}

int only_first(int a, int b, int c)
{
    int x = a;

    return x;
}

int no_dead_store(int a)
{
    int x = a + 10;

    return x;
}

int g;
 
void fg() {
    g = 5;
}

void f(int *p) {
    *p = 5;
}
