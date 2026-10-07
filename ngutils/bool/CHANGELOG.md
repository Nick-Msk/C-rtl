# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] - 2026-10-07

### Added

- `bool_tryparse(str, out)` — case-insensitive parse of `"true"` / `"false"`.
  Returns success flag; `out` may be `NULL`; `*out` is untouched on failure.
- `bool_parsedef(str, def)` — convenience wrapper with a fallback value.
- `bool_version()` — current version string.
- `bool_versions()` — NULL-terminated array of published versions, newest first.
- `BOOL_VERSION` macro holding the current version literal.
- Unit tests for parsing (case, no-trim, no-synonyms), NULL handling,
  fallbacks, and version invariants.
- Unit tests for `bool_str` using Criterion: truth values, distinct string
  literals, and pointer stability across calls.
- `Makefile` with `test` and `clean` targets; test binaries are written to
  `build/debug/`.
- `include/` directory for public headers.

### Changed

- `bool/Makefile` migrated to `pkg-config` for portability (macOS + Linux).
- Version API lives in `src/bool.c` so the published-versions array is
  defined in exactly one translation unit.


### Changed

- Moved `bool.h` from the project root into `include/`.
- README updated for the new layout and build/test workflow.
- Renamed include guard from `_BOOL_H_` to `BOOL_H_` to avoid reserved-identifier UB.
- Fixed mismatched `#endif` comment.

## [0.1.0] - 2026-10-06

### Added

- `bool_str(bool)` — returns `"true"` or `"false"` as a `const char *`.

