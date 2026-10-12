# checker.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

Assertion and validation helpers for C.

Two distinct families:

- **`inv*` — invariants.** Assertions of *internal consistency*, meant
  for **development and testing only**. They must be compiled out
  in release builds (`-DNOINVARIANT`).
- **`check_*` — runtime validators.** Production-grade input and
  environment checks that should *stay enabled* in shipped code.

Depends on `log`.

## What is an invariant

An invariant is a promise *your own code* makes about itself:

- a pointer is never NULL *because the caller guarantees it*,
- a counter is never negative,
- a state machine never reaches `enum` value 7.

A violated invariant means **the programmer made a mistake**, not that
the user did something wrong. Therefore:

- in **development**, an invariant violation should be loud and fatal
  (log + `abort()` — a core dump to attach a debugger to);
- in **release**, invariants should be **compiled out entirely** — they
  cost cycles and code size, and are pointless if the code is correct.

## What is a runtime check

A runtime check validates something that can go wrong **in production**:

- `argc` is too small,
- an environment variable is missing,
- user input is out of range.

These are *expected* failure modes. They must always be present, and
they must return a diagnosable error rather than crash.

## Invariants — `inv*`

> **Not for production code.**
> Compile with `-DNOINVARIANT` in release builds. With that flag,
> all `inv*` macros reduce to plain boolean expressions with no
> side effects.

### Returning form

Evaluate, log on failure, return a `bool`. Use when the caller can
recover or propagate the failure gracefully:

```c
int process(const char *path) {
    if (!inv(path != NULL, "path is NULL in %s", __func__))
        return -1;
    ...
}
```

| Macro | Returns |
|---|---|
| `inv(expr, msg, ...)` | `bool` — `false` on violation |
| `inv2(a, b, msg, ...)` | `bool` — `false` if `a != b` |

### Aborting form

Evaluate, log on failure, then `abort()` (SIGABRT, core dump).
Use for "this cannot happen" cases:

```c
size_t idx = hash(key) % table->size;
invraise(idx < table->size, "hash index out of range");
/* execution stops here if the invariant is broken */
```

| Macro | On violation |
|---|---|
| `invraise(expr, msg, ...)` | `abort()` |
| `inv2raise(a, b, msg, ...)` | `abort()` |

`abort()` is intentional — not catchable by `TRY()`. An invariant
violation is a programming bug; you want a crash and a backtrace, not
a silently handled exception.

### Two-value form

`inv2` / `inv2raise` compare two values of the same type and print
both on mismatch:

```c
inv2(actual_count, expected_count, "counter drift");
inv2raise(magic, 0xDEADBEEF, "bad header magic");
```

Types are dispatched via `_Generic`. Currently supported: `int`,
`short`, `char`, `long`, `long long`, unsigned variants, `float`,
`double`, `char *`, `const char *`, and any object pointer (via
`default:`). Structs and function pointers are **not** supported —
compare fields or cast explicitly.

### Disabling

```c
#define NOINVARIANT   /* or -DNOINVARIANT on the command line */
#include <checker.h>

inv(cond, ...)              /* → (cond) */
inv2(a, b, ...)             /* → (a == b) */
invraise(cond, ...)         /* → (cond) */
inv2raise(a, b, ...)        /* → (a == b) */
```

No calls, no logging, no `abort()`. The optimizer removes the whole
expression when `cond` is a compile-time constant.

## Runtime checks — `check_*`

> **Always enabled.** These are for real production code paths.

| Macro | Description |
|---|---|
| `check_arg(min_argc, msg, ...)` | Fails if `argc < min_argc`. Returns `0` on failure, `1` otherwise. Prints `msg` to stderr on failure. |

`check_arg` expects `argc` to be a variable in scope (typically inside
`main`):

```c
int main(int argc, char **argv) {
    if (!check_arg(3, "usage: %s <input> <output>\n", argv[0]))
        return 1;
    ...
}
```

More `check_*` helpers (range, string, env) will be added as needed.

## API

### Core

| Function | Description |
|---|---|
| `int inv_fprintf_int(FILE *out, const char *expr, bool res, const char *msg, ...)` | Print an invariant failure to `out` and to the log. |

### Per-type converters

These are called by `_Generic` inside `inv2` / `inv2raise`; the user
rarely calls them directly. Each returns `true` if the two values are
equal and prints both on mismatch.

| Function | Types |
|---|---|
| `bool inv_int_bool(FILE *, int, int)` | `int`, `short`, `char`, and their unsigned forms |
| `bool inv_long_bool(FILE *, long, long)` | `long`, `long long`, unsigned variants |
| `bool inv_double_bool(FILE *, double, double)` | `float`, `double` |
| `bool inv_str_bool(FILE *, const char *, const char *)` | C strings (NULL-safe) |
| `bool inv_ptr_bool(FILE *, const void *, const void *)` | Any object pointer |

## Usage

```c
#include <checker.h>

static int
lookup(const char *key)
{
    /* invariant: caller guarantees key is not NULL */
    invraise(key != NULL, "lookup() called with NULL key");

    /* recoverable invariant: log and bail out */
    if (!inv(strlen(key) > 0, "empty key"))
        return -1;

    return 0;
}

int
main(int argc, char **argv)
{
    /* runtime check: user may pass too few arguments */
    if (!check_arg(2, "usage: %s <key>\n", argv[0]))
        return 1;

    return lookup(argv[1]);
}
```

Compile with:

```
cc -std=gnu11 -Ipath/to/checker/include -Ipath/to/log/include \
   main.c \
   -Lpath/to/log/build/debug -llog \
   -Lpath/to/checker/build/debug -lchecker \
   -o main
```

For a release build:

```
cc -std=gnu11 -DNOINVARIANT ...
```

## Building and testing

```
make test       # build and run unit tests
make lib        # build debug library
make librelease # build release library (still keeps -DNOINVARIANT off; add it manually)
make check      # clang-tidy static analysis
make clean      # remove build artifacts
```

Tests use [Criterion](https://criterion.readthedocs.io/). The abort
tests use `.signal = SIGABRT` to verify the process dies rather than
returns.

### Requirements

- A C compiler with GNU extensions — `({ ... })`, `_Generic`,
  `__attribute__((format))` are required.
- [Criterion](https://criterion.readthedocs.io/) — `brew install criterion`
- `log` utility archive — built automatically via `REQUIRES`.

### Layout

```
.
├── include/checker.h      public header
├── src/checker.c          implementation
├── test/test_checker.c    unit tests
├── build/                 build artifacts (gitignored)
├── Makefile               PROJNAME + REQUIRES, includes ../Makefile.common
├── README.md
└── CHANGELOG.md
```

## Thread safety

`inv*` and `check_*` are pure or use `_Thread_local` state in `log` —
safe to call from any thread.

## License

See the top-level `LICENSE` file.

