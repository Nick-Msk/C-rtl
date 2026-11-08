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
// static void
// assert_arr_invariants(const fs2array *a, const char *op) {
//     /* Basic ordering */
//     if (a->count > a->hwm)
//         cr_assert_fail("[%s] count=%zu > hwm=%zu", op, a->count, a->hwm);
//     if (a->hwm > a->size)
//         cr_assert_fail("[%s] hwm=%zu > size=%zu", op, a->hwm, a->size);

//     /* Canonical empty */
//     if (a->arr == NULL) {
//         if (a->hwm != 0 || a->size != 0 || a->count != 0)
//             cr_assert_fail("[%s] arr NULL but hwm=%zu size=%zu count=%zu",
//                            op, a->hwm, a->size, a->count);
//         return;
//     }

//     /* [0, hwm): live count matches, each slot is hole or valid fs2 */
//     size_t  live = 0;
//     for (size_t i = 0; i < a->hwm; i++) {
//         if (fs2arr_is_hole(&a->arr[i]))
//             continue;
//         if (!fs2_validate(NULL, &a->arr[i]))
//             cr_assert_fail("[%s] slot[%zu] invalid fs2 (not a hole)", op, i);
//         live++;
//     }
//     if (live != a->count)
//         cr_assert_fail("[%s] live=%zu in [0,hwm) but count=%zu",
//                        op, live, a->count);

//     /* [hwm, size): all holes (padding) */
//     for (size_t i = a->hwm; i < a->size; i++) {
//         if (!fs2arr_is_hole(&a->arr[i]))
//             cr_assert_fail("[%s] slot[%zu] in padding is not a hole", op, i);
//     }
// } 

static inline void
assert_arr_valid(const fs2array *a, const char *op) {
    if (fs2arr_validate(NULL, a))
        return;
    /* re-run with stderr for diagnostics */
    if (a) {
        fprintf(stderr, "[%s] fs2arr_validate failed:\n", op);
        fs2arr_validate(stderr, a);
        fs2arr_techfprint(stderr, a, op);
    }
    cr_assert_fail("[%s] fs2arr_validate failed", op);
}

/* =========================================================================
 * Simulation 1: push → detach → attach → shrink cycle, repeated rounds
 * ========================================================================= */

