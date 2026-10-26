# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned

- **Comparators** (`fs2_cmp`, `fs2_icmp`, `fs2_ncmp`) — needed before `bsearch` and sorting.
- **Binary search finders** (`fs2_in_bsearch`, etc.) — depends on comparators.
- **Substring search** (`fs2_strstr`, `fs2_istrstr`).
- **Sorting** (`fs2_sort` with custom comparator).
- **Protected layer** (`pfs2.h`, `pfs2.c`) — wraps every primitive with `userraise`/`sysraise` checks.
- **Checked header** (`cfs2.h`) — header-only guards that compile out under `-DNOINVARIANT`.

## [0.2.0] - 2026-10-26

This release introduces the entire `fs2_api.h` layer, transforming the library from a core struct wrapper into a fully-fledged string manipulation utility.

### Added

- **Internal memory wrappers:** `fs2_memcpy`, `fs2_memcpy_str`, `fs2_memmove`, `fs2_selfmove`. These provide safe copying/moving, handling `realloc` and overlapping regions without dangling pointers.
- **Constructors (Value + Heap):** `fs2_fromstr`, `fs2_fromstrf`, `fs2_clone`, `fs2_movefrom`, `fs2_movefromstr`, `fs2_move`, `fs2_wrap` (zero-copy view). Full `fs2_heap_*` twins for all of them.
- **Mutation (Append/Overwrite):** `fs2_cat`, `fs2_catstr`, `fs2_rev_catstr` (prepend), `fs2_cpy`, `fs2_cpystr`.
- **Substrings:** `fs2_substr`, `fs2_substr_new`, `fs2_heap_substr`. Helper `fs2_substr_bounds` for clamping `from`/`len`.
- **Left/Right (PostgreSQL style):** `fs2_left`, `fs2_right` (+ `_new`, `_heap`).
- **Padding (PostgreSQL style):** `fs2_lpad`, `fs2_rpad` (+ `_new`, `_str`).
- **Case Conversion:** `fs2_toupper`, `fs2_tolower` (+ `_new`, `_heap`). Safe for UTF-8 bytes (uses `unsigned char` casts).
- **Random Generators:** `fs2_genrnd` (+ `_new`, `_heap`) with modes `'a'`, `'A'`, `'0'`.
- **Character Finders:** `fs2_chr`, `fs2_nchr`, `fs2_rchr`, `fs2_ichr`, `fs2_nichr`, `fs2_irchr`, `fs2_nirchr`. All return `ptrdiff_t` and never match `'\0'`.
- **Nth-Occurrence Finders:** `fs2_chr_nth`, `fs2_nchr_nth`, etc. `k` is 1-based.
- **Array Finders:** `fs2_in`, `fs2_notin`, `fs2_iin`, `fs2_not_iin` for NULL-terminated arrays. Variadic macros `FS2_IN`, `FS2_IIN`, `FS2_NOT_IN`, `FS2_NOT_IIN`.
- **Formatting:** `fs2_sprintf`, `fs2_sprintf_position`, `fs2_vsprintf_position`.
- **Makefile improvements:** Added `merge-logs` target to always collect raw logs into `all.log`, even when tests fail.
- **CI Matrix:** Separated jobs for `test (clang)`, `test (gcc)`, `sanitize (lsan)`, `sanitize (ubsan)`.

### Changed

- **`fs2_setlen`** now safely handles canonical empty strings (`FS2()` with `v == NULL`). If `pos > 0` on a NULL buffer, it raises a user error (prevents silent memory corruption).
- **`FS2WRAP` macro** replaced with the `fs2_wrap(const char *str)` inline function, fixing multi-evaluation issues and `NULL` pointer crashes.
- **Primitive / Protected split**: Zero-checks in primitive (`fs2_*`), all validation moved to the planned `pfs2_*` layer. `invraise` removed from `fs2_elem`, `fs2_get`, `fs2_partialsort`.

### Fixed

- **`fs2_cat` overflow**: Fixed a critical bug where `memcpy(dst, src, len + 1)` wrote 1 byte beyond the `src` buffer, corrupting adjacent heap memory.
- **`fs2_rev_catstr` overlap**: Replaced `memcpy` with `memmove` to correctly handle overlapping regions when shifting the string right.
- **`fs2_rpad` truncation**: Fixed logic to keep the *left* part of the string when `len < str->len` (was incorrectly using `fs2_right`).
- **`fs2_lpad` infinite loop**: Fixed an endless `while` loop in the padding logic (was conditioned on `j < pad->len` instead of `i < pad_len`).
- **`fs2_cpy` on empty string**: Fixed a SIGSEGV when calling `fs2_cpy` on a canonical empty `FS2()` due to calling `fs2_setlen` on a NULL buffer.
- **`fs2_partialsort` underflow**: Fixed a `size_t` underflow crash if `from > to`.
- **`tolower`/`toupper` UB**: Added `(unsigned char)` casts to prevent Undefined Behavior on non-ASCII bytes (e.g., UTF-8).
- **`rndint` off-by-one**: Fixed generator to use inclusive bounds correctly (e.g., `rndint(25)` for `a-z`).

### Design notes

- **Finders semantics**: NUL is treated as a terminator, not content. `fs2_chr(&s, '\0')` returns `-1`, consistent with `memchr`.
- **OOM handling**: Allocation failures route through `sysraise(...)` from `error.h`. An enclosing `TRY()` can catch them.
- **CI workaround**: Added `-Wno-error=format-truncation` for GCC 13+ in `REL_CFLAGS` to bypass a known false positive in `_FORTIFY_SOURCE=3` (GCC Bugzilla #102218).
- **Tests**: Added over 300 unit and composite tests, including ASan/LSan/UBSan coverage in Docker.

