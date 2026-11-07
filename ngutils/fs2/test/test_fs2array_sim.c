#include <criterion/criterion.h>

#include "test_fs2.h"

#include "fs2array.h"
#include "fs2_api.h"

/* -------------------------------------------------------------------------
 * Fixtures — mute stderr, init log
 * ------------------------------------------------------------------------- */

TestSuite(fs2_array_simulation, .init = test_setup, .fini = test_teardown);

/**
 * @brief Verify all fs2array invariants after an operation.
 *
 * Invariants:
 *   - count <= hwm <= size
 *   - if arr == NULL: hwm == size == count == 0
 *   - [0, hwm): each slot is either a hole (FS2_NULL_ARRAY_ELEMENT) or a
 *     live fs2 (fs2_validate passes); the count of live == count
 *   - [hwm, size): all slots are holes (padding invariant)
 *
 * Crashes via cr_assert on first violation, with slot index and op label.
 */
static void
assert_arr_invariants(const fs2array *a, const char *op) {
    /* Basic ordering */
    if (a->count > a->hwm)
        cr_assert_fail("[%s] count=%zu > hwm=%zu", op, a->count, a->hwm);
    if (a->hwm > a->size)
        cr_assert_fail("[%s] hwm=%zu > size=%zu", op, a->hwm, a->size);

    /* Canonical empty */
    if (a->arr == NULL) {
        if (a->hwm != 0 || a->size != 0 || a->count != 0)
            cr_assert_fail("[%s] arr NULL but hwm=%zu size=%zu count=%zu",
                           op, a->hwm, a->size, a->count);
        return;
    }

    /* [0, hwm): live count matches, each slot is hole or valid fs2 */
    size_t  live = 0;
    for (size_t i = 0; i < a->hwm; i++) {
        if (fs2arr_is_hole(&a->arr[i]))
            continue;
        if (!fs2_validate(NULL, &a->arr[i]))
            cr_assert_fail("[%s] slot[%zu] invalid fs2 (not a hole)", op, i);
        live++;
    }
    if (live != a->count)
        cr_assert_fail("[%s] live=%zu in [0,hwm) but count=%zu",
                       op, live, a->count);

    /* [hwm, size): all holes (padding) */
    for (size_t i = a->hwm; i < a->size; i++) {
        if (!fs2arr_is_hole(&a->arr[i]))
            cr_assert_fail("[%s] slot[%zu] in padding is not a hole", op, i);
    }
}

/* =========================================================================
 * Simulation 1: push → detach → attach → shrink cycle, repeated rounds
 * ========================================================================= */

Test(fs2_array_simulation, push_detach_attach_cycle) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    assert_arr_invariants(&a, "init");

    for (int round = 0; round < 20; round++) {
        /* push 5 values */
        for (int i = 0; i < 5; i++) {
            fs2 v = fs2_from_new(round * 100 + i);
            fs2arr_push(&a, &v);
        }
        assert_arr_invariants(&a, "push5");

        /* detach 2 (oldest) — they become holes */
        for (int i = 0; i < 2; i++) {
            ptrdiff_t hole = fs2arr_find_tillhwm(&a);
            cr_assert_geq(hole, 0, "round %d: expected hole", round);
            fs2 t = fs2arr_detach_new(&a, (size_t)hole);
            fs2_free(&t);
        }
        assert_arr_invariants(&a, "detach2");

        /* attach 2 new — fills the holes */
        for (int i = 0; i < 2; i++) {
            ptrdiff_t hole = fs2arr_find_tillhwm(&a);
            cr_assert_geq(hole, 0, "round %d: expected hole", round);
            fs2 v = fs2_fromstr("refill");
            fs2arr_attach(&a, &v, (size_t)hole);
        }
        assert_arr_invariants(&a, "attach2");

        /* no holes now */
        CHECK_EQ("no holes after refill", -1, fs2arr_find_tillhwm(&a));

        /* detach all remaining, synchwm + shrink → canonical */
        while (a.count > 0) {
            ptrdiff_t i = fs2arr_find_tillhwm(&a);
            if (i < 0) {
                /* no holes: take any live slot (first) — just detach idx 0 */
                fs2 t = fs2arr_detach_new(&a, 0);
                fs2_free(&t);
            } else {
                /* shouldn't happen here */
                break;
            }
        }
        /* hwm may be > 0 with all holes; synchwm then shrink */
        fs2arr_synchwm(&a);
        CHECK_EQ("hwm after empty", 0, a.hwm);
        assert_arr_invariants(&a, "drained");

        fs2arr_shrink(&a);
        CHECK_PTR_EQ("canonical after shrink", NULL, a.arr);
        assert_arr_invariants(&a, "shrunk");
    }

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Simulation 2: pool of items, random detach/attach by hole position
 * ========================================================================= */

