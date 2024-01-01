// Comment

#pragma once

#include "file.c"
#include <stdio.h>
#if !(defined __cplusplus) && __STDC_VERSION__ < 202000
    #include <stdbool.h>
#endif

#define CLASSIC_MACRO thing
#define MACRO_NO_ARGS() other_thing
#define MACRO_ONE_ARG(arg) (arg + arg)
#define MACRO_TWO_ARGS(arg1, arg2) (arg1 + arg2)

#define MY_MACRO(str1, str2, ...) \
    do { \
        printf("%s and %s\n", str1, str2); \
        printf(__VA_ARGS__); \
    } while (false)

#warning "You're compiling file.c"

typedef void nothing_t;

struct data_in_there {
    nothing_t * ptr;
};

void * my_func(void * arg)
{
    return NULL;
}

int main(nothing_t)
{
    int a = 0x89;
#ifdef ERROR
    int b = 089; */ \ #
#endif
    float c = 89.f;
    char d = 'a';

/* Original
 * C
 * multiline
 * comment
 */

#if 0
    printf("This is disabled\n");
#endif

    unsigned int a2 = 0b0101U;
    long c2 = 0b0101LLu;
    long d2 = 0B0101ull;
#ifdef ERROR
    int b2 = 0b201u;
    int b3 = 0B061u;
#endif

    my_func(&c2);

    double const hexadecimal_floating_constant = 0x1.2p3;
    printf("Hello %s!\n", "world");
    printf("See these values: %04d, %04u, %.1lf\n", a, a2, hexadecimal_floating_constant);

    char const * gsv = "GtkSourceView";
    int pos;
    sscanf(gsv, "%*[Gtk]%n", &pos);
    printf("((%.*s))%s\n", pos, gsv, gsv + pos);

    MY_MACRO("1", "2", "%d %d\n", 3, 4);
}
