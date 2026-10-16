#include <criterion/criterion.h>

#include "fs2.h"
#include "log.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Fixtures — mute stderr, init log
 * ------------------------------------------------------------------------- */

TestSuite(fs2, .init = test_setup, .fini = test_teardown);

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(fs2, version_string_matches_macro) {
    CHECK_STR_EQ("fs2_version() == FS2_VERSION", fs2_version(), FS2_VERSION);
    CHECK_STR_EQ("fs2_version()",                fs2_version(), "0.1.0");
}

Test(fs2, version_is_top_of_list) {
    const char *const *v = fs2_versions();

    CHECK_NOT_NULL("fs2_versions()",  v);
    CHECK_NOT_NULL("fs2_versions()[0]", v[0]);
    CHECK_STR_EQ("versions[0] vs fs2_version()", v[0], fs2_version());
    CHECK_STR_EQ("versions[0] vs FS2_VERSION",   v[0], FS2_VERSION);
}

Test(fs2, versions_terminated_by_null) {
    const char *const *v = fs2_versions();
    size_t n = 0;
    while (v[n]) ++n;

    CHECK_TRUE("at least one version", n >= 1);
    CHECK_NULL("versions[n]", v[n]);
}

/* -------------------------------------------------------------------------
 * Value macros — flags, len, sz, v
 * ------------------------------------------------------------------------- */

Test(fs2, macro_fs2_default) {
    fs2 s = FS2();

    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("sz",  s.sz,  0);
    CHECK_NULL("v", s.v);
    CHECK_TRUE ("is_alloc",     fs2_is_alloc(&s));
    CHECK_FALSE("is_static",    fs2_is_static(&s));
    CHECK_FALSE("is_bodyalloc", fs2_is_bodyalloc(&s));
}

Test(fs2, macro_empty_static) {
    fs2 s = FS2_EMPTY;

    CHECK_TRUE("is_static", fs2_is_static(&s));
    CHECK_FALSE("is_alloc", fs2_is_alloc(&s));
    CHECK_EQ("len", s.len, 0);
    CHECK_STR_EQ("v", s.v, "");
}

Test(fs2, macro_literal) {
    fs2 s = FS2_LITERAL("hello");

    CHECK_TRUE("is_static", fs2_is_static(&s));
    CHECK_EQ("len", s.len, 5);
    CHECK_EQ("sz",  s.sz,  6);
    CHECK_STR_EQ("v", s.v, "hello");
}

Test(fs2, macro_local) {
    char buf[32];
    fs2 s = FS2_LOCAL(buf);

    CHECK_TRUE ("is_local",     fs2_is_local(&s));
    CHECK_FALSE("is_static",    fs2_is_static(&s));
    CHECK_FALSE("is_alloc",     fs2_is_alloc(&s));
    CHECK_EQ("sz",  s.sz,  32);
    CHECK_EQ("len", s.len, 0);
    CHECK_PTR_EQ("v points into buf", s.v, buf);
}

/* -------------------------------------------------------------------------
 * Getters
 * ------------------------------------------------------------------------- */

Test(fs2, getters_literal) {
    fs2 s = FS2_LITERAL("abc");

    CHECK_EQ("fs2_len", s.len, 3);
    CHECK_EQ("fs2_sz",  s.sz,  4);
    CHECK_STR_EQ("fs2_cstr", fs2_cstr(&s), "abc");
    CHECK_PTR_EQ("fs2_str == v", fs2_str(&s), s.v);
}

Test(fs2, getters_empty) {
    fs2 s = FS2_EMPTY;

    CHECK_EQ("fs2_len", s.len, 0);
    CHECK_STR_EQ("fs2_cstr", fs2_cstr(&s), "");
}

/* -------------------------------------------------------------------------
 * fs2_init
 * ------------------------------------------------------------------------- */

Test(fs2, init_small) {
    fs2 s = fs2_init(8);

    CHECK_NOT_NULL("v", s.v);
    CHECK_TRUE ("is_alloc",    fs2_is_alloc(&s));
    CHECK_FALSE("is_bodyalloc", fs2_is_bodyalloc(&s));
    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("v[0]", s.v[0], '\0');
    CHECK_TRUE("sz >= 8 (power-of-two overshoot)", s.sz >= 8);

    fs2_free(&s);
}

