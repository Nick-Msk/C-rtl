#include <criterion/criterion.h>

#include "test_fs2.h"

#include "fs2array.h"
#include "fs2_api.h"

/* -------------------------------------------------------------------------
 * Fixtures — mute stderr, init log
 * ------------------------------------------------------------------------- */

TestSuite(fs2_array, .init = test_setup, .fini = test_teardown);


Test(fs2_array, init_zero_canonical) {
    fs2array a = fs2arr_init(0);
    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ   ("len", 0, a.hwm);
    CHECK_EQ   ("sz",  0, a.size);
    fs2arr_free(&a);
}

Test(fs2_array, init_5_is_capacity_not_len) {
    fs2array a = fs2arr_init(5);

    CHECK_NOT_NULL("arr", a.arr);
    CHECK_EQ   ("len", 0, a.hwm);           /* NO live elements */
    CHECK_TRUE ("sz >= 5", a.size >= 5);      /* capacity for 5 */
    for (size_t i = 0; i < a.size; i++)
        CHECK_TRUE("slot FS2EMPTY", fs2_eq_empty(&a.arr[i]));

    fs2arr_free(&a);
}

Test(fs2_array, reserve_does_not_touch_len) {
    fs2array a = fs2arr_init(0);
    fs2arr_reserve(&a, 100);
    CHECK_EQ   ("len", 0, a.hwm);           /* still 0 */
    CHECK_TRUE ("sz >= 100", a.size >= 100);
    fs2arr_free(&a);
}

Test(fs2_array, reserve_initializes_all_new_slots) {
    fs2array a = fs2arr_init(0);
    fs2arr_reserve(&a, 5);                  /* need=5, newsz=16 (floor) */

    cr_assert(a.size >= 5, "sz=%zu", a.size);   /* contract, not implementation */
    cr_assert_eq(a.hwm, 0);                 /* reserve never sets len */

    for (size_t i = 0; i < a.size; i++)
        CHECK_TRUE("slot [i] FS2EMPTY", fs2_eq_empty(&a.arr[i]));   /* padding included */
    fs2arr_free(&a);
}

Test(fs2_array, reserve_noop_when_enough) {
    fs2array a = fs2arr_init(16);
    fs2     *orig = a.arr;
    size_t   cap  = a.size;
    a.hwm = 3;                              /* simulate some live elements */

    fs2arr_reserve(&a, 10);
    CHECK_PTR_EQ("arr unchanged", orig, a.arr);
    CHECK_EQ   ("sz unchanged",   cap,  a.size);
    CHECK_EQ   ("len unchanged",  3,    a.hwm);

    fs2arr_free(&a);
}

Test(fs2_array, clean_resets_len_and_slots) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    a.arr[0] = fs2_fromstr("one");
    a.arr[1] = fs2_fromstr("two");
    a.hwm    = 2;

    fs2arr_clean(&a);

    CHECK_EQ("len", 0, a.hwm);
    for (size_t i = 0; i < a.size; i++)
        CHECK_TRUE("slot FS2EMPTY", fs2_eq_empty(&a.arr[i]));

    fs2arr_clean(&a);                       /* idempotent */
    CHECK_EQ("len still 0", 0, a.hwm);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, free_releases_array) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    a.arr[0] = fs2_fromstr("x");
    a.hwm    = 1;

    fs2arr_free(&a);

    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ   ("len", 0, a.hwm);
    CHECK_EQ   ("sz",  0, a.size);

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

/* ---- hwm / count / size invariants ---- */

Test(fs2_array, init_5_shape) {
    fs2array a = fs2arr_init(5);
    CHECK_NOT_NULL("arr", a.arr);
    CHECK_EQ   ("hwm",   0, a.hwm);
    CHECK_TRUE ("size >= 5", a.size >= 5);
    CHECK_EQ   ("count", 0, a.count);
    for (size_t i = 0; i < a.size; i++)
        CHECK_TRUE("slot FS2EMPTY", fs2_eq_empty(&a.arr[i]));
    fs2arr_free(&a);
}