Test(fs2_array_simulation, push_detach_attach_cycle) {
    __fs2_checkreset();
    fs2array a = fs2arr_init(4);
    assert_arr_valid(&a, "init");

    for (int round = 0; round < 20; round++) {
        /* push 5 values */
        for (int i = 0; i < 5; i++) {
            fs2 v = fs2_from_new(round * 100 + i);
            fs2arr_push(&a, &v);
        }
        assert_arr_valid(&a, "push5");

        /* detach 2 (oldest) — they become holes */
        for (int i = 0; i < 2; i++) {
            ptrdiff_t hole = fs2arr_find_tillhwm(&a);
            cr_assert_geq(hole, 0, "round %d: expected hole", round);
            fs2 t = fs2arr_detach_new(&a, (size_t)hole);
            fs2_free(&t);
        }
        assert_arr_valid(&a, "detach2");

        /* attach 2 new — fills the holes */
        for (int i = 0; i < 2; i++) {
            ptrdiff_t hole = fs2arr_find_tillhwm(&a);
            cr_assert_geq(hole, 0, "round %d: expected hole", round);
            fs2 v = fs2_fromstr("refill");
            fs2arr_attach(&a, &v, (size_t)hole);
        }
        assert_arr_valid(&a, "attach2");

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
        assert_arr_valid(&a, "drained");

        fs2arr_shrink(&a);
        CHECK_PTR_EQ("canonical after shrink", NULL, a.arr);
        assert_arr_valid(&a, "shrunk");
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
    assert_arr_valid(&a, "filled");
    cr_assert_eq(a.hwm, 8);
    cr_assert_eq(a.count, 8);

    /* Free every second slot */
    for (int i = 0; i < 8; i += 2) {
        fs2 t = fs2arr_detach_new(&a, (size_t)i);
        fs2_free(&t);
    }
    assert_arr_valid(&a, "half-detached");
    cr_assert_eq(a.hwm, 8);          /* hwm unchanged (holes not at end) */
    cr_assert_eq(a.count, 4);

    /* Refill via holes — should reuse from position 0, 2, 4, 6 */
    for (int i = 0; i < 4; i++) {
        ptrdiff_t hole = fs2arr_find_tillhwm(&a);
        cr_assert_geq(hole, 0, "expected hole");
        fs2 v = fs2_fromstr("refilled");
        fs2arr_attach(&a, &v, (size_t)hole);
    }
    assert_arr_valid(&a, "refilled");
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
        assert_arr_valid(&a, "grow");
        cr_assert_eq(a.count, (size_t)batch);
    }
    cr_assert_geq(a.size, 50);

    /* freeze: capture size after 50 pushes */
    size_t grown_size = a.size;
    cr_assert_geq(grown_size, 50);

    /* shrink: exact to hwm */
    fs2arr_shrink(&a);
    assert_arr_valid(&a, "shrink");
    cr_assert_eq(a.size, a.hwm);
    cr_assert_eq(a.size, 50);
    cr_assert_lt(a.size, grown_size, "should have shrunk");

    /* grow again after shrink: realloc must work */
    for (int i = 0; i < 10; i++) {
        fs2 v = fs2_from_new(1000 + i);
        fs2arr_push(&a, &v);
        assert_arr_valid(&a, "re-grow");
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
    assert_arr_valid(&a, "tail-holes");
    cr_assert_eq(a.hwm,  10);
    cr_assert_eq(a.count, 7);

    /* detach 2 mid → holes at [3], [5] (they shift indexing, easier via count) */
    fs2 t3 = fs2arr_detach_new(&a, 3);
    fs2_free(&t3);
    fs2 t5 = fs2arr_detach_new(&a, 5);
    fs2_free(&t5);
    assert_arr_valid(&a, "tail-mid-holes");
    cr_assert_eq(a.count, 5);

    /* shrink WITHOUT synchwm: size == hwm == 10, all holes preserved */
    fs2arr_shrink(&a);
    cr_assert_eq(a.hwm,  10);
    cr_assert_eq(a.size, 10);
    assert_arr_valid(&a, "shrink-no-sync");
    CHECK_TRUE("[3] hole preserved", fs2arr_is_hole(&a.arr[3]));
    CHECK_TRUE("[5] hole preserved", fs2arr_is_hole(&a.arr[5]));
    CHECK_TRUE("[7] hole preserved", fs2arr_is_hole(&a.arr[7]));

    /* now synchwm: tail holes [7,8,9] trimmed, but mid holes [3],[5] stay */
    fs2arr_synchwm(&a);
    cr_assert_eq(a.hwm, 7);           /* last live was at index 6 */
    assert_arr_valid(&a, "sync");

    /* shrink again: size → 7 */
    fs2arr_shrink(&a);
    cr_assert_eq(a.size, 7);
    assert_arr_valid(&a, "shrink-after-sync");
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

        assert_arr_valid(&a, "mixed-iter");

        /* occasionally clean tail / shrink */
        if (iter % 20 == 19) {
            fs2arr_synchwm(&a);
            fs2arr_shrink(&a);
            assert_arr_valid(&a, "sync-shrink");
        }
    }

    fs2arr_free(&a);
    assert_balanced();
}

TestSuite(fs2_array_simulation, .init = test_setup, .fini = test_teardown);

/* =========================================================================
 * Helpers
 * ========================================================================= */

/* Verify: array is packed (hwm==count), tight (size==count), sorted by cmp,
 * valid fs2arr_validate, and contains exactly @p expected_count elements. */
