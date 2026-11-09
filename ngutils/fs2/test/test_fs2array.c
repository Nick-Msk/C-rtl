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
        CHECK_TRUE("slot FS2EMPTY", fs2_is_hole(&a.arr[i]));

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
        CHECK_TRUE("slot [i] FS2EMPTY", fs2_is_hole(&a.arr[i]));   /* padding included */
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

    fs2arr_cleanall(&a);

    CHECK_EQ("len", 0, a.hwm);
    for (size_t i = 0; i < a.size; i++)
        CHECK_TRUE("slot FS2EMPTY", fs2_is_hole(&a.arr[i]));

    fs2arr_cleanall(&a);                       /* idempotent */
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
        CHECK_TRUE("slot FS2EMPTY", fs2_is_hole(&a.arr[i]));
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
        CHECK_TRUE("hole [i]", fs2_is_hole(&a.arr[i]));

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

    fs2arr_cleanall(&a);

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
    CHECK_TRUE ("[1] hole", fs2_is_hole(&a.arr[1]));
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
    CHECK_TRUE ("[0] hole",     fs2_is_hole(&a.arr[0]));

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
    CHECK_TRUE ("[0] hole",       fs2_is_hole(&a.arr[0]));
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
    CHECK_TRUE ("[0] hole",     fs2_is_hole(&a.arr[0]));
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
 * fs2_is_hole / set_hole — sentinel semantics
 * ========================================================================= */

Test(fs2_array, is_hole_distinguishes_empty_value_from_hole) {
    fs2 hole = {0};                          /* flags = 0 */
    fs2 empty_val = FS2EMPTY;                /* STATIC */
    fs2 canon = FS2();                       /* ALLOC */
    fs2 live = fs2_fromstr("x");             /* ALLOC */

    CHECK_FALSE("FS2EMPTY",   fs2_is_hole(&empty_val));
    CHECK_FALSE("FS2()",      fs2_is_hole(&canon));
    CHECK_FALSE("live",       fs2_is_hole(&live));

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
    CHECK_TRUE("slot not hole", !fs2_is_hole(&a.arr[0]));
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
    CHECK_TRUE("slot not hole", !fs2_is_hole(&a.arr[0]));
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
    CHECK_TRUE ("[1] is hole", fs2_is_hole(&a.arr[1]));
    fs2_free(&taken);

    /* 3. find hole */
    ptrdiff_t hole = fs2arr_find_tillhwm(&a);
    CHECK_EQ("hole at 1", 1, hole);

    /* 4. attach into hole */
    fs2 vX = fs2_fromstr("X");
    fs2arr_attach_sp(&a, &vX, (size_t)hole);
    CHECK_EQ   ("hwm still 3", 3, a.hwm);
    CHECK_EQ   ("count",       3, a.count);
    CHECK_TRUE ("[1] not hole", !fs2_is_hole(&a.arr[1]));
    CHECK_STR_EQ("[1]",        "X", a.arr[1].v);

    /* 5. detach again (same slot) */
    fs2 taken2 = fs2arr_detach_new(&a, 1);
    CHECK_STR_EQ("detached X", "X", taken2.v);
    CHECK_EQ   ("count",       2, a.count);
    CHECK_TRUE ("[1] hole again", fs2_is_hole(&a.arr[1]));
    fs2_free(&taken2);

    /* 6. push appends at hwm, not into hole */
    fs2 vD = fs2_fromstr("D");
    fs2arr_push_sp(&a, &vD);
    CHECK_EQ   ("hwm",   4, a.hwm);            /* hwm grew (push appends) */
    CHECK_EQ   ("count", 3, a.count);
    CHECK_STR_EQ("[3]", "D", a.arr[3].v);
    CHECK_TRUE ("[1] still hole", fs2_is_hole(&a.arr[1]));

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
    CHECK_TRUE("live empty", !fs2_is_hole(&a.arr[0]));

    fs2 out = fs2arr_detach_new(&a, 0);
    CHECK_TRUE("returned empty", fs2_is_empty(&out));
    CHECK_TRUE("slot now hole",  fs2_is_hole(&a.arr[0]));
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
    CHECK_TRUE ("[0] hole", fs2_is_hole(&a.arr[0]));

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

    CHECK_TRUE ("slot is hole",          fs2_is_hole(&a.arr[0]));
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
    CHECK_FALSE("not hole",  fs2_is_hole(&a.arr[0]));
    CHECK_TRUE ("safe-empty (promoted by fs2_move)", fs2_eq_empty(&a.arr[0]));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, hole_and_fs2_canonical_distinct) {
    fs2 canon = FS2();
    fs2 hole;
    fs2arr_set_hole(&hole);

    CHECK_TRUE ("canon is canon",  fs2_is_canon(&canon));
    CHECK_TRUE ("hole is hole",    fs2_is_hole(&hole));
    CHECK_FALSE("canon not hole",  fs2_is_hole(&canon));
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

    ptrdiff_t slot = fs2arr_attach_dp(&a, &p, 0);

    CHECK_TRUE ("slot >= 0", slot >= 0);
    CHECK_PTR_EQ("p nulled",    NULL, p);
    CHECK_PTR_EQ("slot ptr",    &a.arr[0], &a.arr[slot]);
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

    ptrdiff_t slot = fs2arr_attach_dp(&a, &p, 0);

    CHECK_TRUE ("slot < 0", slot < 0);
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
    CHECK_TRUE ("[1] not hole",    !fs2_is_hole(&a.arr[1]));
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

    ptrdiff_t slot = fs2arr_push_dp(&a, &p);

    CHECK_TRUE("slot >= 0",      slot >= 0); 
    CHECK_PTR_EQ("slot",         &a.arr[0], &a.arr[slot]);
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

    ptrdiff_t slot = fs2arr_push_dp(&a, &p);

    CHECK_LE("stot less than 0",   slot, 0);
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

 Test(fs2_array, clean_range_partial) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    /* hwm=5, count=5 */

    size_t freed = fs2arr_clean(&a, 1, 3);      /* free slots 1, 2 */

    CHECK_EQ("freed",  2, freed);
    CHECK_EQ("hwm",    5, a.hwm);               /* unchanged */
    CHECK_EQ("count",  3, a.count);
    CHECK_TRUE("[1] hole", fs2_is_hole(&a.arr[1]));
    CHECK_TRUE("[2] hole", fs2_is_hole(&a.arr[2]));
    CHECK_STR_EQ("[0]", "0", a.arr[0].v);
    CHECK_STR_EQ("[3]", "3", a.arr[3].v);
    CHECK_STR_EQ("[4]", "4", a.arr[4].v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, clean_range_on_holes_is_noop) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);

    size_t freed = fs2arr_clean(&a, 1, 5);       /* all holes, no live */

    CHECK_EQ("freed", 0, freed);
    CHECK_EQ("count", 1, a.count);               /* unchanged */
    CHECK_EQ("hwm",   1, a.hwm);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, clean_range_bad_range_raise, .signal = SIGINT) {
    fs2array a = fs2arr_init(4);
    fs2arr_clean(&a, 3, 2);                      /* from > to */
    fs2arr_free(&a);
}

