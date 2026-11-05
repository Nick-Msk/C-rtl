#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "fs2.h"

enum {FS2_DUMP_FILE_MAX_SIZE = 8192 };
static const size_t             FS_TECH_PRINT_COUNT     = 100; // symplos to print

/* ─────────────────────────────────────────────────────────────────────────
 * Validator
 * ───────────────────────────────────────────────────────────────────────── */

 // TODO: move that to common.h
#define LOGNPRINTF(out, msg, ...) \
({ if (out) \
    fprintf(out, (msg), ##__VA_ARGS__); \
  logsimpleerr(false, (msg), ##__VA_ARGS__); \
})

/**
 * @brief Validate all invariants of an fs2.
 *
 * Checks:
 * 1. No unknown flag bits.
 * 2. Exactly one of STATIC / LOCAL / ALLOC is set.
 * 3. Buffer presence: STATIC/LOCAL require v; ALLOC allows v==NULL only
 *    in canonical empty state (len==0, sz==0).
 * 4. len < sz.
 * 5. strlen(v) == len.
 *
 * On failure, a diagnostic message is written to @p out (if non-NULL)
 * and to the internal error log.
 *
 * @param out  Optional output stream for the diagnostic; may be NULL.
 * @param s    fs2 to validate (may be NULL — reports and returns false).
 * @return     true if all invariants hold; false otherwise.
 */
bool
fs2_validate(FILE *restrict out, const fs2 *restrict s)
{
    if (!s)
        return LOGNPRINTF(out, "null pointer");

    fs2_flags FS2_ALL_FLAGS = (FS2_CONST | FS2_STATIC | FS2_LOCAL | FS2_ALLOC | FS2_BODYALLOC);
    
    /* 1. No unknown bits. */
    if (s->flags & ~FS2_ALL_FLAGS)
        return LOGNPRINTF(out, "unknown flag bits in 0x%x", (unsigned)s->flags);

    /* 2. Exactly one of STATIC / LOCAL / ALLOC. */
    {
        fs2_flags kind = s->flags & (FS2_STATIC | FS2_LOCAL | FS2_ALLOC);
        if (kind != FS2_STATIC && kind != FS2_LOCAL && kind != FS2_ALLOC)
            return LOGNPRINTF(out,
                "exactly one of STATIC/LOCAL/ALLOC required, got 0x%x", kind);
    }

    /* 3. Buffer presence depends on the string kind.
     *    - STATIC / LOCAL: must have v.
     *    - ALLOC: v may be NULL only in the canonical empty state
     *      (len = 0, sz = 0) — a fresh FS2() before any allocation. */
    if (!s->v) {
        if (s->flags & FS2_ALLOC) {
            if (s->len == 0 && s->sz == 0)
                return true;    /* canonical empty FS2() */
            return LOGNPRINTF(out, "null buffer but len=%zu, sz=%zu",
                              s->len, s->sz);
        }
        return LOGNPRINTF(out, "null buffer for STATIC/LOCAL string");
    }

    /* 4. Capacity invariant. */
    if (s->len >= s->sz)
        return LOGNPRINTF(out, "len [%zu] must be < sz [%zu]", s->len, s->sz);

    /* 5. Consistency with C-string content. */
    size_t len = strlen(s->v);
    if (len != s->len)
        return LOGNPRINTF(out, "strlen [%zu] != len [%zu]", len, s->len);

    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Tech printer
 * ───────────────────────────────────────────────────────────────────────── */
/**
 * @brief Print a one-line technical dump of an fs2 to a stream.
 *
 * Outputs: len, capacity, flags (decimal), buffer pointer, and up to
 * 100 characters of content (truncated with `...` if longer).
 * If @p s is NULL, prints `<NULL>`.
 *
 * @param out   Output stream; if NULL, nothing is written.
 * @param s     fs2 to print (may be NULL).
 * @param name  Label printed before the values.
 * @return      Number of characters written (0 if @p out is NULL).
 */
long
fs2_techfprint(FILE *restrict out, const fs2 *restrict s, const char *restrict name) {
    if (!out)
        return 0;

    /* Snapshot under NO lock — all fields are read once, no re-read */
    char   buf[512];
    int    n;

    if (s) {
        const char *v   = s->v ? s->v : "(nil)";
        size_t      len = s->v ? MIN(FS_TECH_PRINT_COUNT, s->len) : 0;
        int         ell = (s->v && FS_TECH_PRINT_COUNT < s->len) ? 1 : 0;

        n = snprintf(buf, sizeof buf,
                     "FS2: %s: len [%zu], sz [%zu], flags [%d], s(%p)[%.*s%s]\n",
                     name, s->len, s->sz, (int)s->flags, (void *)v,
                     (int) len, v,
                     ell ? "..." : "");
    } else {
        n = snprintf(buf, sizeof buf, "FS2: %s: <NULL>\n", name);
    }

    if (n < 0)
        return -1;

    /* One fprintf call → atomic on the FILE* level (POSIX);
     * no global mutex needed, no deadlock in crash handlers. */
    int w = fprintf(out, "%s", buf);
    return (w < 0) ? -1 : (long)w;
}

/**
 * @brief Technical equality: len + flags + content must all match.
 *
 * Fast path: same pointer or both empty → true.
 * Slow path: compare len, flags, then memcmp up to len bytes.
 *
 * See fs2.h for the full contract.
 */
bool    
fs2_techcmp(const fs2 *s1, const fs2 *s2) {
    if (s1 == s2)
        return true;

    invfatal(ERR_NULL_INPUT, s1 != NULL && s2 != NULL,
        "Null pointers %p %p", s1, s2);

    if (s1->len != s2->len || s1->flags != s2->flags)
        return false;

    if (s1->len == 0)
        return true;

    return memcmp(s1->v, s2->v, s1->len) == 0;
}

long
fs2_techdump(FILE *restrict out, const fs2 *restrict s, const char *restrict name) {
    if (!out)
        return 0;

    const char *n = name ? name : "(unnamed)";
    long        total = 0;
    int         w;

    if (s) {
        if (s->v)
            w = fprintf(out, "FS2: %s: len [%zu], sz [%zu], flags [%d], s(%p)[",
                n, s->len, s->sz, (int)s->flags, (void *)s->v);
        else
            w = fprintf(out, "FS2: %s: len [%zu], sz [%zu], flags [%d], s(nil)[",
                        n, s->len, s->sz, (int)s->flags);
        if (w < 0) return -1;
        total += w;

        /* Write content verbatim. Loop handles partial fwrite
         * (possible on interrupted/piped streams). */
        if (s->v && s->len > 0) {
            const char *p   = s->v;
            size_t      rem = s->len;
            while (rem > 0) {
                size_t put = fwrite(p, 1, rem, out);
                if (put == 0) return -1;   /* short write / error */
                p     += put;
                rem   -= put;
                total += (long)put;
            }
        }

        w = fprintf(out, "]\n");
        if (w < 0) return -1;
        total += w;
    } else {
        w = fprintf(out, "FS2: %s: <NULL>\n", n);
        if (w < 0) return -1;
        total += w;
    }

    return total;
}

bool
fs2_techdump_file(const fs2 *restrict s, const char *restrict name, const char *restrict path) {
    if (!path)
        return false;

    const char *n = name ? name : "(unnamed)";

    FILE *f = fopen(path, "wb");
    if (!f) {
        logsimple("fs2_techdump_file: cannot open '%s' for writing", path);
        return false;
    }

    long written = fs2_techdump(f, s, n);

    /* CRITICAL: fclose can fail even when fwrite succeeded (buffered I/O
     * flush may fail on disk-full / broken pipe). Both must be checked. */
    int closed = fclose(f);

    if (written < 0 || closed != 0) {
        logsimple("fs2_techdump_file: write failed for '%s' (wrote %ld)",
                  path, written);
        return false;
    }

    logsimple("fs2_techdump_file: %s: wrote %ld bytes to '%s'",
              n, written, path);
    return true;
}

const char *
fs2_techdump_emergency(const fs2 *restrict s, const char *restrict name) {

    static _Thread_local char     path[FS2_DUMP_FILE_MAX_SIZE];
    static _Thread_local unsigned seq  = 0;

    /* 1. Choose directory: FS2_DUMP_DIR > TMPDIR > /tmp */
    const char *dir = getenv("FS2_DUMP_DIR");
    if (!dir || !*dir) dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    /* 2. Sanitize name -> [A-Za-z0-9._-], everything else -> '_' */
    char safe[64] = "unnamed";
    if (name && *name) {
        size_t i = 0;
        for (; name[i] && i < sizeof safe - 1; i++) {
            unsigned char c = (unsigned char)name[i];
            safe[i] = (isalnum(c) || c == '.' || c == '_' || c == '-')
                      ? (char)c : '_';
        }
        safe[i] = '\0';
    }

    /* 3. Compose path. Refuse overflow rather than truncate (bad path > crash). */
    int n = snprintf(path, sizeof path,
                     "%s/fs2dump-%s-%lu-%ld-%u.bin",
                     dir, safe,
                     (unsigned long)getpid(),
                     (long)time(NULL),
                     seq++);
    if (n < 0 || (size_t)n >= sizeof path)
        return NULL;

    /* 4. Open + write + close, all checked */
    FILE *f = fopen(path, "wb");
    if (!f) {
        logsimple("fs2_techdump_emergency: cannot open '%s'", path);
        return NULL;
    }

    long written = fs2_techdump(f, s, name);
    int  closed  = fclose(f);   /* fclose can fail even after fwrite success */

    if (written < 0 || closed != 0) {
        logsimple("fs2_techdump_emergency: write failed for '%s' (wrote %ld)",
                  path, written);
        return NULL;
    }

    logsimple("fs2_techdump_emergency: %s: wrote %ld bytes to '%s'",
              name ? name : "(unnamed)", written, path);

    return path;
}
