# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Unit tests for `bool_str` using Criterion: truth values, distinct string
  literals, and pointer stability across calls.
- `Makefile` with `test` and `clean` targets; test binaries are written to
  `build/debug/`.
- `include/` directory for public headers.

### Changed

- Moved `bool.h` from the project root into `include/`.
- README updated for the new layout and build/test workflow.
- Renamed include guard from `_BOOL_H_` to `BOOL_H_` to avoid reserved-identifier UB.
- Fixed mismatched `#endif` comment.

## [0.1.0]

### Added

- `bool_str(bool)` — returns `"true"` or `"false"` as a `const char *`.