Test(fs2_array, clean_range_beyond_size_raise, .signal = SIGINT) {
    fs2array a = fs2arr_init(4);
    fs2arr_clean(&a, 0, 100);                    /* to > size */
    fs2arr_free(&a);
}

Test(fs2_array, cleanall_resets_hwm_count) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }

    fs2arr_cleanall(&a);

    CHECK_EQ  ("hwm",   0, a.hwm);
    CHECK_EQ  ("count", 0, a.count);
    CHECK_GE  ("size", a.size, 16);              /* capacity kept */
    CHECK_NOT_NULL("arr", a.arr);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, cleanall_when_hwm_lt_size) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);
    fs2arr_shrink(&a);                 /* size=1, hwm=1 */

    /* hwm == size, no gap */
    fs2arr_cleanall(&a);
    CHECK_EQ("hwm after cleanall",  0, a.hwm);

    /* Grow again but only use 1 slot, then shrink partially */
    fs2 v2 = fs2_fromstr("y");
    fs2arr_push(&a, &v2);
    fs2arr_reserve(&a, 16);            /* size >= 16, hwm=1 */
    cr_assert(a.hwm < a.size);

    fs2arr_cleanall(&a);

    CHECK_EQ("hwm after cleanall (hwm < size)", 0, a.hwm);  /* ← fails с "to==size" */
    CHECK_EQ("count", 0, a.count);
    CHECK_EQ("size",  a.size, a.size);                       /* unchanged */

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, shrink_does_not_touch_hwm) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t1 = fs2arr_detach_new(&a, 4);
    fs2 t2 = fs2arr_detach_new(&a, 3);
    fs2_free(&t1);
    fs2_free(&t2);
    /* hwm=5, count=3, slots [3],[4] holes */

    fs2arr_shrink(&a);

    CHECK_EQ("hwm preserved",  5, a.hwm);         /* ← unchanged */
    CHECK_EQ("size == hwm",    5, a.size);        /* trailing holes kept */
    CHECK_EQ("count",          3, a.count);
    CHECK_TRUE("[3] hole kept", fs2_is_hole(&a.arr[3]));
    CHECK_TRUE("[4] hole kept", fs2_is_hole(&a.arr[4]));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, shrink_after_synchwm_trims_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t1 = fs2arr_detach_new(&a, 4);
    fs2 t2 = fs2arr_detach_new(&a, 3);
    fs2_free(&t1);
    fs2_free(&t2);
    /* hwm=5, slots [3],[4] holes */

    fs2arr_synchwm(&a);       /* hwm 5 → 3 */
    fs2arr_shrink(&a);        /* size 16 → 3 */

    CHECK_EQ("hwm",   3, a.hwm);
    CHECK_EQ("size",  3, a.size);
    CHECK_EQ("count", 3, a.count);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, shrink_after_synchwm_all_holes_canonical) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v1 = fs2_fromstr("A");
    fs2 v2 = fs2_fromstr("B");
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);
    fs2 t1 = fs2arr_detach_new(&a, 1);
    fs2 t2 = fs2arr_detach_new(&a, 0);
    fs2_free(&t1);
    fs2_free(&t2);
    /* hwm=2, count=0, both slots holes */

    fs2arr_synchwm(&a);       /* hwm → 0 */
    fs2arr_shrink(&a);        /* hwm=0 → free, canonical */

    CHECK_PTR_EQ("arr", NULL, a.arr);
    CHECK_EQ   ("hwm",  0, a.hwm);
    CHECK_EQ   ("size", 0, a.size);
    CHECK_EQ   ("count", 0, a.count);

    assert_balanced();
}

