# bool.h

A tiny C utility header providing a single helper function to convert a
`bool` value into its `"true"` / `"false"` string representation.

Header-only, C99+, no dependencies beyond `<stdbool.h>`.

## Usage

Copy `include/bool.h` into your include path, then:

```c
#include <bool.h>

#include <stdio.h>

int main(void) {
    printf("1 == %s\n", bool_str(true));   // "1 == true"
    printf("0 == %s\n", bool_str(false));  // "0 == false"
    return 0;
}
```

Compile with:

```
cc -std=c99 -Ipath/to/include main.c -o main
```

## API

| Function | Description |
|---|---|
| `const char *bool_str(bool v)` | Returns a pointer to a **string literal** (`"true"` or `"false"`). No allocation, safe to use in any context. |

## Notes

- The returned pointer refers to a **static string literal** — do **not** free or modify it.
- The function is declared `static inline`, so it produces no extra symbol in the
  object file and is inlined at the call site.
- No dependencies beyond `<stdbool.h>` (C99+).

## Development

The project uses [Criterion](https://criterion.readthedocs.io/) for unit tests.

### Requirements

- A C compiler (`cc`, `gcc`, or `clang`)
- Criterion: `brew install criterion` (macOS) or see upstream docs

### Layout

```
.
├── include/         public headers
├── src/             library sources (currently empty — header-only)
├── test/            unit tests
├── build/           build artifacts (gitignored)
├── Makefile
├── README.md
└── CHANGELOG.md
```

### Commands

```
make test      # build and run the unit tests
make clean     # remove build artifacts
```

Test binaries land in `build/debug/`.

