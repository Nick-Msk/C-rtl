# bool.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

A tiny C utility for converting between `bool` and its string
representation.

Core functions are header-only (`include/bool.h`). The version API lives
in `src/bool.c` so the published-versions array is defined in exactly one
translation unit. C99+, no dependencies beyond the C standard library.

## Features

- `bool_str` — `bool` → `"true"` / `"false"`
- `bool_tryparse` — `"true"` / `"false"` → `bool`, with explicit success flag
- `bool_parsedef` — parse with a fallback value on failure
- `bool_version` / `bool_versions` — current and historical version strings

## Usage

Copy `include/bool.h` and `src/bool.c` into your project, then:

```c
#include <bool.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("%s\n", bool_str(true));            /* "true" */

    bool ok;
    if (bool_tryparse("True", &ok))
        printf("parsed: %d\n", ok);            /* parsed: 1 */

    bool debug = bool_parsedef(getenv("DEBUG"), false);
    (void)debug;

    printf("bool.h %s\n", bool_version());     /* "0.2.0" */
    return 0;
}
```

Compile with:

```
cc -std=c99 -Ipath/to/include main.c path/to/src/bool.c -o main
```

## API

### Conversion

| Function | Description |
|---|---|
| `const char *bool_str(bool v)` | Returns a pointer to a **string literal** (`"true"` or `"false"`). |
| `bool bool_tryparse(const char *str, bool *out)` | Parses `"true"` / `"false"` (case-insensitive). Returns `true` on success. `out` may be `NULL`. On failure, `*out` is left untouched. |
| `bool bool_parsedef(const char *str, bool def)` | Like `bool_tryparse`, but returns `def` on failure. Never fails. |

`bool_tryparse` does **not** trim whitespace and does **not** accept
synonyms such as `"yes"`, `"on"`, or `"1"`. For richer parsing, use a
dedicated library.

### Version

| Function | Description |
|---|---|
| `const char *bool_version(void)` | Current version string, e.g. `"0.2.0"`. |
| `const char *const *bool_versions(void)` | NULL-terminated array of published versions, newest first. |

`bool_versions()[0]` is always the current version. The array is static,
read-only, and must not be freed.

## Notes

- Pointers returned by `bool_str` and `bool_version` refer to **static
  string literals** — do not free or modify them.
- Core conversion functions are declared `static inline` and produce no
  extra symbols in the object file.
- `bool_tryparse` is case-insensitive via `strcasecmp`
  (`_stricmp` on MSVC).

## Development

The project uses [Criterion](https://criterion.readthedocs.io/) for unit tests.

### Requirements

- A C compiler (`cc`, `gcc`, or `clang`)
- [Criterion](https://criterion.readthedocs.io/) — `brew install criterion`
- Optional: `clang-tidy`, `scan-build` for `make check` / `make analyze`

### Layout

```
.
├── include/bool.h        public header (core, header-only)
├── src/bool.c            version array (single TU)
├── test/test_bool.c      unit tests
├── build/                build artifacts (gitignored)
├── Makefile
├── Doxyfile
├── README.md
└── CHANGELOG.md
```

### Commands

```
make test       # build and run unit tests
make check      # clang-tidy static analysis
make analyze    # clang static analyzer (scan-build)
make clean      # remove build artifacts
```

Test binaries land in `build/debug/`.

## License

See the top-level `LICENSE` file.