Test(fs2_array, reserve_does_not_touch_hwm_or_count) {
    fs2array a = fs2arr_init(0);
    fs2arr_reserve(&a, 100);
    CHECK_EQ("hwm",   0, a.hwm);
    CHECK_EQ("count", 0, a.count);
    CHECK_TRUE("size >= 100", a.size >= 100);
    fs2arr_free(&a);
}

/* ---- push ---- */

Test(fs2_array, push_sequential_hwm_eq_count) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(0);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
        CHECK_EQ("hwm",   (size_t)(i + 1), a.hwm);
        CHECK_EQ("count", (size_t)(i + 1), a.count);
    }
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach within hwm (hole fill): count++, hwm unchanged ---- */

Test(fs2_array, attach_in_hole_keeps_hwm) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);                /* hwm=3, count=3, slot[1] live */
    }
    /* force hole at slot[1] */
    fs2_free(&a.arr[1]);
    a.arr[1] = FS2EMPTY;
    a.count--;                              /* simulate proper bookkeeping */
    /* hwm=3, count=2, one hole */

    fs2 v = fs2_fromstr("fill");
    fs2arr_attach(&a, &v, 1);

    CHECK_EQ  ("hwm unchanged",   3, a.hwm);
    CHECK_EQ  ("count restored",  3, a.count);
    CHECK_STR_EQ("[1] filled", "fill", a.arr[1].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach beyond hwm: hwm extends, gaps become holes ---- */

Test(fs2_array, attach_beyond_hwm_extends_with_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    /* start empty */

    fs2 v = fs2_fromstr("X");
    fs2arr_attach(&a, &v, 5);               /* hwm = 6, count = 1, slots 0-4 holes */

    CHECK_EQ("hwm",   6, a.hwm);
    CHECK_EQ("count", 1, a.count);
    CHECK_STR_EQ("[5]", "X", a.arr[5].v);
    for (size_t i = 0; i < 5; i++)
        CHECK_TRUE("hole [i]", fs2_eq_empty(&a.arr[i]));

    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach to non-empty slot: SIGINT ---- */

Test(fs2_array, attach_over_nonempty_raise, .signal = SIGINT) {
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("live");
    fs2arr_push(&a, &v);                    /* slot[0] live */

    fs2 w = fs2_fromstr("x");
    fs2arr_attach(&a, &w, 0);               /* slot[0] not empty → userraise */
    fs2_free(&w);
    fs2arr_free(&a);
}

/* ---- attach idx >= size: SIGINT (not src->sz) ---- */

Test(fs2_array, attach_idx_ge_size_raise, .signal = SIGINT) {
    fs2array a = fs2arr_init(4);            /* size >= 4 */
    fs2 v = fs2_fromstr("x");               /* src->sz may be 16 */
    fs2arr_attach(&a, &v, 100);             /* 100 >= a->size → userraise */
    fs2_free(&v);
    fs2arr_free(&a);
}

/* ---- find_free_pos: hole-only scan [0, hwm) ---- */

Test(fs2_array, find_free_pos_no_hole_returns_neg1) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);                /* packed: hwm=5, count=5 */
    }
    CHECK_EQ("no hole", -1, fs2arr_find_tillhwm(&a));
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, find_free_pos_returns_first_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2_free(&a.arr[2]);
    a.arr[2] = FS2EMPTY;
    a.count--;

    CHECK_EQ("first hole", 2, fs2arr_find_tillhwm(&a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, find_free_pos_empty_returns_neg1) {
    fs2array a = fs2arr_init(16);           /* hwm=0 */
    CHECK_EQ("empty", -1, fs2arr_find_tillhwm(&a));
    fs2arr_free(&a);
}

Test(fs2_array, find_free_pos_null_aborts, .signal = SIGABRT) {
    fs2arr_find_tillhwm(NULL);
}

/* ---- clean resets hwm and count, keeps size ---- */

Test(fs2_array, clean_resets_hwm_count) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    size_t cap = a.size;

    fs2arr_clean(&a);

    CHECK_EQ  ("hwm",   0, a.hwm);
    CHECK_EQ  ("count", 0, a.count);
    CHECK_EQ  ("size",  cap, a.size);
    CHECK_NOT_NULL("arr", a.arr);

    fs2arr_free(&a);
    assert_balanced();
}

