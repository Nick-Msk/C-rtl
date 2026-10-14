# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `log_initf(append, fmt, dir, filename_fmt, ...)` — open a log file
  with a printf-formatted filename. Caller controls the uniqueness
  scheme (pid, tid, timestamp, test name).
- `log_init_bypid(dir, base, append, fmt)` — convenience wrapper:
  `<dir>/<base>_<pid>.log`. Uses `getpid()` only; for thread-unique
  files, use `log_initf` with a tid.
- `log_mkdir(dir)` — one-level `mkdir(0755)`; `EEXIST` is success.
- `README.md`: full API reference with quick start, macro tables,
  initialization variants, and notes.

### Changed

- Version bumped 0.1.0 → 0.2.0 (new public API).
- `.gitignore`: `logs/` (test output).

## [0.1.0] - 2026-10-08

### Added

- Initial versioned release.
- File-based logging engine with per-module level filtering
  (`log_init`, `log_modinit`, `log_modsave`, `log_modload`).
- Preambule formats: `LOG_FORMAT_EMPTY/ALL/ONLY_FILE/ONLY_FUNC/SIMPLE/ONLY_TIME`.
- Indent-aware macros: `logenter` / `logret` / `logerr` / `logmsg`.
- Simple (unpreambled) variants: `logsimple*`.
- Auto-format via `_Generic`: `logauto` / `logautoret` / `logautoerr`.
- Raw buffer dump: `log_numbers` (deprecated, off-by-one bug, unused).
- Compile-time removal via `-DNODEBUG`.
- Version API: `log_version()`, `log_versions()`, `LOG_VERSION` macro.
- 22 unit tests covering lifecycle, writing, format selection,
  indentation, module filtering, and persistence.

