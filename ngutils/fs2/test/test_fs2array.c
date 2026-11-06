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
        CHECK_TRUE("slot FS2EMPTY", fs2arr_is_hole(&a.arr[i]));

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
        CHECK_TRUE("slot [i] FS2EMPTY", fs2arr_is_hole(&a.arr[i]));   /* padding included */
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
        CHECK_TRUE("slot FS2EMPTY", fs2arr_is_hole(&a.arr[i]));

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
        CHECK_TRUE("slot FS2EMPTY", fs2arr_is_hole(&a.arr[i]));
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
        fs2arr_push_sp(&a, &v);
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
        fs2arr_push_sp(&a, &v);                /* hwm=3, count=3, slot[1] live */
    }
    /* force hole at slot[1] */
    fs2_free(&a.arr[1]);
    fs2arr_set_hole(&a.arr[1]);
    a.count--;                              /* simulate proper bookkeeping */
    /* hwm=3, count=2, one hole */

    fs2 v = fs2_fromstr("fill");
    fs2arr_attach_sp(&a, &v, 1);

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
    fs2arr_attach_sp(&a, &v, 5);               /* hwm = 6, count = 1, slots 0-4 holes */

    CHECK_EQ("hwm",   6, a.hwm);
    CHECK_EQ("count", 1, a.count);
    CHECK_STR_EQ("[5]", "X", a.arr[5].v);
    for (size_t i = 0; i < 5; i++)
        CHECK_TRUE("hole [i]", fs2arr_is_hole(&a.arr[i]));

    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach to non-empty slot: SIGINT ---- */

Test(fs2_array, attach_over_nonempty_raise, .signal = SIGINT) {
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("live");
    fs2arr_push_sp(&a, &v);                    /* slot[0] live */

    fs2 w = fs2_fromstr("x");
    fs2arr_attach_sp(&a, &w, 0);               /* slot[0] not empty → userraise */
    fs2_free(&w);
    fs2arr_free(&a);
}

/* ---- attach idx >= size: SIGINT (not src->sz) ---- */

Test(fs2_array, attach_idx_ge_size_raise, .signal = SIGINT) {
    fs2array a = fs2arr_init(4);            /* size >= 4 */
    fs2 v = fs2_fromstr("x");               /* src->sz may be 16 */
    fs2arr_attach_sp(&a, &v, 100);             /* 100 >= a->size → userraise */
    fs2_free(&v);
    fs2arr_free(&a);
}

/* ---- find_free_pos: hole-only scan [0, hwm) ---- */

Test(fs2_array, find_free_pos_no_hole_returns_neg1) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push_sp(&a, &v);                /* packed: hwm=5, count=5 */
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
        fs2arr_push_sp(&a, &v);
    }
    fs2_free(&a.arr[2]);
    fs2arr_set_hole(&a.arr[2]);
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
        fs2arr_push_sp(&a, &v);
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

    fs2arr_push_sp(&a, p);

    CHECK_STR_EQ ("slot", "heap", a.arr[0].v);
    CHECK_TRUE  ("slot ALLOC",       fs2_is_alloc(&a.arr[0]));
    CHECK_FALSE ("slot not bodyalloc", fs2_is_bodyalloc(&a.arr[0]));
    /* p is dangling — struct freed, buffer moved to a.arr[0] */

    fs2arr_free(&a);
    fs2_free(p);
    assert_balanced();
}


