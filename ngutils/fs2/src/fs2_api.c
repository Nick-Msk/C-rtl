#include "fs2_api.h"

fs2*             
fs2_reverse(fs2 *str) {
    invraise(str != NULL, "Null pointer");

    if (fs2_is_static(str))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Cannot modify STATIC fs2 in-place");

    if (str->len > 0UL) {
        size_t i = 0UL, j = str->len - 1;
        while (i < j)
            char_exch(str->v + i++, str->v + j--);
    }
    return str;
}

void             
fs2_partialsort(fs2 *str, size_t from, size_t to, bool asc) {
    invraise(str != NULL, "Null pointer");

    if (fs2_is_static(str))
        userraise(0, ERR_UNSUPPORTED_TYPE, "Cannot sort STATIC fs2 in-place");
    qsort(str->v + from, to - from, 1, asc ? pchar_cmp: pchar_revcmp);
}
