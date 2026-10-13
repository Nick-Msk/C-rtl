#include <criterion/criterion.h>

#include "fs2.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Fixtures — mute stderr, init log
 * ------------------------------------------------------------------------- */

static char log_path[256];
static int  saved_stderr = -1;

static void setup(void) {
    snprintf(log_path, sizeof(log_path),
             "/tmp/test_fs2_%d.log", (int)getpid());
    log_init(log_path, false, LOG_FORMAT_EMPTY);

    fflush(stderr);
    saved_stderr = dup(fileno(stderr));
    freopen("/dev/null", "w", stderr);
}

static void teardown(void) {
    log_close();
    remove(log_path);

    fflush(stderr);
    if (saved_stderr >= 0) {
        dup2(saved_stderr, fileno(stderr));
        close(saved_stderr);
        saved_stderr = -1;
    }
}

TestSuite(fs2, .init = setup, .fini = teardown);

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(fs2, version_string_matches_macro) {
    cr_assert_str_eq(fs2_version(), FS2_VERSION);
    cr_assert_str_eq(fs2_version(), "0.1.0");
}

Test(fs2, version_is_top_of_list) {
    const char *const *v = fs2_versions();
    cr_assert_not_null(v);
    cr_assert_not_null(v[0]);
    cr_assert_str_eq(v[0], fs2_version());
    cr_assert_str_eq(v[0], FS2_VERSION);
}

Test(fs2, versions_terminated_by_null) {
    const char *const *v = fs2_versions();
    size_t n = 0;
    while (v[n]) ++n;
    cr_assert_geq(n, 1);
    cr_assert_null(v[n]);
}

/* -------------------------------------------------------------------------
 * Value macros — flags, len, sz, v
 * ------------------------------------------------------------------------- */

Test(fs2, macro_fs2_default) {
    fs2 s = FS2();
    cr_assert_eq(s.len, 0);
    cr_assert_eq(s.sz,  0);
    cr_assert_null(s.v);
    cr_assert(fs2_is_alloc(&s));
    cr_assert_not(fs2_is_static(&s));
    cr_assert_not(fs2_is_bodyalloc(&s));
    cr_assert_not(fs2_is_defunct(&s));
}

Test(fs2, macro_empty_static) {
    fs2 s = FS2_EMPTY;
    cr_assert(fs2_is_static(&s));
    cr_assert_not(fs2_is_alloc(&s));
    cr_assert_eq(s.len, 0);
    cr_assert_str_eq(s.v, "");
}

Test(fs2, macro_defunct) {
    fs2 s = FS2_DEFUNCT_VAL;
    cr_assert(fs2_is_defunct(&s));
    cr_assert_null(s.v);
    cr_assert_eq(s.len, 0);
    cr_assert_eq(s.sz,  0);
}

Test(fs2, macro_literal) {
    fs2 s = FS2_LITERAL("hello");
    cr_assert(fs2_is_static(&s));
    cr_assert_eq(s.len, 5);
    cr_assert_eq(s.sz,  6);
    cr_assert_str_eq(s.v, "hello");
}

Test(fs2, macro_local) {
    char buf[32];
    fs2 s = FS2_LOCAL(buf);
    cr_assert(fs2_is_local(&s));
    cr_assert_not(fs2_is_static(&s));
    cr_assert_not(fs2_is_alloc(&s));
    cr_assert_eq(s.sz, 32);
    cr_assert_eq(s.len, 0);
    cr_assert(s.v == buf);
}

/* -------------------------------------------------------------------------
 * Getters
 * ------------------------------------------------------------------------- */

Test(fs2, getters_literal) {
    fs2 s = FS2_LITERAL("abc");
    cr_assert_eq(fs2_len(&s), 3);
    cr_assert_eq(fs2_sz(&s),  4);
    cr_assert_str_eq(fs2_cstr(&s), "abc");
    cr_assert(fs2_str(&s) == s.v);
}

Test(fs2, getters_empty) {
    fs2 s = FS2_EMPTY;
    cr_assert_eq(fs2_len(&s), 0);
    cr_assert_str_eq(fs2_cstr(&s), "");
}

/* -------------------------------------------------------------------------
 * fs2_init
 * ------------------------------------------------------------------------- */

Test(fs2, init_small) {
    fs2 s = fs2_init(8);
    cr_assert_not_null(s.v);
    cr_assert(fs2_is_alloc(&s));
    cr_assert_not(fs2_is_bodyalloc(&s));
    cr_assert_eq(s.len, 0);
    cr_assert_eq(s.v[0], '\0');
    cr_assert_geq(s.sz, 8);   /* power-of-two growth may overshoot */
    fs2_free(&s);
}

