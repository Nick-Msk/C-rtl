# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-10-12

### Added

- Initial release. Minimal subset of the original monolithic
  `common.h` — only symbols actually consumed by `fs.h` / `fs.c`:
  - **Macros:** `MIN`, `MAX`, `CASE_RETURN`.
  - **Sentinel:** `G_GLOB_AVERAGE`.
  - **Growth policy:** `Tincrease` enum, `round_up_2`, `calcnewsize`.
  - **Character helpers:** `clower`, `cupper`.
  - **Random helpers:** `rndint`, `rndlowchar`, `rndupperchar`,
    `rnddigitchar`.
  - **C-string:** `reverse`.
  - **Parsing:** `try_parse_int`, `try_parse_long`, `try_parse_ulong`,
    `try_parse_double`.
  - **Comparators:** `pchar_cmp`, `pchar_revcmp`.
  - **26 unit tests** covering all of the above.
  - **Version API:** `common_version()`, `common_versions()`,
    `COMMON_VERSION` macro.

### Notes

- The original `common.h` (600+ lines, kitchen sink) is **not**
  included. Symbols it had that are not in this release —
  `compare_*`, `*_exch`, `item_exch`, `fill_*`, `clean_*`, `bits_*`,
  `print_*`, `IOCHECKER*`, `WRITE_OR_RET*`, `IS_COMPATIBLE`, `FREE`,
  `COUNT`, `STRING`, `LEAST`, `GREATEST`, `TYPEFORMAT`, `typeprint`,
  `int_in` / `int_notin`, `common_int_*`, `calcnewsize` umbrella
  additions — will be added back only when a consumer needs them.
- `try_parse_ulong` deliberately rejects a leading minus sign
  (`strtoul` accepts it as valid wrap-around; almost never what the
  caller intends).
- Depends on `log` for the unknown-strategy diagnostic in
  `calcnewsize`.

