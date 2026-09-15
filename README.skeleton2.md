# Reference Implementation: CLI Skeleton (`skeleton2.c`)

`skeleton2.c` serves as a complete, functional template for building command-line applications using the `kr_book` utility suite. It demonstrates the orchestration of error handling, logging, argument parsing, and sanity checking in a unified workflow.

## Architectural Workflow

The application follows a structured lifecycle to ensure stability and observability:

1.  **Initialization:** The logging system is started (`logsimpleinit`).
2.  **Configuration Parsing:** The `parse_keys` function processes command-line arguments using the `parse_keys.h` macros, populating a `Keys` configuration structure.
3.  **Validation:** The `checker.h` module is used to perform "sanity checks" (e.g., `fs_alloc_check`) to ensure the environment is stable before executing core logic.
4.  **Execution & Logging:** The program executes the main logic and uses `logret` to ensure all operations are logged before the program exits.

---

## Core Components in Action

### 1. Robust Argument Parsing
The `parse_keys` function demonstrates how to convert raw `argv` strings into a typed `Keys` structure. It supports:
* **Combined Flags:** Handles grouped arguments like `-vfs` (where `v` is a bool, `f` is a string, and `s` is a bool).
* **Value Association:** Correctly extracts values following a flag (e.g., `-f filename.txt`).
* **Error Reporting:** If an invalid argument is provided, it uses `userraise` to terminate the program with a descriptive error message.

```c
// Example of the parsing logic inside the switch
switch (tolower(c)) {
    parse_bool('v', version);
    parse_string('f', filename);
    // ...
}
```

### 2. Integrated Error Handling
Instead of simple `printf` error messages, the skeleton uses a multi-tiered error system:
* **Invariants:** `invraise` is used to ensure that the configuration structure (`ke`) is not null before proceeding.
* **User Errors:** `userraise` provides clean, formatted error messages for end-users when they provide incorrect command-line input.

### 3. Sanity & Debugging
The skeleton demonstrates how to use the `checker.h` module to perform runtime validation.
* **`fs_alloc_check`**: A non-interrupting check that logs a warning if memory allocation patterns look suspicious, without stopping the program execution.

### 4. Structured Logging
Every major step is wrapped in logging calls:
* `logenter("...")`: Marks the entry point of a function for stack tracing.
* `logret(0, "end...")`: Combines the return of a status code with a final log entry, ensuring the application lifecycle is fully traceable.

---

## How to Use This Skeleton as a Template

To build your own CLI tool:
1.  **Define your `Keys` struct** with all the parameters your program needs.
2.  **Implement `parse_keys`** using the `parse_X` macros for each field in your `Keys` struct.
3.  **Use the `log` and `error` macros** throughout your logic to ensure that every failure is recorded and every critical error is handled gracefully.
4.  **Incorporate `checker.h`** to perform non-critical sanity checks during development to catch subtle logic errors.

## Compilation
To compile the skeleton for development (with debugging enabled):
```bash
gcc skeleton2.c -o skeleton -I. -DDEBUG
```

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       