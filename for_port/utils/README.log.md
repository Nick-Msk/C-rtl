# Logging Engine

A high-performance, programmable, and hierarchical logging system for C. 

Unlike standard logging libraries that simply print messages, this engine is designed to integrate deeply into code execution. It supports hierarchical function tracking (entry/exit), module-specific verbosity, automatic type formatting, and "action-based" logging, all while providing a **zero-overhead** mode for production.

## 🚀 Key Features

* **Hierarchical Logging:** Uses a scope-based approach (`logenter` / `logret`) to track function entry and exit, creating a logical trace of execution.
* **Programmable Actions:** Execute code *inside* a log call using "Action" macros (e.g., `logact`), allowing you to perform operations and log their results in a single atomic-looking statement.
* **Module-Based Control:** Assign specific log levels to different software modules. You can turn off logging for specific parts of the system without affecting others.
* **Zero-Cost Production Mode:** Define `NODEBUG` to strip all logging overhead entirely from your production binary.
* **Automatic Type Formatting:** Uses C11 `_Generic` to automatically detect and format variable types (int, double, string, etc.) without manual format specifiers.
* **Structured Formats:** Supports multiple log formats (Function-only, Time-only, Minimalist, or full Verbose).

## 🏗 Core Concepts

### 1. Log Levels & Actions
The system distinguishes between **what** is being logged and **how** the log is behaving.

* **Log Levels (`Loglevel`):** `LOGOFF`, `LOGERR`, `LOGWARN`, `LOGALL`.
* **Log Actions (`LogAction`):** 
    * `LOG_ENTER` / `LOG_LEAVE`: Used for function tracing.
    * `LOG_MSG`: Standard message logging.
    * `LOG_SIMPLE`: Minimalist, fast logging for non-critical events.

### 2. Hierarchical Tracing
The library uses a local variable `_LG_LV` (Log Level Variable) to manage nested function calls. This allows the logger to know exactly which function it is inside and whether it should log a message based on the current nesting level.

### 3. Automatic Formatting (`logautotype`)
The engine eliminates the need for manual `printf` format strings for common types. It automatically detects the type and applies the correct specifier.

## 📖 Usage Guide

### Basic Logging
Standard logging for messages and errors.

```c
// Simple message
logmsg("Operation started");

// Error logging with return code
return logerr(ERR_FAILED, "Critical error occurred: %s", error_msg);
```

### Hierarchical (Function Tracing)
The most powerful way to track execution flow.

```c
int complex_function(int a, int b) {
    // Initializes the logging scope for this function
    logenter("Entering complex_function with a=%d, b=%d", a, b);

    if (a + b < 0) {
        // Logs error AND returns the error code immediately
        return logerr(ERR_OUT_OF_RANGE, "Sum is negative");
    }

    int result = a + b;

    // Logs the exit and returns the result
    return logret(result, "Function completed successfully");
}
```

### Action-Based Logging
Perform a task and log the result in one line.

```c
// Executes 'i++' and logs the result
logact(i++, "Counter incremented");

// Executing a complex action and returning
return logactret(status_code, "Processing failed during step: %d", error_id);
```

### Automatic Type Logging
No more `%d` or `%f`. Just pass the variable.

```c
int x = 42;
double y = 3.14;

// Automatically detects %d and %f
logauto(x);
logauto(y);
```

## 🛠 Production Optimization

In production, you want maximum performance. By defining `NODEBUG` before including the header, all logging calls are replaced by `nop` or simple return statements, resulting in **zero CPU cycles** wasted on logging.

```c
#define NODEBUG
#include "log.h"
```

## 📂 API Summary

| Macro / Function | Description |
| :--- | :--- |
| `loginit(...)` | Initializes the logging engine and module list. |
| `logenter(fmt, ...)` | Marks the start of a function scope for tracing. |
| `logret(ret, fmt, ...)` | Logs the exit of a function and returns a value. |
| `logerr(...)` | Logs an error level message. |
| `logmsg(fmt, ...)` | Standard message logging. |
| `logact(ACT, fmt, ...)` | Executes an action and logs the outcome. |
| `logauto(val)` | Automatically detects type and logs the value. |
| `logsimple(...)` | High-speed, low-overhead minimal logging. |

## ⚠️ Safety Warning
* **Scope:** Be careful with `logenter` / `logret`. Always ensure every `logenter` is paired with a `logret` or `logtype` to maintain the correct nesting level.
* **Module Initialization:** Always call `log_modinit` with your module list to enable module-level filtering.

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       