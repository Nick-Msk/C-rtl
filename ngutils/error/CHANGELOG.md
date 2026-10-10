# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] - 2026-10-10

### Changed

- **`TRY()` macro is now the primary exception mechanism.**
  Replaces the old `try()` function-call syntax. Uses `sigsetjmp` /
  `siglongjmp` with a 128-slot ring of `sigjmp_buf`s, so nested and
  looped TRY blocks work without manual depth management.

  ```c
  TRY() {
      risky_operation();
  } else {
      err_printstacktrace();
  }
  ```

- **`err_raise` always transfers control when a TRY frame is active.**
  Previously the raise macros distinguished "record only" vs.
  "record + signal" via `sig = 0`. That distinction is gone: if a TRY
  is active, `err_raise` does `siglongjmp`; if not, it falls back to a
  real `raise(SIGINT)` so the program aborts visibly instead of
  jumping to a garbage slot.

- **Raise macros simplified.** All `*sig` and `*int` variants removed.
  The remaining set is `userraise`, `userraiseact`, `sysraise`,
  `sysraiseact`.

- **`ExceptionData` layout changed.** Now holds `sigjmp_buf env[128]`,
  `int depth`, `int overallcnt`. The `depth` field is a ring index,
  `overallcnt` counts wraps.

- `err_resetenv()` now clears both `depth` and `overallcnt`.

### Fixed

- **`errno` preservation in raise macros.** Previously `errno` could
  be clobbered by `_log_and_print` and by the user-supplied `ACTION`
  before `err_put` recorded the system error code. Now `errno` is
  saved at macro entry, used for `strerror()`, and restored before
  `err_raise`. New test:
  `sysraiseact_action_does_not_clobber_errno`.

- **`logsimple(0, ...)` misuses in `error.c`** (`err_increase`)
  replaced with `logsimpleerr` / `logsimpleret`.

### Added

- **`err_msg(int code, char *buf, size_t sz)`** — thread-safe
  `strerror` replacement that handles both POSIX and GNU variants of
  `strerror_r`.

- **`err_raise` fallback to `raise(SIGINT)`** when called with no
  active TRY frame. Visible in a debugger, produces a core dump.

- **22 new tests** (30 total):
  - TRY: normal path, catch, nested, three levels, 1000-iteration
    loops, `return` / `break` leaks, ring overflow (`ERR_MAX_TRY_CNT`,
    10 full wraps, 1.5 wraps).
  - trace: `err_fprintstacktrace` output for empty/user/system stacks,
    ordering, `err_clean`, return-value growth, heap growth past
    `ERROR_INIT_COUNT`.
  - signal: `err_raise` / `userraise` outside any TRY raise a real
    SIGINT (via Criterion `.signal`).
  - `sysraise`: `errno` recorded correctly, `ACTION` runs before raise.

## [0.1.0] - 2026-10-09

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

