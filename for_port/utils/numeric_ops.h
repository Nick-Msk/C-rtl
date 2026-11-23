#ifndef _NUMERIC_OPS_H
#define _NUMERIC_OPS_H

#include <math.h>
#include <stdint.h>
#include "bool.h"

/***************************************************************
                    USEFUL NUMERIC MACRO AND FUNCTIONS
***************************************************************/


static inline int           isprime(int n) // assuming n > 1
{
    int i, root;
    if (n % 2 == 0 || n % 3 == 0)
        return false;
    root = (int) sqrt(n + 0.0);
    for (i = 5; i <= root; i += 6)
    {
        if (n % i == 0)
           return false;
    }
    for (i = 7; i <= root; i += 6)
    {
        if (n % i == 0)
           return 0;
    }
    return 1;
}

static inline int           calc_next_prime(int num){
    while (!isprime(num) )
        num++;
    return num;
}

// adv API for prime number
extern unsigned             next_prime(unsigned n);

extern bool                 is_prime_miller(uint32_t n);

// hash
extern unsigned long        hash_djb2(const char *str);

extern unsigned long        hash_fnv1a(const char *str);

extern unsigned long        hash_sdbm(const char *str);

static inline uint64_t      hash_long(int64_t x) {
    uint64_t z = (uint64_t) x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;    // Magic number omg
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    return z;
}

#endif /* !_NUMERIC_OPS_H */