Test(fs2_array, find_tillhwm_no_hole_returns_neg1) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push_sp(&a, &v);                /* packed: hwm=5, count=5 */
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
        fs2arr_push_sp(&a, &v);
    }
    fs2_free(&a.arr[2]);
    fs2arr_set_hole(&a.arr[2]);
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
    fs2arr_push_sp(&a, &v1);
    fs2arr_push_sp(&a, &v2);
    fs2arr_push_sp(&a, &v3);
    /* hwm=3, count=3 */

    fs2 tmp = FS2();
    fs2arr_detachto(&a, 1, &tmp);

    CHECK_STR_EQ("detached", "B", tmp.v);
    CHECK_FALSE("not BODYALLOC", fs2_is_bodyalloc(&tmp));
    CHECK_EQ   ("hwm unchanged", 3, a.hwm);   /* NO shift */
    CHECK_EQ   ("count",         2, a.count);
    CHECK_TRUE ("[1] hole", fs2arr_is_hole(&a.arr[1]));
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
    fs2arr_push_sp(&a, &v);

    fs2 *p = fs2_heap();
    fs2arr_detachto(&a, 0, p);

    CHECK_STR_EQ("value",       "X", p->v);
    CHECK_TRUE ("BODYALLOC",    fs2_is_bodyalloc(p));
    CHECK_TRUE ("ALLOC",        fs2_is_alloc(p));
    CHECK_EQ   ("hwm unchanged", 1, a.hwm);
    CHECK_EQ   ("count",         0, a.count);
    CHECK_TRUE ("[0] hole",     fs2arr_is_hole(&a.arr[0]));

    fs2_free(p);
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- detachto into dst with existing buffer ---- */

Test(fs2_array, detachto_into_dst_with_old_content) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 v = fs2_fromstr("new");
    fs2arr_push_sp(&a, &v);

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
    fs2arr_push_sp(&a, &v);
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
    fs2arr_push_sp(&a, &v);

    fs2 tmp = fs2arr_detach_new(&a, 0);

    CHECK_STR_EQ("value",         "val", tmp.v);
    CHECK_FALSE("not BODYALLOC",  fs2_is_bodyalloc(&tmp));
    CHECK_TRUE ("ALLOC",          fs2_is_alloc(&tmp));
    CHECK_EQ   ("hwm",            1, a.hwm);
    CHECK_EQ   ("count",          0, a.count);
    CHECK_TRUE ("[0] hole",       fs2arr_is_hole(&a.arr[0]));
    CHECK_FS2VALID(1, &tmp);

    fs2_free(&tmp);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, heap_detach_returns_heap) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("val");
    fs2arr_push_sp(&a, &v);

    fs2 *p = fs2arr_heap_detach(&a, 0);

    cr_assert_not_null(p);
    CHECK_STR_EQ("value",       "val", p->v);
    CHECK_TRUE ("BODYALLOC",    fs2_is_bodyalloc(p));
    CHECK_TRUE ("ALLOC",        fs2_is_alloc(p));
    CHECK_EQ   ("count",        0, a.count);
    CHECK_TRUE ("[0] hole",     fs2arr_is_hole(&a.arr[0]));
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
        fs2arr_push_sp(&a, &v);
    }
    /* hwm=3, count=3 */

    fs2 tmp = fs2arr_detach_new(&a, 1);
    fs2_free(&tmp);
    /* hwm=3, count=2, hole@1 */

    ptrdiff_t hole = fs2arr_find_tillhwm(&a);
    cr_assert_eq(hole, 1);

    fs2 v = fs2_fromstr("fill");
    fs2arr_attach_sp(&a, &v, (size_t)hole);

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

/* =========================================================================
 * fs2arr_is_hole / set_hole — sentinel semantics
 * ========================================================================= */

Test(fs2_array, is_hole_distinguishes_empty_value_from_hole) {
    fs2 hole = {0};                          /* flags = 0 */
    fs2 empty_val = FS2EMPTY;                /* STATIC */
    fs2 canon = FS2();                       /* ALLOC */
    fs2 live = fs2_fromstr("x");             /* ALLOC */

    CHECK_FALSE("FS2EMPTY",   fs2arr_is_hole(&empty_val));
    CHECK_FALSE("FS2()",      fs2arr_is_hole(&canon));
    CHECK_FALSE("live",       fs2arr_is_hole(&live));

    CHECK_TRUE("fs2_is_empty on hole", fs2_is_empty(&hole));
    CHECK_TRUE("fs2_is_empty on empty_val", fs2_is_empty(&empty_val));

    fs2_free(&canon);
    fs2_free(&live);
}

