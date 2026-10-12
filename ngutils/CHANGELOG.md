# Changelog

All notable changes to the ngutils collection will be documented in this
file. Per-utility details live in each utility's own `CHANGELOG.md`.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2026-10-12]

### Added

- `checker` — assert-style invariants (`inv`, `invraise`, `inv2`,
  `inv2raise`) with `_Generic` dispatch, and runtime validators
  (`check_arg`). Aborts on invariant violation; compiles out with
  `-DNOINVARIANT`.
- `common` — minimal cross-utility helpers, subset of the old
  monolithic `common.h`: `MIN`/`MAX`/`CASE_RETURN`, `G_GLOB_AVERAGE`,
  `Tincrease`/`round_up_2`/`calcnewsize`, `clower`/`cupper`,
  `rndint`/`rnd*char`, `reverse`, `try_parse_*`, `pchar_cmp`.
- `cloneproj` script + `make cloneproj NAME=x` — scaffold a new
  utility from `skeleton/`.
- `delete-docker` target — removes the `ngutils-dev` image.
- `clean-docker` target — wipes `/tmp/ngutils-docker-build`.
- `install`/`install-release` targets with `PREFIX` / `DESTDIR`.
- `analyze-test` target — clang static analyzer on tests (opt-in).

### Changed

- All five utilities now expose a uniform version API:
  `<prefix>_version()` / `<prefix>_versions()` and a
  `<PREFIX>_VERSION` macro. `versions[0]` is always the current one.
- `Makefile.common`: `BUILD_DIR ?= build`, `CFLAGS +=` instead of
  `?=`, `-std=gnu11` explicit, Criterion guard with
  `REQUIRE_CRITERION=1`.
- `test-docker` isolates per-project `build/` via bind-mounts to
  `/tmp/ngutils-docker-build/<project>`.

### Fixed

- `error`: `errno` preserved across `ACTION` and logging in raise
  macros.
- `error`: `err_msg()` no longer trusts `strerror_r`'s return value
  (macOS SDK quirk); falls back to `strerror()` if the buffer stays
  empty.
- `error`: `err_raise` outside TRY no longer jumps to a stale slot.

## [2026-10-09]

### Added

- `error` — per-thread error stack and signal-based exceptions
  (`err_raise`, `err_clean`, `try()`, `err_sethandler`). Depends on
  `log` and `bool`.
- `analyze-test` target — clang static analyzer on tests, opt-in.
  `analyze` itself now runs on library sources only.
- `clean-docker` — wipes `/tmp/ngutils-docker-build`.
- `install` target with `PREFIX` / `DESTDIR` support (debug archives).
- `DISCLAIMER.md` — no API stability before 1.0, no warranty, known
  gaps per utility, not thread-safe, not for safety-critical systems.

### Changed

- `Makefile.common`: extracted shared rules; each utility is now a
  2–6 line Makefile (`PROJNAME` + `REQUIRES` + `include`).
- `REQUIRES` declares cross-utility dependencies; the build adds both
  `-I` flags and link dependencies on siblings automatically.
- `test-docker` isolates per-project `build/` directories by mounting
  `/tmp/ngutils-docker-build/<project>` over `/work/<project>/build`.
  Host `build/` is never touched; incremental builds inside the
  container work.
- `MAKEFLAGS` forwarded into `test-docker`, so `make test-docker -B`
  and similar flags reach the inner make.
- `-std=gnu11` explicit; `CFLAGS +=` so env-supplied `CFLAGS` survives.
- Criterion guard: tests skip with an install hint when Criterion is
  missing, or fail hard if `REQUIRE_CRITERION=1` (used in CI and
  `test-docker`).
- `BUILD_DIR ?= build` so the value can come from the environment.

### Fixed

- `error`: `volatile sig_atomic_t` for the try-flag (POSIX signal-safe).
- `error`: `SIGEMT` / `SIGINFO` guarded by `#ifdef` — BSD/macOS only.
- `error`: dropped a spurious argument to `logsimple()`.
- `log/test`: `dup2` guarded against `dup()` failure.
- `Makefile`: `clean` uses `find -mindepth 1` so mount points survive.
- `test-docker`: literal `make` instead of `$(MAKE)` (host path may be
  invalid inside the container on Apple Make 3.81).

### Infrastructure

- GitHub Actions CI: `make all` on `ubuntu-latest` with
  `REQUIRE_CRITERION=1`.
- `Dockerfile.dev` for reproducible Linux builds.

## [2026-10-08]

### Added

- `bool` — tiny helpers to convert between `bool` and strings
  (`bool_str`, `bool_tryparse`, `bool_parsedef`, version API).
- `log` — file-based logging engine with per-module level filtering,
  configurable preambles, indent-aware macros, and `-DNODEBUG` support.
- Shared `Makefile.common`; each utility now sets only `PROJNAME`.
- Dispatcher targets: `all`, `lib`, `librelease`, `test`, `check`,
  `analyze`, `clean`, `install`, `docs`, `test-docker`.
- `libngutils.a` (debug and release) combining all utility objects.
- `Dockerfile.dev` for reproducible Linux builds.
- GitHub Actions CI running tests and building libraries on every push.

### Notes

- Each utility carries its own version, changelog, and release cadence.
- `ngutils` itself does not have a version number; the date of the
  umbrella release is used as the section heading.

