# Error Management System (`error.h`)

A robust, low-level error handling and exception management utility for C applications. It provides structured error codes, signal-to-string mapping, stack trace capabilities, and a macro-based "try/catch" mechanism using `setjmp` and `longjmp`.

## Key Features

* **Structured Error Codes:** A comprehensive enumeration of error and warning codes categorized by domain (Standard, File, System, Custom).
* **Exception Handling:** A `try()` macro implementation that allows non-local jumps to handle errors without manual propagation.
* **Signal Integration:** Built-in utility functions to map POSIX signals to human-readable strings and descriptions.
* **Stack Tracing:** Built-in support for printing stack traces to `stderr`.
* **High-level Macros:** Complex macros (`userraise`, `sysraise`, etc.) that combine logging, signal triggering, and error propagation in a single call.

## Data Structures

### `ErrorCode` (Enum)
Contains a vast range of pre-defined error and warning constants.
- **Core Errors:** `ERR_NULLABLE_PTR`, `ERR_OUT_OF_RANGE`, etc.
- **File Errors:** `ERR_UNABLE_OPEN_FILE`, `ERR_STREAM_ERROR`, etc.
- **Custom/Domain Errors:** `ERR_FS_NOT_ALLOC_FLAG`, `ERR_VALIDATION_FAILED`, etc.
- **Warnings:** `WARN_MEM_LEAK_DETECTED`.

### `ErrorType` (Enum)
Determines the origin of the error:
- `ERR_USER`: Errors originating from application logic.
- `ERR_SYS`: Errors originating from system-level operations (e.g., failures in syscalls).

### `ExceptionData` (Struct)
Internal structure used for the exception handling mechanism.
- `env`: `jmp_buf` for non-local jumps.
- `init_flag`: Boolean flag indicating if the exception environment is active.

## API Reference

### Core Functions
| Function | Description |
| :--- | :--- |
| `err_raise(...)` | Raises an error with a specific type, code, and variadic message. |
| `err_clean(bool)` | Cleans up error states. |
| `err_getexception_info()` | Returns a pointer to the current exception data. |
| `err_sethandler(sig_t)` | Sets a signal handler for the error system. |

### Utility Functions
| Function | Description |
| :--- | :--- |
| `err_printstacktrace()` | Prints the current stack trace to `stderr`. |
| `sig_str(int)` | Converts a POSIX signal to its string name (e.g., `SIGINT`). |
| `sig_str_desc(int)` | Converts a POSIX signal to its description. |

### Control Flow Macros
| Macro | Description |
| :--- | :--- |
| `try()` | Creates a safe execution block. If an error is "raised" inside, control jumps back to this point. |
| `userraise(...)` | Raises a user-level error (no signal triggered). |
| `userraiseint(...)`| Raises a user-level error and interrupts execution via `SIGINT`. |
| `sysraise(...)` | Raises a system-level error (automatically handles `errno`). |

## Usage Example

### Basic Error Raising
```c
#include "error.h"

void process_data(void *ptr) {
    if (ptr == NULL) {
        userraise(ERR_NULLABLE_PTR, "Pointer cannot be NULL");
    }
}
```

### Using the `try()` Mechanism
```c
#include "error.h"
#include <stdio.h>

void risky_operation() {
    // If something goes wrong, we jump out of this function
    userraiseint(ERR_OUT_OF_RANGE, "Value is out of bounds!");
}

int main() {
    // Initialize exception environment
    if (err_resetenv()) {
        printf("Environment initialized.\n");
    }

    try {
        printf("Starting risky operation...\n");
        risky_operation();
    } else {
        // This block executes if a 'userraiseint' occurred
        printf("Caught an exception! Handling error...\n");
    }

    return 0;
}
```

## Implementation Notes

> [!WARNING]  
> This module uses `setjmp` and `longjmp`, which can lead to unexpected behavior if used incorrectly (e.g., jumping over variable allocations or exiting functions without cleaning up local resources). Use with caution in complex systems.

## Dependencies
- `stdio.h`, `stdbool.h`, `signal.h`, `setjmp.h`, `string.h`, `sys/errno.h`.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       