#include <criterion/criterion.h>
#include "bool.h"

Test(bool, str_true) {
    cr_assert_str_eq(bool_str(true), "true");
}

Test(bool, str_false) {
    cr_assert_str_eq(bool_str(false), "false");
}

Test(bool, distinct_literals) {
    cr_assert_neq(bool_str(true), bool_str(false));
}

Test(bool, stable_pointer) {
    /* строковые литералы уникальны в пределах единицы трансляции */
    cr_assert_eq(bool_str(true), bool_str(true));
    cr_assert_eq(bool_str(false), bool_str(false));
}

/* --- bool_tryparse --- */

Test(bool, parse_true) {
    bool ok = false;
    cr_assert(bool_tryparse("true", &ok));
    cr_assert(ok);
}

Test(bool, parse_false) {
    bool ok = true;
    cr_assert(bool_tryparse("false", &ok));
    cr_assert_not(ok);
}

Test(bool, parse_case_insensitive) {
    bool ok = false;
    cr_assert(bool_tryparse("True", &ok));  cr_assert(ok);
    cr_assert(bool_tryparse("FALSE", &ok)); cr_assert_not(ok);
    cr_assert(bool_tryparse("tRuE", &ok));  cr_assert(ok);
}

Test(bool, parse_rejects_garbage) {
    bool ok = true;
    cr_assert_not(bool_tryparse("maybe", &ok));
    cr_assert_not(bool_tryparse("", &ok));
    cr_assert_not(bool_tryparse(" true", &ok));   /* no trim */
    cr_assert_not(bool_tryparse("true ", &ok));   /* no trim */
    cr_assert_not(bool_tryparse("1", &ok));
    cr_assert_not(bool_tryparse("yes", &ok));
}

Test(bool, parse_does_not_touch_out_on_failure) {
    bool ok = true;
    bool_tryparse("garbage", &ok);
    cr_assert(ok);   /* untouched */
}

Test(bool, parse_null_str_fails) {
    bool ok = false;
    cr_assert_not(bool_tryparse(NULL, &ok));
}

Test(bool, parse_null_out_is_allowed) {
    /* valid input, NULL out: should succeed and just not write */
    cr_assert(bool_tryparse("true", NULL));
    cr_assert(bool_tryparse("false", NULL));
}

/* --- bool_parsedef --- */

Test(bool, parsedef_true) {
    cr_assert(bool_parsedef("true", false));
}

Test(bool, parsedef_false) {
    cr_assert_not(bool_parsedef("false", true));
}

Test(bool, parsedef_fallback) {
    cr_assert_not(bool_parsedef("garbage", false));
    cr_assert(bool_parsedef("garbage", true));
}

Test(bool, parsedef_null) {
    cr_assert(bool_parsedef(NULL, true));
    cr_assert_not(bool_parsedef(NULL, false));
}

/* --- roundtrip: bool_str ↔ bool_tryparse --- */

Test(bool, roundtrip) {
    bool ok = false;
    cr_assert(bool_tryparse(bool_str(true), &ok));  cr_assert(ok);
    cr_assert(bool_tryparse(bool_str(false), &ok)); cr_assert_not(ok);
}