Test(fs2_array, shrink_preserves_all_holes_when_hwm_gt0) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v = fs2_fromstr("A");
    fs2arr_push(&a, &v);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    /* hwm=1, count=0, slot[0] hole */

    fs2arr_shrink(&a);

    /* shrink alone doesn't see hwm=0 (hwm=1), so it doesn't free */
    CHECK_EQ("hwm preserved",  1, a.hwm);
    CHECK_EQ("size == hwm",    1, a.size);
    CHECK_EQ("count",          0, a.count);
    CHECK_NOT_NULL("arr",     a.arr);
    CHECK_TRUE("[0] still hole", fs2_is_hole(&a.arr[0]));

    fs2arr_free(&a);
    assert_balanced();
}

/* test_fs2_tech.c (или test_fs2array.c — где удобнее) */

Test(fs2_array, techfprint_basic) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v1 = fs2_fromstr("hello");
    fs2 v2 = fs2_fromstr("world");
    fs2arr_push(&a, &v1);
    fs2arr_push(&a, &v2);

    FILE *f = test_tmpfile(__func__);
    cr_assert_not_null(f);

    long w = fs2arr_techfprint(f, &a, "test");
    cr_assert_gt(w, 0);

    rewind(f);
    char buf[2048] = {0};
    size_t rd = test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    buf[rd] = '\0';

    cr_assert(strstr(buf, "FS2ARR: test:") != NULL, "header: %s", buf);
    cr_assert(strstr(buf, "hwm [2]")       != NULL);
    cr_assert(strstr(buf, "count [2]")     != NULL);
    cr_assert(strstr(buf, "[hello")        != NULL);
    cr_assert(strstr(buf, "[world")        != NULL);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, techfprint_marks_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    /* slot 0 = hole, hwm=1, count=0 */

    FILE *f = test_tmpfile(__func__);
    fs2arr_techfprint(f, &a, "holes");
    rewind(f);
    char buf[1024] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);

    cr_assert(strstr(buf, "<hole>")    != NULL, "hole marker: %s", buf);
    cr_assert(strstr(buf, "count [0]") != NULL);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, techfprint_null_a) {
    FILE *f = test_tmpfile(__func__);
    long w = fs2arr_techfprint(f, NULL, "null");
    cr_assert_gt(w, 0);
    rewind(f);
    char buf[256] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    cr_assert(strstr(buf, "<NULL>") != NULL);
}

Test(fs2_array, techfprint_null_out_returns_zero) {
    fs2array a = fs2arr_init(2);
    CHECK_EQ("null out", 0, fs2arr_techfprint(NULL, &a, "x"));
    fs2arr_free(&a);
}

Test(fs2_array, techfprint_canonical) {
    fs2array a = FS2ARR();
    FILE *f = test_tmpfile(__func__);
    long w = fs2arr_techfprint(f, &a, "canon");
    cr_assert_gt(w, 0);
    rewind(f);
    char buf[512] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);
    cr_assert(strstr(buf, "hwm [0]")   != NULL);
    cr_assert(strstr(buf, "size [0]")  != NULL);
    cr_assert(strstr(buf, "count [0]") != NULL);
    /* %p impl-defined: either "(nil)" or "0x..." */
    cr_assert(strstr(buf, "arr((nil))") != NULL || strstr(buf, "arr(0x") != NULL);
}

Test(fs2_array, techfprint_notes_padding) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);            /* hwm=1, size >= 16 (likely 32) */

    FILE *f = test_tmpfile(__func__);
    fs2arr_techfprint(f, &a, "pad");
    rewind(f);
    char buf[1024] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);

    /* Size is impl-defined (power-of-2 rounding may give 16, 32, ...).
     * Build expected substring from actual a.size. */
    char expected[128];
    snprintf(expected, sizeof expected, "padding [1, %zu)", a.size);
    cr_assert(strstr(buf, expected) != NULL,
              "expected '%s', got: %s", expected, buf);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, techfprint_truncates_at_limit) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(FS2ARR_TECH_PRINT_COUNT + 5);
    for (int i = 0; i < FS2ARR_TECH_PRINT_COUNT + 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }

    FILE *f = test_tmpfile(__func__);
    fs2arr_techfprint(f, &a, "many");
    rewind(f);
    char buf[8192] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);

    cr_assert(strstr(buf, "more slots") != NULL, "got: %s", buf);
    /* exactly FS2ARR_TECH_PRINT_COUNT slot lines shown, +5 hidden */
    size_t slot_lines = 0;
    for (const char *p = buf; (p = strstr(p, "  [")) != NULL; p++) slot_lines++;
    cr_assert_eq(slot_lines, FS2ARR_TECH_PRINT_COUNT);

    fs2arr_free(&a);
    assert_balanced();
}

/* ---- macros ---- */

Test(fs2_array, techfprint_macros) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("macro");
    fs2arr_push(&a, &v);

    FILE *f = test_tmpfile(__func__);
    cr_assert_not_null(f);

    long w = FS2ARRTECHFPRINT(f, &a);       /* name = "a" via #a */
    cr_assert_gt(w, 0);

    rewind(f);
    char buf[1024] = {0};
    test_fread(buf, sizeof buf - 1, f, __func__);
    fclose(f);

    cr_assert(strstr(buf, "FS2ARR: &a:") != NULL, "macro name missing: %s", buf);

    fs2arr_free(&a);
    assert_balanced();
}

// fs2arr_validate unit

Test(fs2_array, validate_null_returns_false) {
    cr_assert_not(fs2arr_validate(NULL, NULL));
}

Test(fs2_array, validate_canonical_ok) {
    fs2array a = FS2ARR();
    CHECK_TRUE("canonical", fs2arr_validate(NULL, &a));
    fs2arr_free(&a);
}

