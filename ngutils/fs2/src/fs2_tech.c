#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "fs2.h"

enum {FS2_DUMP_FILE_MAX_SIZE = 8192 };

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
