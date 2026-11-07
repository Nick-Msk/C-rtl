#include <criterion/criterion.h>

#include "test_fs2.h"
#include "fs2_api.h"
#include <unistd.h>
#include <sys/stat.h>

TestSuite(fs2_api_tech, .init = test_setup, .fini = test_teardown);

/* Read errno (satisfies unix.Errno), reset to 0 for the next call.
 * Returns the prior value so caller can log it on failure. */
static inline int errno_take(void) {
    int e = errno;
    errno = 0;
    return e;
}

/* Read whole file; NULL on failure. No errno leaked to caller. */
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    int  e = errno_take();
    if (!f) {
        logsimple("read_file: fopen '%s': %s", path, strerror(e));
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        e = errno_take();
        logsimple("read_file: fseek '%s': %s", path, strerror(e));
        fclose(f);
        errno_take();
        return NULL;
    }
    errno_take();   /* discard success-path errno */

    long sz = ftell(f);
    e = errno_take();
    if (sz < 0) {
        logsimple("read_file: ftell '%s': %s", path, strerror(e));
        fclose(f);
        errno_take();
        return NULL;
    }
    rewind(f);
    errno_take();

    char *buf = malloc((size_t)sz + 1);
    e = errno_take();
    if (!buf) {
        logsimple("read_file: malloc %zu: %s", (size_t)sz + 1, strerror(e));
        fclose(f);
        errno_take();
        return NULL;
    }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    e = errno_take();
    if (rd != (size_t)sz) {
        logsimple("read_file: fread '%s': %zu/%zu %s",
                  path, rd, (size_t)sz,
                  ferror(f) ? strerror(e) : "(EOF)");
        free(buf);
        fclose(f);
        errno_take();
        return NULL;
    }

    if (fclose(f) != 0) {
        e = errno_take();
        logsimple("read_file: fclose '%s': %s", path, strerror(e));
        free(buf);
        return NULL;
    }
    errno_take();   /* discard success-path errno */

    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

/* fread wrapper: check errno on short read, abort test on I/O error. */
static size_t fread_or_fail(FILE *f, char *buf, size_t n, const char *where) {
    errno_take();
    size_t rd = fread(buf, 1, n, f);
    
    int    e  = errno_take();
    if (rd < n && ferror(f)) {
        cr_assert_fail("%s: fread %zu/%zu: errno=%d (%s)",
                       where, rd, n, e, strerror(e));
    }
    return rd;
}

/* tmpfile wrapper: check errno, abort if NULL. */
static FILE *tmpfile_or_fail(const char *where) {
    FILE *f = tmpfile();
    int   e = errno_take();
    if (!f) {
        cr_assert_fail("%s: tmpfile: errno=%d (%s)", where, e, strerror(e));
    }
    return f;
}

/* =========================================================================
 * fs2_techdump — basic
 * ========================================================================= */

Test(fs2_api, techdump_basic_to_tmpfile) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello");

    FILE *f = tmpfile_or_fail(__func__);
    cr_assert_not_null(f);

    long w = fs2_techdump(f, &s, "test");
    int  e = errno_take();                      /* ← errno from techdump's fprintf/fwrite */
    cr_assert_gt(w, 0, "techdump: %ld, errno=%d (%s)", w, e, strerror(e));


    rewind(f);
    char buf[256] = {0};
    size_t rd = fread_or_fail(f, buf, sizeof buf - 1, __func__);
    if (fclose(f) != 0) {
        e = errno_take();
        cr_assert_fail("fclose: %s", strerror(e));
    }
    errno_take();

    /* Header contains key fields */
    cr_assert(strstr(buf, "FS2: test:") != NULL, "header missing: %s", buf);
    cr_assert(strstr(buf, "len [5]") != NULL);
    cr_assert(strstr(buf, "sz [") != NULL);
    cr_assert(strstr(buf, "flags [") != NULL);
    /* Body is verbatim "hello" */
    cr_assert(strstr(buf, "[hello]") != NULL, "body missing: %s", buf);
    /* Trailing newline */
    cr_assert(buf[rd - 1] == '\n');

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_techdump — canonical empty (v == NULL, no crash)
 * ========================================================================= */

