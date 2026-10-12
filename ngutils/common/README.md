# common.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

A small set of cross-utility C helpers. Deliberately minimal — grows
only when a consumer actually needs a new symbol, not in advance.

Depends on `log`.

## Contents

| Group | Symbols |
|---|---|
| Macros | `MIN`, `MAX`, `CASE_RETURN` |
| Sentinel | `G_GLOB_AVERAGE` |
| Growth | `Tincrease`, `round_up_2`, `calcnewsize` |
| Character | `clower`, `cupper` |
| Random | `rndint`, `rndlowchar`, `rndupperchar`, `rnddigitchar` |
| C-string | `reverse` |
| Parsing | `try_parse_int`, `try_parse_long`, `try_parse_ulong`, `try_parse_double` |
| Comparators | `pchar_cmp`, `pchar_revcmp` (for `qsort` / `bsearch`) |

## Usage

```c
#include <common.h>

int main(void) {
    printf("min = %d\n", MIN(3, 5));            /* 3 */

    int x;
    if (try_parse_int("42", &x))
        printf("parsed: %d\n", x);

    unsigned long cap = round_up_2(100);        /* 128 */
    long sz = calcnewsize(SIZE_POWER2, 100);    /* 128 */

    char buf[] = "hello";
    reverse(buf, 5);                            /* "olleh" */

    return 0;
}
```

Compile with:

```
cc -std=gnu11 -Ipath/to/common/include -Ipath/to/log/include \
   main.c \
   -Lpath/to/log/build/debug -llog \
   -Lpath/to/common/build/debug -lcommon \
   -o main
```

## API

### Macros

| Macro | Description |
|---|---|
| `MIN(x, y)` | Smaller of two values. Each evaluated once. |
| `MAX(x, y)` | Larger of two values. Each evaluated once. |
| `CASE_RETURN(x)` | Switch helper: `case x: return #x`. |

### Growth policy

| Symbol | Description |
|---|---|
| `Tincrease` | Enum: `SIZE_NONE`, `SIZE_POWER2`, `SIZE_MIN10`. |
| `round_up_2(v)` | Smallest power of two strictly greater than `v`. Returns `0` for `0`. |
| `calcnewsize(t, n)` | Applies policy `t` to a requested size `n`. Returns `-1` on unknown policy. |

`SIZE_POWER2` is intentionally **strictly greater** — `round_up_2(4)` is
`8`, not `4`. This ensures there is always room for an appended NUL
terminator.

### Character helpers

| Function | Description |
|---|---|
| `clower(c, true/false)` | Conditionally apply `tolower`. |
| `cupper(c, true/false)` | Conditionally apply `toupper`. |

### Random helpers

Caller must seed with `srand()` before use.

| Function | Range |
|---|---|
| `rndint(max)` | `[0, max]` inclusive. `0` for `max <= 0`. |
| `rndlowchar()` | `'a'..'z'`. |
| `rndupperchar()` | `'A'..'Z'`. |
| `rnddigitchar()` | `'0'..'9'`. |

### C-string

| Function | Description |
|---|---|
| `reverse(s, len)` | In-place reversal of the first `len` chars. Returns `s`. |

### Parsing

All `try_parse_*` functions share the same contract:

- return `true` on success, `false` on any error;
- on failure the output is left untouched;
- reject empty strings, trailing garbage, overflow, `NULL` arguments;
- `try_parse_ulong` explicitly rejects a leading minus.

| Function | Notes |
|---|---|
| `try_parse_int(str, &v)` | Range-checked against `INT_MIN..INT_MAX`. |
| `try_parse_long(str, &v)` | Uses `strtol`. |
| `try_parse_ulong(str, &v)` | Uses `strtoul`; leading `-` is rejected. |
| `try_parse_double(str, &v)` | Uses `strtod`. |

### Version

| Function | Description |
|---|---|
| `const char *common_version(void)` | Current version string, e.g. `"0.1.0"`. |
| `const char *const *common_versions(void)` | NULL-terminated array of published versions, newest first. |

`common_versions()[0]` is always the current version. The array is
static, read-only, and must not be freed.

### Comparators for `qsort` / `bsearch`

Byte-wise comparators over `unsigned char`:

| Function | Order |
|---|---|
| `pchar_cmp` | Ascending. |
| `pchar_revcmp` | Descending. |

```c
char buf[] = "cab";
qsort(buf, 3, 1, pchar_cmp);      /* "abc" */
qsort(buf, 3, 1, pchar_revcmp);   /* "cba" */
```

## Notes

- **No kitchen sink.** The original monolithic `common.h` had 600+ lines
  covering random, fillers, IO-checker macros, bit-printing, array
  membership tests, and more. This version keeps only what current
  consumers need. Add new symbols one at a time when a real caller
  appears.
- `G_GLOB_AVERAGE` (= `INT_MAX`) is a sentinel used by `fs` to cap
  iteration over NULL-terminated arrays of non-pointer structs. It is
  not a general-purpose "average".
- `calcnewsize` logs unknown strategies via `logsimple`, hence the
  `log` dependency.

## Development

The project uses [Criterion](https://criterion.readthedocs.io/) for unit tests.

### Requirements

- C compiler with GNU extensions (`({ ... })`, `typeof`,
  `__attribute__((format))`).
- [Criterion](https://criterion.readthedocs.io/) — `brew install criterion`
- `log` utility archive — built automatically via `REQUIRES`.

### Layout

```
.
├── include/common.h       public header
├── src/common.c           implementation
├── test/test_common.c     unit tests
├── build/                 build artifacts (gitignored)
├── Makefile               PROJNAME + REQUIRES, includes ../Makefile.common
├── README.md
└── CHANGELOG.md
```

### Commands

```
make test       # build and run unit tests
make lib        # build debug library
make librelease # build release library
make check      # clang-tidy static analysis
make analyze    # clang static analyzer on library sources
make clean      # remove build artifacts
```

## License

See the top-level `LICENSE` file.