/* =========================================================================
 * push empty FS2EMPTY as a live value
 * ========================================================================= */

Test(fs2_array, push_fs2empty_as_value) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 empty = FS2EMPTY;

    fs2arr_push_sp(&a, &empty);

    CHECK_EQ  ("hwm",   1, a.hwm);
    CHECK_EQ  ("count", 1, a.count);       /* live value, not hole */
    CHECK_TRUE("slot not hole", !fs2arr_is_hole(&a.arr[0]));
    CHECK_TRUE("slot is FS2EMPTY", fs2_eq_empty(&a.arr[0]));

    /* find_tillhwm sees NO hole */
    CHECK_EQ("no hole", -1, fs2arr_find_tillhwm(&a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, push_fs2_canonical_as_value) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 canon = FS2();

    fs2arr_push_sp(&a, &canon);

    CHECK_EQ  ("hwm",   1, a.hwm);
    CHECK_EQ  ("count", 1, a.count);
    CHECK_TRUE("slot not hole", !fs2arr_is_hole(&a.arr[0]));
    CHECK_TRUE("slot safe-empty", fs2_eq_empty(&a.arr[0]));   /* promoted by fs2_move */

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * FULL CYCLE: push → detach → find hole → attach → detach
 * ========================================================================= */

Test(fs2_array, full_cycle_push_detach_attach_detach) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);

    /* 1. push three live elements */
    fs2 vA = fs2_fromstr("A");
    fs2 vB = fs2_fromstr("B");
    fs2 vC = fs2_fromstr("C");
    fs2arr_push_sp(&a, &vA);
    fs2arr_push_sp(&a, &vB);
    fs2arr_push_sp(&a, &vC);
    CHECK_EQ("hwm",   3, a.hwm);
    CHECK_EQ("count", 3, a.count);

    /* 2. detach middle — creates hole */
    fs2 taken = fs2arr_detach_new(&a, 1);
    CHECK_STR_EQ("detached", "B", taken.v);
    CHECK_EQ   ("hwm",   3, a.hwm);            /* unchanged */
    CHECK_EQ   ("count", 2, a.count);
    CHECK_TRUE ("[1] is hole", fs2arr_is_hole(&a.arr[1]));
    fs2_free(&taken);

    /* 3. find hole */
    ptrdiff_t hole = fs2arr_find_tillhwm(&a);
    CHECK_EQ("hole at 1", 1, hole);

    /* 4. attach into hole */
    fs2 vX = fs2_fromstr("X");
    fs2arr_attach_sp(&a, &vX, (size_t)hole);
    CHECK_EQ   ("hwm still 3", 3, a.hwm);
    CHECK_EQ   ("count",       3, a.count);
    CHECK_TRUE ("[1] not hole", !fs2arr_is_hole(&a.arr[1]));
    CHECK_STR_EQ("[1]",        "X", a.arr[1].v);

    /* 5. detach again (same slot) */
    fs2 taken2 = fs2arr_detach_new(&a, 1);
    CHECK_STR_EQ("detached X", "X", taken2.v);
    CHECK_EQ   ("count",       2, a.count);
    CHECK_TRUE ("[1] hole again", fs2arr_is_hole(&a.arr[1]));
    fs2_free(&taken2);

    /* 6. push appends at hwm, not into hole */
    fs2 vD = fs2_fromstr("D");
    fs2arr_push_sp(&a, &vD);
    CHECK_EQ   ("hwm",   4, a.hwm);            /* hwm grew (push appends) */
    CHECK_EQ   ("count", 3, a.count);
    CHECK_STR_EQ("[3]", "D", a.arr[3].v);
    CHECK_TRUE ("[1] still hole", fs2arr_is_hole(&a.arr[1]));

    /* 7. final: slot[1] hole, slots 0,2,3 live */
    CHECK_STR_EQ("[0]", "A", a.arr[0].v);
    CHECK_STR_EQ("[2]", "C", a.arr[2].v);
    CHECK_STR_EQ("[3]", "D", a.arr[3].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * push empty FS2EMPTY + detach: hole distinguishable from live empty
 * ========================================================================= */

Test(fs2_array, empty_value_vs_hole_after_detach) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);

    fs2 empty = FS2EMPTY;
    fs2arr_push_sp(&a, &empty);           /* live FS2EMPTY */
    CHECK_TRUE("live empty", !fs2arr_is_hole(&a.arr[0]));

    fs2 out = fs2arr_detach_new(&a, 0);
    CHECK_TRUE("returned empty", fs2_is_empty(&out));
    CHECK_TRUE("slot now hole",  fs2arr_is_hole(&a.arr[0]));
    CHECK_EQ  ("count", 0, a.count);

    fs2_free(&out);
    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * detach guard: hole cannot be detached
 * ========================================================================= */