Test(fs2, init_is_power_of_two_rounded_up) {
    fs2 s = fs2_init(5);
    /* round_up_2(5) == 8 -> floored at FS2_MIN_ALLOC = 16 */
    CHECK_EQ("sz", s.sz, 16);
    fs2_free(&s);
}

Test(fs2, init_exact_power_of_two_bumps_up) {
    fs2 s = fs2_init(4);
    CHECK_EQ("sz", s.sz, 16);
    fs2_free(&s);
}

Test(fs2, init_large_power_of_two) {
    fs2 s = fs2_init(100);
    /* round_up_2(100) = 128 */
    CHECK_EQ("sz", s.sz, 128);
    fs2_free(&s);
}

#if TEST_NOT_SANITIZED
Test(fs2, init_large_fails_with_sigint, .signal = SIGINT) {
    /* 2^62 rounds up to 2^63 — malloc will fail on any realistic host */
    (void)fs2_init((size_t)1 << 62);
}
#endif

Test(fs2, init_zero_does_not_crash) {
    fs2 s = fs2_init(0);

    CHECK_NOT_NULL("v", s.v);
    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("v[0]", s.v[0], '\0');
    CHECK_EQ("sz floored at FS2_MIN_ALLOC", s.sz, 16);
    CHECK_TRUE("is_alloc", fs2_is_alloc(&s));
    fs2_free(&s);
}

Test(fs2, init_small_bumps_to_min) {
    fs2 s = fs2_init(5);
    CHECK_EQ("sz", s.sz, 16);
    fs2_free(&s);
}

/* -------------------------------------------------------------------------
 * fs2_free — negatives and idempotency
 * ------------------------------------------------------------------------- */

Test(fs2, free_null_noop) {
    fs2_free(NULL);   /* must not crash */
    CHECK_TRUE("reached after free(NULL)", 1);
}

Test(fs2, free_static_noop) {
    fs2 s = FS2_LITERAL("hello");
    fs2_free(&s);

    CHECK_STR_EQ("v untouched", s.v, "hello");
    CHECK_EQ("len untouched", s.len, 5);
    CHECK_EQ("sz  untouched", s.sz,  6);
}

Test(fs2, free_empty_static_noop) {
    fs2 s = FS2_EMPTY;
    fs2_free(&s);

    CHECK_TRUE("is_static", fs2_is_static(&s));
    CHECK_STR_EQ("v", s.v, "");
}

Test(fs2, free_local_noop) {
    char buf[16] = "abc";
    fs2 s = FS2_LOCAL(buf);
    s.len = 3;
    fs2_free(&s);

    CHECK_STR_EQ("v untouched", s.v, "abc");
    CHECK_PTR_EQ("v still points into buf", s.v, buf);
}

Test(fs2, free_alloc_releases_buffer) {
    fs2 s = fs2_init(32);
    CHECK_NOT_NULL("v before free", s.v);

    fs2_free(&s);

    CHECK_NULL("v after free", s.v);
    CHECK_EQ("len", s.len, 0);
    CHECK_EQ("sz",  s.sz,  0);
}

Test(fs2, free_alloc_is_idempotent) {
    fs2 s = fs2_init(32);
    fs2_free(&s);
    fs2_free(&s);   /* must not crash on double free */
    CHECK_NULL("v", s.v);
}

/* -------------------------------------------------------------------------
 * fs2_setlen
 * ------------------------------------------------------------------------- */

Test(fs2, setlen_truncates) {
    fs2 s = fs2_init(16);
    memcpy(s.v, "hello world", 12);
    s.len = 11;

    fs2_setlen(&s, 5);

    CHECK_EQ("len", s.len, 5);
    CHECK_EQ("v[5] is NUL", s.v[5], '\0');
    CHECK_STR_EQ("v", s.v, "hello");
    fs2_free(&s);
}

