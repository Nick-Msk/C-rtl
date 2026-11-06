#include <criterion/criterion.h>

#include "test_fs2.h"

#include "fs2array.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Fixtures — mute stderr, init log
 * ------------------------------------------------------------------------- */

TestSuite(fs2_array, .init = test_setup, .fini = test_teardown);


Test(fs2_array, init_zero_canonical) {
    fs2array a = fs2arr_init(0);
    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ   ("len", 0, a.len);
    CHECK_EQ   ("sz",  0, a.sz);
    fs2arr_free(&a);
}

Test(fs2_array, init_5_is_capacity_not_len) {
    fs2array a = fs2arr_init(5);

    CHECK_NOT_NULL("arr", a.arr);
    CHECK_EQ   ("len", 0, a.len);           /* NO live elements */
    CHECK_TRUE ("sz >= 5", a.sz >= 5);      /* capacity for 5 */
    for (size_t i = 0; i < a.sz; i++)
        CHECK_TRUE("slot FS2EMPTY", fs2_eq_empty(&a.arr[i]));

    fs2arr_free(&a);
}

Test(fs2_array, reserve_does_not_touch_len) {
    fs2array a = fs2arr_init(0);
    fs2arr_reserve(&a, 100);
    CHECK_EQ   ("len", 0, a.len);           /* still 0 */
    CHECK_TRUE ("sz >= 100", a.sz >= 100);
    fs2arr_free(&a);
}

Test(fs2_array, reserve_initializes_all_new_slots) {
    fs2array a = fs2arr_init(0);
    fs2arr_reserve(&a, 5);                  /* need=5, newsz=16 (floor) */

    cr_assert(a.sz >= 5, "sz=%zu", a.sz);   /* contract, not implementation */
    cr_assert_eq(a.len, 0);                 /* reserve never sets len */
    
    for (size_t i = 0; i < a.sz; i++)
        CHECK_TRUE("slot [i] FS2EMPTY", fs2_eq_empty(&a.arr[i]));   /* padding included */
    fs2arr_free(&a);
}

Test(fs2_array, reserve_noop_when_enough) {
    fs2array a = fs2arr_init(16);
    fs2     *orig = a.arr;
    size_t   cap  = a.sz;
    a.len = 3;                              /* simulate some live elements */

    fs2arr_reserve(&a, 10);
    CHECK_PTR_EQ("arr unchanged", orig, a.arr);
    CHECK_EQ   ("sz unchanged",   cap,  a.sz);
    CHECK_EQ   ("len unchanged",  3,    a.len);

    fs2arr_free(&a);
}

Test(fs2_array, clean_resets_len_and_slots) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    a.arr[0] = fs2_fromstr("one");
    a.arr[1] = fs2_fromstr("two");
    a.len    = 2;

    fs2arr_clean(&a);

    CHECK_EQ("len", 0, a.len);
    for (size_t i = 0; i < a.sz; i++)
        CHECK_TRUE("slot FS2EMPTY", fs2_eq_empty(&a.arr[i]));

    fs2arr_clean(&a);                       /* idempotent */
    CHECK_EQ("len still 0", 0, a.len);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, free_releases_array) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    a.arr[0] = fs2_fromstr("x");
    a.len    = 1;

    fs2arr_free(&a);

    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ   ("len", 0, a.len);
    CHECK_EQ   ("sz",  0, a.sz);

    fs2arr_free(&a);                        /* idempotent */
    CHECK_PTR_EQ("arr still NULL", NULL, a.arr);
    assert_balanced();
}

Test(fs2_array, reserve_overflow_sigint, .signal = SIGINT) {
    fs2array a = FS2ARR();
    fs2arr_reserve(&a, SIZE_MAX);   /* overflow → sysraise → SIGINT */
}

Test(fs2_array, init_overflow_sigint, .signal = SIGINT) {
    fs2arr_init(SIZE_MAX);
}