Test(fs2_array, validate_after_init_ok) {
    fs2array a = fs2arr_init(16);
    CHECK_TRUE("empty with capacity", fs2arr_validate(NULL, &a));
    fs2arr_free(&a);
}

Test(fs2_array, validate_after_push_ok) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("hello");
    fs2arr_push(&a, &v);
    CHECK_TRUE("one live", fs2arr_validate(NULL, &a));
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, validate_after_detach_ok) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    CHECK_TRUE("hole in [0, hwm)", fs2arr_validate(NULL, &a));
    fs2arr_free(&a);
    assert_balanced();
}

/* ---- negative tests: corrupt state, expect false ---- */

Test(fs2_array, validate_rejects_count_gt_hwm) {
    fs2array a = fs2arr_init(4);
    a.count = 5;                          /* hwm=0, count=5 → corrupt */
    CHECK_FALSE("count > hwm", fs2arr_validate(NULL, &a));
    a.count = 0;                          /* restore for clean free */
    fs2arr_free(&a);
}

Test(fs2_array, validate_rejects_hwm_gt_size) {
    fs2array a = fs2arr_init(4);
    a.hwm = a.size + 1;
    CHECK_FALSE("hwm > size", fs2arr_validate(NULL, &a));
    a.hwm = 0;
    fs2arr_free(&a);
}

Test(fs2_array, validate_rejects_null_arr_nonzero) {
    fs2array a = FS2ARR();
    a.hwm = 3;                            /* arr=NULL but hwm=3 */
    CHECK_FALSE("null arr with hwm", fs2arr_validate(NULL, &a));
    a.hwm = 0;
}

Test(fs2_array, validate_rejects_padding_not_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);                  /* hwm=1, size>=16 */

    /* corrupt padding: put a live fs2 at [5] without touching hwm/count */
    a.arr[5] = fs2_fromstr("ghost");

    CHECK_FALSE("[5] not hole in padding", fs2arr_validate(NULL, &a));

    fs2_free(&a.arr[5]);                  /* clean corrupt slot */
    a.arr[5] = FS2EMPTY;                  /* restorable to valid padding */
    fs2arr_set_hole(&a.arr[5]);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, validate_rejects_count_mismatch) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);                  /* hwm=1, count=1 */
    a.count = 0;                          /* corrupt: 1 live but count=0 */
    CHECK_FALSE("live != count", fs2arr_validate(NULL, &a));
    a.count = 1;
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, clone_packs_holes_and_tight) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t1 = fs2arr_detach_new(&a, 3);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);
    /* src: [live0, hole, live2, hole, live4], hwm=5, count=3 */

    fs2array c = fs2arr_clone(&a);

    CHECK_EQ  ("count", 3, c.count);
    CHECK_EQ  ("hwm",   3, c.hwm);              /* packed */
    CHECK_GE  ("size",  c.size, 3);             /* tight (с shrink) */
    CHECK_STR_EQ("[0]", "0", c.arr[0].v);
    CHECK_STR_EQ("[1]", "2", c.arr[1].v);
    CHECK_STR_EQ("[2]", "4", c.arr[2].v);
    CHECK_FS2VALID(1, &c.arr[0]);

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, cloneexact_preserves_holes_and_trailing) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    /* hwm=3. Now detach last twice to create trailing holes. */
    fs2 t1 = fs2arr_detach_new(&a, 2);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);
    /* src: [live0, hole, hole], hwm=3, count=1 */

    fs2array c = fs2arr_cloneexact(&a);

    CHECK_EQ  ("count", 1, c.count);
    CHECK_EQ  ("hwm",   3, c.hwm);              /* ← exact preserves trailing holes */
    CHECK_STR_EQ("[0]", "0", c.arr[0].v);
    CHECK_TRUE("[1] hole", fs2_is_hole(&c.arr[1]));
    CHECK_TRUE("[2] hole", fs2_is_hole(&c.arr[2]));

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, clone_deep_independent_buffers) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("hello");
    fs2arr_push(&a, &v);

    fs2array c = fs2arr_clone(&a);

    CHECK_TRUE("independent buffer", a.arr[0].v != c.arr[0].v);
    /* mutate original, clone unaffected */
    a.arr[0].v[0] = 'X';
    CHECK_STR_EQ("clone unaffected", "hello", c.arr[0].v);
    CHECK_STR_EQ("src mutated",      "Xello", a.arr[0].v);

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, clone_empty_and_canonical) {
    fs2array canon = FS2ARR();
    fs2array c1 = fs2arr_clone(&canon);
    CHECK_PTR_EQ("canon arr", NULL, c1.arr);
    CHECK_EQ   ("canon hwm", 0, c1.hwm);

    fs2array empty = fs2arr_init(16);
    fs2array c2 = fs2arr_clone(&empty);
    CHECK_PTR_EQ("empty arr", NULL, c2.arr);    /* no elements → canonical */

    fs2array c3 = fs2arr_cloneexact(&empty);
    CHECK_PTR_EQ("empty exact", NULL, c3.arr);

    fs2arr_free(&empty);
    /* clones are canonical — no free needed */
}

Test(fs2_array, clone_null_aborts, .signal = SIGABRT) {
    fs2arr_clone(NULL);
}

Test(fs2_array, cloneexact_null_aborts, .signal = SIGABRT) {
    fs2arr_cloneexact(NULL);
}

/* =========================================================================
 * fs2arr_get — read-only slot access
 * ========================================================================= */

