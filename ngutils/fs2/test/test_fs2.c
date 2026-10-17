#include <criterion/criterion.h>

#include "test_fs2.h"

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
    /* STATIC with v == NULL — invalid (STATIC must have a buffer). */
    fs2 s = FS2_LITERAL("x");
    s.v = NULL;
    CHECK_FALSE("validate on STATIC with NULL v", fs2_validate(NULL, &s));
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

    fs2 s = FS2_LITERAL("x");
    s.v = NULL;                    /* STATIC with NULL buffer — invalid */

    CHECK_FALSE("validate(f, STATIC|NULL)", fs2_validate(f, &s));
    fclose(f);

    char buf[512];
    size_t n = test_read_file(path, buf, sizeof buf);
    remove(path);

    CHECK_TRUE("non-empty output", n > 0);
    CHECK_NOT_NULL("strstr \"null buffer\"", strstr(buf, "null buffer"));
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

/* -------------------------------------------------------------------------
 * fs2_adopt / fs2_adoptn — take ownership of a heap C-string
 *
 * Distinguished from fs2_movefromstr by:
 *   - no flag argument (always FS2_ALLOC — we take ownership);
 *   - NULL inputs are accepted (return FS2(), no error);
 *   - adoptn takes an explicit length, so it works on buffers that are
 *     not (yet) NUL-terminated and enforces the terminator.
 * ------------------------------------------------------------------------- */

