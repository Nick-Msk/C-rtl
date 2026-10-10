# error.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

Per-thread error stack and setjmp/longjmp exception facility for C.

Depends on `log` and `bool`.

## Features

- **Per-thread error stack** — `err_raise` records a formatted record;
  `err_fprintstacktrace` dumps the stack.
- **TRY / CATCH exceptions** via `sigsetjmp` / `siglongjmp` — a 128-slot
  ring of `sigjmp_buf`s, no manual depth management.
- **Convenience raise macros** — `userraise*` / `sysraise*` (with
  optional cleanup ACTION) for application-level and errno-based errors.
- **Signal name helpers** — `sig_str`, `sig_str_desc`.
- **`err_raise` fallback** — when no TRY frame is active, it raises a
  real `SIGINT` (visible in a debugger / produces a core dump) instead
  of jumping to a garbage slot.

## Usage

```c
#include <error.h>

int process(const char *path) {
    err_clean(true);

    TRY() {
        FILE *f = fopen(path, "r");
        if (!f)
            sysraise(-1, "cannot open %s", path);   /* errno recorded */

        /* ... work ... */

        fclose(f);
    } else {
        err_fprintstacktrace(stderr);
        return -1;
    }
    return 0;
}
```

Compile with:

```
cc -std=gnu11 -Ipath/to/error/include \
   -Ipath/to/log/include -Ipath/to/bool/include \
   main.c \
   -Lpath/to/log/build/debug   -llog \
   -Lpath/to/bool/build/debug  -lbool \
   -Lpath/to/error/build/debug -lerror \
   -o main
```

Requires GNU Make + GCC/Clang statement-expressions and `sigsetjmp`.

## TRY / exception mechanism

```c
TRY() {
    /* body — runs on first entry */
    if (fail)
        err_raise(ERR_USER, ERR_NULLABLE_PTR, "null argument");
} else {
    /* catch — reached via siglongjmp from err_raise */
    err_printstacktrace();
}
```

- On first entry, `TRY()` does `sigsetjmp(env[depth], 1)` and runs the
  body.
- If the body calls `err_raise` (directly or via a raise macro), and a
  TRY frame is active, `err_raise` does `siglongjmp` to the innermost
  frame.
- The catch branch runs, then execution continues after the `} else {`.
- `depth` / `overallcnt` are restored on every normal exit **and** on
  every `siglongjmp`, so nested TRY blocks and TRY blocks in loops are
  safe.

**Notes:**

- `break` / `return` / `goto` **from the middle of the body** skips the
  for-loop increment that restores `depth`. This leaks one ring slot.
  It is *not fatal*: the ring wraps, and subsequent TRY blocks keep
  working. But do not rely on the leak.
- If `err_raise` is called with **no active TRY frame**, it falls back
  to `raise(SIGINT)`. In a debugger this lands on the raise; without
  one, it aborts and produces a core dump.

## Raise macros

All raise macros execute an optional `ACTION` (cleanup / rollback),
print the message to the log and stderr, then call `err_raise`.

### User errors

| Macro | Signature |
|---|---|
| `userraise(rc, code, fmt, ...)` | Record + raise, no cleanup |
| `userraiseact(rc, ACTION, code, fmt, ...)` | Record + cleanup + raise |

### System errors (`errno`)

| Macro | Signature |
|---|---|
| `sysraise(rc, fmt, ...)` | Record + raise, errno captured |
| `sysraiseact(rc, ACTION, fmt, ...)` | Record + cleanup + raise |

**`errno` is preserved** across `ACTION` and logging — the value
recorded in the stack is the one that was current at macro entry.

Example:

```c
int *p = malloc(n);
if (!p)
    return userraiseact(0, cleanup(), ERR_UNABLE_ALLOCATE,
                        "alloc %d failed", n);

FILE *f = fopen(path, "r");
if (!f)
    return sysraiseact(-1, fclose(f), "cannot open %s", path);
```

## API

### Error stack

| Function | Description |
|---|---|
| `void err_raise(ErrorType tp, int code, const char *fmt, ...)` | Record + transfer control (see TRY notes). |
| `void err_clean(bool force)` | Reset the stack. `force=true` frees heap buffer. |
| `int err_fprintstacktrace(FILE *out)` | Dump all records to a stream. |
| `int err_msg(int code, char *buf, size_t sz)` | Thread-safe `strerror`. |
| `int err_count(void)` | Number of records currently on the stack. |
| `bool err_last(ErrInfo *out)` | Fetch type + code of the top record. `out` may be NULL. |
| `bool err_pop(void)` | Remove the top record. |

`ErrInfo` is a small struct (defined in `error.h`):

```c
typedef struct {
    ErrorType   type;   /* ERR_USER or ERR_SYS */
    int         code;   /* app code or errno */
} ErrInfo;
```

Example — check what happened without dumping the whole stack:

```c
TRY() { risky(); } else {
    ErrInfo info;
    if (err_last(&info) && info.type == ERR_SYS && info.code == ENOENT) {
        /* special handling for "file not found" */
    }
    err_clean(true);
}

/* later: how many errors accumulated? */
if (err_count() > 0)
    err_printstacktrace();

/* drop the last record and continue */
err_pop();
```

### Exception environment

| Function / macro | Description |
|---|---|
| `TRY()` | Open a try block (see above). |
| `errenv` | Accessor macro: `ErrorExceptionData` lvalue. |
| `ErrorExceptionData *err_getexception_info(void)` | Pointer to the thread-local env. |
| `err_resetenv()` | Reset `depth` and `overallcnt` to 0. |

### Signal helpers

| Function | Description |
|---|---|
| `const char *sig_str(int sig)` | Symbolic name (`"SIGINT"`, ...). |
| `const char *sig_str_desc(int sig)` | Human-readable description. |

### Version

| Function | Description |
|---|---|
| `const char *err_version(void)` | Current version string, e.g. `"0.2.0"`. |
| `const char *const *err_versions(void)` | NULL-terminated array, newest first. |

## Thread safety

- The error stack and `ErrorExceptionData` are `_Thread_local` — each thread
  has its own.
- `err_raise` / `TRY()` from multiple threads is safe as long as each
  thread manages its own TRY blocks.
- **`siglongjmp` across threads is UB** — do not raise on one thread
  and catch on another.

## Development

The project uses [Criterion](https://criterion.readthedocs.io/) for unit tests.

### Requirements

- A C compiler with GNU extensions — `({ ... })`, `typeof`,
  `__attribute__((format))` are required.
- [Criterion](https://criterion.readthedocs.io/) — `brew install criterion`
- `log` and `bool` utility archives — built automatically via `REQUIRES`.

### Layout

```
.
├── include/error.h        public header
├── src/error.c            implementation
├── test/test_error.c      unit tests
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

See the top-level `LICENSE` file
.
