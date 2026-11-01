# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Architecture shift: `fs2.*` is now the Protected layer

The previous plan of a separate `pfs2.h` / `cfs2.h` layer is dropped. The
current `fs2.*` / `fs2_api.*` code **is** the protected layer: every public
function validates its preconditions and aborts on contract violations via
`invfatal`. No compile-time flag disables these checks — a bug surfaces at
the call site, not as silent memory corruption downstream. The future bare
(unchecked) layer will live in `ufs2.*` with the same API and layout.

### Added

- **`invfatal(exception, expr, msg, ...)`** — always-on contract guard.
  Aborts with `SIGABRT` when `expr` is false; **not catchable** by `TRY()`
  (a broken invariant means undefined process state). Replaces the
  previous `checkraise`-as-`userraise` for invariant checks; distinct
  from `userraise`/`sysraise` (catchable `SIGINT` for runtime/OOM errors).
- **Comparators:** `fs2_cmp`, `fs2_ncmp`, `fs2_cmp_ci`, `fs2_ncmp_ci`,
  `fs2_cmpstr`. Wrappers for `qsort`/`bsearch`: `fs2_cmp_wrap`,
  `fs2_cmp_ci_wrap`.
- **Binary search:** `fs2_in_bsearch`, `fs2_in_bsearch_ci`,
  `fs2_in_bsearch_str` — return index or `-1`.
- **Substring search (`instr`):** `fs2_instr_posn` (base), `fs2_instr_from`,
  `fs2_instr_nth`, `fs2_instr`, plus `_ci` and `_str` variants.
  Empty pattern matches at every position (Python semantics: `"abc".find("")`
  → `0`, `nth` → `k-1` up to `len+1`).
- **Scanners:** `fs2_spn`, `fs2_cspn`, `fs2_nonspace_span` — length of
  matching / non-matching prefix, locale-independent whitespace skip.
- **Predicates (text):** `fs2_starts_with` / `fs2_ends_with` / `fs2_contains`
  / `fs2_count` (+ `_str` variants). `fs2_starts_with(s, "")` → true,
  `fs2_contains(s, "")` → true, `fs2_count(s, "")` → 0.
- **Trim (PostgreSQL style):** `fs2_trim` (= `fs2_btrim`), `fs2_ltrim`,
  `fs2_rtrim` + `_str` variants. Character-set trim; not substring.
- **Case:** `fs2_initcap` (PostgreSQL semantics, non-alnum boundaries).
  All case functions use `(unsigned char)` casts for UTF-8 safety.
- **Replace:** `fs2_replace`, `fs2_replace_fromstr`, `fs2_replace_cstr`
  — replace all non-overlapping occurrences (`"aaa"` / `"aa"` → `"ba"`).
- **Strip:** `fs2_strip_prefix`, `fs2_strip_suffix` + `_str` variants,
  plus `_new` and `_heap_` constructors. No-op when pattern doesn't match.
- **Repeat:** `fs2_repeat(s, n)` (in-place), `fs2_repeat_new`,
  `fs2_heap_repeat`, `fs2_repeat_str_new`, `fs2_heap_repeat_str`.
- **Slice:** `fs2_slice(s, from, len)` — zero-copy `STATIC` view.
- **Capacity management:** `fs2_setsize` (exact grow, no rounding,
  no `FS_MIN_ALLOC` floor), `fs2_shrink` (exact `len+1`, no-op for
  STATIC/LOCAL — capacity hint). `fs2_reserve` and `fs2_setsize` are
  grow-only; `shrink` is the only shrinker.
- **`fs2_promote_empty(s)`** — internal: canonical `FS2()` (`v==NULL`) →
  `FS2EMPTY` shape, preserving `BODYALLOC`. Used by all grow paths.
- **Shape predicates:** `fs2_eq_empty(s)` (strict `FS2EMPTY` shape),
  `fs2_is_canon(s)` (strict `FS2()` shape).
- **Adopt:** `fs2_adopt(char **)`, `fs2_adoptn(char **, len)` — take
  ownership of a heap C-string. Preserves `FS2_ALLOC` on empty (`adoptn("")`
  → `ALLOC empty`, not `FS2EMPTY`) — consistent with `movefrom`.