Test(fs2_array, get_live_slot) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("live");
    fs2arr_push(&a, &v);

    const fs2 *p = fs2arr_get(&a, 0);

    cr_assert_not_null(p);
    CHECK_PTR_EQ("same slot", &a.arr[0], p);
    CHECK_FALSE("not hole",  fs2_is_hole(p));
    CHECK_STR_EQ("value",    "live", p->v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, get_hole_slot) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    /* hwm=1, slot[0] hole */

    const fs2 *p = fs2arr_get(&a, 0);

    cr_assert_not_null(p);
    CHECK_TRUE("hole", fs2_is_hole(p));
    CHECK_PTR_EQ("v", NULL, p->v);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, get_out_of_hwm_returns_null) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);                       /* hwm=1, size>=16 */

    CHECK_PTR_EQ("idx = hwm",   NULL, fs2arr_get(&a, 1));
    CHECK_PTR_EQ("idx > hwm",   NULL, fs2arr_get(&a, 5));
    CHECK_PTR_EQ("idx = 10000", NULL, fs2arr_get(&a, 10000));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, get_empty_returns_null) {
    fs2array canon = FS2ARR();
    CHECK_PTR_EQ("canon", NULL, fs2arr_get(&canon, 0));

    fs2array empty = fs2arr_init(16);
    CHECK_PTR_EQ("empty with cap", NULL, fs2arr_get(&empty, 0));
    fs2arr_free(&empty);
}

Test(fs2_array, get_null_aborts, .signal = SIGABRT) {
    fs2arr_get(NULL, 0);
}

/* =========================================================================
 * fs2arr_clone — packed, tight, deep copy
 * ========================================================================= */

Test(fs2_array, clone_packs_and_tight) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    /* detach mid to create holes */
    fs2 t1 = fs2arr_detach_new(&a, 3);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);
    /* src: [0, hole, 2, hole, 4], hwm=5, count=3 */

    fs2array c = fs2arr_clone(&a);

    CHECK_EQ  ("count", 3, c.count);
    CHECK_EQ  ("hwm",   3, c.hwm);             /* packed: no holes */
    CHECK_GE  ("size",  c.size, 3);            /* tight */
    CHECK_STR_EQ("[0]", "0", c.arr[0].v);
    CHECK_STR_EQ("[1]", "2", c.arr[1].v);      /* original [2] */
    CHECK_STR_EQ("[2]", "4", c.arr[2].v);      /* original [4] */
    CHECK_FALSE("[0] live", fs2_is_hole(&c.arr[0]));
    CHECK_FALSE("[1] live", fs2_is_hole(&c.arr[1]));
    CHECK_FALSE("[2] live", fs2_is_hole(&c.arr[2]));

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, clone_deep_copy_independent) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("hello");
    fs2arr_push(&a, &v);

    fs2array c = fs2arr_clone(&a);

    CHECK_TRUE("independent buffers", a.arr[0].v != c.arr[0].v);
    CHECK_STR_EQ("same content", "hello", c.arr[0].v);

    /* mutate original — clone unaffected */
    a.arr[0].v[0] = 'X';
    CHECK_STR_EQ("clone still hello", "hello", c.arr[0].v);
    CHECK_STR_EQ("src now Xello",     "Xello", a.arr[0].v);

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, clone_canonical_returns_canonical) {
    fs2array canon = FS2ARR();
    fs2array c = fs2arr_clone(&canon);

    CHECK_PTR_EQ("arr", NULL, c.arr);
    CHECK_EQ   ("hwm",  0, c.hwm);
    CHECK_EQ   ("size", 0, c.size);
    CHECK_EQ   ("count",0, c.count);
    /* canonical — no fs2arr_free needed */
}

Test(fs2_array, clone_empty_capacity_returns_canonical) {
    fs2array empty = fs2arr_init(16);          /* arr != NULL, count=0 */

    fs2array c = fs2arr_clone(&empty);

    CHECK_PTR_EQ("arr", NULL, c.arr);          /* no live elements → canonical */
    CHECK_EQ   ("count", 0, c.count);

    fs2arr_free(&empty);
    /* clone is canonical */
}

/* =========================================================================
 * fs2arr_cloneexact — preserves hwm (holes + trailing), tight size
 * ========================================================================= */

Test(fs2_array, cloneexact_preserves_mid_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 4; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 1);
    fs2_free(&t);
    /* src: [0, hole, 2, 3], hwm=4, count=3 */

    fs2array c = fs2arr_cloneexact(&a);

    CHECK_EQ  ("count", 3, c.count);
    CHECK_EQ  ("hwm",   4, c.hwm);
    CHECK_GE  ("size",  c.size, 4);
    CHECK_STR_EQ("[0]", "0", c.arr[0].v);
    CHECK_TRUE ("[1] hole", fs2_is_hole(&c.arr[1]));
    CHECK_STR_EQ("[2]", "2", c.arr[2].v);
    CHECK_STR_EQ("[3]", "3", c.arr[3].v);

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, cloneexact_deep_independent) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2 v = fs2_fromstr("data");
    fs2arr_push(&a, &v);

    fs2array c = fs2arr_cloneexact(&a);

    CHECK_TRUE("independent", a.arr[0].v != c.arr[0].v);
    a.arr[0].v[0] = 'X';
    CHECK_STR_EQ("clone intact", "data", c.arr[0].v);

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, cloneexact_canonical_returns_canonical) {
    fs2array canon = FS2ARR();
    fs2array c = fs2arr_cloneexact(&canon);
    CHECK_PTR_EQ("arr", NULL, c.arr);
}