Test(fs2_array_simulation, pool_reuse_by_hole) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(8);

    /* Fill pool */
    for (int i = 0; i < 8; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    assert_arr_invariants(&a, "filled");
    cr_assert_eq(a.hwm, 8);
    cr_assert_eq(a.count, 8);

    /* Free every second slot */
    for (int i = 0; i < 8; i += 2) {
        fs2 t = fs2arr_detach_new(&a, (size_t)i);
        fs2_free(&t);
    }
    assert_arr_invariants(&a, "half-detached");
    cr_assert_eq(a.hwm, 8);          /* hwm unchanged (holes not at end) */
    cr_assert_eq(a.count, 4);

    /* Refill via holes — should reuse from position 0, 2, 4, 6 */
    for (int i = 0; i < 4; i++) {
        ptrdiff_t hole = fs2arr_find_tillhwm(&a);
        cr_assert_geq(hole, 0, "expected hole");
        fs2 v = fs2_fromstr("refilled");
        fs2arr_attach(&a, &v, (size_t)hole);
    }
    assert_arr_invariants(&a, "refilled");
    cr_assert_eq(a.count, 8);
    CHECK_EQ("no holes", -1, fs2arr_find_tillhwm(&a));

    /* All original values are gone, all "refilled" */
    for (size_t i = 0; i < a.hwm; i++) {
        CHECK_TRUE("live slot", !fs2arr_is_hole(&a.arr[i]));
        CHECK_STR_EQ("refilled", "refilled", a.arr[i].v);
    }

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Simulation 3: grow/shrink dance
 * ========================================================================= */

Test(fs2_array_simulation, grow_shrink_dance) {
    __fs2_checkreset();
    fs2array a = FS2ARR();

    /* grow: push in increasing batches */
    for (int batch = 1; batch <= 50; batch++) {
        fs2 v = fs2_from_new(batch);
        fs2arr_push(&a, &v);
        assert_arr_invariants(&a, "grow");
        cr_assert_eq(a.count, (size_t)batch);
    }
    cr_assert_geq(a.size, 50);

    /* freeze: capture size after 50 pushes */
    size_t grown_size = a.size;
    cr_assert_geq(grown_size, 50);

    /* shrink: exact to hwm */
    fs2arr_shrink(&a);
    assert_arr_invariants(&a, "shrink");
    cr_assert_eq(a.size, a.hwm);
    cr_assert_eq(a.size, 50);
    cr_assert_lt(a.size, grown_size, "should have shrunk");

    /* grow again after shrink: realloc must work */
    for (int i = 0; i < 10; i++) {
        fs2 v = fs2_from_new(1000 + i);
        fs2arr_push(&a, &v);
        assert_arr_invariants(&a, "re-grow");
    }
    cr_assert_eq(a.count, 60);
    cr_assert_eq(a.hwm, 60);

    /* content: [0, 49] = 1..50, [50, 59] = 1000..1009 */
    CHECK_STR_EQ("[0]",  "1",    a.arr[0].v);
    CHECK_STR_EQ("[49]", "50",   a.arr[49].v);
    CHECK_STR_EQ("[50]", "1000", a.arr[50].v);
    CHECK_STR_EQ("[59]", "1009", a.arr[59].v);

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Simulation 4: trailing holes trim via synchwm + shrink, mid-holes stay
 * ========================================================================= */

Test(fs2_array_simulation, tail_vs_mid_holes) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(16);

    /* Fill 10 */
    for (int i = 0; i < 10; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&a, &v);
    }
    cr_assert_eq(a.hwm,  10);
    cr_assert_eq(a.size, 16);

    /* detach 3 tail → holes at [7,8,9] */
    for (int i = 0; i < 3; i++) {
        fs2 t = fs2arr_detach_new(&a, 9 - i);
        fs2_free(&t);
    }
    assert_arr_invariants(&a, "tail-holes");
    cr_assert_eq(a.hwm,  10);
    cr_assert_eq(a.count, 7);

    /* detach 2 mid → holes at [3], [5] (they shift indexing, easier via count) */
    fs2 t3 = fs2arr_detach_new(&a, 3);
    fs2_free(&t3);
    fs2 t5 = fs2arr_detach_new(&a, 5);
    fs2_free(&t5);
    assert_arr_invariants(&a, "tail-mid-holes");
    cr_assert_eq(a.count, 5);

    /* shrink WITHOUT synchwm: size == hwm == 10, all holes preserved */
    fs2arr_shrink(&a);
    cr_assert_eq(a.hwm,  10);
    cr_assert_eq(a.size, 10);
    assert_arr_invariants(&a, "shrink-no-sync");
    CHECK_TRUE("[3] hole preserved", fs2arr_is_hole(&a.arr[3]));
    CHECK_TRUE("[5] hole preserved", fs2arr_is_hole(&a.arr[5]));
    CHECK_TRUE("[7] hole preserved", fs2arr_is_hole(&a.arr[7]));

    /* now synchwm: tail holes [7,8,9] trimmed, but mid holes [3],[5] stay */
    fs2arr_synchwm(&a);
    cr_assert_eq(a.hwm, 7);           /* last live was at index 6 */
    assert_arr_invariants(&a, "sync");

    /* shrink again: size → 7 */
    fs2arr_shrink(&a);
    cr_assert_eq(a.size, 7);
    assert_arr_invariants(&a, "shrink-after-sync");
    CHECK_TRUE("[3] mid hole survives", fs2arr_is_hole(&a.arr[3]));
    CHECK_TRUE("[5] mid hole survives", fs2arr_is_hole(&a.arr[5]));

    fs2arr_free(&a);
    assert_balanced();
}

