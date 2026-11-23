# Argument Parsing Macros (`parse_keys.h`)

> [!WARNING]
> ### ⚠️ Context-Dependent Macros
> These macros are **not standalone functions**. They are designed to be used exclusively within a `switch` statement inside a parsing loop. They rely on specific local variables being present in the scope where they are called.

## Overview

`parse_keys.h` provides a set of specialized macros to simplify the parsing of command-line arguments. It supports both "separated" and "attached" argument formats:
* **Separated:** `-option value`
* **Attached:** `-optionvalue`

The macros automatically handle type conversion (for integers and longs), boolean flags, and error reporting if a required value is missing.

---

## Requirements for Use

To use these macros, the following context must be available within the scope of the `switch` statement:

1.  **`ke`**: A pointer to a structure (typically a `Keys` struct) where the parsed values will be stored. The members of this structure must match the names passed to the macros.
2.  **`argv`**: An array of strings (typically `char **`) representing the command-line arguments.
3.  **`params`**: An integer counter used to track the number of successfully parsed parameters.
4.  **`userraise`**: An error-handling function (from `error.h`) to report missing arguments.

---

## Supported Types

| Macro | Supported Type | Description |
| :--- | :--- | :--- |
| `parse_string(c, name)` | `char*` | Parses a string. |
| `parse_int(c, name)` | `int` | Parses an integer (via `atoi`). |
| `parse_long(c, name)` | `long` | Parses a long integer (via `atol`). |
| `parse_bool(c, name)` | `bool` | Sets the key to `true`. |
| `parse_bool_false(c, name)` | `bool` | Sets the key to `false`. |

---

## Usage Example

The following example demonstrates how to implement a command-line parser using these macros.

```c
#include "parse_keys.h"
#include "error.h"
#include <stdio.h>

// Structure to hold parsed keys
typedef struct {
    char *name;
    int  count;
    bool verbose;
    int  port;
} AppKeys;

void parse_args(int argc, char **argv) {
    AppKeys ke = { .name = "Config", .count = 0, .verbose = false, .port = 0 };
    int params = 0;

    // The macros must be inside a switch statement
    for (int i = 1; i < argc; i++) {
        char flag = argv[i][1]; // Assumes flags are single characters like '-v'

        switch (flag) {
            case 'v':
                parse_bool('v', &ke.verbose);
                break;

            case 'p':
                // Parses integer. Supports "-p 8080" or "-p8080"
                parse_int('p', &ke.port);
                break;

            case 'n':
                // Parses string. Supports "-n filename" or "-nfilename"
                parse_string('n', &ke.name);
                break;

            default:
                // Handle unknown flags
                break;
        }
    }

    printf("Parsed: Name=%s, Port=%d, Verbose=%s\n", 
            ke.name, ke.port, ke.verbose ? "true" : "false");
}

int main(int argc, char **argv) {
    parse_args(argc, argv);
    return 0;
}
```

## Design Notes

* **Efficiency:** The macros use `static inline` where possible and avoid unnecessary computations.
* **Error Handling:** If an expected value is missing (e.g., `-p` is the last argument), the `parse_value` macro will trigger a `userraise` with a detailed error message.
* **Complexity:** Time complexity for parsing is $O(N)$ where $N$ is the number of arguments.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       