Test(fs2, init_is_power_of_two_rounded_up) {
    fs2 s = fs2_init(5);
    /* round_up_2(5) == 8 -> 16 */
    cr_assert_eq(s.sz, 16);
    fs2_free(&s);
}

Test(fs2, init_exact_power_of_two_bumps_up) {
    fs2 s = fs2_init(4);
    cr_assert_eq(s.sz, 16);
    fs2_free(&s);
}

Test(fs2, init_large_power_of_two) {
    fs2 s = fs2_init(100);
    /* round_up_2(100) = 128, > FS_MIN_ACCOC → 128 */
    cr_assert_eq(s.sz, 128);
    fs2_free(&s);
}

Test(fs2, init_large_fails_with_sigint, .signal = SIGINT) {
    /* 2^62 rounds up to 2^63 — malloc will fail on any realistic host */
    (void)fs2_init((size_t)1 << 62);
}

Test(fs2, init_zero_does_not_crash) {
    fs2 s = fs2_init(0);
    cr_assert_not_null(s.v);
    cr_assert_eq(s.len, 0);
    cr_assert_eq(s.v[0], '\0');
    cr_assert_eq(s.sz, 16);       /* FS_MIN_ACCOC */
    cr_assert(fs2_is_alloc(&s));
    fs2_free(&s);
}

Test(fs2, init_small_bumps_to_min) {
    fs2 s = fs2_init(5);
    /* round_up_2(5) = 8, but 8 < 16 → 16 */
    cr_assert_eq(s.sz, 16);
    fs2_free(&s);
}


/* -------------------------------------------------------------------------
 * fs2_free — negatives and idempotency
 * ------------------------------------------------------------------------- */

Test(fs2, free_null_noop) {
    fs2_free(NULL);       /* must not crash */
    cr_assert(1);
}

Test(fs2, free_static_noop) {
    fs2 s = FS2_LITERAL("hello");
    fs2_free(&s);
    cr_assert_str_eq(s.v, "hello");
    cr_assert_eq(s.len, 5);
    cr_assert_eq(s.sz,  6);
}

Test(fs2, free_empty_static_noop) {
    fs2 s = FS2_EMPTY;
    fs2_free(&s);
    cr_assert(fs2_is_static(&s));
    cr_assert_str_eq(s.v, "");
}

Test(fs2, free_local_noop) {
    char buf[16] = "abc";
    fs2 s = FS2_LOCAL(buf);
    s.len = 3;
    fs2_free(&s);
    cr_assert_str_eq(s.v, "abc");   /* buffer untouched */
    cr_assert(s.v == buf);
}

Test(fs2, free_defunct_noop) {
    fs2 s = FS2_DEFUNCT_VAL;
    fs2_free(&s);
    cr_assert(fs2_is_defunct(&s));
    cr_assert_null(s.v);
}

Test(fs2, free_alloc_releases_buffer) {
    fs2 s = fs2_init(32);
    cr_assert_not_null(s.v);
    fs2_free(&s);
    cr_assert_null(s.v);
    cr_assert_eq(s.len, 0);
    cr_assert_eq(s.sz,  0);
}

Test(fs2, free_alloc_is_idempotent) {
    fs2 s = fs2_init(32);
    fs2_free(&s);
    fs2_free(&s);          /* must not crash on double free */
    cr_assert_null(s.v);
}

Test(fs2, free_static_with_bodyalloc) {
    /* struct is heap-allocated, string is a literal — free must release body only */
    fs2 *s = malloc(sizeof *s);
    cr_assert_not_null(s);
    *s = FS2_LITERAL("still here");
    s->flags = (fs2_flags)(s->flags | FS2_BODYALLOC);

    fs2_free(s);           /* frees the struct, not the literal */
    /* no assertion on `s` — it is dangling */
    cr_assert(1);
}

/*
Test(fs2, free_alloc_with_bodyalloc) {
    // both buffer and struct on the heap 
    fs2 *s = fs2_heap_init(16);
    cr_assert_not_null(s);
    cr_assert(fs2_is_bodyalloc(s));
    fs2_free(s);
    cr_assert(1);
}*/

/* -------------------------------------------------------------------------
 * fs2_setlen
 * ------------------------------------------------------------------------- */

Test(fs2, setlen_truncates) {
    fs2 s = fs2_init(16);
    memcpy(s.v, "hello world", 12);
    s.len = 11;
    fs2_setlen(&s, 5);
    cr_assert_eq(s.len, 5);
    cr_assert_eq(s.v[5], '\0');
    cr_assert_str_eq(s.v, "hello");
    fs2_free(&s);
}

Test(fs2, setlen_to_zero) {
    fs2 s = fs2_init(16);
    memcpy(s.v, "abc", 4);
    s.len = 3;
    fs2_setlen(&s, 0);
    cr_assert_eq(s.len, 0);
    cr_assert_str_eq(s.v, "");
    fs2_free(&s);
}

