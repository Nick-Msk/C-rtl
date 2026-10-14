# log.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

Lightweight file-based logging for C with per-module level control,
configurable preambles, and indentation-aware macros.

C99+ with GNU extensions (`__thread`, `({ ... })`,
`__attribute__((format))`). No dependencies beyond the C standard
library and POSIX (`pthread`, `strerror_r`).

## Features

- **Per-module level filtering** — silence noisy modules, keep others
  verbose.
- **Configurable preamble** — file, function, line, timestamp, or none.
- **Indent-aware** — `logenter` / `logret` / `logerr` adjust indentation.
- **Simple variants** — one-liners without preamble or indent.
- **Auto-formatting** — `logauto` picks a `printf` specifier via
  `_Generic`.
- **Runtime on/off** — global switch to silence everything.
- **Module persistence** — save/load level tables to text files.
- **Compile-time removal** — `-DNODEBUG` erases all logging calls.
- **Per-thread state** — file pointer, offset, module table, format are
  all `__thread`; each thread logs to its own file.

## Quick start

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
cc -std=gnu11 -Ipath/to/log/include main.c path/to/log/src/log.c -o main
```

## Initialization

The most flexible form is `log_initf`: the caller controls the
filename, including any uniqueness scheme (pid, tid, timestamp).

| Function / macro | Purpose |
|---|---|
| `log_init(path, append, fmt)` | Low-level: open a specific file. |
| `log_initf(append, fmt, dir, filename_fmt, ...)` | Open `<dir>/<formatted filename>`. `dir` may be NULL/`""` for CWD. |
| `log_init_bypid(dir, base, append, fmt)` | Convenience: `<dir>/<base>_<pid>.log`. |
| `log_mkdir(dir)` | One-level `mkdir(0755)`; `EEXIST` is success. |
| `loginit(path, append, modules, fmt, ...)` | Init with a module list (macro). |
| `logsimpleinit(fmt, ...)` | Init in `log/` directory. |
| `logclose(fmt, ...)` | Log a final message and close. |

```c
/* Typical test fixture */
log_mkdir("logs");
log_init_bypid("logs", "run", true, LOG_FORMAT_ALL);
...
log_close();
```

Note: `log_init_bypid` uses `getpid()` only. All threads of a process
share a pid, so for thread-unique files use `log_initf` with a tid:

```c
log_initf(true, LOG_FORMAT_ALL, "logs", "run_%d_%lu.log",
          (int)getpid(), (unsigned long)pthread_self());
```

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

## Module levels

```c
Loglevel levels[] = {
    { .module = "parser", .level = LOGALL  },
    { .module = "io",     .level = LOGOFF  },
    { .module = "",       .level = _LOGSTOP }
};
log_modinit(levels);
```

| Function | Purpose |
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

| Function | Purpose |
|---|---|
| `bool log_isinit(void)` | Is the log file currently open? |
| `FILE *log_file(void)` | The active stream, or `NULL`. |
| `int log_offset(void)` | Current indentation depth (in spaces). |
| `int log_prog_switch(bool)` | Globally on/off; returns previous state. |
| `bool log_format(LogFormat)` | Change the preamble format. |
| `void log_close(void)` | Close file, reset format and offset, free modules. |

Convenience macros: `logon()`, `logoff()`, `logfile`, `logoffset`.

## Preamble formats

`LogFormat` values passed to `log_init` / `log_initf` / `log_format`:

| Value | Preamble |
|---|---|
| `LOG_FORMAT_EMPTY` | None — just indentation and message. |
| `LOG_FORMAT_ALL` | File, function, line, action tag, timestamp. |
| `LOG_FORMAT_ONLY_FILE` | Module, file, line, action tag. |
| `LOG_FORMAT_ONLY_FUNC` | Function, line, action tag. |
| `LOG_FORMAT_SIMPLE` | Function, line, action tag (compact). |
| `LOG_FORMAT_ONLY_TIME` | Function, line, action tag, timestamp. |

## Version

| Function | Purpose |
|---|---|
| `const char *log_version(void)` | Current version string, e.g. `"0.2.0"`. |
| `const char *const *log_versions(void)` | NULL-terminated array, newest first. |

`log_versions()[0]` is always the current version. The array is static,
read-only, and must not be freed.

## `-DNODEBUG` mode

Compile with `-DNODEBUG` to erase all logging at compile time:

- All macros become no-ops or `(rc)` pass-throughs.
- No calls into `log.c`, so the linker may drop it entirely.
- `log_version()` / `log_versions()` still work — they are metadata.

Useful for release builds where you want zero logging overhead.

## Notes

- Thread state is `__thread`: file pointer, offset, module table,
  format. Each thread logs independently.
- A global inode registry (`dev_t` + `ino_t`, mutex-protected) prevents
  the same file from being opened by two threads at once. The mutex is
  only held during `log_init` / `log_close`; the hot path (`log_msg`)
  is lock-free.
- The log stream is unbuffered (`_IONBF`); each call flushes.
- `log_init` with `logname == NULL` reuses the current `g_logname` if
  one is set, otherwise falls back to `default.log` in the CWD.

## Development

The project uses [Criterion](https://criterion.readthedocs.io/) for
unit tests.

```
make test       # build and run unit tests
make lib        # debug library
make librelease # release library
make check      # clang-tidy
make analyze    # clang static analyzer
make clean      # remove build artifacts
make clean-logs # remove logs/*.log
```

### Layout

```
.
├── include/log.h       public header
├── src/log.c           implementation
├── test/test_log.c     unit tests
├── build/              artifacts (gitignored)
├── logs/               test output (gitignored)
├── Makefile            PROJNAME + REQUIRES
├── README.md
└── CHANGELOG.md
```

## License

See the top-level `LICENSE` file.
