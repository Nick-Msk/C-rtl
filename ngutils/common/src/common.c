/**
 * @file common.c
 * @brief Implementation of the minimal common helpers.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "common.h"
#include "log.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

static const char *const versions[] = {
    COMMON_VERSION,   /* current — always computed from macros */
    NULL
};

const char *const *
common_versions(void)
{
    return versions;
}

const char *
common_version(void)
{
    return versions[0];
}

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
 *  Comparators
 * ------------------------------------------------------------------------- */

// ------------------- Pointer comparators -------------------
// simple char comparator
int                             pchar_cmp(const void *restrict s1, const void *restrict s2){
    return compare_char( *(const unsigned char *) s1, *(const unsigned char *) s2);
}
// simple char reverse comparator
int                             pchar_revcmp(const void *s1, const void *s2){
    return -compare_char( *(const unsigned char *) s1, *(const unsigned char *) s2);
}
// simple comparator pointer int
int                             pint_cmp(const void *restrict i1, const void *restrict i2){ 
    return compare_int( *(const int *) i1, *(const int *) i2); 
}
// simple reverse comparator pointer int
int                             pint_revcmp(const void *i1, const void *i2){
    return -compare_int( *(const int *) i1, *(const int *) i2); 
}
// simple comparator pointer long
int                              plong_cmp(const void *l1, const void *l2){
    return compare_long( *(const long *) l1, *(const long *) l2);
}
// simple reverse comparator pointer long
int                             plong_revcmp(const void *l1, const void *l2){
    return -compare_long( *(const long *) l1, *(const long *) l2);
}
// simple comparator pointer to pointer
int                             pptr_cmp(const void *p1, const void *p2) {
    const void *pa = *(const void **) p1;
    const void *pb = *(const void **) p2;
    return compare_ptr(pa, pb);
}
// simple reverse comparator pointer to pointer
int                             pptr_revcmp(const void *p1, const void *p2){
    const void *pa = *(const void **) p1;
    const void *pb = *(const void **) p2;
    return -compare_ptr(pa, pb);
}
// item
int                             pitem_cmp(const void *restrict p1, const void *restrict p2, size_t sz) {
    return memcmp(p1, p2, sz);
}
// c-str wrappers
// normal
int                             pcstr_cmp(const void *restrict s1, const void *restrict s2) {
    const char *const *str1 = (const char *const *)s1;
    const char *const *str2 = (const char *const *)s2;
    return strcmp(*str1, *str2);
}
// insensitive
int                             pcstr_icmp(const void *restrict s1, const void *restrict s2) {
    const char *const *str1 = (const char *const *)s1;
    const char *const *str2 = (const char *const *)s2;
    return strcasecmp(*str1, *str2);
}