Test(fs2, setlen_to_zero) {
    fs2 s = fs2_init(16);
    memcpy(s.v, "abc", 4);
    s.len = 3;

    fs2_setlen(&s, 0);

    CHECK_EQ("len", s.len, 0);
    CHECK_STR_EQ("v", s.v, "");
    fs2_free(&s);
}

/* -------------------------------------------------------------------------
 * fs2_is_empty / fs2_is_null
 * ------------------------------------------------------------------------- */

Test(fs2, is_empty_true_on_zero_len) {
    fs2 s = FS2_LITERAL("");
    CHECK_TRUE("is_empty", fs2_is_empty(&s));
}

Test(fs2, is_empty_false_on_nonempty) {
    fs2 s = FS2_LITERAL("x");
    CHECK_FALSE("is_empty", fs2_is_empty(&s));
}

Test(fs2, is_null_on_pointer_null) {
    CHECK_TRUE("is_null(NULL)", fs2_is_null(NULL));
}

Test(fs2, is_null_on_empty_alloc) {
    fs2 s = FS2();
    CHECK_TRUE("is_null(FS2())", fs2_is_null(&s));
}

Test(fs2, is_null_false_on_literal) {
    fs2 s = FS2_LITERAL("ok");
    CHECK_FALSE("is_null", fs2_is_null(&s));
}

/* -------------------------------------------------------------------------
 * fs2_validate
 * ------------------------------------------------------------------------- */

Test(fs2, validate_ok_literal) {
    fs2 s = FS2_LITERAL("hello");
    CHECK_TRUE("validate", fs2_validate(NULL, &s));
}

Test(fs2, validate_ok_init) {
    fs2 s = fs2_init(16);
    CHECK_TRUE("validate", fs2_validate(NULL, &s));
    fs2_free(&s);
}

Test(fs2, validate_null_pointer) {
    CHECK_FALSE("validate(NULL)", fs2_validate(NULL, NULL));
}

Test(fs2, validate_null_buffer) {
    fs2 s = FS2();   /* v == NULL */
    CHECK_FALSE("validate(FS2())", fs2_validate(NULL, &s));
}

Test(fs2, validate_len_ge_sz) {
    fs2 s = FS2_LITERAL("abc");
    s.len = s.sz;   /* len >= sz — forged invariant violation */
    CHECK_FALSE("validate with len >= sz", fs2_validate(NULL, &s));
}

Test(fs2, validate_len_gt_strlen) {
    fs2 s = FS2_LITERAL("abc");
    s.len = 100;
    CHECK_FALSE("validate with len=100", fs2_validate(NULL, &s));
}

Test(fs2, validate_writes_to_stream) {
    char path[256];
    test_tmpname(path, sizeof path, "test_fs2_validate", NULL, "txt");

    FILE *f = fopen(path, "w");
    CHECK_NOT_NULL("fopen write", f);

    fs2 s = FS2();   /* invalid: v == NULL */
    CHECK_FALSE("validate(FS2())", fs2_validate(f, &s));
    fclose(f);

    char buf[512];
    size_t n = test_read_file(path, buf, sizeof buf);
    remove(path);

    CHECK_TRUE("non-empty output", n > 0);
    CHECK_NOT_NULL("strstr \"nullable\"", strstr(buf, "nullable"));
}

/* -------------------------------------------------------------------------
 * fs_techfprint
 * ------------------------------------------------------------------------- */

Test(fs2, techfprint_literal) {
    char path[256];
    test_tmpname(path, sizeof path, "test_fs2_tech", NULL, "txt");

    FILE *f = fopen(path, "w");
    CHECK_NOT_NULL("fopen write", f);

    fs2  s = FS2_LITERAL("payload");
    long n = fs_techfprint(f, &s, "s");
    fclose(f);

    CHECK_TRUE("positive write count", n > 0);

    char buf[512];
    size_t got = test_read_file(path, buf, sizeof buf);
    remove(path);

    CHECK_TRUE("non-empty output", got > 0);
    CHECK_NOT_NULL("strstr \"FS2:\"",   strstr(buf, "FS2:"));
    CHECK_NOT_NULL("strstr \"payload\"", strstr(buf, "payload"));
}