static void
assert_sorted_packed_tight(const fs2array *r,
                           pointer_comparator_t cmp,
                           size_t expected_count,
                           const char *op) {
    if (r->count != expected_count)
        cr_assert_fail("[%s] count=%zu, expected %zu",
                       op, r->count, expected_count);
    if (r->hwm != r->count)
        cr_assert_fail("[%s] not packed: hwm=%zu count=%zu",
                       op, r->hwm, r->count);
    if (r->size != r->count)
        cr_assert_fail("[%s] not tight: size=%zu count=%zu",
                       op, r->size, r->count);
    if (!fs2arr_issorted(r, cmp))
        cr_assert_fail("[%s] not sorted", op);
    if (!fs2arr_validate(NULL, r))
        cr_assert_fail("[%s] validate failed", op);
}

/* Multiset-preservation: sorted-then-compare via merge vs sort of concatenation */
static fs2array
sort_concat_manual(const fs2array *a, const fs2array *b, pointer_comparator_t cmp) {
    fs2array c = fs2arr_init(a->count + b->count);
    for (size_t i = 0; i < a->count; i++) {
        fs2 v = fs2_clone(&a->arr[i]);
        fs2arr_push(&c, &v);
    }
    for (size_t i = 0; i < b->count; i++) {
        fs2 v = fs2_clone(&b->arr[i]);
        fs2arr_push(&c, &v);
    }
    fs2arr_sort(&c, cmp);
    return c;
}

/* =========================================================================
 * Simulation 1: pairwise merge of N sorted chunks → final sorted, multiset preserved
 * ========================================================================= */

Test(fs2_array_simulation, pairwise_merge_chain) {
    __fs2_checkreset();

    /* 6 sorted arrays: {a,c}, {b,d}, {e,g}, {f,h}, {i,k}, {j,l} */
    const char *chunks[][2] = {
        { "a", "c" }, { "b", "d" },
        { "e", "g" }, { "f", "h" },
        { "i", "k" }, { "j", "l" },
    };
    fs2array arr[6];
    for (int i = 0; i < 6; i++) {
        arr[i] = fs2arr_init(2);
        for (int j = 0; j < 2; j++) {
            fs2 v = fs2_fromstr(chunks[i][j]);
            fs2arr_push(&arr[i], &v);
        }
        fs2arr_sort(&arr[i], fs2_cmp_wrap);
        assert_sorted_packed_tight(&arr[i], fs2_cmp_wrap, 2, "chunk");
    }

    /* Pairwise merge: (0+1), (2+3), (4+5) → 3 arrays of 4 */
    fs2array m01 = fs2arr_mergesorted_new(&arr[0], &arr[1], fs2_cmp_wrap);
    fs2array m23 = fs2arr_mergesorted_new(&arr[2], &arr[3], fs2_cmp_wrap);
    fs2array m45 = fs2arr_mergesorted_new(&arr[4], &arr[5], fs2_cmp_wrap);
    assert_sorted_packed_tight(&m01, fs2_cmp_wrap, 4, "m01");
    assert_sorted_packed_tight(&m23, fs2_cmp_wrap, 4, "m23");
    assert_sorted_packed_tight(&m45, fs2_cmp_wrap, 4, "m45");

    /* Second level: (m01+m23), m45 → {8, 4} */
    fs2array m0123 = fs2arr_mergesorted_new(&m01, &m23, fs2_cmp_wrap);
    assert_sorted_packed_tight(&m0123, fs2_cmp_wrap, 8, "m0123");

    /* Final: 8 + 4 = 12 */
    fs2array final = fs2arr_mergesorted_new(&m0123, &m45, fs2_cmp_wrap);
    assert_sorted_packed_tight(&final, fs2_cmp_wrap, 12, "final");

    /* Content check: a..l sorted */
    const char *expected[] = {
        "a","b","c","d","e","f","g","h","i","j","k","l"
    };
    for (size_t i = 0; i < 12; i++)
        CHECK_STR_EQ("final content", expected[i], final.arr[i].v);

    /* Cleanup */
    for (int i = 0; i < 6; i++) fs2arr_free(&arr[i]);
    fs2arr_free(&m01); fs2arr_free(&m23); fs2arr_free(&m45);
    fs2arr_free(&m0123);
    fs2arr_free(&final);
    assert_balanced();
}

