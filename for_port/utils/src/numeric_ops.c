#include <stdint.h>
#include <math.h>

#include "iterator.h"
#include "numeric_ops.h"


// --------------------------------- prime generation ------------------------------

bool                is_prime_miller(uint32_t n) {
    if (n < 2)
        return 0;
    if (n % 2 == 0)
        return n == 2;

    uint32_t d = n - 1, s = 0;
    while (d % 2 == 0)
        d /= 2, s++;

    //uint32_t bases[] = {2, 7, 61};
    //for (int i = 0; i < 3; i++) {  // TODO: via iterator
    foreachuint(i, 2, 7, 61) {
        uint32_t a = i % n;
        if (a == 0)
            continue;

        uint64_t x = 1, y = a, t = d;
        while (t) {
            if (t & 1)
                x = (x * y) % n;
            y = (y * y) % n;
            t >>= 1;
        }
        if (x == 1 || x == n - 1)
            continue;
        for (uint32_t r = 1; r < s; r++) {
            x = (x * x) % n;
            if (x == n - 1)
                break;
        }
        if (x != n - 1)
            return false;
    }
    return true;
}

unsigned            next_prime(unsigned n) {
    while (!is_prime_miller(n))
        n++;
    return n;
}

// --------------------------------- hash --------------------------

unsigned long hash_djb2(const char *str) {
    unsigned long hash = 5381;
    if (str) {
        int c;
        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

unsigned long hash_fnv1a(const char *str) {
    unsigned long hash = 14695981039346656037UL; /* FNV offset basis (64-bit) */
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 1099511628211UL; /* FNV prime (64-bit) */
    }
    return hash;
}

unsigned long hash_sdbm(const char *str) {
    unsigned long hash = 0;
    int c;
    while ((c = *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}

// -------------------------------Testing --------------------------
#ifdef NUMERIC_OPS_TESTING

#include "test.h"
#include "checker.h"

//types for testing

static unsigned          check_prime_by_divition(unsigned val){
    unsigned last = sqrt( (double) val);
    while (last > 1){
        if (val % last == 0)
            return last;
        last--;
    }
    return 1;       // OK
}

// ------------------------- TEST 1 ---------------------------------

static TestStatus
tf1(const char *name)
{
    logenter("%s", name);
    int         subnum = 0;

    test_sub("subtest %d: up to 10M", ++subnum);
    {
        const unsigned         MAX = 10000000;
        for (unsigned i = 3; i < MAX; i = next_prime(i + 1) ){
            unsigned res = check_prime_by_divition(i);
            test_validate(res == 1,
                "Div found %u, mod = %u", res, i % res);
        }
    }
    return logret(TEST_PASSED, "done"); // TEST_FAILED, TEST_PASSED, TEST_MANUAL
}
// ------------------------------------------------------------------------------------------------------------------------------
int
main( /* int argc, const char *argv[] */)
{
    logsimpleinit("Start");

    testenginestd(
        testnew(.f2 = tf1,  .num =  1, .name = "Simple prime_miller test"     , .desc=""                , .mandatory=true)

    );

    return logret(0, "end...");  // as replace of logclose()
}

#endif /* NUMERIC_OPS_TESTING */