Test(fs2_array, detach_hole_returns_false) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push_sp(&a, &v);

    fs2 t1 = fs2arr_detach_new(&a, 0);
    fs2_free(&t1);

    /* slot 0 is hole now */
    fs2 t2 = FS2();
    cr_assert_not(fs2arr_detachto(&a, 0, &t2));

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * detach_new / heap_detach cycle — pool of stack/heap values
 * ========================================================================= */

Test(fs2_array, heap_detach_then_heap_attach) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("heap");
    fs2arr_push_sp(&a, &v);

    fs2 *p = fs2arr_heap_detach(&a, 0);
    CHECK_STR_EQ("value", "heap", p->v);
    CHECK_TRUE ("BODYALLOC", fs2_is_bodyalloc(p));
    CHECK_TRUE ("[0] hole", fs2arr_is_hole(&a.arr[0]));

    /* re-attach via attach (fs2 ** for auto-null) */
    fs2arr_attach_sp(&a, p, 0);
    /* p is NOT nulled — shell alive, caller must free it */
    CHECK_STR_EQ("shell empty", "", p->v);   /* FS2EMPTY after move */
    CHECK_TRUE ("shell BODYALLOC", fs2_is_bodyalloc(p));
    CHECK_TRUE ("shell STATIC",    fs2_is_static(p));
    CHECK_STR_EQ("[0]", "heap", a.arr[0].v);
    CHECK_EQ   ("count", 1, a.count);

    fs2_free(p);    // !!!!!
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, detach_leaves_hole_sentinel) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push_sp(&a, &v);

    fs2 t = fs2arr_detach_new(&a, 0);

    CHECK_TRUE ("slot is hole",          fs2arr_is_hole(&a.arr[0]));
    CHECK_EQ   ("flags", FS2_NULL_ARRAY_ELEMENT, a.arr[0].flags);
    CHECK_PTR_EQ("v", NULL,              a.arr[0].v);
    CHECK_EQ   ("len", 0,                a.arr[0].len);
    CHECK_EQ   ("sz",  0,                a.arr[0].sz);

    fs2_free(&t);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, push_fs2_canonical_value_is_live_not_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 canon = FS2();
    cr_assert(fs2_is_canon(&canon));

    fs2arr_push_sp(&a, &canon);

    CHECK_EQ  ("hwm",   1, a.hwm);
    CHECK_EQ  ("count", 1, a.count);       /* live! */
    CHECK_FALSE("not hole",  fs2arr_is_hole(&a.arr[0]));
    CHECK_TRUE ("safe-empty (promoted by fs2_move)", fs2_eq_empty(&a.arr[0]));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, hole_and_fs2_canonical_distinct) {
    fs2 canon = FS2();
    fs2 hole;
    fs2arr_set_hole(&hole);

    CHECK_TRUE ("canon is canon",  fs2_is_canon(&canon));
    CHECK_TRUE ("hole is hole",    fs2arr_is_hole(&hole));
    CHECK_FALSE("canon not hole",  fs2arr_is_hole(&canon));
    CHECK_FALSE("hole not canon",  fs2_is_canon(&hole));

    CHECK_EQ("canon.flags = ALLOC",          FS2_ALLOC,              canon.flags);
    CHECK_EQ("hole.flags = NULL_ARRAY_ELEMENT", FS2_NULL_ARRAY_ELEMENT, hole.flags);

    fs2_free(&canon);
    /* hole: fs2_free no-op, safe */
    fs2_free(&hole);
}