- **Stack↔heap converters:** `fs2_stacktoheap(fs2 *)`,
  `fs2_heaptostack(fs2 **)` — source reset to `NULL` / `FS2EMPTY` resp.
- **Swap:** `fs2_swap(a, b)` (value swap), `fs2_exch(a, b)` (buffer-identity
  swap; requires both `FS2_ALLOC`, preserves `BODYALLOC` per side).
- **Generic serialization:** `_Generic` macros over `char`, `short`, `int`,
  `long`, `long long`, unsigned variants, `float`, `double`, pointers:
  `fs2_from_new(val)`, `fs2_heap_from(val)`, `fs2_serialize(dst, val)`,
  `fs2_serialize_concat(dst, val)`.
- **Formatting:** `fs2_sprintf_concat` — append formatted text (updates len).
- **Parsers (3-level abstraction):**
  - `fs2_pos_parse_*(s, pos, &newpos, &out)` — raw scanner.
  - `fs2_tryparse_*(s, &out, strict)` — parses from 0, strict mode requires
    consuming the whole string.
  - `fs2_parse_*(s)` — lenient tryparse, raises on failure.
  - Supported: `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `llong`,
    `ullong`, `double`, `char`.
  - All `pos_parse`/`tryparse`/`parse` wrappers chain through `llong` / `ullong`
    (the widest type) and check the target range on the way back.
- **Guards (internal):** `fs2_cant_mutate(s)` (empty + STATIC), 
  `fs2_cant_growth(s)` (NULL + non-empty STATIC), `fs2_cant_realloc(s)`.
- **`__clang_analyzer__` guard** around `free(s->v)` in `fs2_freestr`
  — `scan-build` can't prove `v` is malloc'd (no data-flow from `flags`
  to `v`); the runtime path is proven safe.
- **Makefile:** `merge-logs-<proj>` target — idempotent, fold raw logs into
  `logs/result/all.log` after a failed test run.

### Changed

- **Naming convention:** case-insensitive functions use the `_ci` suffix
  globally (`fs2_chr_ci`, `fs2_in_ci`, `fs2_cmp_ci`, `fs2_instr_ci`)
  instead of the `i` prefix. Avoids unreadable collisions like `iinstr`
  and aligns with SQL collation convention (`utf8mb4_general_ci`).
- **`fs2_reverse` → `fs2_flip`** — `reserve` and `reverse` are visually
  confusable; `flip` is unambiguous.
- **`fs2_wrap`**: `NULL` → `FS2EMPTY` (already in 0.2.0, reaffirmed).
- **`fs2_move` semantics:** source is reset to `FS2EMPTY` (safe empty,
  `v==""`, `sz=1`) instead of `FS2()` (`v=NULL`). Both sides end up safe
  empty after move. Callers can `printf("%s", dst.v)` and `src.v` without
  NULL-checks; `fs2_reserve` on the source auto-promotes it back to `ALLOC`.
- **Empty results from constructors:** all constructors return `FS2EMPTY`
  for empty results, so callers never have to NULL-check `v`. `fs2_clone`,
  `fs2_fromstr`, `fs2_substr_new`, `fs2_fromstrf_va` (`sz==0` case),
  `fs2_repeat_new` (`n==0`), etc. — all `FS2EMPTY`, not `FS2()`.
- **`fs2_fromstrf_va`:** returns `FS2EMPTY` when `sz == 0` (previously
  returned an `ALLOC` empty).
- **`fs2_setsize`:** exact grow, no `FS_MIN_ALLOC` floor, no power-of-2
  rounding. Use `fs2_reserve` for amortized growth, `fs2_shrink` for shrink.
- **`fs2_shrink`:** no longer aborts on non-empty STATIC/LOCAL — silent
  no-op (capacity hint, like `std::string::shrink_to_fit`). Only `NULL`
  aborts. Exact `len+1` (no floor).
- **`fs2_free`:** documented idempotency for non-`BODYALLOC` values
  (after `freestr`, state is canonical, second call is no-op). `BODYALLOC`
  values are not idempotent (freeing freed memory is UB).
- **C standard:** `gnu11` → `gnu2x` (C23) for digit separators and stricter
  `-Wformat` diagnostics. Requires GCC 14+ / Clang 16+.
- **POSIX extensions:** `-D_GNU_SOURCE` in `Makefile.common` exposes
  `strcasestr` and `strtod_l`.
- **`invfatal` guard order:** exception code first, boolean expr second —
  `invfatal(ERR_*, cond, msg)`. (Several call sites had args swapped;
  fixed below.)

### Fixed

- **`fs2_cat` overflow:** `memcpy(dst, src, len + 1)` wrote 1 byte past
  `src` — fixed to `src->len`.
- **`fs2_rev_catstr` overlap:** `memcpy` → `memmove` for the in-place
  shift.
- **`fs2_lpad` infinite loop:** iterated over `pad->len` instead of
  `pad_len` (missing bytes).
- **`fs2_rpad` truncation:** kept the right part instead of the left
  (PostgreSQL semantics: `rpad("hello", 2)` → `"he"`).
- **`fs2_sprintf`:** `vsnprintf` consumed the last character (missing
  `+1` for NUL); `fs2_setlen` now called with the correct length.
- **`fs2_cpy` on empty:** SIGSEGV when `dst` was canonical `FS2()`;
  fixed via `fs2_promote_empty` in the reserve path.
- **`fs2_setlen`:** correct handling of `pos == len` on non-writable
  fs2 (no-op, not abort). Readonly path checks `pos == s->len`, not
  `pos > 0`.
- **`fs2_setsize` / `fs2_reserve`:** `sz == need` no-op (was
  realloc-every-time in one variant). Distinguish grow-only (`reserve`,
  `setsize`) from exact-shrink (`shrink`).
- **`fs2_move`:** `src` reset to `FS2EMPTY` (was `FS2()`, leaving
  `v==NULL` — `printf` crashed).
- **`invfatal` args swapped** in `fs2_move`, `fs2_exch`,
  `fs2_heaptostack`, `fs2_stacktoheap` — `!fs2_is_local(...)` was
  passed as exception, `ERR_*` as expr → check passed always, abort
  never fired. Fixed to `invfatal(ERR_*, !fs2_is_local(...), ...)`.
- **`fs2_heaptostack`:** `res.flags &= ~FS2_BODYALLOC` was dead code
  (`fs2_move` already clears BODYALLOC on dst); removed.
- **`fs2_stacktoheap` / `fs2_heaptostack`:** duplicate `!fs2_is_local`
  guard removed (fs2_move internally rejects LOCAL).
- **Parsers:** reject `\n`/`\r` by manually skipping only spaces and
  tabs (`skip_spaces` in `common`); `strtol`/`strtod` would silently
  eat newlines.
- **`tolower` / `toupper` UB:** `(unsigned char)` cast on non-ASCII bytes.
- **`rndint` off-by-one:** inclusive bounds correctly (`rndint(25)` for
  `a-z`).
- **`fs2_fromstrf_va`:** `va_end(ap2)` moved before the `sz < 0` check
  (noreturn path skipped `va_end`).
- **`Makefile.common`:** log merge runs even when tests fail
  (`; status=$$?; ...; exit $$status`); `clean-logs` uses `find -delete`
  (avoids `ARG_MAX` on thousands of files); `arch-logs` removed from
  `clean-logs` prereq (no more duplicate gz archives).

### Design notes

- **Finders semantics:** NUL is a terminator, not content.
  `fs2_chr(&s, '\0')` returns `-1`, consistent with `memchr`.
- **Three error tiers:** `invfatal` (contract violation, `SIGABRT`,
  not catchable), `userraise` (runtime error, `SIGINT`, catchable via
  `TRY()`), `sysraise` (OOM, `SIGINT`, catchable).
- **`fs2_move` vs `fs2_clone`:** `clone(empty)` → `FS2EMPTY` (normalize);
  `movefrom(empty)` preserves the source shape (`FS2()` stays `FS2()`
  before move; `FS2EMPTY` stays `FS2EMPTY`).
- **`fs2_adoptn("")`:** preserves `FS2_ALLOC` (adopts the 1-byte heap
  buffer). Not normalized to `FS2EMPTY` — adopt is a transfer, not a
  constructor.
- **`fs2_shrink`:** capacity hint (no-op for non-ALLOC); use
  `fs2_setsize` for exact capacity with error reporting.
- **`fs2_free`:** idempotent for non-`BODYALLOC`; after `BODYALLOC`
  free, calling again is UB.
- **`fs2_cant_growth` vs `fs2_cant_mutate`:** `cant_growth` aborts on
  `NULL` and non-empty STATIC (calls from grow paths); `cant_mutate`
  aborts only on non-empty STATIC (empty is a silent skip — nothing to
  mutate).
- **Locales:** `fs2_parse_double` uses standard `strtod` and respects
  `LC_NUMERIC`. Call `setlocale(LC_NUMERIC, "C")` at app startup for
  locale-independent parsing.
- **OOM handling:** `sysraise(...)` — an enclosing `TRY()` can catch it.
- **CI workaround:** `-Wno-error=format-truncation` for GCC 13+ in
  `REL_CFLAGS` (GCC Bugzilla #102218, `_FORTIFY_SOURCE=3` false positive).
- **`fs2_free` analyzer workaround:** `#ifndef __clang_analyzer__`
  guard around `free(s->v)` — scan-build can't prove `v` is malloc'd;
  runtime is proven safe (all callers guarantee `fs2_is_alloc(s)`).