/* ---- push with BODYALLOC src ---- */

Test(fs2_array, push_bodyalloc_slot_loses_bodyalloc) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 *p = fs2_heap_fromstr("heap");

    fs2arr_push(&a, p);

    CHECK_STR_EQ ("slot", "heap", a.arr[0].v);
    CHECK_TRUE  ("slot ALLOC",       fs2_is_alloc(&a.arr[0]));
    CHECK_FALSE ("slot not bodyalloc", fs2_is_bodyalloc(&a.arr[0]));
    /* p is dangling — struct freed, buffer moved to a.arr[0] */

    fs2arr_free(&a);
    assert_balanced();
}


Test(fs2_array, find_tillhwm_no_hole_returns_neg1) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);                /* packed: hwm=5, count=5 */
    }
    CHECK_EQ("no hole", -1, fs2arr_find_tillhwm(&a));
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, find_tillhwm_returns_first_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2_free(&a.arr[2]);
    a.arr[2] = FS2EMPTY;
    a.count--;

    CHECK_EQ("first hole", 2, fs2arr_find_tillhwm(&a));
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, find_tillhwm_empty_returns_neg1) {
    fs2array a = FS2ARR();                  /* arr=NULL, hwm=0 */
    CHECK_EQ("empty", -1, fs2arr_find_tillhwm(&a));
    /* no fs2arr_free needed — nothing allocated */
}

Test(fs2_array, find_tillhwm_null_aborts, .signal = SIGABRT) {
    fs2arr_find_tillhwm(NULL);
}

/* ---- detachto: stack dst, leaves hole ---- */

Test(fs2_array, detachto_stack_leaves_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v1 = fs2_fromstr("A");
    fs2 v2 = fs2_fromstr("B");
    fs2 v3 = fs2_fromstr("C");
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2arr_push(&a, &v3);
    /* hwm=3, count=3 */

    fs2 tmp = FS2();
    fs2arr_detachto(&a, 1, &tmp);

    CHECK_STR_EQ("detached", "B", tmp.v);
    CHECK_FALSE("not BODYALLOC", fs2_is_bodyalloc(&tmp));
    CHECK_EQ   ("hwm unchanged", 3, a.hwm);   /* NO shift */
    CHECK_EQ   ("count",         2, a.count);
    CHECK_TRUE ("[1] hole", fs2_eq_empty(&a.arr[1]));
    CHECK_STR_EQ("[0]", "A", a.arr[0].v);     /* untouched */
    CHECK_STR_EQ("[2]", "C", a.arr[2].v);     /* NOT shifted */

    /* find_tillhwm detects the hole */
    CHECK_EQ("first hole", 1, fs2arr_find_tillhwm(&a));

    fs2_free(&tmp);
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- detachto: heap dst ---- */

Test(fs2_array, detachto_heap_dst) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("X");
    fs2arr_push(&a, &v);

    fs2 *p = fs2_heap();
    fs2arr_detachto(&a, 0, p);

    CHECK_STR_EQ("value",       "X", p->v);
    CHECK_TRUE ("BODYALLOC",    fs2_is_bodyalloc(p));
    CHECK_TRUE ("ALLOC",        fs2_is_alloc(p));
    CHECK_EQ   ("hwm unchanged", 1, a.hwm);
    CHECK_EQ   ("count",         0, a.count);
    CHECK_TRUE ("[0] hole",     fs2_eq_empty(&a.arr[0]));

    fs2_free(p);
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- detachto into dst with existing buffer ---- */

Test(fs2_array, detachto_into_dst_with_old_content) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 v = fs2_fromstr("new");
    fs2arr_push(&a, &v);

    fs2 tmp = fs2_fromstr("old");        /* own ALLOC buffer */
    fs2arr_detachto(&a, 0, &tmp);

    CHECK_STR_EQ("replaced", "new", tmp.v);   /* old buffer freed by fs2_move */
    CHECK_FS2VALID(1, &tmp);

    fs2_free(&tmp);
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- detachto guards ---- */

Test(fs2_array, detachto_idx_ge_hwm_returns_false) {
    fs2array a = fs2arr_init(16);
    fs2 tmp = FS2();
    cr_assert_not(fs2arr_detachto(&a, 0, &tmp));   /* hwm=0 */
    fs2arr_free(&a);
}

Test(fs2_array, detachto_already_empty_returns_false) {
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("live");
    fs2arr_push(&a, &v);
    fs2 tmp1 = FS2();
    cr_assert(fs2arr_detachto(&a, 0, &tmp1));
    fs2_free(&tmp1);

    fs2 tmp2 = FS2();
    cr_assert_not(fs2arr_detachto(&a, 0, &tmp2));  /* already empty */
    fs2arr_free(&a);
}

Test(fs2_array, detachto_null_dst_aborts, .signal = SIGABRT) {
    fs2array a = fs2arr_init(4);
    fs2arr_detachto(&a, 0, NULL);
    fs2arr_free(&a);
}

Test(fs2_array, detachto_null_arr_aborts, .signal = SIGABRT) {
    fs2 tmp = FS2();
    fs2arr_detachto(NULL, 0, &tmp);
}

/* ---- detach_new / heap_detach wrappers ---- */

Test(fs2_array, detach_new_returns_stack) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("val");
    fs2arr_push(&a, &v);

    fs2 tmp = fs2arr_detach_new(&a, 0);

    CHECK_STR_EQ("value",         "val", tmp.v);
    CHECK_FALSE("not BODYALLOC",  fs2_is_bodyalloc(&tmp));
    CHECK_TRUE ("ALLOC",          fs2_is_alloc(&tmp));
    CHECK_EQ   ("hwm",            1, a.hwm);
    CHECK_EQ   ("count",          0, a.count);
    CHECK_TRUE ("[0] hole",       fs2_eq_empty(&a.arr[0]));
    CHECK_FS2VALID(1, &tmp);

    fs2_free(&tmp);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, heap_detach_returns_heap) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("val");
    fs2arr_push(&a, &v);

    fs2 *p = fs2arr_heap_detach(&a, 0);

    cr_assert_not_null(p);
    CHECK_STR_EQ("value",       "val", p->v);
    CHECK_TRUE ("BODYALLOC",    fs2_is_bodyalloc(p));
    CHECK_TRUE ("ALLOC",        fs2_is_alloc(p));
    CHECK_EQ   ("count",        0, a.count);
    CHECK_TRUE ("[0] hole",     fs2_eq_empty(&a.arr[0]));
    CHECK_FS2VALID(1, p);

    fs2_free(p);
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- detach then attach reuses the hole (pool cycle) ---- */

