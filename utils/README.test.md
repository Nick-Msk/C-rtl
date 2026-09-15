# Testing Framework (`test.h` & `test.c`)

> [!WARNING]
> ### 🛠 REFACTORING REQUIRED
> This module is a **legacy component** currently undergoing architectural redesign. It is intended for testing complex logic, stateful systems, and dependency-based test suites. Many parts of the implementation (especially the global state for subtests) are slated for removal/refactoring.

## Overview

This is a specialized testing framework designed for complex scenarios where simple `assert()` calls are insufficient. It provides a "Command-pattern" approach to testing, allowing for:

* **Dependency-Aware Testing:** Tests can depend on the success of previous tests (`dep_list`).
* **Subtesting:** Hierarchical testing where a single test can contain multiple sub-steps.
* **Exception Handling:** Integration with the project's exception system to catch crashes (via `setjmp`) and mark them as `TEST_FAILED_EXCEPTION`.
* **Rich Reporting:** Detailed statistics on passed, failed, skipped, and exception-based errors.
* **Flexible Comparisons:** High-level comparison macros for strings, files, and pattern matching (`like`, `ulike`).

---

## Core Concepts

### 1. Test Units (`Utest`)
The primary object being tested. It contains:
* `name` / `desc`: Identification and description.
* `num`: Logical sequence number (used for sorting and dependencies).
* `mandatory`: If `true`, a failure in this test halts the entire suite.
* `dep_list`: Array of IDs of tests that must pass before this test is allowed to run.
* `f2`: The actual function containing the test logic.

### 2. Test Status (`TestStatus`)
| Status | Description |
| :--- | :--- |
| `TEST_NOT_RUN` | Initial state. |
| `TEST_PASSED` | Test finished successfully. |
| `TEST_SKIPPED` | Test was skipped because a dependency failed. |
| `TEST_FAILED` | Test logic failed (assertion failed). |
| `TEST_MANUAL` | Test finished, but requires manual inspection of logs. |
| `TEST_FAILED_EXCEPTION`| Test caused a system exception (e.g., SIGINT). |

---

## API Reference

### Test Execution
| Function | Description |
| :--- | :--- |
| `test_engine2(tests, num, out)` | The core runner. Executes a suite of tests and returns a `TestRes` summary. |
| `test_getnextnum()` | Incremental counter to assign unique IDs to tests. |

### Verification Macros
These macros are designed to be used inside test functions to validate conditions.

* **Basic Assertions:**
    * `test_validate(expr, msg, ...)`: If `expr` is false, fails the test and logs `msg`.
    * `test_validatefree(expr, action, msg)`: Fails the test and executes an `action` (e.g., cleanup).
* **Command/Action Assertions:**
    * `test_act_equal(ACTION, tf, pt)`: Performs `ACTION`, then compares file/string `tf` against pattern `pt`.
    * `test_act_like(ACTION, tf, pt)`: Performs `ACTION`, then performs a "fuzzy" pattern match.
* **Subtests:**
    * `test_sub(msg, ...)`: Marks the start of a sub-step within a test.

### File & String Comparison
* `test_str_equal(src, sz, pattern)`: Strict string comparison.
* `test_str_like(src, sz, pattern)`: Pattern matching (`strstr` style).
* `test_file_equal(tf, from, to, pt)`: Compares file content within a range.

---

## Usage Example

```c
#include "test.h"
#include <stdio.h>

// Example of a test function
static TestStatus test_complex_logic(const char *name) {
    // 1. Subtest
    test_sub("Checking initial state");
    test_validate(1 == 1, "Math is broken");

    // 2. Action with comparison (Checks if an action results in expected output)
    // This is useful for testing functions that print to a file
    test_act_equal(
        printf("Hello World"), // ACTION
        stdout,                // Target stream
        "Hello World"          // Expected pattern
    );

    return TEST_PASSED;
}

int main(int argc, char **argv) {
    // Define a test suite
    Utest suite[] = {
        testnew(.f2 = test_complex_logic, .name = "Logic Test", .mandatory = true),
        testnew(.f2 = test_complex_logic, .name = "Logic Test 2", .dep_list = {1}), // Depends on test #1
    };

    // Run the engine
    TestRes res = test_engine2(suite, 2, stdout);

    return res.passed ? 0 : 1;
}
```

## Implementation Notes

> [!IMPORTANT]
> The current implementation of `test_sub` and `test_engine2` relies on **global static state** (`g_out`, `g_prev_subtest`). This makes the engine non-thread-safe. A planned refactoring will move these into a context-based structure.

## Dependencies
* `bool.h`
* `error.h`
* `log.h`
* `common.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       