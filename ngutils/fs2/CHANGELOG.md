# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Comparators:** `fs2_cmp`, `fs2_ncmp`, `fs2_cmp_ci`, `fs2_ncmp_ci`, `fs2_cmpstr` (fs2 vs C-string). Wrappers for `qsort`/`bsearch`: `fs2_cmp_wrap`, `fs2_cmp_ci_wrap`.
- **Binary Search:** `fs2_in_bsearch`, `fs2_in_bsearch_ci`, `fs2_in_bsearch_str`. Returns the index (or `-1`) instead of `bool`.
- **Substring Search (`instr`):**
  - `fs2_instr_posn` (base), `fs2_instr_from`, `fs2_instr_nth`, `fs2_instr`.
  - Case-insensitive variants with `_ci` suffix (e.g., `fs2_instr_posn_ci`).
  - Overlapping matches are counted separately (like `grep -o`).
- **Scanners:** `fs2_spn`, `fs2_cspn`, `fs2_nonspace_span` (locale-independent whitespace skip).
- **Generic Serialization:** `_Generic` macros supporting `char`, `int`, `long`, `long long`, unsigned types, `float`, `double`, `short`, `unsigned short`, and pointers.
  - `fs2_from_new(val)` — create stack `fs2`.
  - `fs2_heap_from(val)` — create heap `fs2`.
  - `fs2_serialize(dst, val)` — overwrite `dst`.
  - `fs2_serialize_concat(dst, val)` — append to `dst`.
- **Formatting:** `fs2_sprintf_concat` — append formatted text to the end of a string.
- **Parsers (3-level abstraction):**
  - Level 1 (`fs2_pos_parse_*`): raw scanner starting at `pos` with `newpos` out-param.
  - Level 2 (`fs2_tryparse_*`): parses from start, `strict` flag requires consuming the whole string.
  - Level 3 (`fs2_parse_*`): calls `tryparse` in lenient mode, raises `userraise` on failure.
  - Supported types: `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `llong`, `ullong`, `double`, `char`.
- **`fs2_slice`**: zero-copy STATIC view (like a Rust `&[T]` slice).

### Changed

- **Naming Convention:** Case-insensitive functions now use the `_ci` suffix globally (e.g., `fs2_chr_ci`, `fs2_in_ci`, `fs2_cmp_ci`) instead of the `i` prefix. This avoids unreadable collisions like `iinstr` and aligns with SQL collation conventions (`utf8mb4_general_ci`).
- **C Standard:** Build system bumped from `gnu11` to `gnu2x` (C23) to support digit separators and stricter `-Wformat` checks.
- **POSIX Extensions:** Added `-D_GNU_SOURCE` in `Makefile.common` to expose `strcasestr` and `strtod_l`.

### Fixed

- **`fs2_setlen`** safely handles canonical empty strings (`FS2()` with `v == NULL`). Raises a user error if `pos > 0` on a NULL buffer.
- **`fs2_cat` overflow:** Fixed a critical bug where `memcpy(dst, src, len + 1)` wrote 1 byte beyond the `src` buffer.
- **`fs2_lpad` infinite loop:** Fixed loop condition to iterate over missing length, not `pad->len`.
- **`fs2_sprintf`:** Fixed `vsnprintf` consuming the last character (added `+1` for NUL).
- **Parsers:** Safely reject `\n` and `\r` by manually skipping only spaces and tabs (`skip_spaces` in `common`), preventing `strtol`/`strtod` from silently consuming newlines.
- **UB in `tolower`/`toupper`:** Added `(unsigned char)` casts to prevent undefined behavior on non-ASCII bytes.

### Design notes

- **Finders semantics**: NUL is treated as a terminator, not content. `fs2_chr(&s, '\0')` returns `-1`, consistent with `memchr`.
- **Parsers & Locales**: `fs2_parse_double` uses standard `strtod` and respects `LC_NUMERIC`. To parse JSON/CSV regardless of user locale, call `setlocale(LC_NUMERIC, "C")` at app startup.
- **OOM handling**: Allocation failures route through `sysraise(...)`. An enclosing `TRY()` can catch them.
- **CI workaround**: Added `-Wno-error=format-truncation` for GCC 13+ in `REL_CFLAGS` to bypass a known false positive in `_FORTIFY_SOURCE=3` (GCC Bugzilla #102218).

## [0.2.0] - 2026-10-26

This release introduces the entire `fs2_api.h` layer, transforming the library from a core struct wrapper into a fully-fledged string manipulation utility.

### Added

- **Internal memory wrappers:** `fs2_memcpy`, `fs2_memcpy_str`, `fs2_memmove`, `fs2_selfmove`. Safe copying/moving, handling `realloc` and overlaps.
- **Constructors (Value + Heap):** `fs2_fromstr`, `fs2_fromstrf`, `fs2_clone`, `fs2_movefrom`, `fs2_wrap` (zero-copy). Full `fs2_heap_*` twins.
- **Mutation (Append/Overwrite):** `fs2_cat`, `fs2_catstr`, `fs2_rev_catstr` (prepend), `fs2_cpy`, `fs2_cpystr`.
- **Substrings:** `fs2_substr`, `fs2_substr_new`, `fs2_heap_substr`.
- **Left/Right (PostgreSQL style):** `fs2_left`, `fs2_right`.
- **Padding (PostgreSQL style):** `fs2_lpad`, `fs2_rpad`.
- **Case Conversion:** `fs2_toupper`, `fs2_tolower`.
- **Random Generators:** `fs2_genrnd`.
- **Character Finders:** `fs2_chr`, `fs2_rchr`, etc.
- **Array Finders:** `fs2_in`, `fs2_notin`.
- **Formatting:** `fs2_sprintf`, `fs2_sprintf_position`.
- **Makefile improvements:** Added `merge-logs` target to always collect raw logs into `all.log`, even when tests fail.
- **CI Matrix:** Separated jobs for `test (clang)`, `test (gcc)`, `sanitize (lsan)`, `sanitize (ubsan)`.

### Changed

- **`fs2_setlen`** safely handles canonical empty strings (`FS2()`).
- **`FS2WRAP` macro** replaced with the `fs2_wrap` inline function, fixing multi-evaluation issues.
- **Primitive / Protected split**: Zero-checks in primitive (`fs2_*`), validation moved to planned `pfs2_*`.

### Fixed

- **`fs2_rev_catstr` overlap**: Replaced `memcpy` with `memmove` for overlapping regions.
- **`fs2_rpad` truncation**: Fixed logic to keep the *left* part of the string.
- **`fs2_cpy` on empty string**: Fixed SIGSEGV on canonical empty `FS2()`.
- **`rndint` off-by-one**: Fixed generator to use inclusive bounds.