/* =========================================================================
 * Simulation 5: many small ops — 200 iterations, invariants after each
 * ========================================================================= */

Test(fs2_array_simulation, long_run_mixed_ops) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);

    for (int iter = 0; iter < 200; iter++) {
        int op = iter % 6;

        if (op <= 2 || a.count == 0) {
            /* push */
            fs2 v = fs2_from_new(iter);
            fs2arr_push(&a, &v);
        } else if (op == 3 || op == 4) {
            /* detach a random-ish slot: pick first live at index iter % hwm */
            size_t cand = (size_t)(iter * 37) % a.hwm;
            /* find first live at or after cand (wrap) */
            size_t found = SIZE_MAX;
            for (size_t k = 0; k < a.hwm; k++) {
                size_t i = (cand + k) % a.hwm;
                if (!fs2arr_is_hole(&a.arr[i])) { found = i; break; }
            }
            cr_assert_neq(found, SIZE_MAX, "no live to detach at iter %d", iter);
            fs2 t = fs2arr_detach_new(&a, found);
            fs2_free(&t);
        } else {
            /* attach into a hole if exists, else push */
            ptrdiff_t hole = fs2arr_find_tillhwm(&a);
            if (hole >= 0) {
                fs2 v = fs2_fromstr("filler");
                fs2arr_attach(&a, &v, (size_t)hole);
            } else {
                fs2 v = fs2_fromstr("filler");
                fs2arr_push(&a, &v);
            }
        }

        assert_arr_invariants(&a, "mixed-iter");

        /* occasionally clean tail / shrink */
        if (iter % 20 == 19) {
            fs2arr_synchwm(&a);
            fs2arr_shrink(&a);
            assert_arr_invariants(&a, "sync-shrink");
        }
    }

    fs2arr_free(&a);
    assert_balanced();
}
