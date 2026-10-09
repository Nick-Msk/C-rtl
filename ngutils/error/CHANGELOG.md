# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- `try()` now supports multiple nesting levels (up to `ERR_MAX_TRY_CNT = 8`)
  via a static per-thread stack of `jmp_buf`s. No heap allocation.
- `ExceptionData` uses `env[ERR_MAX_TRY_CNT]` + `depth` instead of a single
  `jmp_buf` + `init_flag`.
- `err_default_handler` longjmps to `env[depth-1]` (innermost try).
- `err_resetenv()` zeroes `depth` (cancels all active try blocks).

## [0.1.0] - 2026-10-09

### Added

- Initial versioned release.
- Per-thread error stack: `err_raise`, `err_clean`,
  `err_fprintstacktrace`.
- setjmp/longjmp exception mechanism: `try()`, `err_sethandler`,
  `errenv`, `err_resetenv`.
- Convenience macro families: `userraise*`, `sysraise*`, `*act*`,
  `*int*`.
- Signal name / description helpers: `sig_str`, `sig_str_desc`.
- Version API: `err_version()`, `err_versions()`, `ERROR_VERSION` macro.

### Notes

- Depends on `log` and `bool`.
- `depth` is `volatile sig_atomic_t` (POSIX signal-safe); `env[]` is a static array of `jmp_buf`.
- `SIGEMT` / `SIGINFO` are BSD/macOS only and are guarded by `#ifdef`.

