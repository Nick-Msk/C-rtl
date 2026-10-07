#include <criterion/criterion.h>
#include <criterion/new/assert.h>

#include "log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>   /* getpid */

/*
 * Each test gets its own file in /tmp, keyed by PID (Criterion forks
 * per test by default, so PIDs are unique). teardown closes the logger
 * and removes the file.
 */
static char test_log_path[256];

static void setup(void) {
    snprintf(test_log_path, sizeof(test_log_path),
             "/tmp/test_log_%d.log", (int)getpid());
    remove(test_log_path);
}

static void teardown(void) {
    log_close();
    remove(test_log_path);
}

TestSuite(log, .init = setup, .fini = teardown);

/* --- pure function: no file needed --- */

Test(log, levelstr_known) {
    cr_assert_str_eq(log_levelstr(LOGOFF), "LOGOFF");
    cr_assert_str_eq(log_levelstr(LOGERR), "LOGERR");
    cr_assert_str_eq(log_levelstr(LOGWARN), "LOGWARN");
    cr_assert_str_eq(log_levelstr(LOGALL), "LOGALL");
}

Test(log, levelstr_unknown) {
    cr_assert_str_eq(log_levelstr((Loglevel)42), "Unknown log level");
}

/* --- lifecycle --- */

Test(log, init_close) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));
    cr_assert(log_isinit());
    cr_assert_not_null(log_file());
    log_close();
    cr_assert_not(log_isinit());
    cr_assert_null(log_file());
}

Test(log, init_idempotent) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));
    FILE *first = log_file();
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL)); /* 2nd call is no-op */
    cr_assert_eq(log_file(), first);
}

/* --- global switches --- */

Test(log, offset_starts_zero) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));
    cr_assert_eq(log_offset(), 0);
}

Test(log, format_accepts_valid) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));
    cr_assert(log_format(LOG_FORMAT_EMPTY));
    cr_assert(log_format(LOG_FORMAT_ONLY_FUNC));
    cr_assert(log_format(LOG_FORMAT_ONLY_TIME));
}

Test(log, format_rejects_invalid) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));

    /* library writes to stderr on purpose; silence it for this test */
    fflush(stderr);
    int saved_stderr = dup(fileno(stderr));
    cr_assert_geq(saved_stderr, 0);
    freopen("/dev/null", "w", stderr);

    cr_assert_not(log_format((LogFormat)999));

    fflush(stderr);
    dup2(saved_stderr, fileno(stderr));
    close(saved_stderr);
}

Test(log, prog_switch_returns_previous) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));
    /* default is "on" (true == 1) */
    cr_assert_eq(log_prog_switch(false), 1);
    cr_assert_eq(log_prog_switch(true),  0);
}

/* --- writing and reading back --- */

Test(log, write_simple_message) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__,
            "hello %s", "world");
    log_close();

    FILE *f = fopen(test_log_path, "r");
    cr_assert_not_null(f, "cannot open %s", test_log_path);

    char buf[256] = {0};
    cr_assert_not_null(fgets(buf, sizeof(buf), f));
    fclose(f);

    cr_assert_str_eq(buf, "hello world\n");
}

Test(log, write_two_messages) {
    cr_assert(log_init(test_log_path, false, LOG_FORMAT_ALL));

    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "first");
    log_msg(LOG_MSG | LOG_SIMPLE | LOG_NOPREAMBULE, 0, LOGALL,
            DEFAULT_MOD, __FILE__, __func__, __LINE__, "second");
    log_close();

    FILE *f = fopen(test_log_path, "r");
    cr_assert_not_null(f);

    char line1[64] = {0}, line2[64] = {0};
    cr_assert_not_null(fgets(line1, sizeof(line1), f));
    cr_assert_not_null(fgets(line2, sizeof(line2), f));
    fclose(f);

    cr_assert_str_eq(line1, "first\n");
    cr_assert_str_eq(line2, "second\n");
}