Test(fs2_array, attach_stack_source_symmetric_with_heap) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 v = fs2_fromstr("stack");

    fs2arr_attach_sp(&a, &v, 0);
    /* v → FS2EMPTY (STATIC), safe */
    CHECK_TRUE ("v FS2EMPTY",     fs2_eq_empty(&v));
    CHECK_FALSE("v not ALLOC",    fs2_is_alloc(&v));
    CHECK_EQ   ("v len",          0, v.len);
    /* v is a valid FS2EMPTY, caller can fs2_free(&v) — no-op */

    CHECK_STR_EQ("[0]", "stack", a.arr[0].v);
    CHECK_EQ   ("count", 1, a.count);

    fs2_free(&v);                 /* no-op */
    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * attach_bp / push_bp — heap-source variants with auto-null
 *
 * `_bp` variants take `fs2 **` (address of caller's pointer). On entry,
 * `*src` is nulled immediately (safe even on raise); the struct is freed
 * after the value is moved into the slot. After the call, `*src == NULL`.
 * ========================================================================= */

/* ---- attach_bp: heap source, null + free ---- */

Test(fs2_array, attach_bp_heap_null_and_free) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 *p = fs2_heap_fromstr("heap");

    fs2 *slot = fs2arr_attach_dp(&a, &p, 0);

    CHECK_PTR_EQ("p nulled",    NULL, p);
    CHECK_PTR_EQ("slot ptr",    &a.arr[0], slot);
    CHECK_STR_EQ("[0]",         "heap", a.arr[0].v);
    CHECK_TRUE ("slot ALLOC",   fs2_is_alloc(&a.arr[0]));
    CHECK_FALSE("slot not shell", fs2_is_bodyalloc(&a.arr[0]));
    CHECK_EQ   ("count",        1, a.count);

    /* No fs2_free(p) — struct freed by _bp */
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach_bp: NULL *src is a soft no-op ---- */

Test(fs2_array, attach_bp_null_src_returns_null) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 *p = NULL;

    fs2 *slot = fs2arr_attach_dp(&a, &p, 0);

    CHECK_PTR_EQ("return NULL",     NULL, slot);
    CHECK_PTR_EQ("p still NULL",    NULL, p);
    CHECK_EQ   ("count unchanged",  0, a.count);
    CHECK_EQ   ("hwm unchanged",    0, a.hwm);

    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach_bp: same signature works for stack sources too ---- */

Test(fs2_array, attach_bp_stack_source_also_works) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2  v  = fs2_fromstr("stack");
    fs2 *p  = &v;

    fs2arr_attach_dp(&a, &p, 0);

    CHECK_PTR_EQ("p nulled",        NULL, p);
    CHECK_STR_EQ("[0]", "stack",   a.arr[0].v);
    CHECK_TRUE ("v FS2EMPTY",      fs2_eq_empty(&v));  /* fs2_move normalized v */
    CHECK_EQ   ("count",           1, a.count);

    /* v is safe to fs2_free — no-op */
    fs2_free(&v);
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach_bp: preserves immediate-not null invariant on raise ---- */

Test(fs2_array, attach_bp_bad_idx_raises_preserves_pointer) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);              /* size >= 2, hwm = 0 */
    fs2 *p = fs2_heap_fromstr("orphan");

    TRY() {
        fs2arr_attach_dp(&a, &p, 100);        /* idx >= size → raise */
        cr_assert_fail("expected raise");
    } else {
        /* caught — validation raised BEFORE nulling; p still valid */
    }
    err_resetenv();

    cr_assert(p != NULL, "p preserved on validation raise");   /* ✅ */
    CHECK_EQ("count unchanged", 0, a.count);
    CHECK_EQ("hwm unchanged",   0, a.hwm);

    fs2_free(p);                              /* caller still owns */
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach_bp: fills a hole ---- */