/* =========================================================================
 * get + clone + cloneexact — integration
 * ========================================================================= */

Test(fs2_array, clone_then_get_all_live) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);
    /* src has hole at [2] */

    fs2array c = fs2arr_clone(&a);

    /* clone is packed: all [0, count) live — get returns non-hole */
    for (size_t i = 0; i < c.count; i++) {
        const fs2 *p = fs2arr_get(&c, i);
        cr_assert_not_null(p);
        CHECK_FALSE("[i] live", fs2_is_hole(p));
    }

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, cloneexact_then_get_preserves_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);
    /* src: [0, 1, hole, 3, 4], hwm=5 */

    fs2array c = fs2arr_cloneexact(&a);

    /* get returns slots as-is — hole at [2] preserved */
    const fs2 *hole = fs2arr_get(&c, 2);
    cr_assert_not_null(hole);
    CHECK_TRUE("hole at [2]", fs2_is_hole(hole));

    const fs2 *live = fs2arr_get(&c, 3);
    cr_assert_not_null(live);
    CHECK_FALSE("live at [3]", fs2_is_hole(live));
    CHECK_STR_EQ("[3] value", "3", live->v);

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, clone_preserves_all_live_with_mid_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);            /* 0,1,2,3,4 */
        fs2arr_push(&a, &v);
    }
    /* detach mid: holes at [1] and [3] */
    fs2 t1 = fs2arr_detach_new(&a, 3);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);
    /* src: [0, hole, 2, hole, 4], count=3, hwm=5 */

    fs2array c = fs2arr_clone(&a);

    CHECK_EQ("count", 3, c.count);          /* ← fails without fix: 2 */
    CHECK_STR_EQ("[0]", "0", c.arr[0].v);
    CHECK_STR_EQ("[1]", "2", c.arr[1].v);   /* ← fails without fix: missing */
    CHECK_STR_EQ("[2]", "4", c.arr[2].v);   /* ← fails without fix: missing */

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, clone_drops_trailing_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t1 = fs2arr_detach_new(&a, 2);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1); fs2_free(&t2);
    /* src: [0, hole, hole], count=1, hwm=3 */

    fs2array c = fs2arr_clone(&a);

    CHECK_EQ  ("count", 1, c.count);
    CHECK_EQ  ("hwm",   1, c.hwm);           /* packed: no trailing */
    CHECK_STR_EQ("[0]", "0", c.arr[0].v);

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_equalexact — semantic comparison up to hwm
 * ========================================================================= */

Test(fs2_array, eqexact_canonical_vs_canonical) {
    fs2array a = FS2ARR();
    fs2array b = FS2ARR();
    CHECK_TRUE("both canonical", fs2arr_equalexact(&a, &b));
}

Test(fs2_array, eqexact_canonical_vs_empty_with_capacity) {
    fs2array a = FS2ARR();
    fs2array b = fs2arr_init(16);            /* arr != NULL, hwm=0 */
    CHECK_TRUE("both empty up to hwm", fs2arr_equalexact(&a, &b));
    CHECK_TRUE("symmetric",            fs2arr_equalexact(&b, &a));
    fs2arr_free(&b);
}

Test(fs2_array, eqexact_same_content) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2array b = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 va = fs2_from_new(i);
        fs2 vb = fs2_from_new(i);
        fs2arr_push(&a, &va);
        fs2arr_push(&b, &vb);
    }
    CHECK_TRUE("same content", fs2arr_equalexact(&a, &b));
    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

Test(fs2_array, eqexact_different_content) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2array b = fs2arr_init(8);
    fs2 va = fs2_fromstr("hello");
    fs2 vb = fs2_fromstr("world");
    fs2arr_push(&a, &va);
    fs2arr_push(&b, &vb);
    CHECK_FALSE("different strings", fs2arr_equalexact(&a, &b));
    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

Test(fs2_array, eqexact_different_hwm) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2array b = fs2arr_init(8);
    fs2 va = fs2_fromstr("x");
    fs2 vb = fs2_fromstr("x");
    fs2arr_push(&a, &va);                    /* hwm=1 */
    fs2arr_push(&b, &vb);
    fs2 vc = fs2_fromstr("y");
    fs2arr_push(&b, &vc);                    /* hwm=2 */
    CHECK_FALSE("hwm differs", fs2arr_equalexact(&a, &b));
    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

Test(fs2_array, eqexact_same_holes_at_same_positions) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2array b = fs2arr_init(8);
    /* build identical arrays then detach same indices */
    for (int i = 0; i < 4; i++) {
        fs2 va = fs2_from_new(i);
        fs2 vb = fs2_from_new(i);
        fs2arr_push(&a, &va);
        fs2arr_push(&b, &vb);
    }
    fs2 ta = fs2arr_detach_new(&a, 1);
    fs2 tb = fs2arr_detach_new(&b, 1);
    fs2_free(&ta);
    fs2_free(&tb);
    /* both: [0, hole, 2, 3], hwm=4, count=3 */

    CHECK_TRUE("same holes", fs2arr_equalexact(&a, &b));
    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

Test(fs2_array, eqexact_hole_mismatch) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2array b = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 va = fs2_from_new(i);
        fs2 vb = fs2_from_new(i);
        fs2arr_push(&a, &va);
        fs2arr_push(&b, &vb);
    }
    fs2 t = fs2arr_detach_new(&a, 1);        /* a: [0, hole, 2] */
    fs2_free(&t);
    /* b: [0, 1, 2] — no hole */

    CHECK_FALSE("hole vs live at [1]", fs2arr_equalexact(&a, &b));
    CHECK_FALSE("symmetric",           fs2arr_equalexact(&b, &a));

    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