Test(fs2, adopt_simple) {
    __fs2_checkreset();
    char *p = strdup("hello");
    CHECK_NOT_NULL("strdup", p);

    fs2 s = fs2_adopt(&p);

    CHECK_NULL ("source pointer", p);
    CHECK_STR_EQ("s.v",   s.v,   "hello");
    CHECK_EQ   ("s.len",  s.len, 5);
    CHECK_EQ   ("s.sz",   s.sz,  6);
    CHECK_TRUE ("s is_alloc", fs2_is_alloc(&s));
    CHECK_FALSE("s is_static", fs2_is_static(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, adopt_empty) {
    __fs2_checkreset();
    char *p = strdup("");
    CHECK_NOT_NULL("strdup", p);

    fs2 s = fs2_adopt(&p);

    CHECK_NULL ("source pointer", p);
    CHECK_EQ   ("s.len", s.len, 0);
    CHECK_EQ   ("s.sz",  s.sz,  1);
    CHECK_STR_EQ("s.v", s.v, "");
    CHECK_TRUE ("s is_alloc", fs2_is_alloc(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, adopt_null_ptr) {
    __fs2_checkreset();
    fs2 s = fs2_adopt(NULL);

    CHECK_NULL("s.v", s.v);
    CHECK_EQ  ("s.len", s.len, 0);
    CHECK_TRUE("s is_alloc", fs2_is_alloc(&s));
    /* no allocation, no free — balanced */
    assert_balanced();
}

Test(fs2, adopt_null_value) {
    __fs2_checkreset();
    char *p = NULL;
    fs2 s = fs2_adopt(&p);

    CHECK_NULL("s.v", s.v);
    CHECK_EQ  ("s.len", s.len, 0);
    assert_balanced();
}

/* ── adoptn ─────────────────────────────────────────────────────────── */

Test(fs2, adoptn_explicit_length) {
    __fs2_checkreset();
    /* Buffer longer than the "logical" portion; adoptn should cut
     * at saved_len and enforce NUL there. */
    char *p = strdup("hello world");
    CHECK_NOT_NULL("strdup", p);

    fs2 s = fs2_adoptn(&p, 5);   /* keep "hello" only */

    CHECK_NULL ("source pointer", p);
    CHECK_STR_EQ("s.v",   s.v,   "hello");
    CHECK_EQ   ("s.len",  s.len, 5);
    CHECK_EQ   ("s.sz",   s.sz,  6);
    CHECK_TRUE ("s is_alloc", fs2_is_alloc(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, adoptn_zero_length) {
    __fs2_checkreset();
    char *p = strdup("some data");
    CHECK_NOT_NULL("strdup", p);

    fs2 s = fs2_adoptn(&p, 0);

    CHECK_NULL ("source pointer", p);
    CHECK_EQ   ("s.len", s.len, 0);
    CHECK_EQ   ("s.sz",  s.sz,  1);
    CHECK_STR_EQ("s.v", s.v, "");   /* NUL enforced at position 0 */
    CHECK_TRUE ("s is_alloc", fs2_is_alloc(&s));

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, adoptn_full_length) {
    __fs2_checkreset();
    const char *src = "exact";
    char *p = strdup(src);
    CHECK_NOT_NULL("strdup", p);

    fs2 s = fs2_adoptn(&p, strlen(src));

    CHECK_STR_EQ("s.v", s.v, src);
    CHECK_EQ   ("s.len", s.len, 5);
    CHECK_NULL ("source pointer", p);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, adoptn_null_ptr) {
    __fs2_checkreset();
    fs2 s = fs2_adoptn(NULL, 10);

    CHECK_NULL("s.v", s.v);
    CHECK_EQ  ("s.len", s.len, 0);
    assert_balanced();
}

Test(fs2, adoptn_null_value) {
    __fs2_checkreset();
    char *p = NULL;
    fs2 s = fs2_adoptn(&p, 10);

    CHECK_NULL("s.v", s.v);
    CHECK_EQ  ("s.len", s.len, 0);
    assert_balanced();
}

Test(fs2, adoptn_independent_of_source) {
    __fs2_checkreset();
    char *p = strdup("mutable");
    CHECK_NOT_NULL("strdup", p);
    char *orig = p;               /* keep the raw pointer to mutate */
    (void) orig;

    fs2 s = fs2_adoptn(&p, 7);

    /* s.v == orig — same buffer, fs2 now owns it. Mutating via s.v
     * (or via orig, since they alias) is allowed and visible. */
    s.v[0] = 'M';
    CHECK_STR_EQ("s.v after mutation", s.v, "Mutable");

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, adoptn_no_terminator_yet) {
    __fs2_checkreset();
    char *p = malloc(5);
    CHECK_NOT_NULL("malloc", p);
    memcpy(p, "abcde", 5);      /* no NUL in the buffer */

    fs2 s = fs2_adoptn(&p, 3);  /* keep only "abc" */

    CHECK_STR_EQ("s.v", s.v, "abc");
    CHECK_EQ   ("s.len", s.len, 3);
    CHECK_NULL ("source pointer", p);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, validate_accepts_empty_alloc) {
    fs2 s = FS2();                 /* ALLOC, v=NULL, len=sz=0 */
    CHECK_TRUE("FS2() is valid", fs2_validate(NULL, &s));
}

Test(fs2, validate_rejects_unknown_flag) {
    fs2 s = FS2_LITERAL("x");
    s.flags = (fs2_flags)(s.flags | 0x8000);
    CHECK_FALSE("unknown bit", fs2_validate(NULL, &s));
}

Test(fs2, validate_rejects_no_kind_flag) {
    fs2 s = FS2_LITERAL("x");
    s.flags = (fs2_flags)(s.flags & ~(FS2_STATIC | FS2_LOCAL | FS2_ALLOC));
    CHECK_FALSE("no kind flag", fs2_validate(NULL, &s));
}

Test(fs2, validate_rejects_two_kind_flags) {
    fs2 s = FS2_LITERAL("x");
    s.flags = (fs2_flags)(s.flags | FS2_ALLOC);   /* STATIC | ALLOC */
    CHECK_FALSE("two kind flags", fs2_validate(NULL, &s));
}

Test(fs2, validate_rejects_alloc_null_nonempty) {
    fs2 s = FS2();
    s.len = 5;   /* forged: v == NULL, but len != 0 */
    CHECK_FALSE("alloc null with len", fs2_validate(NULL, &s));
}

// Test(fs2, heaptostack_no_leak) {
//     __fs2_checkreset();
//     fs2 *h = fs2_heap_fromstr("payload");
//     CHECK_TRUE("chech body alloc status", fs2_is_bodyalloc(h));

//     fs2 s = fs2_heaptostack(&h);

//     CHECK_NULL("caller pointer nulled", h);
//     CHECK_STR_EQ("s.v", s.v, "payload");
//     CHECK_TRUE ("s is_alloc",      fs2_is_alloc(&s));
//     CHECK_FALSE("s not bodyalloc", fs2_is_bodyalloc(&s));

//     fs2_free(&s);
//     assert_balanced();   /* ловит: alloc_body vs free_body */
// }

Test(fs2, stacktoheap_value) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("copy up");

    fs2 *h = fs2_stacktoheap(&s);

    CHECK_NOT_NULL("h", h);
    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("h is_alloc",     fs2_is_alloc(h));
    CHECK_STR_EQ("h->v", h->v, "copy up");

    /* src reset to canonical empty */
    CHECK_NULL("s.v", s.v);
    CHECK_EQ  ("s.len", s.len, 0);

    fs2_free(h);
    assert_balanced();
}

Test(fs2, stacktoheap_static) {
    __fs2_checkreset();
    fs2 s = FS2_LITERAL("literal");
    CHECK_TRUE ("validate 1", fs2_validate(logfile, &s) );

    fs2 *h = fs2_stacktoheap(&s);
    CHECK_TRUE ("validate 2", fs2_validate(logfile, h) );

    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("h is_static",    fs2_is_static(h));
    CHECK_STR_EQ("h->v", h->v, "literal");

    /* STATIC source not detached — same rule as fs2_free */
    CHECK_STR_EQ("s.v untouched", s.v, "literal");

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_init_basic) {
    __fs2_checkreset();

    fs2 *h = fs2_heap_init(32);
    CHECK_FS2VALID(1, h);

    CHECK_NOT_NULL("h", h);
    CHECK_TRUE ("is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("is_alloc",     fs2_is_alloc(h));
    CHECK_EQ   ("len", h->len, 0);
    CHECK_EQ   ("v[0]", h->v[0], '\0');
    CHECK_FS2VALID(2, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_init_zero) {
    __fs2_checkreset();

    fs2 *h = fs2_heap_init(0);
    CHECK_FS2VALID(1, h);

    CHECK_TRUE("is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_EQ  ("sz floored",   h->sz, 16);
    CHECK_FS2VALID(2, h);

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_clone_from_literal) {
    __fs2_checkreset();

    fs2 a = FS2_LITERAL("hello");
    CHECK_FS2VALID(1, &a);

    fs2 *h = fs2_heap_clone(&a);
    CHECK_FS2VALID(2, h);

    CHECK_TRUE ("is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("is_alloc",     fs2_is_alloc(h));
    CHECK_STR_EQ("h->v",        h->v, "hello");
    CHECK_EQ   ("len",          h->len, 5);
    CHECK_PTR_NEQ("h->v vs a.v", h->v, a.v);

    /* source unchanged */
    CHECK_FS2VALID(3, &a);
    CHECK_STR_EQ("a.v", a.v, "hello");
    CHECK_EQ   ("a.len", a.len, 5);

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_clone_from_alloc) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("original");
    CHECK_FS2VALID(1, &a);

    fs2 *h = fs2_heap_clone(&a);
    CHECK_FS2VALID(2, h);

    CHECK_STR_EQ("h->v", h->v, "original");
    CHECK_PTR_NEQ("h->v vs a.v", h->v, a.v);
    CHECK_FS2VALID(3, &a);   /* source не тронут */

    fs2_free(&a);
    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_clone_empty) {
    __fs2_checkreset();

    fs2 a = FS2_EMPTY;
    CHECK_FS2VALID(1, &a);

    fs2 *h = fs2_heap_clone(&a);
    CHECK_FS2VALID(2, h);

    CHECK_EQ   ("len", h->len, 0);
    CHECK_STR_EQ("v",  h->v, "");
    CHECK_TRUE ("is_bodyalloc", fs2_is_bodyalloc(h));

    fs2_free(h);
    assert_balanced();
}

Test(fs2, fromstrf_va_multiple_args) {
    __fs2_checkreset();
    fs2 s = fs2_fromstrf("%d %d %d %s", 1, 2, 3, "end");

    CHECK_STR_EQ("v", s.v, "1 2 3 end");
    CHECK_EQ   ("len", s.len, 9);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2, heap_fromstr_basic) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstr("hello");

    CHECK_TRUE ("is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("is_alloc",     fs2_is_alloc(h));
    CHECK_STR_EQ("h->v", h->v, "hello");

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_fromstrf_multiple) {
    __fs2_checkreset();
    fs2 *h = fs2_heap_fromstrf("%s=%d", "x", 42);

    CHECK_TRUE ("is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_STR_EQ("h->v", h->v, "x=42");

    fs2_free(h);
    assert_balanced();
}

/* -------------------------------------------------------------------------
 * fs2_heap_movefromstr — take a raw C-string into a heap fs2
 *
 * Composes fs2_movefromstr (flag decides ownership) and fs2_heap
 * (BODYALLOC). *from is nulled in both cases.
 * ------------------------------------------------------------------------- */

Test(fs2, heap_movefromstr_alloc) {
    __fs2_checkreset();

    char *p = strdup("owned payload");
    CHECK_NOT_NULL("strdup", p);

    fs2 *h = fs2_heap_movefromstr((const char **)&p, FS2_ALLOC);
    CHECK_FS2VALID(1, h);

    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("h is_alloc",     fs2_is_alloc(h));
    CHECK_STR_EQ("h->v",          h->v, "owned payload");
    CHECK_EQ   ("h->len",         h->len, 13);
    CHECK_NULL ("source pointer", p);

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_movefromstr_static) {
    __fs2_checkreset();

    const char *p = "literal data";

    fs2 *h = fs2_heap_movefromstr(&p, FS2_STATIC);
    CHECK_FS2VALID(1, h);

    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("h is_static",    fs2_is_static(h));
    CHECK_STR_EQ("h->v",          h->v, "literal data");
    CHECK_EQ   ("h->len",         h->len, 12);
    CHECK_NULL ("source pointer", p);

    fs2_free(h);       /* body freed, literal untouched */
    assert_balanced();
}

Test(fs2, heap_movefromstr_empty_alloc) {
    __fs2_checkreset();

    char *p = strdup("");
    CHECK_NOT_NULL("strdup", p);

    fs2 *h = fs2_heap_movefromstr((const char **)&p, FS2_ALLOC);
    CHECK_FS2VALID(1, h);

    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("h is_alloc",     fs2_is_alloc(h));
    CHECK_EQ   ("h->len",         h->len, 0);
    CHECK_EQ   ("h->sz",          h->sz,  1);
    CHECK_STR_EQ("h->v",          h->v, "");
    CHECK_NULL ("source pointer", p);

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_movefromstr_empty_static) {
    __fs2_checkreset();

    const char *p = "";

    fs2 *h = fs2_heap_movefromstr(&p, FS2_STATIC);
    CHECK_FS2VALID(1, h);

    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_TRUE ("h is_static",    fs2_is_static(h));
    CHECK_EQ   ("h->len",         h->len, 0);
    CHECK_STR_EQ("h->v",          h->v, "");

    fs2_free(h);
    assert_balanced();
}

/* ── negatives ─────────────────────────────────────────────────────── */

Test(fs2, heap_movefromstr_null_from_aborts, .signal = SIGABRT) {
    /* invraise(from != NULL && *from != NULL) → abort() */
    fs2_heap_movefromstr(NULL, FS2_ALLOC);
}

Test(fs2, heap_movefromstr_null_value_aborts, .signal = SIGABRT) {
    const char *p = NULL;
    fs2_heap_movefromstr(&p, FS2_ALLOC);
}

Test(fs2, heap_movefromstr_rejects_local, .signal = SIGINT) {
    const char *p = "x";
    /* fs2_movefromstr: userraise for FS2_LOCAL → SIGINT */
    fs2_heap_movefromstr(&p, FS2_LOCAL);
}

Test(fs2, heap_movefromstr_rejects_bodyalloc, .signal = SIGINT) {
    const char *p = "x";
    fs2_heap_movefromstr(&p, FS2_BODYALLOC);
}

/* ── round-trip / independence ─────────────────────────────────────── */

Test(fs2, heap_movefromstr_alloc_independent) {
    __fs2_checkreset();

    char *orig = strdup("mutable");
    CHECK_NOT_NULL("strdup", orig);

    fs2 *h = fs2_heap_movefromstr((const char **)&orig, FS2_ALLOC);
    CHECK_FS2VALID(1, h);

    /* h->v == orig (before nulling) — now owned by h. Mutation via
     * h->v is allowed and does not need any special handling. */
    h->v[0] = 'M';
    CHECK_STR_EQ("h->v after mutation", h->v, "Mutable");

    fs2_free(h);
    assert_balanced();
}

Test(fs2, heap_movefrom_not_implemented_raises, .signal = SIGINT) {
    fs2 a = fs2_fromstr("x");
    fs2_heap_movefrom(&a);   /* userraise → SIGINT */
}

/* -------------------------------------------------------------------------
 * Transitions: value ↔ heap, alloc/static/local, chains and swaps
 *
 * Each test walks an object through several states, validating after
 * every step and asserting counters are balanced at the end. This
 * catches flag loss, double-free, and buffer leaks that single-step
 * tests do not reach.
 * ------------------------------------------------------------------------- */

/* ── alloc: stack → heap → stack ──────────────────────────────────── */

Test(fs2, transition_alloc_roundtrip) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("data");
    CHECK_FS2VALID(1, &a);
    CHECK_TRUE("a is_alloc", fs2_is_alloc(&a));

    fs2 *h = fs2_stacktoheap(&a);
    CHECK_FS2VALID(2, h);
    CHECK_STR_EQ("h->v",   h->v,   "data");
    CHECK_TRUE ("h is_alloc",     fs2_is_alloc(h));
    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_NULL ("a.v", a.v);       /* src reset */

    fs2 b = fs2_heaptostack(&h);
    CHECK_FS2VALID(3, &b);
    CHECK_STR_EQ("b.v",    b.v,    "data");
    CHECK_TRUE ("b is_alloc",     fs2_is_alloc(&b));
    CHECK_FALSE("b not bodyalloc", fs2_is_bodyalloc(&b));
    CHECK_NULL ("h", h);           /* caller pointer nulled */

    fs2_free(&b);
    assert_balanced();
}

/* ── static: stack → heap → stack ─────────────────────────────────── */

Test(fs2, transition_static_roundtrip) {
    __fs2_checkreset();

    fs2 a = FS2_LITERAL("lit");
    CHECK_FS2VALID(1, &a);

    fs2 *h = fs2_stacktoheap(&a);
    CHECK_FS2VALID(2, h);
    CHECK_TRUE ("h is_static",    fs2_is_static(h));
    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_STR_EQ("h->v", h->v, "lit");

    /* STATIC source not detached */
    CHECK_STR_EQ("a.v", a.v, "lit");

    fs2 b = fs2_heaptostack(&h);
    CHECK_FS2VALID(3, &b);
    CHECK_TRUE ("b is_static", fs2_is_static(&b));
    CHECK_STR_EQ("b.v", b.v, "lit");

    fs2_free(&b);  /* no-op */
    fs2_free(&a);  /* no-op */
    assert_balanced();
}

/* ── local: stack → heap → stack ──────────────────────────────────── */

Test(fs2, transition_local_roundtrip) {
    __fs2_checkreset();

    FS2LOCAL(a, 32);
    memcpy(a.v, "on-stack", 9);
    a.len = 8;
    CHECK_FS2VALID(1, &a);

    fs2 *h = fs2_stacktoheap(&a);
    CHECK_FS2VALID(2, h);
    CHECK_TRUE ("h is_local",     fs2_is_local(h));
    CHECK_TRUE ("h is_bodyalloc", fs2_is_bodyalloc(h));
    CHECK_PTR_EQ("h->v == a.v",   h->v, a.v);   /* same buffer */

    fs2 b = fs2_heaptostack(&h);
    CHECK_FS2VALID(3, &b);
    CHECK_TRUE("b is_local", fs2_is_local(&b));
    CHECK_STR_EQ("b.v", b.v, "on-stack");

    fs2_free(&b);
    fs2_free(&a);
    assert_balanced();
}

/* ── movefrom chain: one buffer, many hands ───────────────────────── */

Test(fs2, transition_movefrom_chain) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("chain");
    CHECK_FS2VALID(1, &a);

    fs2 b = fs2_movefrom(&a);
    CHECK_FS2VALID(2, &b);
    CHECK_STR_EQ("b.v", b.v, "chain");
    CHECK_NULL("a.v", a.v);
    CHECK_EQ  ("a.len", a.len, 0);

    fs2 c = fs2_movefrom(&b);
    CHECK_FS2VALID(3, &c);
    CHECK_STR_EQ("c.v", c.v, "chain");
    CHECK_NULL("b.v", b.v);

    fs2 d = fs2_movefrom(&c);
    CHECK_FS2VALID(4, &d);
    CHECK_STR_EQ("d.v", d.v, "chain");
    CHECK_NULL("c.v", c.v);

    fs2_free(&d);
    /* one alloc, one free — the whole chain carried the same buffer */
    assert_balanced();
}

/* ── swap via move (two alloc) ────────────────────────────────────── */

Test(fs2, transition_swap_two_alloc) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("AAA");
    fs2 b = fs2_fromstr("BBB");
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);

    fs2 t = fs2_movefrom(&a);   /* t=AAA, a=empty */
    CHECK_FS2VALID(3, &t);

    fs2_move(&a, &b);            /* a=BBB, b=empty */
    CHECK_FS2VALID(4, &a);
    CHECK_NULL("b.v", b.v);

    fs2_move(&b, &t);            /* b=AAA, t=empty */
    CHECK_FS2VALID(5, &b);
    CHECK_NULL("t.v", t.v);

    CHECK_STR_EQ("a.v", a.v, "BBB");
    CHECK_STR_EQ("b.v", b.v, "AAA");

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* ── swap with mixed flags (alloc ↔ static) ───────────────────────── */

Test(fs2, transition_swap_mixed_flags) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("heap");     /* ALLOC */
    fs2 b = FS2_LITERAL("lit");      /* STATIC */

    fs2 t = fs2_movefrom(&a);        /* t=heap ALLOC, a=empty ALLOC */
    CHECK_FS2VALID(1, &t);

    fs2_move(&a, &b);
    /* a=lit STATIC; b still STATIC — STATIC source is not detached */
    CHECK_FS2VALID(2, &a);
    CHECK_TRUE ("a is_static", fs2_is_static(&a));
    CHECK_TRUE ("b is_static", fs2_is_static(&b));
    CHECK_STR_EQ("a.v", a.v, "lit");
    CHECK_STR_EQ("b.v", b.v, "lit");

    fs2_move(&b, &t);               /* b=heap ALLOC, t=empty */
    CHECK_FS2VALID(3, &b);
    CHECK_TRUE ("b is_alloc", fs2_is_alloc(&b));
    CHECK_STR_EQ("b.v", b.v, "heap");

    fs2_free(&b);
    assert_balanced();
}

/* ── cyclic rotation of three alloc values ────────────────────────── */

Test(fs2, transition_rotate_three) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("A");
    fs2 b = fs2_fromstr("B");
    fs2 c = fs2_fromstr("C");
    CHECK_FS2VALID(1, &a);
    CHECK_FS2VALID(2, &b);
    CHECK_FS2VALID(3, &c);

    fs2 t = fs2_movefrom(&a);       /* t=A,   a=empty */
    fs2_move(&a, &b);                /* a=B,   b=empty */
    fs2_move(&b, &c);                /* b=C,   c=empty */
    fs2_move(&c, &t);                /* c=A,   t=empty */

    CHECK_FS2VALID(4, &a);
    CHECK_FS2VALID(5, &b);
    CHECK_FS2VALID(6, &c);
    CHECK_STR_EQ("a.v", a.v, "B");
    CHECK_STR_EQ("b.v", b.v, "C");
    CHECK_STR_EQ("c.v", c.v, "A");

    fs2_free(&a);
    fs2_free(&b);
    fs2_free(&c);
    assert_balanced();
}

/* ── heap_movefromstr + stack↔heap chain ──────────────────────────── */

Test(fs2, transition_raw_to_heap_to_stack) {
    __fs2_checkreset();

    char *p = strdup("raw");
    CHECK_NOT_NULL("strdup", p);

    fs2 *h1 = fs2_heap_movefromstr((const char **)&p, FS2_ALLOC);
    CHECK_FS2VALID(1, h1);
    CHECK_TRUE ("h1 is_bodyalloc", fs2_is_bodyalloc(h1));
    CHECK_NULL ("p", p);

    fs2 s = fs2_heaptostack(&h1);
    CHECK_FS2VALID(2, &s);
    CHECK_STR_EQ("s.v", s.v, "raw");
    CHECK_FALSE("s not bodyalloc", fs2_is_bodyalloc(&s));
    CHECK_NULL ("h1", h1);

    fs2 *h2 = fs2_stacktoheap(&s);
    CHECK_FS2VALID(3, h2);
    CHECK_STR_EQ("h2->v", h2->v, "raw");
    CHECK_NULL ("s.v", s.v);

    fs2_free(h2);
    assert_balanced();
}

/* ── heap clone survives a transition ─────────────────────────────── */

Test(fs2, transition_heap_clone_then_roundtrip) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("origin");
    fs2 *h = fs2_heap_clone(&a);
    CHECK_FS2VALID(1, h);
    CHECK_PTR_NEQ("h->v vs a.v", h->v, a.v);

    fs2 b = fs2_heaptostack(&h);
    CHECK_FS2VALID(2, &b);
    CHECK_STR_EQ("b.v", b.v, "origin");
    CHECK_PTR_NEQ("b.v vs a.v", b.v, a.v);   /* still independent copy */

    fs2_free(&a);
    fs2_free(&b);
    assert_balanced();
}

/* ── empty FS2 through the whole cycle ────────────────────────────── */

Test(fs2, transition_empty_roundtrip) {
    __fs2_checkreset();

    fs2 a = FS2();
    CHECK_FS2VALID(1, &a);

    fs2 *h = fs2_stacktoheap(&a);
    CHECK_FS2VALID(2, h);
    CHECK_NULL("h->v", h->v);

    fs2 b = fs2_heaptostack(&h);
    CHECK_FS2VALID(3, &b);
    CHECK_NULL("b.v", b.v);

    fs2_free(&b);
    assert_balanced();   /* nothing was allocated, nothing freed */
}

/* ── same pointer passed twice: no-op, no leak ───────────────────── */

Test(fs2, transition_move_self_noop) {
    __fs2_checkreset();

    fs2 a = fs2_fromstr("self");
    CHECK_FS2VALID(1, &a);

    fs2 *ret = fs2_move(&a, &a);
    CHECK_PTR_EQ("ret == &a", ret, &a);
    CHECK_FS2VALID(2, &a);
    CHECK_STR_EQ("a.v", a.v, "self");

    fs2_free(&a);
    assert_balanced();
}