/* -------------------------------------------------------------------------
 * fs2_is_empty / fs2_is_null
 * ------------------------------------------------------------------------- */

Test(fs2, is_empty_true_on_zero_len) {
    fs2 s = FS2_LITERAL("");
    cr_assert(fs2_is_empty(&s));
}

Test(fs2, is_empty_false_on_nonempty) {
    fs2 s = FS2_LITERAL("x");
    cr_assert_not(fs2_is_empty(&s));
}

Test(fs2, is_null_on_pointer_null) {
    cr_assert(fs2_is_null(NULL));
}

Test(fs2, is_null_on_defunct) {
    fs2 s = FS2_DEFUNCT_VAL;
    cr_assert(fs2_is_null(&s));     /* v == NULL */
}

Test(fs2, is_null_false_on_literal) {
    fs2 s = FS2_LITERAL("ok");
    cr_assert_not(fs2_is_null(&s));
}

/* -------------------------------------------------------------------------
 * fs2_validate
 * ------------------------------------------------------------------------- */

Test(fs2, validate_ok_literal) {
    fs2 s = FS2_LITERAL("hello");
    cr_assert(fs2_validate(NULL, &s));
}

Test(fs2, validate_ok_init) {
    fs2 s = fs2_init(16);
    cr_assert(fs2_validate(NULL, &s));
    fs2_free(&s);
}

Test(fs2, validate_null_pointer) {
    cr_assert_not(fs2_validate(NULL, NULL));
}

Test(fs2, validate_null_buffer) {
    fs2 s = FS2();          /* v == NULL */
    cr_assert_not(fs2_validate(NULL, &s));
}

Test(fs2, validate_len_ge_sz) {
    fs2 s = FS2_LITERAL("abc");
    s.len = s.sz;           /* len >= sz — forged invariant violation */
    cr_assert_not(fs2_validate(NULL, &s));
}

Test(fs2, validate_len_gt_strlen) {
    fs2 s = FS2_LITERAL("abc");
    s.len = 100;            /* len > sz too — but check order in impl */
    cr_assert_not(fs2_validate(NULL, &s));
}

Test(fs2, validate_writes_to_stream) {
    char path[256];
    snprintf(path, sizeof(path),
             "/tmp/test_fs2_validate_%d.txt", (int)getpid());

    FILE *f = fopen(path, "w");
    cr_assert_not_null(f);

    fs2 s = FS2();          /* invalid: v == NULL */
    cr_assert_not(fs2_validate(f, &s));
    fclose(f);

    FILE *r = fopen(path, "r");
    cr_assert_not_null(r);
    char buf[512] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, r);
    fclose(r);
    remove(path);

    cr_assert_gt(n, 0);
    cr_assert(strstr(buf, "nullable") != NULL, "buf: %s", buf);
}

/* -------------------------------------------------------------------------
 * fs_techfprint
 * ------------------------------------------------------------------------- */

Test(fs2, techfprint_literal) {
    char path[256];
    snprintf(path, sizeof(path),
             "/tmp/test_fs2_tech_%d.txt", (int)getpid());

    FILE *f = fopen(path, "w");
    cr_assert_not_null(f);

    fs2 s = FS2_LITERAL("payload");
    long n = fs_techfprint(f, &s, "s");
    fclose(f);

    cr_assert_gt(n, 0);

    FILE *r = fopen(path, "r");
    char buf[512] = {0};
    size_t got = fread(buf, 1, sizeof(buf) - 1, r);
    fclose(r);
    remove(path);

    cr_assert_gt(got, 0);
    cr_assert(strstr(buf, "FS2:") != NULL, "buf: %s", buf);
    cr_assert(strstr(buf, "payload") != NULL, "buf: %s", buf);
}

Test(fs2, techfprint_null_s) {
    char path[256];
    snprintf(path, sizeof(path),
             "/tmp/test_fs2_tech_null_%d.txt", (int)getpid());

    FILE *f = fopen(path, "w");
    cr_assert_not_null(f);

    long n = fs_techfprint(f, NULL, "s");
    fclose(f);

    cr_assert_gt(n, 0);

    FILE *r = fopen(path, "r");
    char buf[512] = {0};
    size_t got = fread(buf, 1, sizeof(buf) - 1, r);
    fclose(r);
    remove(path);

    cr_assert_gt(got, 0);
    cr_assert(strstr(buf, "<NULL>") != NULL, "buf: %s", buf);
}

Test(fs2, techfprint_null_out_returns_zero) {
    fs2 s = FS2_LITERAL("x");
    cr_assert_eq(fs_techfprint(NULL, &s, "s"), 0);
}