Test(fs2, techfprint_null_s) {
    char path[256];
    test_tmpname(path, sizeof path, "test_fs2_tech_null", NULL, "txt");

    FILE *f = fopen(path, "w");
    CHECK_NOT_NULL("fopen write", f);

    long n = fs_techfprint(f, NULL, "s");
    fclose(f);

    CHECK_TRUE("positive write count", n > 0);

    char buf[512];
    size_t got = test_read_file(path, buf, sizeof buf);
    remove(path);

    CHECK_TRUE("non-empty output", got > 0);
    CHECK_NOT_NULL("strstr \"<NULL>\"", strstr(buf, "<NULL>"));
}

Test(fs2, techfprint_null_out_returns_zero) {
    fs2 s = FS2_LITERAL("x");
    CHECK_EQ("fs_techfprint(NULL, ...)", fs_techfprint(NULL, &s, "s"), 0);
}

/* -------------------------------------------------------------------------
 * FS2LOCAL — stack buffer, no ownership
 * ------------------------------------------------------------------------- */

Test(fs2, local_creates_empty) {
    FS2LOCAL(t, 32);

    CHECK_NOT_NULL("v", t.v);
    CHECK_EQ("sz",  t.sz,  32);
    CHECK_EQ("len", t.len, 0);
    CHECK_EQ("v[0] is NUL", t.v[0], '\0');
    CHECK_TRUE ("is_local",     fs2_is_local(&t));
    CHECK_FALSE("is_static",    fs2_is_static(&t));
    CHECK_FALSE("is_alloc",     fs2_is_alloc(&t));
    CHECK_FALSE("is_bodyalloc", fs2_is_bodyalloc(&t));
    CHECK_TRUE ("is_empty",     fs2_is_empty(&t));
}

Test(fs2, local_buffer_is_writable) {
    FS2LOCAL(t, 16);

    memcpy(t.v, "hello", 6);
    t.len = 5;

    CHECK_STR_EQ("v", t.v, "hello");
    CHECK_EQ("len", t.len, (size_t) 5);
    CHECK_EQ("sz",  t.sz,  (size_t) 16);
    CHECK_TRUE("is_local", fs2_is_local(&t));
}

Test(fs2, local_free_is_noop) {
    FS2LOCAL(t, 16);
    memcpy(t.v, "keep me", 8);
    t.len = 7;

    fs2_free(&t);

    /* STATIC / LOCAL: free must not touch the buffer */
    CHECK_STR_EQ("v untouched", t.v, "keep me");
    CHECK_EQ("len untouched", t.len, 7);
    CHECK_TRUE("still local", fs2_is_local(&t));
}

Test(fs2, local_two_instances_independent) {
    FS2LOCAL(a, 8);
    FS2LOCAL(b, 16);

    memcpy(a.v, "aaa",  4); a.len = 3;
    memcpy(b.v, "bbbb", 5); b.len = 4;

    CHECK_STR_EQ("a.v", a.v, "aaa");
    CHECK_STR_EQ("b.v", b.v, "bbbb");
    CHECK_EQ("a.sz", a.sz, 8);
    CHECK_EQ("b.sz", b.sz, 16);
    CHECK_PTR_NEQ("a.v vs b.v", a.v, b.v);
}

/* -------------------------------------------------------------------------
 * FS2LOCALFILL — format into stack buffer
 * ------------------------------------------------------------------------- */

Test(fs2, localfill_empty_format) {
    FS2LOCALFILL(t, "%s", "");

    CHECK_EQ("len", t.len, 0);
    CHECK_EQ("sz",  t.sz,  1);
    CHECK_STR_EQ("v", t.v, "");
    CHECK_TRUE("is_local", fs2_is_local(&t));
}

Test(fs2, localfill_plain_string) {
    FS2LOCALFILL(t, "hello world");

    CHECK_EQ("len", t.len, 11);
    CHECK_STR_EQ("v", t.v, "hello world");
    CHECK_EQ("sz",  t.sz,  12);
    CHECK_TRUE("is_local", fs2_is_local(&t));
}

Test(fs2, localfill_int) {
    FS2LOCALFILL(t, "x=%d", 42);

    CHECK_EQ("len", t.len, 4);
    CHECK_STR_EQ("v", t.v, "x=42");
}

