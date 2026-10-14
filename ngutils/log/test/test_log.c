#include <criterion/criterion.h>

#include "log.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* -------------------------------------------------------------------------
 * Fixtures
 * ------------------------------------------------------------------------- */

static char log_path[256];

static void setup(void) {
    test_tmpname(log_path, sizeof log_path, "ngutils_log", NULL, "log");
    remove(log_path);
}

static void teardown(void) {
    log_close();
    remove(log_path);
}

TestSuite(log, .init = setup, .fini = teardown);

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static char   out_buf[4096];
static size_t out_len = 0;

static size_t slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { out_len = 0; out_buf[0] = '\0'; return 0; }
    out_len = fread(out_buf, 1, sizeof(out_buf) - 1, f);
    fclose(f);
    out_buf[out_len] = '\0';
    return out_len;
}

static int has(const char *needle) {
    size_t m = strlen(needle);
    if (m == 0 || m > out_len) return 0;
    for (size_t i = 0; i + m <= out_len; i++)
        if (memcmp(out_buf + i, needle, m) == 0) return 1;
    return 0;
}

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(log, version_string_matches_macro) {
    cr_assert_str_eq(log_version(), LOG_VERSION);
    cr_assert_str_eq(log_version(), "0.2.0");
}

Test(log, version_is_top_of_list) {
    const char *const *v = log_versions();
    cr_assert_not_null(v);
    cr_assert_not_null(v[0]);
    cr_assert_str_eq(v[0], log_version());
    cr_assert_str_eq(v[0], LOG_VERSION);
}

Test(log, versions_terminated_by_null) {
    const char *const *v = log_versions();
    size_t n = 0;
    while (v[n]) ++n;
    cr_assert_geq(n, 1);
    cr_assert_null(v[n]);
}

/* -------------------------------------------------------------------------
 * log_levelstr
 * ------------------------------------------------------------------------- */

Test(log, levelstr_all_known) {
    cr_assert_str_eq(log_levelstr(LOGOFF),  "LOGOFF");
    cr_assert_str_eq(log_levelstr(LOGERR),  "LOGERR");
    cr_assert_str_eq(log_levelstr(LOGWARN), "LOGWARN");
    cr_assert_str_eq(log_levelstr(LOGALL),  "LOGALL");
}

Test(log, levelstr_unknown) {
    cr_assert_str_eq(log_levelstr((Loglevel)99), "Unknown log level");
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

Test(log, init_close) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));
    cr_assert(log_isinit());
    cr_assert_not_null(log_file());

    log_close();

    cr_assert_not(log_isinit());
    cr_assert_null(log_file());
}

Test(log, init_is_idempotent) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));
    FILE *first = log_file();
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));
    cr_assert_eq(log_file(), first);
}

Test(log, close_resets_offset) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));
    logenter("x");
    (void)_LG_LV;
    cr_assert_eq(log_offset(), 4);

    log_close();
    cr_assert_eq(log_offset(), 0);
}

/* -------------------------------------------------------------------------
 * Writing
 * ------------------------------------------------------------------------- */

Test(log, simple_message_verbatim) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "hello %s", "world");

    log_close();
    cr_assert_eq(slurp(log_path), 12);
    cr_assert_str_eq(out_buf, "hello world\n");
}

Test(log, nonewline_suppresses_trailing_newline) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE | LOG_NONEWLINE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "abc");

    log_close();
    cr_assert_eq(slurp(log_path), 3);
    cr_assert_str_eq(out_buf, "abc");
}

Test(log, two_messages_appended) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "a");
    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "b");

    log_close();
    cr_assert_eq(slurp(log_path), 4);
    cr_assert_str_eq(out_buf, "a\nb\n");
}

/* -------------------------------------------------------------------------
 * Global switch
 * ------------------------------------------------------------------------- */