## [0.2.0] - 2026-10-26

This release introduces the entire `fs2_api.h` layer, transforming the
library from a core struct wrapper into a fully-fledged string manipulation
utility.

### Added

- **Internal memory wrappers:** `fs2_memcpy`, `fs2_memcpy_str`,
  `fs2_memmove`, `fs2_selfmove`. Safe copying/moving, handling `realloc`
  and overlaps.
- **Constructors (Value + Heap):** `fs2_fromstr`, `fs2_fromstrf`,
  `fs2_clone`, `fs2_movefrom`, `fs2_wrap` (zero-copy). Full `fs2_heap_*`
  twins.
- **Mutation (Append/Overwrite):** `fs2_cat`, `fs2_catstr`,
  `fs2_rev_catstr` (prepend), `fs2_cpy`, `fs2_cpystr`.
- **Substrings:** `fs2_substr`, `fs2_substr_new`, `fs2_heap_substr`.
- **Left/Right (PostgreSQL style):** `fs2_left`, `fs2_right`.
- **Padding (PostgreSQL style):** `fs2_lpad`, `fs2_rpad`.
- **Case Conversion:** `fs2_toupper`, `fs2_tolower`.
- **Random Generators:** `fs2_genrnd`.
- **Character Finders:** `fs2_chr`, `fs2_rchr`, etc.
- **Array Finders:** `fs2_in`, `fs2_notin`.
- **Formatting:** `fs2_sprintf`, `fs2_sprintf_position`.
- **Makefile improvements:** `merge-logs` target — always collect raw logs
  into `all.log`, even when tests fail.
- **CI Matrix:** separated jobs for `test (clang)`, `test (gcc)`,
  `sanitize (lsan)`, `sanitize (ubsan)`.

### Changed

- **`fs2_setlen`** safely handles canonical empty strings (`FS2()`).
- **`FS2WRAP` macro** replaced with the `fs2_wrap` inline function,
  fixing multi-evaluation issues.
- **Primitive / Protected split:** zero-checks in primitive (`fs2_*`),
  validation moved to planned `pfs2_*` *(later dropped — see Unreleased)*.

### Fixed

- **`fs2_rev_catstr` overlap:** `memcpy` → `memmove` for overlapping regions.
- **`fs2_rpad` truncation:** keep the *left* part of the string.
- **`fs2_cpy` on empty string:** SIGSEGV on canonical empty `FS2()`.
- **`rndint` off-by-one:** inclusive bounds.