Test(fs2_api, techdump_canon_v_null_no_crash) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v);

    FILE *f = tmpfile();
    cr_assert_not_null(f);

    long w = fs2_techdump(f, &s, "canon");
    cr_assert_gt(w, 0);

    rewind(f);
    errno_take();

    char buf[256] = {0};
    size_t rd = fread(buf, 1, sizeof buf - 1, f);
    errno_take();

    fclose(f);
    cr_assert(rd > 0);
    buf[rd] = '\0';

    cr_assert(strstr(buf, "(nil)") != NULL, "expected (nil): %s", buf);
    cr_assert(strstr(buf, "[]") != NULL, "expected empty body: %s", buf);

    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, techdump_fs2empty_prints_empty_body) {
    __fs2_checkreset();
    fs2 s = FS2EMPTY;

    FILE *f = tmpfile();
    cr_assert_not_null(f);
    fs2_techdump(f, &s, "empty");
    rewind(f);
    errno_take();

    char buf[256] = {0};
    size_t rd = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[rd] = '\0';

    cr_assert(strstr(buf, "len [0]") != NULL);
    cr_assert(strstr(buf, "[]") != NULL);

    assert_balanced();
}

/* =========================================================================
 * fs2_techdump — NULL s
 * ========================================================================= */

Test(fs2_api, techdump_null_s_prints_null_marker) {
    __fs2_checkreset();

    FILE *f = tmpfile();
    cr_assert_not_null(f);
    long w = fs2_techdump(f, NULL, "null-s");
    cr_assert_gt(w, 0);

    rewind(f);
    char buf[128] = {0};
    errno_take();

    size_t rd = fread(buf, 1, sizeof buf - 1, f);
    

    fclose(f);
    buf[rd] = '\0';

    cr_assert(strstr(buf, "<NULL>") != NULL, "got: %s", buf);
    assert_balanced();
}

/* =========================================================================
 * fs2_techdump — full content, no truncation (contrast with techfprint)
 * ========================================================================= */

