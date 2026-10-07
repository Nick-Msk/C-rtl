/**
 * @file bool.c
 * @brief Version array and functions.
 */

#include <bool.h>
#include <stddef.h>

static const char *const versions[] = {
    BOOL_VERSION,
    "0.1.0",
    NULL
};

const char *const *bool_versions(void) {
    return versions;
}

const char *bool_version(void) {
    return versions[0];
}
