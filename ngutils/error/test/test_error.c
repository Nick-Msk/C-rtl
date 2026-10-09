#include <criterion/criterion.h>
#include <error.h>

/* -------------------------------------------------------------------------
 * Version
 * ------------------------------------------------------------------------- */

Test(error, version_string_matches_macro) {
    cr_assert_str_eq(err_version(), ERROR_VERSION);
    cr_assert_str_eq(err_version(), "0.1.0");
}

Test(error, version_is_top_of_list) {
    const char *const *v = err_versions();
    cr_assert_not_null(v);
    cr_assert_not_null(v[0]);
    cr_assert_str_eq(v[0], err_version());
    cr_assert_str_eq(v[0], ERROR_VERSION);
}

Test(error, versions_terminated_by_null) {
    const char *const *v = err_versions();
    size_t n = 0;
    while (v[n]) ++n;
    cr_assert_geq(n, 1);
    cr_assert_null(v[n]);
}
