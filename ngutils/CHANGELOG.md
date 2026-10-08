# Changelog

All notable changes to the ngutils collection will be documented in this
file. Per-utility details live in each utility's own `CHANGELOG.md`.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2026-10-08]

### Added

- `bool` — tiny helpers to convert between `bool` and strings
  (`bool_str`, `bool_tryparse`, `bool_parsedef`, version API).
- `log` — file-based logging engine with per-module level filtering,
  configurable preambles, indent-aware macros, and `-DNODEBUG` support.
- Shared `Makefile.common` — per-project rules extracted; each utility
  is now a 2–6 line Makefile (`PROJNAME` + `include`).
- Dispatcher targets: `all`, `lib`, `librelease`, `test`, `check`,
  `analyze`, `clean`, `install`, `docs`, `test-docker`.
- `libngutils.a` (debug and release) combining all utility objects.
- `install` target with `PREFIX`/`DESTDIR` support.
- `Dockerfile.dev` for reproducible Linux builds and CI parity.
- GitHub Actions CI running tests and building libraries on every push.

### Notes

- Each utility carries its own version, changelog, and release cadence.
- `ngutils` itself does not have a version number; the date of the
  umbrella release is used as the section heading.

