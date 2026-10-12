# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-10-12

- **36 unit tests** (was 33, +3 for the version API), including
  5 SIGABRT-abort cases.

### Added

  - Initial release.
  - **Invariant macros** (`inv*`) — assertions for development and
  testing, compiled out with `-DNOINVARIANT`:
  - `inv(expr, msg, ...)` / `inv2(a, b, msg, ...)` — returning forms
    (log on failure, return `bool`).
  - `invraise(expr, msg, ...)` / `inv2raise(a, b, msg, ...)` —
    aborting forms (log on failure, `abort()` → SIGABRT + core dump).
  - `inv2` / `inv2raise` dispatch on argument type via `_Generic`;
    both values are printed on mismatch.
  - **Runtime validators** (`check_*`) — always enabled:
  - `check_arg(min_argc, msg, ...)` — verifies `argc` in `main`.
  - Per-type equality converters used by `_Generic`:
  `inv_int_bool`, `inv_long_bool`, `inv_double_bool`,
  `inv_str_bool`, `inv_ptr_bool`.
  - `inv_fprintf_int()` — core failure reporter (stream + log).
  - **33 unit tests**, including 5 SIGABRT-abort cases.
  - **Version API:** `checker_version()`, `checker_versions()`,
  `CHECKER_VERSION` macro.
### Notes

- Requires GNU extensions (`({ ... })`, `_Generic`,
  `__attribute__((format))`), compiles with `-std=gnu11`.
- `inv*` semantics mirror `assert()`: **not catchable by TRY()**, and
  intended to be disabled in release builds.
- `check_*` are for real production paths and must stay enabled.
- Depends on `log` for diagnostic output.