Test(fs2, localfill_string_arg) {
    const char *name = "alice";
    FS2LOCALFILL(t, "user=%s", name);

    CHECK_EQ("len", t.len, 10);
    CHECK_STR_EQ("v", t.v, "user=alice");
}

Test(fs2, localfill_mixed) {
    FS2LOCALFILL(t, "%s=%d (%c)", "count", 7, 'x');

    CHECK_STR_EQ("v", t.v, "count=7 (x)");
    CHECK_EQ("len", t.len, 11);
}

Test(fs2, localfill_negative_int) {
    FS2LOCALFILL(t, "%d", -123);

    CHECK_STR_EQ("v", t.v, "-123");
    CHECK_EQ("len", t.len, 4);
}

Test(fs2, localfill_two_instances) {
    FS2LOCALFILL(a, "first=%d",  1);
    FS2LOCALFILL(b, "second=%d", 2);

    CHECK_STR_EQ("a.v", a.v, "first=1");
    CHECK_STR_EQ("b.v", b.v, "second=2");
    CHECK_PTR_NEQ("a.v vs b.v", a.v, b.v);
    CHECK_TRUE("a is_local", fs2_is_local(&a));
    CHECK_TRUE("b is_local", fs2_is_local(&b));
}

Test(fs2, localfill_free_is_noop) {
    FS2LOCALFILL(t, "keep me please");

    fs2_free(&t);

    CHECK_STR_EQ("v untouched", t.v, "keep me please");
    CHECK_EQ("len untouched", t.len, (int)strlen("keep me please"));
    CHECK_TRUE("still local", fs2_is_local(&t));
}

/* -------------------------------------------------------------------------
 * fs2_fromstr / fs2_clone / fs2_fromstrf
 *
 * All tests reset the allocation counters first and verify balance at
 * the end. Requires -DFS2TEST (see fs2/Makefile).
 * ------------------------------------------------------------------------- */

static void
assert_balanced(void)
{
    int a = -1, f = -1, ba = -1, bf = -1;
    bool ok = fs2_checkleak(&a, &f, &ba, &bf);
    if (!ok)
        logsimple("FAIL %s: alloc=%d freed=%d body_alloc=%d body_freed=%d",
                  __func__, a, f, ba, bf);
    cr_assert(ok, "counters not balanced");
}

/* ── fromstr ────────────────────────────────────────────────────────── */