Test(log, switch_off_returns_prev_and_blocks) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    cr_assert_eq(log_prog_switch(false), 1);

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "hidden");
    log_close();

    cr_assert_eq(slurp(log_path), 0);
}

Test(log, switch_on_restores_output) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));
    log_prog_switch(false);

    cr_assert_eq(log_prog_switch(true), 0);

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "yes");
    log_close();

    cr_assert_eq(slurp(log_path), 4);
    cr_assert_str_eq(out_buf, "yes\n");
}

/* -------------------------------------------------------------------------
 * Format selection
 * ------------------------------------------------------------------------- */

Test(log, format_rejects_out_of_range) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));
    int saved = test_mute_stderr();
    cr_assert_not(log_format((LogFormat)999));
    test_restore_stderr(saved);
}

Test(log, format_only_func_prefix) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ONLY_FUNC));

    log_msg(LOG_MSG, 0, LOGALL,
            DEFAULT_MOD, "some.c", "myfunc", 42, "body");

    log_close();
    cr_assert_gt(slurp(log_path), 0);
    cr_assert(has("myfunc(42)"));
    cr_assert(has("body"));
}

Test(log, format_only_file_prefix) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ONLY_FILE));

    log_msg(LOG_MSG, 0, LOGALL,
            DEFAULT_MOD, "src/some.c", "fn", 7, "msg");

    log_close();
    cr_assert_gt(slurp(log_path), 0);
    cr_assert(has("src/some.c"));
    cr_assert(has("msg"));
}

/* -------------------------------------------------------------------------
 * Indentation
 * ------------------------------------------------------------------------- */

Test(log, enter_increments_offset) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));
    cr_assert_eq(log_offset(), 0);

    logenter("enter");
    (void)_LG_LV;

    cr_assert_eq(log_offset(), 4);
}

Test(log, leave_returns_offset_to_zero) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_ENTER, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "enter");
    cr_assert_eq(log_offset(), 4);

    log_msg(LOG_LEAVE, 4, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "leave");
    cr_assert_eq(log_offset(), 0);
}

Test(log, simple_enter_does_not_change_offset) {
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_ENTER | LOG_SIMPLE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "x");
    cr_assert_eq(log_offset(), 0);
}

/* -------------------------------------------------------------------------
 * Module filtering
 * ------------------------------------------------------------------------- */

Test(log, known_module_logall_emits) {
    LogModlevel mods[] = {
        { .module = "mymod", .level = LOGALL },
        { .module = "",      .level = _LOGSTOP }
    };
    cr_assert(log_modinit(mods));
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            "mymod", __FILE__, __func__, __LINE__, "visible");
    log_close();

    cr_assert_eq(slurp(log_path), 8);
    cr_assert_str_eq(out_buf, "visible\n");
}

Test(log, known_module_logoff_suppresses) {
    LogModlevel mods[] = {
        { .module = "mymod", .level = LOGOFF },
        { .module = "",      .level = _LOGSTOP }
    };
    cr_assert(log_modinit(mods));
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    int saved = test_mute_stderr();
    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            "mymod", __FILE__, __func__, __LINE__, "hidden");
    test_restore_stderr(saved);
    log_close();

    cr_assert_eq(slurp(log_path), 0);
}

Test(log, unknown_module_suppresses) {
    LogModlevel mods[] = {
        { .module = "known", .level = LOGALL },
        { .module = "",      .level = _LOGSTOP }
    };
    cr_assert(log_modinit(mods));
    cr_assert(log_init(log_path, false, LOG_FORMAT_ALL));

    int saved = test_mute_stderr();
    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            "unknown", __FILE__, __func__, __LINE__, "hidden");
    test_restore_stderr(saved);
    log_close();

    cr_assert_eq(slurp(log_path), 0);
}

Test(log, modinit_rejects_double_init) {
    LogModlevel mods[] = {
        { .module = "m", .level = LOGALL },
        { .module = "",  .level = _LOGSTOP }
    };
    int saved = test_mute_stderr();
    cr_assert(log_modinit(mods));
    cr_assert_not(log_modinit(mods));
    test_restore_stderr(saved);
}

