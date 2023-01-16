// Comment

#include <stdio.h>

int main (void)
{
    int a = 0x89;
    int b = 089;
    int c = 89.;
    int d = 'a';

    int a = 0b0101U;
    int c = 0b0101L;
    int d = 0B0101L;
    int b = 0b201u;
    int b = 0B061u;

    double hexadecimal_floating_constant = 0x1.2p3;
    printf ("Hello %s!\n", "world");
    return 0;
}