Test(fs2_array, eqexact_same_count_different_positions) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2array b = fs2arr_init(8);
    /* a: [live, hole, live], b: [live, live, hole] — same count=2, hwm=3 */
    for (int i = 0; i < 3; i++) {
        fs2 va = fs2_from_new(i);
        fs2 vb = fs2_from_new(i);
        fs2arr_push(&a, &va);
        fs2arr_push(&b, &vb);
    }
    fs2 ta = fs2arr_detach_new(&a, 1);
    fs2 tb = fs2arr_detach_new(&b, 2);
    fs2_free(&ta);
    fs2_free(&tb);

    CHECK_FALSE("different hole positions", fs2arr_equalexact(&a, &b));

    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

Test(fs2_array, eqexact_size_ignored) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    fs2array b = fs2arr_init(64);            /* different capacity */
    fs2 va = fs2_fromstr("x");
    fs2 vb = fs2_fromstr("x");
    fs2arr_push(&a, &va);
    fs2arr_push(&b, &vb);

    CHECK_TRUE("size ignored", fs2arr_equalexact(&a, &b));

    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

Test(fs2_array, eqexact_null_handling) {
    fs2array a = FS2ARR();

    CHECK_TRUE ("both NULL",      fs2arr_equalexact(NULL, NULL));
    CHECK_FALSE("a NULL",         fs2arr_equalexact(NULL, &a));
    CHECK_FALSE("b NULL",         fs2arr_equalexact(&a, NULL));
    CHECK_TRUE ("same pointer",   fs2arr_equalexact(&a, &a));
}

Test(fs2_array, eqexact_same_pointer_short_circuit) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);

    CHECK_TRUE("a == a fast path", fs2arr_equalexact(&a, &a));

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, eqexact_after_clone) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);
    /* a: [0, 1, hole, 3, 4], hwm=5, count=4 */

    fs2array c_packed = fs2arr_clone(&a);     /* packed, hwm=4 */
    fs2array c_exact  = fs2arr_cloneexact(&a);/* preserved, hwm=5 */

    CHECK_FALSE("packed differs in hwm", fs2arr_equalexact(&a, &c_packed));
    CHECK_TRUE ("exact matches",         fs2arr_equalexact(&a, &c_exact));

    fs2arr_free(&a);
    fs2arr_free(&c_packed);
    fs2arr_free(&c_exact);
    assert_balanced();
}

/* =========================================================================
 * Negative: corrupt state visible to eqexact
 * ========================================================================= */

Test(fs2_array, eqexact_count_mismatch_false) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2array b = fs2arr_init(8);
    fs2 va = fs2_fromstr("x");
    fs2 vb = fs2_fromstr("x");
    fs2arr_push(&a, &va);
    fs2arr_push(&b, &vb);
    /* same content, but corrupt b.count */
    b.count = 0;
    CHECK_FALSE("count mismatch", fs2arr_equalexact(&a, &b));
    b.count = 1;                              /* restore for free */
    fs2arr_free(&a);
    fs2arr_free(&b);
    assert_balanced();
}

/* =========================================================================
 * Invariant: eqexact(a, cloneexact(a)) == true for ANY valid a
 *
 * cloneexact preserves hwm, hole positions, and content. So the exact
 * clone must be eqexact-equal to the source — this is the defining
 * property of "exact" clone. If this fails, either cloneexact or eqexact
 * or the hole/hwm invariants are broken.
 *
 * Note: eqexact(a, clone(a)) is NOT an invariant — clone packs holes
 * and tightens hwm, so positional equality may legitimately fail.
 * ========================================================================= */

Test(fs2_array, eqexact_cloneexact_invariant_packed) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    /* packed: hwm=3, no holes */

    fs2array c = fs2arr_cloneexact(&a);
    CHECK_TRUE("cloneexact of packed == source", fs2arr_equalexact(&a, &c));

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, eqexact_cloneexact_invariant_mid_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);
    /* [0, 1, hole, 3, 4], hwm=5, count=4 */

    fs2array c = fs2arr_cloneexact(&a);
    CHECK_TRUE("mid hole preserved", fs2arr_equalexact(&a, &c));

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, eqexact_cloneexact_invariant_trailing_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t1 = fs2arr_detach_new(&a, 2);
    fs2 t2 = fs2arr_detach_new(&a, 1);
    fs2_free(&t1);
    fs2_free(&t2);
    /* [0, hole, hole], hwm=3, count=1 */

    fs2array c = fs2arr_cloneexact(&a);
    CHECK_EQ  ("hwm preserved", 3, c.hwm);
    CHECK_TRUE("trailing holes preserved", fs2arr_equalexact(&a, &c));

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

Test(fs2_array, eqexact_cloneexact_invariant_empty) {
    fs2array canon = FS2ARR();
    fs2array c1 = fs2arr_cloneexact(&canon);
    CHECK_TRUE("canonical", fs2arr_equalexact(&canon, &c1));

    fs2array empty = fs2arr_init(16);
    fs2array c2 = fs2arr_cloneexact(&empty);
    CHECK_TRUE("empty with capacity", fs2arr_equalexact(&empty, &c2));
    fs2arr_free(&empty);
}

