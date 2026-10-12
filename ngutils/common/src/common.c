/**
 * @file common.c
 * @brief Implementation of the minimal common helpers.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "common.h"
#include "log.h"

/* -------------------------------------------------------------------------
 * Size growth
 * ------------------------------------------------------------------------- */

long
calcnewsize(Tincrease t, long n)
{
    long sz = n;

    switch (t) {
        case SIZE_NONE:
            break;

        case SIZE_MIN10:
            if (sz < 10)
                sz = 10;
            break;

        case SIZE_POWER2:
            sz = (long)round_up_2((unsigned long)sz);
            break;

        default:
            logsimple("Unknown size growth type %d", (int)t);
            sz = -1;
            break;
    }
    return sz;
}

/* -------------------------------------------------------------------------
 * C-string helpers
 * ------------------------------------------------------------------------- */

char *
reverse(char *s, int len)
{
    if (!s || len <= 1)
        return s;

    for (int i = 0; i < len / 2; i++) {
        char c          = s[i];
        s[i]            = s[len - 1 - i];
        s[len - 1 - i]  = c;
    }
    return s;
}

/* -------------------------------------------------------------------------
 * try_parse_*
 *
 * All use strtoX with full input consumption validation.
 * ------------------------------------------------------------------------- */

bool
try_parse_int(const char *restrict str, int *restrict res)
{
    if (!str || !res || *str == '\0')
        return false;

    errno = 0;
    char *end = NULL;
    long v = strtol(str, &end, 10);

    if (errno != 0 || end == str || *end != '\0')
        return false;
    if (v < INT_MIN || v > INT_MAX)
        return false;

    *res = (int)v;
    return true;
}

bool
try_parse_long(const char *restrict str, long *restrict res)
{
    if (!str || !res || *str == '\0')
        return false;

    errno = 0;
    char *end = NULL;
    long v = strtol(str, &end, 10);

    if (errno != 0 || end == str || *end != '\0')
        return false;

    *res = v;
    return true;
}

bool
try_parse_ulong(const char *restrict str, unsigned long *restrict res)
{
    if (!str || !res || *str == '\0')
        return false;

    /* Reject a leading minus explicitly: strtoul accepts it as a
     * valid unsigned wrap-around, which is almost never what the
     * caller meant. */
    const char *p = str;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '-')
        return false;

    errno = 0;
    char *end = NULL;
    unsigned long v = strtoul(str, &end, 10);

    if (errno != 0 || end == str || *end != '\0')
        return false;

    *res = v;
    return true;
}

bool
try_parse_double(const char *restrict str, double *restrict res)
{
    if (!str || !res || *str == '\0')
        return false;

    errno = 0;
    char *end = NULL;
    double v = strtod(str, &end);

    if (errno != 0 || end == str || *end != '\0')
        return false;

    *res = v;
    return true;
}

/* -------------------------------------------------------------------------
 * Byte comparators
 * ------------------------------------------------------------------------- */

int
pchar_cmp(const void *restrict a, const void *restrict b)
{
    unsigned char ca = *(const unsigned char *)a;
    unsigned char cb = *(const unsigned char *)b;
    return (ca > cb) - (ca < cb);
}

int
pchar_revcmp(const void *restrict a, const void *restrict b)
{
    return -pchar_cmp(a, b);
}
