# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