Test(log, modinit_null_uses_default) {
    cr_assert(log_modinit(NULL));
}

/* -------------------------------------------------------------------------
 * Persistence
 * ------------------------------------------------------------------------- */

Test(log, modsave_writes_file) {
    LogModlevel mods[] = {
        { .module = "alpha", .level = LOGALL },
        { .module = "beta",  .level = LOGOFF },
        { .module = "",      .level = _LOGSTOP }
    };
    cr_assert(log_modinit(mods));

    char path[256];
    snprintf(path, sizeof(path), "/tmp/ngutils_mods_%d.txt", (int)getpid());
    remove(path);

    cr_assert(log_modsave(path));
    cr_assert_gt(slurp(path), 0);
    cr_assert(has("Total modules"));
    cr_assert(has("alpha"));
    cr_assert(has("beta"));

    remove(path);
}

/* -------------------------------------------------------------------------
 * log_mkdir / log_initf / log_init_uniq
 * ------------------------------------------------------------------------- */

Test(log, mkdir_creates) {
    char path[256];
    test_tmpname(path, sizeof path, "test_log_mkdir", NULL, "tmp");
    /* make a directory path under /tmp */
    snprintf(path, sizeof path, "/tmp/test_log_mkdir_%d", (int)getpid());
    remove(path);   /* in case */
    rmdir(path);    /* in case */

    cr_assert(log_mkdir(path));
    struct stat st;
    cr_assert_eq(stat(path, &st), 0);
    cr_assert(S_ISDIR(st.st_mode));

    rmdir(path);
}

Test(log, mkdir_existing_ok) {
    cr_assert(log_mkdir("/tmp"));
    cr_assert(log_mkdir("/tmp"));   /* idempotent */
}

Test(log, mkdir_null_fails) {
    cr_assert_not(log_mkdir(NULL));
    cr_assert_not(log_mkdir(""));
}

Test(log, initf_plain_filename) {
    char base[256];
    snprintf(base, sizeof base, "/tmp/test_logf_%d", (int)getpid());

    cr_assert(log_initf(false, LOG_FORMAT_EMPTY, "/tmp",
                        "test_logf_%d.log", (int)getpid()));
    cr_assert(log_isinit());
    log_close();

    char path[300];
    snprintf(path, sizeof path, "%s.log", base);
    cr_assert_eq(access(path, F_OK), 0);
    remove(path);
}

Test(log, initf_null_dir_uses_cwd) {
    char name[256];
    snprintf(name, sizeof name, "test_logf_cwd_%d.log", (int)getpid());

    cr_assert(log_initf(false, LOG_FORMAT_EMPTY, NULL, "%s", name));
    cr_assert(log_isinit());
    log_close();

    cr_assert_eq(access(name, F_OK), 0);
    remove(name);
}

Test(log, initf_empty_dir_uses_cwd) {
    char name[256];
    snprintf(name, sizeof name, "test_logf_empty_%d.log", (int)getpid());

    cr_assert(log_initf(false, LOG_FORMAT_EMPTY, "", "%s", name));
    log_close();

    cr_assert_eq(access(name, F_OK), 0);
    remove(name);
}

Test(log, init_uniq_creates_pid_named_file) {
    char dir[256];
    snprintf(dir, sizeof dir, "/tmp/test_log_uniq_%d", (int)getpid());
    remove(dir);
    rmdir(dir);

    cr_assert(log_mkdir(dir));
    cr_assert(log_init_bypid(dir, "run", false, LOG_FORMAT_EMPTY));
    cr_assert(log_isinit());
    log_close();

    char path[300];
    snprintf(path, sizeof path, "%s/run_%d.log", dir, (int)getpid());
    cr_assert_eq(access(path, F_OK), 0);

    remove(path);
    rmdir(dir);
}
