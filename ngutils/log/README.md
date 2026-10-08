# log.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

A lightweight file-based logging engine for C with per-module level
control, configurable preambles, and indentation-aware macros.

C99+, no dependencies beyond the C standard library. Logging can be
compiled out entirely with `-DNODEBUG`.

## Features

- **Per-module level filtering** — silence noisy modules, keep others verbose
- **Configurable preamble** — file, function, line, timestamp, or none
- **Indent-aware** — `logenter` / `logret` / `logerr` adjust indentation
- **Simple variants** — one-liners without preamble or indent
- **Auto-formatting** — `logauto` picks a `printf` specifier via `_Generic`
- **Runtime on/off** — global switch to silence everything
- **Module persistence** — save/load level tables to text files
- **Compile-time removal** — `-DNODEBUG` erases all logging calls

## Usage

```c
#include <log.h>

int process(const char *name) {
    logenter("processing %s", name);           /* indent +1 */

    if (!name)
        return logerr(-1, "null name");        /* indent -1 */

    for (int i = 0; i < 3; i++)
        logmsg("step %d", i);                  /* indent unchanged */

    return logret(0, "done, %s", name);        /* indent -1 */
}

int main(void) {
    LOG("logs");                               /* open logs/<file>.log, truncate */
    int rc = process("demo");
    logclose("exit rc=%d", rc);
    return rc;
}
```

Compile with:

```
cc -std=c99 -Ipath/to/include main.c path/to/src/log.c -o main
```

## Initialization

| Macro | Description |
|---|---|
| `LOG(dir)` | Open `dir/<file>.log` (truncate) and log a start message. |
| `LOGAPPEND(dir)` | Same, but append instead of truncate. |
| `loginit(path, append, modules, fmt, ...)` | Explicit init with module list. |
| `logsimpleinit(fmt, ...)` | Init in `log/` directory (convenience). |
| `logclose(fmt, ...)` | Log a final message and close the file. |

## Logging macros

### Indent-aware

| Macro | Effect | Returns |
|---|---|---|
| `logenter(fmt, ...)` | Declares `_LG_LV`, indent +1 | `0` |
| `logret(rc, fmt, ...)` | Normal return, indent −1 | `rc` |
| `logerr(rc, fmt, ...)` | Error return, indent −1 | `rc` |
| `logmsg(fmt, ...)` | Mid-function info, no indent change | `0` |
| `logact(ACTION, fmt, ...)` | Run `ACTION`, log, return `0` | `0` |
| `logactret(ACTION, rc, fmt, ...)` | Run `ACTION`, log, return `rc` | `rc` |
| `logacterr(ACTION, rc, fmt, ...)` | Run `ACTION`, log as error, return `rc` | `rc` |

`logenter` must be the first statement in a function that uses
`logret` / `logerr`. It declares a local `_LG_LV` used by the return
macros to track indentation.

### Simple (no preamble, no indent)

| Macro | Effect | Returns |
|---|---|---|
| `logsimple(fmt, ...)` | One-liner | `0` |
| `logsimpleret(rc, fmt, ...)` | One-liner with return | `rc` |
| `logsimpleerr(rc, fmt, ...)` | Error one-liner | `rc` |
| `logsimpleact(ACTION, fmt, ...)` | Run `ACTION`, log | `0` |

### Auto-format via `_Generic`

| Macro | Effect | Returns |
|---|---|---|
| `logauto(val)` | Print `name = value` with auto format | `0` |
| `logautoret(val)` | Same, and return `val` | `val` |
| `logautoerr(val)` | Same, but as error, and return `val` | `val` |

Supports all scalar types, `char *`, `const char *`, and `void *`.

### Raw buffer

| Macro | Description |
|---|---|
| `lognumbers(bytes, sz)` | Dump a byte buffer. **Deprecated** — see `log.h`. |

## Module levels

```c
Loglevel levels[] = {
    { .module = "parser", .level = LOGALL  },
    { .module = "io",     .level = LOGOFF  },
    { .module = "",       .level = _LOGSTOP }
};
log_modinit(levels);
```

| Function | Description |
|---|---|
| `bool log_modinit(LogModlevel *)` | Register a NULL-sentinel-terminated module list. |
| `bool log_modsave(const char *path)` | Write the current table to a text file. |
| `bool log_modload(const char *path)` | Load a table from a file. |
| `void log_modclear(void)` | Free the table and restore the default. |

Levels: `LOGOFF` (silent), `LOGERR`, `LOGWARN`, `LOGALL`. Current
implementation only distinguishes `LOGOFF` from anything else; the
intermediate levels are reserved for future use.

The list must be terminated by an entry with `.level = _LOGSTOP`.
Messages for a module not in the list are suppressed.

## Runtime control

| Function | Description |
|---|---|
| `bool log_init(name, append, fmt)` | Open a log file. No-op if already open. |
| `void log_close(void)` | Close file, reset format and offset, free modules. |
| `bool log_isinit(void)` | Is the log file currently open? |
| `FILE *log_file(void)` | The active stream, or `NULL`. |
| `int log_offset(void)` | Current indentation depth (in spaces). |
| `int log_prog_switch(bool)` | Globally on/off; returns previous state. |
| `bool log_format(LogFormat)` | Change the preamble format. |

Convenience macros: `logon()`, `logoff()`, `logfile`, `logoffset`.

## Preamble formats

`LogFormat` values passed to `log_init` or `log_format`:

| Value | Preamble |
|---|---|
| `LOG_FORMAT_EMPTY` | None — just indentation and message. |
| `LOG_FORMAT_ALL` | File, function, line, action tag, timestamp. |
| `LOG_FORMAT_ONLY_FILE` | Module, file, line, action tag. |
| `LOG_FORMAT_ONLY_FUNC` | Function, line, action tag. |
| `LOG_FORMAT_SIMPLE` | Function, line, action tag (compact). |
| `LOG_FORMAT_ONLY_TIME` | Function, line, action tag, timestamp. |

## Version

| Function | Description |
|---|---|
| `const char *log_version(void)` | Current version string, e.g. `"0.1.0"`. |
| `const char *const *log_versions(void)` | NULL-terminated array, newest first. |

`log_versions()[0]` is always the current version. The array is static,
read-only, and must not be freed.

## `-DNODEBUG` mode

Compile with `-DNODEBUG` to erase all logging at compile time:

- All macros become no-ops or `(rc)` pass-throughs.
- No calls into `log.c`, so the linker may drop it entirely.
- `log_version()` / `log_versions()` still work — they are metadata.

This is useful for release builds where you want zero logging overhead.

## Notes

- Thread-unsafe by design (single-writer assumption).
- The log stream is set to unbuffered (`_IONBF`); each call flushes.
- Pointers returned by `log_version` / `log_versions` refer to static
  string literals — do not free or modify them.

## Development

The project uses [Criterion](https://criterion.readthedocs.io/) for unit tests.

### Requirements

- A C compiler (`cc`, `gcc`, or `clang`)
- [Criterion](https://criterion.readthedocs.io/) — `brew install criterion`
- Optional: `clang-tidy`, `scan-build` for `make check` / `make analyze`

### Layout

```
.
├── include/log.h        public header (declarations + macros)
├── src/log.c            implementation
├── test/test_log.c      unit tests
├── build/               build artifacts (gitignored)
├── Makefile
├── README.md
└── CHANGELOG.md
```

### Commands

```
make test       # build and run unit tests
make lib        # build debug library
make librelease # build release library
make check      # clang-tidy static analysis
make analyze    # clang static analyzer (scan-build)
make clean      # remove build artifacts
```

## License

See the top-level `LICENSE` file.

