# Invariant & Assertion Engine (`checker.h`)

> [!IMPORTANT]
> `checker.h` is an advanced assertion and invariant validation engine. Unlike the standard C `assert.h`, it allows for sophisticated error handling, including value comparison, detailed logging, and the ability to choose between halting execution or returning a boolean.

## Overview

This module is designed for **Contract-Based Programming**. It allows developers to define "invariants" (conditions that must always be true) and "pre-conditions/post-conditions" with two distinct modes of operation:

1.  **Interrupt Mode (Raise):** If a condition fails, the program logs the error and immediately triggers a `SIGINT` (via the error system) to halt execution. Use this for fatal logic errors.
2.  **Silent/Return Mode (Non-interrupting):** If a condition fails, the program logs the error but returns a `bool`. This allows the calling code to handle the error gracefully without crashing.

## Features

*   **Zero-Overhead Mode:** By defining `NOINVARIANT`, all macros resolve to simple boolean expressions, allowing you to strip all checking logic from production builds.
*   **Value-Comparison Validation (`inv2` series):** Compares the actual result of an expression against an expected value. If they differ, it logs both the actual and the expected values, providing invaluable debugging data.
*   **Integrated Logging:** Automatically integrates with the project's `log` and `error` modules.
*   **Command-line Validation:** Includes lightweight macros for validating `argc`.

## API Reference

### Basic Invariants
| Macro | Mode | Behavior on Failure |
| :--- | :--- | :--- |
| `inv(expr, msg, ...)` | **Silent** | Logs `msg` and returns `false`. |
| `invraise(expr, msg, ...)` | **Interrupt** | Logs `msg` and triggers `SIGINT`. |
| `invraisecode(expr, errcode, msg, ...)` | **Interrupt** | Logs `msg` and triggers `SIGINT` with a specific `ErrorCode`. |

### Value-Comparison Invariants (Advanced)
*Used when you need to know **why** a condition failed by seeing the mismatched values.*

| Macro | Mode | Behavior on Failure |
| :--- | :--- | :--- |
| `inv2(expr, val, msg, ...)` | **Silent** | Checks if `expr == val`. Logs mismatch and returns `false`. |
| `inv2raise(expr, val, msg, ...)`| **Interrupt** | Checks if `expr == val`. Logs mismatch and triggers `SIGINT`. |

### Argument Checking
| Macro | Description |
| :--- | :--- |
| `check_arg(argcmax, msg, ...)` | Validates if `argc` is within the allowed limit. Returns `1` if OK, `0` otherwise. |

## Usage Example

### Graceful Error Handling (Silent Mode)
```c
#include "checker.h"

bool process_data(int input) {
    // If input is negative, log error and return false, but DON'T crash.
    inv(input >= 0, "Input must be non-negative. Received: %d", input);
    
    // ... logic ...
    return true;
}
```

### Debugging with Value Comparison (Interrupt Mode)
```c
#include "checker.h"

void complex_calculation(int a, int b) {
    int result = a * b;
    
    // If result is not equal to expected, log both values and crash immediately.
    // This is perfect for catching mathematical edge cases.
    inv2raise(result == (a * b), a * b, "Calculation mismatch! Expected %d, got %d", a, b);
}
```

### Production Build (Zero Cost)
To remove all checking overhead in your production release, define the `NOINVARIANT` macro:
```bash
gcc -DNOINVARIANT main.c -o my_app
```

## Dependencies
- `error.h` (for signal triggering and error codes)
- `log.h` (for logging invariant violations)
- `common.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       