Test(fs2, fromstr_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    CHECK_STR_EQ("v", s.v, "hello");
    CHECK_EQ("len", s.len, 5);
    CHECK_GE("sz >= len + 1", s.sz, s.len + 1);
    CHECK_TRUE ("is_alloc",     fs2_is_alloc(&s));
    CHECK_FALSE("is_static",    fs2_is_static(&s));
    CHECK_FALSE("is_bodyalloc", fs2_is_bodyalloc(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstr_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("");

    CHECK_EQ("len", s.len, 0);
    CHECK_GT("sz",  s.sz,  1);
    CHECK_STR_EQ("v", s.v, "");
    CHECK_TRUE("is_alloc", fs2_is_alloc(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstr_single_char) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");

    CHECK_EQ("len", s.len, 1);
    CHECK_STR_EQ("v", s.v, "x");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstr_longer_than_min_alloc) {
    __fs2_checkreset();
    const char *src = "a string longer than FS2_MIN_ALLOC";
    fs2 s = fs2_fromstr(src);

    CHECK_STR_EQ("v", s.v, src);
    CHECK_EQ("len", s.len, strlen(src));
    CHECK_TRUE("sz >= len + 1", s.sz >= s.len + 1);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstr_source_is_independent) {
    __fs2_checkreset();
    char src[] = "mutable";
    fs2 s = fs2_fromstr(src);

    src[0] = 'X';                      /* mutate the source */
    CHECK_STR_EQ("v independent of src", s.v, "mutable");

    fs2_free(&s);
    assert_balanced();
}

/* ── clone ──────────────────────────────────────────────────────────── */

Test(fs2, clone_from_literal) {
    __fs2_checkreset();
    fs2 a = FS2_LITERAL("literal");
    fs2 b = fs2_clone(&a);

    CHECK_STR_EQ("b.v", b.v, "literal");
    CHECK_EQ("b.len vs a.len", b.len, a.len);
    CHECK_TRUE("b is_alloc",   fs2_is_alloc(&b));
    CHECK_PTR_NEQ("b.v vs a.v", b.v, a.v);
    CHECK_TRUE("a still static", fs2_is_static(&a));

    fs2_free(&b);
    assert_balanced();
}

Test(fs2, clone_from_alloc) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("original");
    fs2 b = fs2_clone(&a);

    CHECK_STR_EQ("b.v vs a.v", b.v, a.v);
    CHECK_PTR_NEQ("b.v vs a.v pointer", b.v, a.v);
    CHECK_EQ("b.len vs a.len", b.len, a.len);
    CHECK_EQ("b.sz vs a.sz",   b.sz,  a.sz);

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

Test(fs2, clone_empty_static) {
    __fs2_checkreset();
    fs2 a = FS2_EMPTY;
    fs2 b = fs2_clone(&a);

    CHECK_EQ("len", b.len, (size_t) 0);
    CHECK_STR_EQ("v", b.v, "");

    fs2_free(&b);
    assert_balanced();
}

Test(fs2, clone_empty_alloc) {
    __fs2_checkreset();
    fs2 a = FS2();
    fs2 b = fs2_clone(&a);

    CHECK_EQ("len", b.len, (size_t) 0);
    CHECK_NULL("v", b.v);

    fs2_free(&b);
    assert_balanced();
}

/* ── fromstrf ───────────────────────────────────────────────────────── */

Test(fs2, fromstrf_int) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("n=%d", 42);

    CHECK_STR_EQ("v", s.v, "n=42");
    CHECK_EQ("len", s.len, strlen("n=%d"));
    CHECK_TRUE("is_alloc", fs2_is_alloc(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstrf_string_arg) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("user=%s", "alice");

    CHECK_STR_EQ("v", s.v, "user=alice");
    CHECK_EQ("len", s.len, (size_t) 10);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstrf_mixed) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%s=%d (%c)", "count", -7, 'x');

    CHECK_STR_EQ("v", s.v, "count=-7 (x)");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstrf_empty) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%s", "");   /* %s is safer than "" for -Wformat */

    CHECK_EQ("len", s.len, (size_t) 0);
    CHECK_STR_EQ("v", s.v, "");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, fromstrf_negative) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%d", -123);

    CHECK_STR_EQ("v", s.v, "-123");
    CHECK_EQ("len", s.len, 4);

    fs2_free(&s);
    assert_balanced();
}

/* ── round-trips under leak detection ───────────────────────────────── */

Test(fs2, roundtrip_fromstr_clone_100) {
    __fs2_checkreset();
    for (int i = 0; i < 100; i++) {
        fs2 a = fs2_fromstr("round-trip");
        fs2 b = fs2_clone(&a);
        fs2_free(&a);
        fs2_free(&b);
    }
    assert_balanced();
}

Test(fs2, roundtrip_fromstrf_clone_100) {
    __fs2_checkreset();
    for (int i = 0; i < 100; i++) {
        fs2 a = fs2_fromstrf("item %d", i);
        fs2 b = fs2_clone(&a);
        CHECK_STR_EQ("a.v vs b.v", a.v, b.v);
        fs2_free(&a);
        fs2_free(&b);
    }
    assert_balanced();
}

/* ── fs2_checkleak: negative case ───────────────────────────────────── */

Test(fs2, checkleak_detects_dangling) {
    __fs2_checkreset();

    fs2 leaky = fs2_fromstr("intentional leak");

    int a = -1, f = -1;
    CHECK_FALSE("fs2_checkleak on unbalanced", fs2_checkleak(&a, &f, NULL, NULL));
    CHECK_EQ("alloc - freed", a - f, 1);

    fs2_free(&leaky);       /* cleanup so the next test is not poisoned */
    assert_balanced();
}

