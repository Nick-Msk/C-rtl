# Common Utilities (`common.h`)

> [!NOTE]
> `common.h` is a multi-purpose utility header providing a collection of macros and inline functions. It is designed to support various subsystems of the project. Due to its broad scope, many functions here may be relocated to specialized modules (e.g., `math.h`, `string_utils.h`) as the project matures.

## Overview

This header provides "quality of life" tools for C development, including:
*   **Advanced Macros:** Expression evaluation, stringification, and type-generic debugging.
*   **String & Char Utilities:** Parsing, searching, and character manipulation.
*   **Mathematical Helpers:** Bitwise operations, power-of-two rounding, and growth strategies.
*   **Data Manipulation:** Comparators, exchangers (swapping), and fillers.
*   **Randomization:** High-level wrappers for random number generation.
*   **Error & IO Handling:** Robust macros for checking return codes and handling I/O errors.

---

## 1. Core Macros & Debugging

### Expression Macros
* `MIN(x, y)` / `MAX(x, y)`: Type-safe selection of minimum/maximum values (using GCC statement expressions).
* `LEAST(a, ...)` / `GREATEST(a, ...)`: Finds the min/max among a variable list of arguments.
* `COUNT(arr)`: Returns the number of elements in a static array.
* `STRINGIFY(x)` / `TOSTRING(x)`: Preprocessor stringification.
* `IS_COMPATIBLE(x, T)`: Compile-time type checking using `_Generic`.

### Debugging Tools
* `TYPEFORMAT(x)`: A powerful `_Generic` macro that provides a format string corresponding to the type of `x`.
* `typeprint(a)`: Prints a variable with its automatically detected type format (enabled only when `NDEBUG` is not defined).
* `DUMMY`: A no-op macro for empty statements.

---

## 2. Data Manipulation

### Comparators
Provides highly optimized comparison functions for various primitive types (`int`, `double`, `char`, `void*`, etc.). 
* Includes specialized `compare_dbl` with `NaN` (Not-a-Number) logic.
* Supports pointer-based comparators (e.g., `pchar_cmp`, `pdbl_cmp`) for use with `qsort`.

### Exchangers (Swappers)
* `char_exch`, `int_exch`, `double_exch`, `ptr_exch`: Type-specific variable swapping.
* `item_exch(v1, v2, sz)`: A generic memory exchanger capable of swapping items of any size using a stack buffer or heap allocation.

### Fillers
Functions to initialize arrays or memory blocks with specific values (`fill_int`, `clean_double`, `clean_ptr`, etc.).

---

## 3. String & Character Processing

* `countstrings(p)`: Counts non-NULL pointers in a NULL-terminated array of strings.
* `skip_leading_spaces(str)`: Removes leading whitespace from a string.
* `reverse(s, len)` / `reversel(s)`: Reverses a C-string.
* `strisempty(str)`: Safely checks if a string is NULL or empty.
* `isalpha_u`, `isalnum_u`, `isdigit_signed`: Augmented character class checks (e.g., including `_` or signs).

---

## 4. Mathematical & Bitwise Utilities

* `round_up_2(val)`: Calculates the next power of two strictly greater than `val`. Uses optimized `std::bit_ceil` if C23 is available.
* `calcnewsize(strategy, n)`: Predicts the next allocation size based on growth strategies (`SIZE_NONE`, `SIZE_MIN10`, `SIZE_POWER2`).
* `cycleinc(val, cycle)`: Increment with modulo-like cycling.
* `bits_str` / `fprint_bits`: Utilities for binary representation and printing.

---

## 5. Random Number Generation

Provides wrappers for `rand()` and `drand48()` to simplify generating different types of data:
* `rndint(max)`, `rnduint(max)`, `rndlong(max)`, `rnddouble(max)`.
* Character helpers: `rndlowchar()`, `rndupperchar()`, `rnddigitchar()`.

---

## 6. IO & Error Handling Macros

A collection of "guard" macros to simplify error propagation in C:
* `WRITE_OR_RET(cmd, ret)`: Executes a command; if it fails (returns < 0), logs the error and returns immediately.
* `WRITE_OR_RET_ACTION(cmd, ret, act)`: Executes a command; if it fails, performs an action (e.g., cleanup) and returns.
* `IOCHECKER(w, cmd, ret)`: A loop-based guard for iterative I/O operations.

---

## ⚠️ Deprecated APIs

> [!WARNING]
> The following functions are **obsolete** and have been moved to the `ds_adapter` API. Do not use them in new code:
> - `try_parse_int`, `try_parse_double`, etc.

## Dependencies
* `stdlib.h`, `string.h`, `limits.h`, `ctype.h`, `time.h`, `math.h`, `sys/errno.h`
* `bool.h`, `log.h`

## License
GNU GENERAL PUBLIC LICENSE
                       Version 3, 29 June 2007
                       