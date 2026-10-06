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

