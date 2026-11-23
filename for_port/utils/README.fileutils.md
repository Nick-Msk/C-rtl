# File Utilities (`fileutils.h`)

> [!WARNING]
> ### ⚠️ DEPRECATED
> **This module is in the process of being phased out.** 
> Most functionality is being migrated to the **`ds` (Data Source)** and **`ds_adapter`** modules. This header is maintained for backward compatibility with existing codebases during the transition.

## Overview

`fileutils.h` provides a suite of tools for file stream manipulation, pattern-based reading, and line-oriented processing. The module is currently split into two main paradigms: an optimized **`fs` (fast-string) API** and a **Legacy C-style API**.

## API Categories

### 1. Fast-String (`fs`) Based API
This is the primary (but migrating) way to handle files. It uses the `fs` (fast-string) type for efficient memory management and string operations.

| Function | Description |
| :--- | :--- |
| `fgetline_cmn_fs(...)` | Core function for reading lines into an `fs` object. |
| `readfs_file(FILE*)` | Reads an entire file into a single `fs` object. |
| `freadlines(FILE*, fs**)` | Reads a file into an array of `fs` objects (one per line). |
| `fwritelines(FILE*, fs*, int)` | Writes multiple `fs` objects to a stream. |
| `fprint_file(FILE*, FILE*)` | Copies content from one file stream to another. |

### 2. Pattern-Based Reading
Provides tools to search for specific patterns or templates within a file stream.

| Function | Description |
| :--- | :--- |
| `fread_pattern(...)` | Reads a file until a specific pattern is found. |
| `fread_pattern_printf(...)`| Reads a file using a printf-style dynamic pattern. |

**Macros for Pattern Handling:**
* `FUSKIPFORMAT_RAISE`: Attempts to match a pattern; if it fails, it triggers an error via the `error.h` system.
* `FUGETUNSIGNED_RAISE`: A specialized macro for strict unsigned integer reading.

### 3. Legacy C-Style API (Obsolete)
These functions use standard `char*` buffers and should be avoided in new development.

| Function | Description |
| :--- | :--- |
| `get_line(char*, int)` | Classic C-style line reading. |
| `read_from_file(FILE*, int*)` | Reads a file into a `malloc`'d buffer (caller must `free`). |

### 4. Strict Input & Metadata
| Function | Description |
| :--- | :--- |
| `fstrict_scanf(...)` | A strict version of `scanf` with improved error detection. |
| `getfilesize(FILE*)` | Returns the size of a file in bytes (`off_t`). |

## Migration Guide

When moving from `fileutils.h` to the new system:

1.  **Replace `fs` operations** with the `DS` (Data Source) API.
2.  **Replace pattern matching** with the `ds_adapter` pattern matching functions.
3.  **Avoid all functions** marked as "Legacy" or returning `char*` (to prevent memory management errors).

## Dependencies
- `fs.h` (Fast-string implementation)
- `error.h` (Error raising)
- `stdbool.h`, `stdio.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       