Test(fs2, checkleak_passes_when_balanced) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("balanced");
    fs2_free(&s);

    CHECK_TRUE("fs2_checkleak on balanced", fs2_checkleak(NULL, NULL, NULL, NULL));
}

/* -------------------------------------------------------------------------
 * fs2_move — value transfer
 * ------------------------------------------------------------------------- */

Test(fs2, move_alloc_to_alloc) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("owned");
    fs2 b = FS2();

    fs2 *ret = fs2_move(&b, &a);

    CHECK_PTR_EQ("returned pointer", ret, &b);
    CHECK_STR_EQ("b.v", b.v, "owned");
    CHECK_TRUE ("b is_alloc", fs2_is_alloc(&b));

    /* src becomes a canonical empty ALLOC */
    CHECK_NULL("a.v",  a.v);
    CHECK_EQ  ("a.len", a.len, 0);
    CHECK_EQ  ("a.sz",  a.sz,  0);
    CHECK_TRUE("a is_empty", fs2_is_empty(&a));

    fs2_free(&a);   /* no-op on empty */
    fs2_free(&b);
    assert_balanced();
}

Test(fs2, move_alloc_to_static_dst) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("payload");
    fs2 b = FS2_LITERAL("old");

    fs2_move(&b, &a);

    CHECK_STR_EQ("b.v", b.v, "payload");
    CHECK_TRUE ("b is_alloc", fs2_is_alloc(&b));

    CHECK_NULL("a.v", a.v);
    CHECK_EQ  ("a.len", a.len, 0);

    fs2_free(&b);
    assert_balanced();
}

Test(fs2, move_static_to_empty) {
    __fs2_checkreset();
    fs2 a = FS2_LITERAL("literal");
    fs2 b = FS2();

    fs2_move(&b, &a);

    CHECK_STR_EQ("b.v", b.v, "literal");
    CHECK_TRUE ("b is_static", fs2_is_static(&b));

    /* STATIC source is not detached — same rule as fs2_free */
    CHECK_STR_EQ("a.v still readable", a.v, "literal");
    CHECK_EQ   ("a.len unchanged",     a.len, 7);
    CHECK_TRUE ("a is_static",         fs2_is_static(&a));

    fs2_free(&a);   /* no-op */
    fs2_free(&b);   /* no-op */
    assert_balanced();
}

Test(fs2, move_self_noop) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("keep me");

    fs2 *ret = fs2_move(&a, &a);

    CHECK_PTR_EQ("returned pointer", ret, &a);
    CHECK_STR_EQ("a.v", a.v, "keep me");
    CHECK_EQ   ("a.len", a.len, 7);

    fs2_free(&a);
    assert_balanced();
}

Test(fs2, move_null_dst_frees_src) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("to be freed");

    fs2 *ret = fs2_move(NULL, &a);

    CHECK_NULL("return value", ret);
    CHECK_NULL("a.v after free", a.v);
    CHECK_EQ  ("a.len", a.len, 0);
    CHECK_EQ  ("a.sz",  a.sz,  0);

    assert_balanced();
}

