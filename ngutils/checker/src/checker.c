
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"
#include "checker.h"



/********************************************************************
                 CHECK MODULE IMPLEMENTATION
********************************************************************/

/* ─────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────── */

static const char *const versions[] = {
    CHECKER_VERSION,   /* current — always computed from macros */
    NULL
};

const char *const *
checker_versions(void)
{
    return versions;
}

const char *
checker_version(void)
{
    return versions[0];
}

/* -------------------------------------------------------------------------
 * Core printing
 * ------------------------------------------------------------------------- */

int                     
inv_fprintf_int(FILE *restrict out, const char *restrict expr,
                    bool res, const char *restrict msg, ...){
	int 	cnt = 0;

    if (res)
        return 0;

    // TODO: must do err_put instead!
    /* if (out) {
        cnt += fprintf(out, "ERRNUM_INVARIANT_VIOLATION: [%s] is violated\n", expr);
        cnt += fprintf(out, "  ");
    } */
    logsimple("ERRNUM_INVARIANT_VIOLATION: [%s] is violated", expr);

    /*if (msg) {
        va_list  ap;
        va_start(ap, msg);
        if (out) {
            cnt += vfprintf(out, msg, ap);
            cnt += fprintf(out, "\n");
        }
        va_end(ap);
    } */
    logsimple("%s", msg);   // logsimple_ap is required TODO:

	return cnt;
}

/* -------------------------------------------------------------------------
 * Per-type converters
 *
 * Each: if equal → return true, no output. Otherwise print both values
 * to @p out (if non-NULL) and to the log, return false.
 * ------------------------------------------------------------------------- */

#define INV_MISMATCH_PRINT(fmt, r, v)                                 \
    do {                                                              \
        if (out) {                                                    \
            fprintf(out, "  expected [" fmt "], got [" fmt "]\n", (v), (r)); \
        }                                                             \
        logsimple("  expected [" fmt "], got [" fmt "]", (v), (r));   \
    } while (0)

bool
inv_char_bool(FILE *out, char res, char val)
{
    if (res == val) return true;
    INV_MISMATCH_PRINT("%d", res, val);
    return false;
}

bool
inv_int_bool(FILE *out, int res, int val)
{
    if (res == val) return true;
    INV_MISMATCH_PRINT("%d", res, val);
    return false;
}

bool
inv_uint_bool(FILE *out, unsigned res, unsigned int val)
{
    if (res == val) return true;
    INV_MISMATCH_PRINT("%d", res, val);
    return false;
}

bool
inv_long_bool(FILE *out, long res, long val)
{
    if (res == val) return true;
    INV_MISMATCH_PRINT("%ld", res, val);
    return false;
}

bool
inv_ulong_bool(FILE *out, unsigned long res, unsigned long val)
{
    if (res == val) return true;
    INV_MISMATCH_PRINT("%ld", res, val);
    return false;
}

bool
inv_double_bool(FILE *out, double res, double val)
{
    if (res == val)
        return true;
    INV_MISMATCH_PRINT("%g", res, val);
    return false;
}

bool
inv_str_bool(FILE *out, const char *res, const char *val)
{
    if (res == val)
        return true;                              /* both NULL */
    if (res && val && strcmp(res, val) == 0)
        return true;
    if (out)
        fprintf(out, "  expected [\"%s\"], got [\"%s\"]\n",
                val ? val : "(null)", res ? res : "(null)");
    logsimple("  expected [\"%s\"], got [\"%s\"]",
              val ? val : "(null)", res ? res : "(null)");
    return false;
}

bool
inv_ptr_bool(FILE *out, const void *res, const void *val)
{
    if (res == val)
        return true;
    if (out)
        fprintf(out, "  expected [%p], got [%p]\n", val, res);
    logsimple("  expected [%p], got [%p]", val, res);
    return false;
}
