#include <criterion/criterion.h>
#include "skeleton.h"

/*
 * Trivial smoke test so `make test` succeeds out of the box.
 * Delete this file's body when you add real tests.
 */

Test(skeleton, passthrough_true) {
    cr_assert(skeleton_passthrough(true));
}

Test(skeleton, passthrough_false) {
    cr_assert_not(skeleton_passthrough(false));
}