Test(fs2, move_empty_to_empty) {
    __fs2_checkreset();
    fs2 a = FS2();
    fs2 b = FS2();

    fs2_move(&b, &a);

    CHECK_NULL("b.v", b.v);
    CHECK_EQ  ("b.len", b.len, 0);
    CHECK_NULL("a.v", a.v);

    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_movefrom — return the value, src becomes empty
 * ------------------------------------------------------------------------- */

Test(fs2, movefrom_alloc) {
    __fs2_checkreset();
    fs2 a = fs2_fromstr("transfer me");

    fs2 b = fs2_movefrom(&a);

    CHECK_STR_EQ("b.v", b.v, "transfer me");
    CHECK_TRUE ("b is_alloc", fs2_is_alloc(&b));
    CHECK_EQ   ("b.len", b.len, 11);

    /* src reset */
    CHECK_NULL("a.v", a.v);
    CHECK_EQ  ("a.len", a.len, 0);
    CHECK_EQ  ("a.sz",  a.sz,  0);

    fs2_free(&b);
    fs2_free(&a);   /* no-op */
    assert_balanced();
}

Test(fs2, movefrom_static) {
    __fs2_checkreset();
    fs2 a = FS2_LITERAL("static payload");

    fs2 b = fs2_movefrom(&a);

    CHECK_STR_EQ("b.v", b.v, "static payload");
    CHECK_TRUE ("b is_static", fs2_is_static(&b));

    /* STATIC src keeps its buffer (same rule as fs2_free) */
    CHECK_STR_EQ("a.v untouched", a.v, "static payload");
    CHECK_EQ   ("a.len", a.len, 14);

    fs2_free(&a);   /* no-op */
    fs2_free(&b);   /* no-op */
    assert_balanced();
}

Test(fs2, movefrom_empty) {
    __fs2_checkreset();
    fs2 a = FS2();

    fs2 b = fs2_movefrom(&a);

    CHECK_NULL("b.v", b.v);
    CHECK_EQ  ("b.len", b.len, 0);
    CHECK_TRUE("b is_alloc", fs2_is_alloc(&b));

    CHECK_NULL("a.v", a.v);

    assert_balanced();
}

// Test(fs2, movefrom_usable_after) {
//     __fs2_checkreset();
//     fs2 a = fs2_fromstr("original");
//     fs2 b = fs2_movefrom(&a);

//     /* src is now a valid empty ALLOC — can be reused */
//     fs2_appendstr(&a, "new");       /* when appendstr exists */
//     CHECK_STR_EQ("a.v", a.v, "new");

//     fs2_free(&a);
//     fs2_free(&b);
//     assert_balanced();
// }

/* -------------------------------------------------------------------------
 * fs2_movefromstr — take a raw char*, flag decides ownership
 * ------------------------------------------------------------------------- */

Test(fs2, movefromstr_alloc_takes_ownership) {
    __fs2_checkreset();
    char *src = strdup("owned string");
    CHECK_NOT_NULL("strdup result", src);

    fs2 s = fs2_movefromstr((const char **)&src, FS2_ALLOC);

    CHECK_STR_EQ("s.v", s.v, "owned string");
    CHECK_EQ   ("s.len", s.len, 12);
    CHECK_EQ   ("s.sz",  s.sz,  13);
    CHECK_TRUE ("s is_alloc", fs2_is_alloc(&s));
    CHECK_NULL ("source pointer nulled", src);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, movefromstr_static_keeps_buffer) {
    __fs2_checkreset();
    const char *src = "literal-ish";

    fs2 s = fs2_movefromstr(&src, FS2_STATIC);

    CHECK_STR_EQ("s.v", s.v, "literal-ish");
    CHECK_EQ   ("s.len", s.len, 11);
    CHECK_TRUE ("s is_static", fs2_is_static(&s));
    CHECK_NULL ("source pointer nulled", src);   /* caller loses reference */

    fs2_free(&s);   /* no-op, STATIC */
    assert_balanced();
}

Test(fs2, movefromstr_null_pointer_raises, .signal = SIGABRT) {
    fs2_movefromstr(NULL, FS2_ALLOC);
}

Test(fs2, movefromstr_null_value_raises, .signal = SIGABRT) {
    const char *src = NULL;
    fs2_movefromstr(&src, FS2_ALLOC);
}

Test(fs2, movefromstr_rejects_local_flag, .signal = SIGINT) {
    const char *src = "x";
    fs2_movefromstr(&src, FS2_LOCAL);   /* userraise → SIGINT */
}

Test(fs2, movefromstr_rejects_bodyalloc_flag, .signal = SIGINT) {
    const char *src = "x";
    fs2_movefromstr(&src, FS2_BODYALLOC);
}

Test(fs2, movefromstr_empty_string) {
    __fs2_checkreset();
    char *src = strdup("");
    CHECK_NOT_NULL("strdup result", src);

    fs2 s = fs2_movefromstr((const char **)&src, FS2_ALLOC);

    CHECK_EQ   ("s.len", s.len, 0);
    CHECK_EQ   ("s.sz",  s.sz,  1);
    CHECK_STR_EQ("s.v", s.v, "");
    CHECK_NULL ("source pointer nulled", src);

    fs2_free(&s);
    assert_balanced();
}