/* =========================================================================
 * Simulation 2: merge(merge(a,b), merge(c,d)) ≡ sort(concat(a,b,c,d))
 *   — multiset preservation, result identical to full sort
 * ========================================================================= */

Test(fs2_array_simulation, merge_equals_full_sort) {
    __fs2_checkreset();

    const char *av[] = { "a", "d", "g" };
    const char *bv[] = { "b", "e", "h" };
    const char *cv[] = { "c", "f", "i" };
    const char *dv[] = { "j", "k", "l" };

    fs2array a = fs2arr_init(3), b = fs2arr_init(3);
    fs2array c = fs2arr_init(3), d = fs2arr_init(3);
    for (int i = 0; i < 3; i++) {
        fs2 va = fs2_fromstr(av[i]); fs2arr_push(&a, &va);
        fs2 vb = fs2_fromstr(bv[i]); fs2arr_push(&b, &vb);
        fs2 vc = fs2_fromstr(cv[i]); fs2arr_push(&c, &vc);
        fs2 vd = fs2_fromstr(dv[i]); fs2arr_push(&d, &vd);
    }
    fs2arr_sort(&a, fs2_cmp_wrap);
    fs2arr_sort(&b, fs2_cmp_wrap);
    fs2arr_sort(&c, fs2_cmp_wrap);
    fs2arr_sort(&d, fs2_cmp_wrap);

    /* Path 1: merge-merge */
    fs2array mab = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);
    fs2array mcd = fs2arr_mergesorted_new(&c, &d, fs2_cmp_wrap);
    fs2array merged = fs2arr_mergesorted_new(&mab, &mcd, fs2_cmp_wrap);

    /* Path 2: clone all + concat + sort */
    fs2array mab2 = sort_concat_manual(&a, &b, fs2_cmp_wrap);
    fs2array mcd2 = sort_concat_manual(&c, &d, fs2_cmp_wrap);
    fs2array concat = sort_concat_manual(&mab2, &mcd2, fs2_cmp_wrap);

    CHECK_TRUE("merge == sort of concat",
               fs2arr_eqexact(&merged, &concat));

    fs2arr_free(&a); fs2arr_free(&b); fs2arr_free(&c); fs2arr_free(&d);
    fs2arr_free(&mab); fs2arr_free(&mcd); fs2arr_free(&merged);
    fs2arr_free(&mab2); fs2arr_free(&mcd2); fs2arr_free(&concat);
    assert_balanced();
}

/* =========================================================================
 * Simulation 3: duplicates across sources — multiset preserved, stable
 * ========================================================================= */