Test(fs2_array, detach_then_attach_reuses_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    /* hwm=3, count=3 */

    fs2 tmp = fs2arr_detach_new(&a, 1);
    fs2_free(&tmp);
    /* hwm=3, count=2, hole@1 */

    ptrdiff_t hole = fs2arr_find_tillhwm(&a);
    cr_assert_eq(hole, 1);

    fs2 v = fs2_fromstr("fill");
    fs2arr_attach(&a, &v, (size_t)hole);

    CHECK_EQ  ("hwm unchanged", 3, a.hwm);
    CHECK_EQ  ("count",         3, a.count);
    CHECK_STR_EQ("[1]", "fill", a.arr[1].v);
    CHECK_EQ  ("no holes", -1, fs2arr_find_tillhwm(&a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, heap_detach_invalid_idx_raises_and_no_leak) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);           /* hwm=0 */

    TRY() {
        fs2 *p = fs2arr_heap_detach(&a, 0);   /* detachto false → raise */
        (void)p;
        cr_assert_fail("expected raise");
    } else {
        /* caught — p struct was freed by fs2_free in wrapper */
    }
    err_resetenv();

    fs2arr_free(&a);
    assert_balanced();                     /* leak-detected: 0 mismatch */
}

Test(fs2_array, detach_new_invalid_idx_raises_and_no_leak) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);

    TRY() {
        fs2 tmp = fs2arr_detach_new(&a, 0);
        (void)tmp;
        cr_assert_fail("expected raise");
    } else {
        /* caught — tmp was FS2(), nothing to free (fs2_free is no-op) */
    }
    err_resetenv();

    fs2arr_free(&a);
    assert_balanced();
}
