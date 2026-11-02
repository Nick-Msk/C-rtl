/* ══════════════════════════════════════════════════════════════════════════
 * Internal view helpers (zero-copy STATIC wrappers).
 *
 * ⚠ VIEW SEMANTICS: result aliases `str`. `str` must outlive every use.
 * Do NOT pass the result as both dst and src to mutators (restrict → UAF).
 * Do NOT use after `str` goes out of scope.
 *
 * Exposed only when FS2_INTERNAL_UTILS is defined (by fs2_api.h, or by
 * the .c implementation). Application code should NOT call these directly —
 * use fs2_fromstr (owned copy) or _fs2_wrap (from fs2_utils.h, if provided).
 * ══════════════════════════════════════════════════════════════════════════ */
#ifndef __FS2_WRAP_INTERNAL_H
#define __FS2_WRAP_INTERNAL_H

/**
 * @brief Wrap a C-string as an fs2 view (STATIC, zero-copy).
 *
 * @param str  NUL-terminated string, or NULL.
 *             NULL is treated as the empty string.
 * @return     fs2 view aliasing @p str (or the empty view if NULL).
 *             @p str must outlive every use of the result.
 * @note       No allocation. `fs2_free` on the result is a no-op.
 *             Do not pass the result as both `dst` and `src` to fs2_cat
 *             (restrict violation).
 */
static inline fs2 
_fs2_wrap(const char *str) {
    if (!str)
        return FS2EMPTY;
    size_t  len = strlen(str), sz = len + 1;
    fs2 res = (fs2) {.len = len, .sz = sz, .v = (char *) str, .flags = FS2_STATIC};
    return res;
}

/**
 * @brief Wrap a not null C-string as an fs2 view (STATIC, zero-copy).
 *
 * @param str  NUL-terminated string, or NULL.
 *             NULL is treated as the empty string.
 * @return     fs2 view aliasing @p str.
 *             @p str must outlive every use of the result.
 * @note       No allocation. `fs2_free` on the result is a no-op.
 *             Do not pass the result as both `dst` and `src` to fs2_cat
 *             (restrict violation).
 */
static inline fs2 
_fs2_wrap_notnull(const char *str) {
    invfatal(ERR_NULL_INPUT, str != NULL, "Null src");

    size_t  len = strlen(str), sz = len + 1;
    fs2 res = (fs2) {.len = len, .sz = sz, .v = (char *) str, .flags = FS2_STATIC};
    return res;
}

#endif /* __FS2_WRAP_INTERNAL_H */