Test(fs2_api, techdump_full_content_not_truncated) {
    __fs2_checkreset();

    /* Build content > FS2_TECH_PRINT_COUNT (which techfprint truncates) */
    fs2 s = FS2();
    for (int i = 0; i < 500; i++)
        elem(s, i) = (char)('A' + (i % 26));
    fs2_setlen(&s, 500);
    cr_assert_eq(s.len, 500);

    FILE *f = tmpfile();
    cr_assert_not_null(f);
    fs2_techdump(f, &s, "long");
    rewind(f);

    size_t rd;
    char *buf = NULL;
    {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        cr_assert_gt(sz, 500);   /* > 500 bytes: header + full content */
        rewind(f);
        buf = malloc((size_t)sz + 1);
        rd = fread(buf, 1, (size_t)sz, f);
        buf[rd] = '\0';
    }
    fclose(f);

    /* Content: 500 chars verbatim, no "..." ellipsis */
    cr_assert(strstr(buf, "...") == NULL, "should NOT truncate");
    /* The 500-char pattern must appear verbatim */
    cr_assert(strstr(buf, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") != NULL);

    free(buf);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_techdump_file — round-trip
 * ========================================================================= */

Test(fs2_api, techdump_file_roundtrip) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello world");

    const char *path = "/tmp/fs2_techdump_test.txt";
    unlink(path);

    cr_assert(fs2_techdump_file(&s, "roundtrip", path));

    size_t rd;
    char *buf = read_file(path, &rd);
    cr_assert_not_null(buf, "file should exist");
    cr_assert_gt(rd, 0);

    cr_assert(strstr(buf, "FS2: roundtrip:") != NULL);
    cr_assert(strstr(buf, "[hello world]") != NULL);

    free(buf);
    unlink(path);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, techdump_file_null_path_returns_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    cr_assert_not(fs2_techdump_file(&s, "n", NULL));
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, techdump_file_bad_path_returns_false) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");

    /* Directory that doesn't exist */
    cr_assert_not(fs2_techdump_file(&s, "n", "/nonexistent_xyz_12345/file.txt"));

    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * fs2_techdump_file — canonical empty (no crash on v == NULL)
 * ========================================================================= */

Test(fs2_api, techdump_file_canon_v_null_no_crash) {
    __fs2_checkreset();
    fs2 s = FS2();
    const char *path = "/tmp/fs2_techdump_empty.txt";
    unlink(path);

    cr_assert(fs2_techdump_file(&s, "canon", path));

    size_t rd;
    char *buf = read_file(path, &rd);
    cr_assert_not_null(buf);
    cr_assert(strstr(buf, "(nil)") != NULL);
    cr_assert(strstr(buf, "[]") != NULL);

    free(buf);
    unlink(path);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Macro
 * ========================================================================= */

Test(fs2_api, techdump_macro) {
    __fs2_checkreset();
    fs2  val = fs2_fromstr("m");
    fs2 *s   = &val; 

    FILE *f = tmpfile();
    cr_assert_not_null(f);
    long w = FS2TECHDUMP(f, s);   /* name = "s" via #s */
    cr_assert_gt(w, 0);

    rewind(f);
    errno_take();
    
    char buf[256] = {0};
    fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    cr_assert(strstr(buf, "FS2: s:") != NULL, "macro name missing: %s", buf);

    fs2_free(s);
    assert_balanced();
}

/* =========================================================================
 * fs2_techdump — s == NULL out (no crash)
 * ========================================================================= */

Test(fs2_api, techdump_null_out_returns_zero) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");
    CHECK_EQ("no out, no write", fs2_techdump(NULL, &s, "n"), 0);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, techdump_file_null_s_ok) {
    __fs2_checkreset();
    const char *path = "/tmp/fs2_techdump_null.txt";
    unlink(path);

    cr_assert(fs2_techdump_file(NULL, "null", path));

    size_t rd;
    char *buf = read_file(path, &rd);
    cr_assert_not_null(buf);
    cr_assert(strstr(buf, "<NULL>") != NULL);
    free(buf);
    unlink(path);
    assert_balanced();
}

/* =========================================================================
 * fs2_techdump_emergency — nominal
 * ========================================================================= */

Test(fs2_api, techdump_emergency_basic) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("hello emergency");

    const char *path = fs2_techdump_emergency(&s, "test1");
    cr_assert_not_null(path, "should return a path");

    /* Path format sanity */
    cr_assert(strstr(path, "fs2dump-") != NULL, "bad prefix: %s", path);
    cr_assert(strstr(path, "test1") != NULL,    "name missing: %s", path);
    cr_assert(strstr(path, ".bin") != NULL,     "bad suffix: %s", path);
    cr_assert(strstr(path, "/tmp/") != NULL ||
              strstr(path, getenv("TMPDIR") ? getenv("TMPDIR") : "\x01") != NULL,
              "unexpected dir: %s", path);

    /* File exists and contains content verbatim */
    size_t rd;
    char *buf = read_file(path, &rd);
    cr_assert_not_null(buf, "file should exist: %s", path);
    cr_assert_gt(rd, 0);
    cr_assert(strstr(buf, "[hello emergency]") != NULL, "got: %s", buf);

    free(buf);
    unlink(path);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * FS2_DUMP_DIR override
 * ========================================================================= */

Test(fs2_api, techdump_emergency_respects_env_dir) {
    __fs2_checkreset();

    const char *dir = "/tmp/fs2_dump_test_dir";
    mkdir(dir, 0755);   /* ok if exists */

    setenv("FS2_DUMP_DIR", dir, 1);

    fs2 s = fs2_fromstr("x");
    const char *path = fs2_techdump_emergency(&s, "env");

    cr_assert_not_null(path);
    cr_assert(strncmp(path, dir, strlen(dir)) == 0,
              "path not in FS2_DUMP_DIR: %s", path);

    unlink(path);
    unsetenv("FS2_DUMP_DIR");
    fs2_free(&s);
    rmdir(dir);
    assert_balanced();
}

/* =========================================================================
 * NULL fs2 / canonical empty (v == NULL) — no crash
 * ========================================================================= */

Test(fs2_api, techdump_emergency_null_s) {
    __fs2_checkreset();

    const char *path = fs2_techdump_emergency(NULL, "null-s");
    cr_assert_not_null(path);

    size_t rd;
    char *buf = read_file(path, &rd);
    cr_assert_not_null(buf);
    cr_assert(strstr(buf, "<NULL>") != NULL, "got: %s", buf);

    free(buf);
    unlink(path);
    assert_balanced();
}

Test(fs2_api, techdump_emergency_canon_v_null) {
    __fs2_checkreset();
    fs2 s = FS2();
    cr_assert_null(s.v);

    const char *path = fs2_techdump_emergency(&s, "canon");
    cr_assert_not_null(path);

    size_t rd;
    char *buf = read_file(path, &rd);
    cr_assert_not_null(buf);
    cr_assert(strstr(buf, "(nil)") != NULL, "got: %s", buf);
    cr_assert(strstr(buf, "[]")    != NULL);

    free(buf);
    unlink(path);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Name sanitization
 * ========================================================================= */

Test(fs2_api, techdump_emergency_sanitizes_name) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");

    /* Name has "/" (would break path), spaces, colon */
    const char *path = fs2_techdump_emergency(&s, "a/b c:d");
    cr_assert_not_null(path);

    /* "/" and " " and ":" all became "_" */
    cr_assert(strstr(path, "a_b_c_d") != NULL, "sanitization failed: %s", path);

    unlink(path);
    fs2_free(&s);
    assert_balanced();
}

Test(fs2_api, techdump_emergency_null_name) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");

    const char *path = fs2_techdump_emergency(&s, NULL);
    cr_assert_not_null(path);
    cr_assert(strstr(path, "unnamed") != NULL, "default name missing: %s", path);

    unlink(path);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Multiple calls → distinct paths (seq counter)
 * ========================================================================= */

Test(fs2_api, techdump_emergency_distinct_paths) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("x");

    const char *p1 = fs2_techdump_emergency(&s, "multi");
    cr_assert_not_null(p1);
    char *copy1 = strdup(p1);
    cr_assert_not_null(copy1);

    const char *p2 = fs2_techdump_emergency(&s, "multi");
    cr_assert_not_null(p2);

    /* Distinct filenames (seq differs even within same second) */
    cr_assert(strcmp(copy1, p2) != 0, "paths must differ: %s == %s", copy1, p2);

    unlink(copy1);
    unlink(p2);
    free(copy1);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Thread-local path buffer — separate threads get separate buffers
 * (best-effort check, single-threaded test verifies the invariant)
 * ========================================================================= */

Test(fs2_api, techdump_emergency_returned_path_valid_after_call) {
    __fs2_checkreset();
    fs2 s = fs2_fromstr("test");

    const char *path = fs2_techdump_emergency(&s, "persist");
    cr_assert_not_null(path);

    /* Path must still be readable immediately after return */
    cr_assert_gt(strlen(path), 0);

    /* File must exist */
    FILE *f = fopen(path, "rb");
    cr_assert_not_null(f, "file disappeared: %s", path);
    fclose(f);

    unlink(path);
    fs2_free(&s);
    assert_balanced();
}

/* =========================================================================
 * Macro
 * ========================================================================= */

Test(fs2_api, techdump_emergency_macro) {
    __fs2_checkreset();
    fs2 res = fs2_fromstr("m");
    fs2 *s = &res;

    const char *path = FS2TECHDUMP_EMERGENCY(s);
    cr_assert_not_null(path);
    /* #s expands to "s" (the variable name), which is alnum -> kept as-is */
    cr_assert(strstr(path, "-s-") != NULL, "macro name missing: %s", path);

    unlink(path);
    fs2_free(s);
    assert_balanced();
}

/* =========================================================================
 * Bad env dir -> returns NULL, logs error, no crash
 * ========================================================================= */

Test(fs2_api, techdump_emergency_bad_dir_returns_null) {
    __fs2_checkreset();
    setenv("FS2_DUMP_DIR", "/nonexistent_dir_xyz_12345", 1);

    fs2 s = fs2_fromstr("x");
    const char *path = fs2_techdump_emergency(&s, "bad");

    cr_assert_null(path, "should return NULL on open failure");

    unsetenv("FS2_DUMP_DIR");
    fs2_free(&s);
    assert_balanced();
}