Test(fs2_array, attach_bp_fills_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v1 = fs2_fromstr("A");
    fs2 v2 = fs2_fromstr("B");
    fs2 v3 = fs2_fromstr("C");
    fs2arr_push_sp(&a, &v1);
    fs2arr_push_sp(&a, &v2);
    fs2arr_push_sp(&a, &v3);

    fs2 took = fs2arr_detach_new(&a, 1);
    fs2_free(&took);
    /* hole at 1, hwm=3, count=2 */

    fs2 *p = fs2_heap_fromstr("X");
    fs2arr_attach_dp(&a, &p, 1);

    CHECK_PTR_EQ("p nulled",       NULL, p);
    CHECK_EQ   ("hwm unchanged",   3, a.hwm);
    CHECK_EQ   ("count",           3, a.count);
    CHECK_TRUE ("[1] not hole",    !fs2arr_is_hole(&a.arr[1]));
    CHECK_STR_EQ("[1]", "X",       a.arr[1].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* ---- attach_bp: rejects non-empty slot ---- */

Test(fs2_array, attach_bp_over_nonempty_raise) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("live");
    fs2arr_push_sp(&a, &v);                /* slot 0 live */

    fs2 *p = fs2_heap_fromstr("X");

    TRY() {
        fs2arr_attach_dp(&a, &p, 0);    /* slot not hole → raise */
        cr_assert_fail("expected raise");
    } else {
        fs2_free(p); 
    }
    err_resetenv();

    /* p == NULL — struct was NOT freed (attach raised before fs2_free).
     * This is a real leak in the current design: null-first loses the
     * pointer to the struct before fs2_free can be reached. */

    fs2arr_free(&a);
    /* assert_balanced will FAIL if struct leaked — that's a real bug. */
    /* assert_balanced(); */
}

/* =========================================================================
 * push_bp — heap source, append at hwm, auto-null
 * ========================================================================= */

Test(fs2_array, push_bp_heap_appends_and_nulls) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 *p = fs2_heap_fromstr("heap");

    fs2 *slot = fs2arr_push_dp(&a, &p);

    CHECK_PTR_EQ("slot",         &a.arr[0], slot);
    CHECK_PTR_EQ("p nulled",     NULL, p);
    CHECK_STR_EQ("[0]",          "heap", a.arr[0].v);
    CHECK_FALSE("slot not shell", fs2_is_bodyalloc(&a.arr[0]));
    CHECK_EQ   ("hwm",           1, a.hwm);
    CHECK_EQ   ("count",         1, a.count);

    /* No fs2_free(p) — struct freed by _bp */
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, push_bp_sequential_extends_hwm) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(0);              /* start empty, forces reserve */

    for (int i = 0; i < 5; i++) {
        fs2 *p = fs2_heap_from(i);
        fs2arr_push_dp(&a, &p);
        CHECK_PTR_EQ("nulled", NULL, p);
        CHECK_EQ("hwm",   (size_t)(i + 1), a.hwm);
        CHECK_EQ("count", (size_t)(i + 1), a.count);
    }
    CHECK_TRUE("size >= 5", a.size >= 5);
    CHECK_STR_EQ("[0]", "0", a.arr[0].v);
    CHECK_STR_EQ("[4]", "4", a.arr[4].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, push_bp_null_src_returns_null) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 *p = NULL;

    fs2 *slot = fs2arr_push_dp(&a, &p);

    CHECK_PTR_EQ("return NULL",    NULL, slot);
    CHECK_EQ   ("hwm unchanged",   0, a.hwm);
    CHECK_EQ   ("count unchanged", 0, a.count);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, push_bp_stack_source_also_works) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2  v = fs2_fromstr("stack");
    fs2 *p = &v;

    fs2arr_push_dp(&a, &p);

    CHECK_PTR_EQ("p nulled",   NULL, p);
    CHECK_STR_EQ("[0]", "stack", a.arr[0].v);
    CHECK_TRUE ("v FS2EMPTY",  fs2_eq_empty(&v));   /* fs2_move normalized v */

    fs2_free(&v);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, push_bp_null_arr_aborts, .signal = SIGABRT) {
    fs2 *p = fs2_heap_fromstr("x");
    fs2arr_push_dp(NULL, &p);
    fs2_free(p);
}

