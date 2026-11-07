#include "fs2array_api.h"
#include "fs2_api.h"

fs2array
fs2_split_toarray(const fs2 *restrict s, const fs2 *restrict delim) {
    invfatal(ERR_NULL_INPUT, s != NULL && delim != NULL,
             "Null pointers %p %p", s, delim);

    fs2array a = FS2ARR();

    /* Empty delimiter: no split — whole string is one part (even empty). */
    if (fs2_is_empty(delim)) {
        fs2 part = fs2_clone(s);              /* empty → FS2EMPTY, else copy */
        fs2arr_push(&a, &part);
        return a;
    }

    /* Non-empty delimiter: split at each occurrence, keep empty parts. */
    size_t    pos = 0;
    ptrdiff_t hit;
    while ((hit = fs2_instr_from(s, delim, pos)) >= 0) {
        fs2 part = fs2_substr_new(s, pos, (size_t) hit - pos);
        fs2arr_push(&a, &part);               /* part → FS2EMPTY */
        pos = (size_t) hit + delim->len;       /* skip delim: non-overlapping */
    }

    /* Final tail: [pos, s->len). Always pushed — even when empty.
     * For "a,b" → parts ["a","b"]. For "a," → ["a",""].
     * For empty s → [""]. This matches Python split() and fs2_split_part. */
    fs2 tail = fs2_substr_new(s, pos, s->len - pos);
    fs2arr_push(&a, &tail);

    return a;
}
