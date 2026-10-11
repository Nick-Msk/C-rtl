#include <criterion/criterion.h>
#include "checker.h"

/*
 * Trivial smoke test so `make test` succeeds out of the box.
 * Delete this file's body when you add real tests.
 */

Test(checker, passthrough_true) {
    cr_assert(checker_passthrough(true));
}

Test(checker, passthrough_false) {
    cr_assert_not(checker_passthrough(false));
}