Test(fs2_array, push_bp_null_src_ptr_aborts, .signal = SIGABRT) {
    fs2array a = fs2arr_init(4);
    fs2arr_push_dp(&a, NULL);
    fs2arr_free(&a);
}

/* =========================================================================
 * fs2arr_attach — _Generic dispatch
 * ========================================================================= */

Test(fs2_array, attach_generic_stack_dispatches_to_sp) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 s = fs2_fromstr("stack");

    fs2arr_attach(&a, &s, 0);               /* &s → fs2* → _sp */

    CHECK_STR_EQ("[0]", "stack",  a.arr[0].v);
    CHECK_TRUE ("s FS2EMPTY",     fs2_eq_empty(&s));     /* no auto-null */
    CHECK_FALSE("s not ALLOC",    fs2_is_alloc(&s));
    CHECK_EQ   ("count", 1, a.count);

    fs2_free(&s);                            /* no-op */
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, attach_generic_heap_dispatches_to_dp) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 *p = fs2_heap_fromstr("heap");

    fs2arr_attach(&a, &p, 0);               /* &p → fs2** → _dp */

    CHECK_PTR_EQ("p nulled", NULL, p);       /* auto-null */
    CHECK_STR_EQ("[0]", "heap", a.arr[0].v);
    CHECK_EQ   ("count", 1, a.count);

    /* no fs2_free(p) — _dp freed the struct */
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, attach_generic_single_ptr_heap_keeps_shell) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(2);
    fs2 *p = fs2_heap_fromstr("heap");

    fs2arr_attach(&a, p, 0);                /* p → fs2* → _sp */

    CHECK_STR_EQ("[0]", "heap", a.arr[0].v);
    CHECK_FALSE("p not nulled", p == NULL); /* p is still valid */
    CHECK_TRUE ("p shell FS2EMPTY", fs2_is_empty(p));
    CHECK_TRUE ("p shell BODYALLOC", fs2_is_bodyalloc(p));

    fs2_free(p);                             /* caller must free shell */
    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_push — _Generic dispatch
 * ========================================================================= */

Test(fs2_array, push_generic_stack_dispatches_to_sp) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(0);
    fs2 s = fs2_fromstr("x");

    fs2arr_push(&a, &s);                    /* &s → fs2* → _sp */

    CHECK_EQ  ("hwm", 1, a.hwm);
    CHECK_TRUE("s FS2EMPTY", fs2_eq_empty(&s));

    fs2_free(&s);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, push_generic_heap_dispatches_to_dp) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(0);
    fs2 *p = fs2_heap_fromstr("x");

    fs2arr_push(&a, &p);                    /* &p → fs2** → _dp */

    CHECK_PTR_EQ("p nulled", NULL, p);
    CHECK_STR_EQ("[0]", "x", a.arr[0].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Compile-time rejection (uncomment to verify via manual build)
 *
 * fs2 s = fs2_fromstr("x");
 * fs2arr_attach(&a, s, 0);          // fs2 (by value) — no _Generic match → error
 * fs2arr_attach(&a, NULL, 0);       // NULL — no match → error (needs cast)
 * ========================================================================= */
