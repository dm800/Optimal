#include <stdio.h>

struct S {
    int a;
    int b;
};

int f(int a, int b) {
    return a * 2 - b;
}

int main() {
    int a = 3;
    int b = 8;
    int c = 0;
    int d = 1;

    while (b > 0) {
        c = c + a;
        b = b - 2;
        d = d * 2;
    }

    if (c != d) {
        a = c % d;
    } else {
        a = c + d;
    }

    int e[4];
    e[0] = a;
    e[1] = c;
    e[2] = d;
    e[3] = e[0] + e[1];

    int g = 15;
    int *h = &g;
    int i = *h + e[2];

    struct S j;
    j.a = i;
    j.b = e[3];

    struct S *k = &j;
    int l = k->a - k->b;

    int m = f(l, a);

    printf("%d\n", m);

    return 0;
}