/* All-holes array (count==0, hwm>0) — exact clone must preserve hwm */
Test(fs2_array, eqexact_cloneexact_invariant_all_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v = fs2_fromstr("A");
    fs2arr_push(&a, &v);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    /* [hole], hwm=1, count=0 */

    fs2array c = fs2arr_cloneexact(&a);
    CHECK_EQ  ("hwm",   1, c.hwm);
    CHECK_EQ  ("count", 0, c.count);
    CHECK_TRUE("all holes preserved", fs2arr_equalexact(&a, &c));

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

/* Source after shrink (tight) — cloneexact still exact */
Test(fs2_array, eqexact_cloneexact_invariant_after_shrink) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2arr_shrink(&a);                          /* size == hwm == 3 */

    fs2array c = fs2arr_cloneexact(&a);
    CHECK_TRUE("after shrink", fs2arr_equalexact(&a, &c));

    fs2arr_free(&a);
    fs2arr_free(&c);
    assert_balanced();
}

/* Iterated: cloneexact(cloneexact(a)) == cloneexact(a) semantically */
Test(fs2_array, eqexact_cloneexact_invariant_idempotent) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);

    fs2array c1 = fs2arr_cloneexact(&a);
    fs2array c2 = fs2arr_cloneexact(&c1);

    CHECK_TRUE("c1 == a",  fs2arr_equalexact(&a, &c1));
    CHECK_TRUE("c2 == c1", fs2arr_equalexact(&c1, &c2));
    CHECK_TRUE("c2 == a",  fs2arr_equalexact(&a, &c2));

    fs2arr_free(&a);
    fs2arr_free(&c1);
    fs2arr_free(&c2);
    assert_balanced();
}

/* Contrast: clone is packed/tight — eqexact may legitimately fail
 * when source had holes (hwm shrinks, positions change). */
Test(fs2_array, eqexact_clone_NOT_invariant_with_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 2);
    fs2_free(&t);
    /* a: [0,1,hole,3,4], hwm=5, count=4 */

    fs2array packed = fs2arr_clone(&a);
    /* packed: [0,1,3,4], hwm=4, count=4 */

    CHECK_FALSE("packed differs from source in hwm/positions",
                fs2arr_equalexact(&a, &packed));

    fs2arr_free(&a);
    fs2arr_free(&packed);
    assert_balanced();
}

/* =========================================================================
 * fs2arr_ispacked
 * ========================================================================= */

Test(fs2_array, ispacked_canonical) {
    fs2array a = FS2ARR();
    CHECK_TRUE("canonical 0==0", fs2arr_ispacked(&a));
}

Test(fs2_array, ispacked_empty_with_capacity) {
    fs2array a = fs2arr_init(16);              /* hwm=0, count=0 */
    CHECK_TRUE("empty packed", fs2arr_ispacked(&a));
    fs2arr_free(&a);
}

Test(fs2_array, ispacked_after_push_only) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 5; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    CHECK_TRUE("push-only packed", fs2arr_ispacked(&a));
    CHECK_EQ  ("hwm",   5, a.hwm);
    CHECK_EQ  ("count", 5, a.count);
    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, ispacked_false_after_detach) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 3; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }

    fs2 t = fs2arr_detach_new(&a, 1);           /* hole at [1] */
    fs2_free(&t);

    CHECK_FALSE("hole breaks packed", fs2arr_ispacked(&a));
    CHECK_EQ   ("hwm",   3, a.hwm);             /* hwm preserved */
    CHECK_EQ   ("count", 2, a.count);           /* count decremented */

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, ispacked_true_after_compact) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    for (int i = 0; i < 4; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    fs2 t = fs2arr_detach_new(&a, 1);
    fs2_free(&t);
    CHECK_FALSE("pre-compact", fs2arr_ispacked(&a));

    fs2arr_compact(&a);
    CHECK_TRUE("post-compact", fs2arr_ispacked(&a));
    CHECK_EQ  ("hwm == count", a.hwm, a.count);

    fs2arr_free(&a);
    assert_balanced();
}

Test(fs2_array, ispacked_true_after_clone) {
    __fs2_checkreset();
    fs2array src = FS2ARR();
    fs2arr_pushstr(&src, "a");
    fs2arr_pushstr(&src, "b");
    fs2arr_pushstr(&src, "c");
    //fs2array src = FS2ARRFROMSTR("a", "b", "c");
    fs2 t = fs2arr_detach_new(&src, 1);         /* src has hole */
    fs2_free(&t);
    CHECK_FALSE("src not packed", fs2arr_ispacked(&src));

    fs2array c = fs2arr_clone(&src);            /* clone packs */
    CHECK_TRUE("clone packed", fs2arr_ispacked(&c));

    fs2array ce = fs2arr_cloneexact(&src);      /* cloneexact preserves holes */
    CHECK_FALSE("cloneexact not packed", fs2arr_ispacked(&ce));

    fs2arr_free(&src);
    fs2arr_free(&c);
    fs2arr_free(&ce);
    assert_balanced();
}

Test(fs2_array, ispacked_all_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);
    fs2 v = fs2_fromstr("x");
    fs2arr_push(&a, &v);
    fs2 t = fs2arr_detach_new(&a, 0);
    fs2_free(&t);
    /* hwm=1, count=0 → not packed */

    CHECK_FALSE("all holes not packed", fs2arr_ispacked(&a));

    fs2arr_compact(&a);                         /* hwm → 0, count=0 */
    CHECK_TRUE("compact → 0==0", fs2arr_ispacked(&a));
    CHECK_EQ  ("hwm",   0, a.hwm);

    fs2arr_free(&a);
    assert_balanced();
}