Test(fs2_array_simulation, merge_duplicates_multiset) {
    __fs2_checkreset();

    const char *av[] = { "a", "b", "b", "c" };
    const char *bv[] = { "b", "b", "d", "d" };
    fs2array a = fs2arr_init(4), b = fs2arr_init(4);
    for (int i = 0; i < 4; i++) {
        fs2 va = fs2_fromstr(av[i]); fs2arr_push(&a, &va);
        fs2 vb = fs2_fromstr(bv[i]); fs2arr_push(&b, &vb);
    }
    fs2arr_sort(&a, fs2_cmp_wrap);
    fs2arr_sort(&b, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);
    assert_sorted_packed_tight(&r, fs2_cmp_wrap, 8, "dups");

    /* Content: a, b×4, c, d×2 */
    const char *expected[] = { "a","b","b","b","b","c","d","d" };
    for (size_t i = 0; i < 8; i++)
        CHECK_STR_EQ("dups", expected[i], r.arr[i].v);

    fs2arr_free(&a); fs2arr_free(&b); fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Simulation 4: split → sort → merge back ≡ original sorted
 *   Real-world scenario: split string, sort parts, merge halves, compare to sort-all
 * ========================================================================= */

Test(fs2_array_simulation, split_sort_merge_roundtrip) {
    __fs2_checkreset();

    fs2 csv = fs2_fromstr("delta,alpha,charlie,bravo,echo,fox,golf,hotel");
    fs2 comma = fs2_fromstr(",");

    fs2array parts = fs2_split_toarray(&csv, &comma);
    cr_assert_eq(parts.count, 8);

    /* Option A: sort the whole */
    fs2array all = fs2arr_clone(&parts);
    fs2arr_sort(&all, fs2_cmp_wrap);
    assert_sorted_packed_tight(&all, fs2_cmp_wrap, 8, "sort-all");

    /* Option B: sort halves, merge */
    fs2array half1 = fs2arr_init(4);
    fs2array half2 = fs2arr_init(4);
    for (size_t i = 0; i < 4; i++) {
        fs2 v = fs2_clone(&parts.arr[i]);
        fs2arr_push(&half1, &v);
    }
    for (size_t i = 4; i < 8; i++) {
        fs2 v = fs2_clone(&parts.arr[i]);
        fs2arr_push(&half2, &v);
    }
    fs2arr_sort(&half1, fs2_cmp_wrap);
    fs2arr_sort(&half2, fs2_cmp_wrap);

    fs2array via_merge = fs2arr_mergesorted_new(&half1, &half2, fs2_cmp_wrap);
    assert_sorted_packed_tight(&via_merge, fs2_cmp_wrap, 8, "sort-halves-merge");

    /* Both give identical results */
    CHECK_TRUE("sort-all == sort-halves-merge",
               fs2arr_eqexact(&all, &via_merge));

    /* Content check */
    const char *expected[] = {
        "alpha","bravo","charlie","delta","echo","fox","golf","hotel"
    };
    for (size_t i = 0; i < 8; i++)
        CHECK_STR_EQ("result", expected[i], via_merge.arr[i].v);

    fs2_free(&csv); fs2_free(&comma);
    fs2arr_free(&parts); fs2arr_free(&all);
    fs2arr_free(&half1); fs2arr_free(&half2);
    fs2arr_free(&via_merge);
    assert_balanced();
}

/* =========================================================================
 * Simulation 5: repeated merges (growing accumulator)
 *   Start with 1-element array, merge with each next 1-element array.
 *   Like incremental insertion via merge.
 * ========================================================================= */

Test(fs2_array_simulation, incremental_grow_merge) {
    __fs2_checkreset();

    const char *vals[] = {
        "m","a","z","c","f","b","x","d","q","e",
        "n","g","w","h","v","i","u","j","t","k"
    };
    const size_t n = sizeof vals / sizeof vals[0];

    /* Start with sorted [vals[0]] */
    fs2array acc = fs2arr_init(1);
    fs2 v0 = fs2_fromstr(vals[0]);
    fs2arr_push(&acc, &v0);

    for (size_t i = 1; i < n; i++) {
        /* Single-element array */
        fs2array single = fs2arr_init(1);
        fs2 v = fs2_fromstr(vals[i]);
        fs2arr_push(&single, &v);

        /* Merge acc + single → new acc */
        fs2array next = fs2arr_mergesorted_new(&acc, &single, fs2_cmp_wrap);

        assert_sorted_packed_tight(&next, fs2_cmp_wrap, i + 1, "incremental");

        fs2arr_free(&acc);
        fs2arr_free(&single);
        acc = next;
    }

    CHECK_EQ("final count", n, acc.count);
    CHECK_STR_EQ("[0]",  "a", acc.arr[0].v);
    CHECK_STR_EQ("[19]", "z", acc.arr[19].v);

    fs2arr_free(&acc);
    assert_balanced();
}

/* =========================================================================
 * Simulation 6: merge desc chain
 * ========================================================================= */

Test(fs2_array_simulation, desc_merge_chain) {
    __fs2_checkreset();

    const char *av[] = { "z", "w", "u" };       /* desc sorted: z>w>u */
    const char *bv[] = { "y", "x", "v" };
    const char *cv[] = { "t", "s", "r" };
    fs2array a = fs2arr_init(3), b = fs2arr_init(3), c = fs2arr_init(3);
    for (int i = 0; i < 3; i++) {
        fs2 va = fs2_fromstr(av[i]); fs2arr_push(&a, &va);
        fs2 vb = fs2_fromstr(bv[i]); fs2arr_push(&b, &vb);
        fs2 vc = fs2_fromstr(cv[i]); fs2arr_push(&c, &vc);
    }
    fs2arr_sort(&a, fs2_cmp_desc_wrap);
    fs2arr_sort(&b, fs2_cmp_desc_wrap);
    fs2arr_sort(&c, fs2_cmp_desc_wrap);

    fs2array mab = fs2arr_mergesorted_new(&a, &b, fs2_cmp_desc_wrap);
    fs2array final = fs2arr_mergesorted_new(&mab, &c, fs2_cmp_desc_wrap);

    assert_sorted_packed_tight(&final, fs2_cmp_desc_wrap, 9, "desc");

    /* Content: z..r descending */
    const char *expected[] = {
        "z","y","x","w","v","u","t","s","r"
    };
    for (size_t i = 0; i < 9; i++)
        CHECK_STR_EQ("desc", expected[i], final.arr[i].v);

    fs2arr_free(&a); fs2arr_free(&b); fs2arr_free(&c);
    fs2arr_free(&mab); fs2arr_free(&final);
    assert_balanced();
}

/* =========================================================================
 * Simulation 7: many small random-ish merges with multiset invariant check
 * ========================================================================= */

Test(fs2_array_simulation, pairwise_many_multiset_preserved) {
    __fs2_checkreset();

    /* 5 sources, each with 3 elements */
    const char *srcs[5][3] = {
        { "apple",  "cherry", "elderberry" },
        { "banana", "date",   "fig" },
        { "grape",  "kiwi",   "lemon" },
        { "mango",  "nectar", "orange" },
        { "papaya", "quince", "raspberry" },
    };
    fs2array arr[5];
    for (int i = 0; i < 5; i++) {
        arr[i] = fs2arr_init(3);
        for (int j = 0; j < 3; j++) {
            fs2 v = fs2_fromstr(srcs[i][j]);
            fs2arr_push(&arr[i], &v);
        }
        fs2arr_sort(&arr[i], fs2_cmp_wrap);
    }

    /* Tree merge: ((0+1)+(2+3))+4 */
    fs2array m01 = fs2arr_mergesorted_new(&arr[0], &arr[1], fs2_cmp_wrap);
    fs2array m23 = fs2arr_mergesorted_new(&arr[2], &arr[3], fs2_cmp_wrap);
    fs2array m0123 = fs2arr_mergesorted_new(&m01, &m23, fs2_cmp_wrap);
    fs2array final = fs2arr_mergesorted_new(&m0123, &arr[4], fs2_cmp_wrap);

    assert_sorted_packed_tight(&final, fs2_cmp_wrap, 15, "tree");

    /* Verify multiset via sort of flat concat */
    fs2array flat = fs2arr_init(15);
    for (int i = 0; i < 5; i++)
        for (size_t j = 0; j < arr[i].count; j++) {
            fs2 v = fs2_clone(&arr[i].arr[j]);
            fs2arr_push(&flat, &v);
        }
    fs2arr_sort(&flat, fs2_cmp_wrap);

    CHECK_TRUE("merged multiset == sorted flat",
               fs2arr_eqexact(&final, &flat));

    for (int i = 0; i < 5; i++) fs2arr_free(&arr[i]);
    fs2arr_free(&m01); fs2arr_free(&m23); fs2arr_free(&m0123);
    fs2arr_free(&final); fs2arr_free(&flat);
    assert_balanced();
}

/* =========================================================================
 * Simulation 8: merge with one very large / one tiny — asymmetry
 * ========================================================================= */

Test(fs2_array_simulation, merge_large_small) {
    __fs2_checkreset();

    /* Large: numbers 0..99 as strings (sorted asc) */
    fs2array big = fs2arr_init(100);
    for (int i = 0; i < 100; i++) {
        fs2 v = fs2_from_new(i);
        fs2arr_push(&big, &v);
    }
    /* Already sorted (from_new(i) asc for 0..99). */

    /* Small: "-5", "-1", "150" (inserts at edges) */
    fs2array small = fs2arr_init(3);
    fs2 v1 = fs2_fromstr("-5"); fs2arr_push(&small, &v1);
    fs2 v2 = fs2_fromstr("-1"); fs2arr_push(&small, &v2);
    fs2 v3 = fs2_fromstr("150"); fs2arr_push(&small, &v3);
    fs2arr_sort(&small, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&small, &big, fs2_cmp_wrap);

    assert_sorted_packed_tight(&r, fs2_cmp_wrap, 103, "large-small");
    CHECK_STR_EQ("[0]",   "-5", r.arr[0].v);
    CHECK_STR_EQ("[1]",   "-1", r.arr[1].v);
    CHECK_STR_EQ("[2]",   "0",  r.arr[2].v);
    CHECK_STR_EQ("[101]", "99", r.arr[101].v);
    CHECK_STR_EQ("[102]", "150", r.arr[102].v);

    fs2arr_free(&big); fs2arr_free(&small); fs2arr_free(&r);
    assert_balanced();
}

/* =========================================================================
 * Simulation 9: merge preserves deep-copy (result independent from inputs)
 * ========================================================================= */

Test(fs2_array_simulation, merge_independent_from_inputs) {
    __fs2_checkreset();

    const char *av[] = { "a", "c" };
    const char *bv[] = { "b", "d" };
    fs2array a = fs2arr_init(2), b = fs2arr_init(2);
    for (int i = 0; i < 2; i++) {
        fs2 va = fs2_fromstr(av[i]); fs2arr_push(&a, &va);
        fs2 vb = fs2_fromstr(bv[i]); fs2arr_push(&b, &vb);
    }
    fs2arr_sort(&a, fs2_cmp_wrap);
    fs2arr_sort(&b, fs2_cmp_wrap);

    fs2array r = fs2arr_mergesorted_new(&a, &b, fs2_cmp_wrap);

    /* Mutate result — inputs must stay untouched */
    r.arr[0].v[0] = 'X';            /* "a" → "X" */
    CHECK_STR_EQ("a still 'a'", "a", a.arr[0].v);
    CHECK_STR_EQ("b still 'b'", "b", b.arr[0].v);
    CHECK_STR_EQ("r[0] mutated", "X", r.arr[0].v);

    /* Free result — inputs still valid (deep copy) */
    fs2arr_free(&r);
    CHECK_STR_EQ("a intact after r free", "a", a.arr[0].v);
    CHECK_STR_EQ("b intact after r free", "b", b.arr[0].v);

    fs2arr_free(&a); fs2arr_free(&b);
    assert_balanced();
}

/* =========================================================================
 * Simulation 10: repeated sort+merge cycles in a loop — stability of invariants
 * ========================================================================= */

Test(fs2_array_simulation, repeated_cycles_invariants) {
    __fs2_checkreset();

    for (int round = 0; round < 20; round++) {
        /* Build 4 sorted arrays of 2 elements each from round numbers */
        fs2array arr[4];
        for (int k = 0; k < 4; k++) {
            arr[k] = fs2arr_init(2);
            for (int j = 0; j < 2; j++) {
                fs2 v = fs2_from_new(round * 100 + k * 10 + j);
                fs2arr_push(&arr[k], &v);
            }
            fs2arr_sort(&arr[k], fs2_cmp_wrap);
            assert_sorted_packed_tight(&arr[k], fs2_cmp_wrap, 2, "round-chunk");
        }

        fs2array m01 = fs2arr_mergesorted_new(&arr[0], &arr[1], fs2_cmp_wrap);
        fs2array m23 = fs2arr_mergesorted_new(&arr[2], &arr[3], fs2_cmp_wrap);
        fs2array fin = fs2arr_mergesorted_new(&m01, &m23, fs2_cmp_wrap);

        assert_sorted_packed_tight(&fin, fs2_cmp_wrap, 8, "round-final");

        for (int k = 0; k < 4; k++) fs2arr_free(&arr[k]);
        fs2arr_free(&m01); fs2arr_free(&m23); fs2arr_free(&fin);
    }

    assert_balanced();
}