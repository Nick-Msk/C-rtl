# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added — fs2array

New container: `fs2array` — growable array of `fs2` **values** (not pointers),
with hole-based detach, packed/tight invariants, and a complete set of
build/sort/merge/search/join operations.

#### Core struct

```c
typedef struct fs2array {
    fs2    *arr;    /* heap array of fs2 values (never BODYALLOC inside) */
    size_t  hwm;    /* high-water mark: [0, hwm) may contain holes       */
    size_t  size;   /* capacity in elements                             */
    size_t  count;  /* live (non-hole) elements in [0, hwm)            */
} fs2array;

#define FS2ARR()  (fs2array){ .arr = NULL, .hwm = 0, .size = 0, .count = 0 }
```

Invariants: `count <= hwm <= size`. Holes only in `[0, hwm)`; padding
`[hwm, size)` is always holes. `fs2arr_validate` checks all of this.

#### Slots — hole sentinel

`FS2_NULL_ARRAY_ELEMENT = 0x100` flag in `fs2_flags`. Never set by any `fs2`
constructor — only by `fs2arr` operations. Distinguishes:

| Slot state | `fs2_is_hole` | `fs2_is_empty` |
|---|---|---|
| Hole | **true** | true |
| Live `FS2EMPTY` (empty string) | false | true |
| Live `"..."` | false | false |

New predicates:
- `fs2_is_hole(slot)` — `slot->flags == FS2_NULL_ARRAY_ELEMENT` (moved to `fs2.h`)
- `fs2arr_ispacked(a)` — `hwm == count` (precondition for sort/unique/reverse/bsearch/merge)

#### Lifecycle

`init(cnt)`, `reserve(a, need)`, `clean(a, from, to)` (range), `cleanall(a)`,
`free(a)`, `shrink(a)`, `compact(a)`, `synchwm(a)`.

- `init(n)` — capacity `>= n`, `count == 0`. No live elements.
- `shrink` — realloc down to `hwm`; canonical reset when `hwm == 0`.
- `compact` — pack live to `[0, count)`, holes to `[count, size)`, `hwm = count`.
- `synchwm` — trim trailing holes only, no move.

#### Build & mutate

`push` (+`_sp`/`_dp` via `_Generic`), `pushstr`, `attach` (same `_Generic`),
`setat` (+`_str`), `detachto`, `detach_new`, `heap_detach`.

- `push(a, src)` — `_Generic` dispatch: `fs2 *` → `_sp` (stack, source → FS2EMPTY);
  `fs2 **` → `_dp` (heap, source nulled, struct freed).
- `pushstr` / `setat_str` — `fs2_fromstr` (owned copy), **not** `_fs2_wrap`.
- `attach` — same dispatch as `push`; for filling holes; raises on non-hole slot.
- `setat` — replace-if-live, attach-if-hole; extends `hwm` if `idx >= hwm`.
- `detachto` / `detach_new` / `heap_detach` — move out, slot becomes hole,
  `count--`, `hwm` unchanged.

#### Constructors

`clone(src)` (packed, tight), `cloneexact(src)` (preserves `hwm`, holes),
`fromstrarr_new(cstr[])`, `fromarr_new(const fs2 *const[], cnt)` (deep copy),
`fromarr_move(fs2 *[], cnt)` (move), `split_toarray(s, delim)`,
`concat(a, b)` / `concat_new(a, b)`, `mergesorted_new(a, b, cmp)`.

`FS2ARRFROMSTR(...)` — variadic macro for `fromstrarr_new`.

#### Transform

`sort(a, cmp)` / `_asc` / `_desc` / `_asc_nullbefore` / `_asc_nullafter` /
`_desc_nullbefore` / `_desc_nullafter`, `unique(a, cmp)` (+`_new`),
`reverse(a)` (+`_new`).

Precondition for sort/unique/reverse: packed (`fs2arr_ispacked`).
Null-variants of sort handle holes as `v == NULL` — `_asc_nullafter` and
`_desc_nullbefore` effectively compact + sort in one call.

#### Query & search

`get(a, idx)`, `find_tillhwm(a)`, `issorted(a, cmp)`, `ispacked(a)`,
`eqexact(a, b)`, `validate(out, a)`, `bsearch(a, key, cmp)` (+`_str`),
`find(a, key, cmp)` (+`_str`), `contains`, `count`.

- `get` returns the slot pointer (may be a hole — check `fs2_is_hole`).
- `find` / `count` work on any array (holes skipped); `bsearch` requires packed.
- `eqexact` — semantic comparison up to `hwm`, holes match holes,
  live match live via `fs2_cmp`.

#### Join / concat

`join(dst, src)`, `join_delim(dst, src, delim)` (+`_str`) — append live
elements to `fs2 dst` with optional separator. `dst` acts as first element:
non-empty dst → delimiter before the first src element too.

#### Tech

`fs2arr_techfprint(out, a, name)` + macros `FS2ARRTECHFPRINT` /
`FS2ARRTECHPRINT` / `FS2ARRTECHLOG`. Shows `hwm`/`size`/`count`, up to
`FS2ARR_TECH_PRINT_COUNT` slot lines, `<hole>` markers, padding summary.

#### Contracts summary

- `_new` constructors return **packed** arrays (`hwm == count`), capacity
  rounded to power-of-2 (floor `FS2_MIN_ALLOC`). Call `fs2arr_shrink` for tight.
- In-place mutators on non-empty STATIC abort (`invfatal`, SIGABRT).
- `fs2_is_empty(slot)` is **not** a hole check — `FS2EMPTY` slots are live.
- Precondition for `sort`/`unique`/`reverse`/`bsearch`/`mergesorted_new`:
  packed. Call `fs2arr_compact` after `detach`.
- Hole-vs-empty-string distinction is fundamental — `eqexact` and
  `fs2arr_validate` enforce it.

#### Changed — fs2array

- `fs2arr_is_hole` renamed to `fs2_is_hole`, moved to `fs2.h` (unified
  with other `fs2_is_*` predicates; used by both `fs2array` layers).
- `fs2arr_push` / `fs2arr_attach` return `ptrdiff_t` (slot index) instead
  of `fs2 *`. Pointer was useless for the caller (could dangle after next
  push); index is stable and useful for `get` / `detach`.
- `fs2arr_concat` — `restrict` removed from parameters; self-concat
  (`dst == src`) supported via `hwm` snapshot + upfront `reserve`.

#### Fixed — fs2array

- `fs2arr_compact` — `hwm = write` at end (holes fully trimmed to padding);
  previously left holes inside `[0, hwm)`. Also `fs2_is_hole` reset on
  source slot prevents duplicate ownership → double-free.
- `fs2arr_mergesorted_new` — overflow-check direction fixed (was aborting
  on safe inputs: `count <= SIZE_MAX - b->count` should trigger raise
  when **false**).
- `fs2arr_issorted` — off-by-one fixed (loop started at `i = 1`, skipped
  `arr[0]` comparison → always returned true on unsorted).
- `fs2arr_sort` — `qsort` range fixed to `[0, hwm)` (was `[0, count)`,
  crashed with SEGV when holes present in the logical range).
- `fs2arr_attach_sp` — bounds-check against `a->size` (was `src->sz`,
  wrong field).
- `fs2arr_pushstr` — uses `fs2_fromstr` (copy) instead of `_fs2_wrap`
  (STATIC view would dangle after caller’s buffer goes out of scope, and
  mutate through the shared buffer).
- `fs2arr_setat_str` — same copy-vs-view fix.
- `fs2arr_attach_dp` — missing `fs2_free(p)` after `fs2arr_plug` leaked
  the heap struct shell. Now freed; caller’s `*src` nulled via `*src = NULL`
  in `_dp` (with validate-before-null so raise preserves pointer).
- `fs2_cmp_nullbefore` / `fs2_cmp_nullafter` — ternary branches used
  `!str1->v` twice instead of `!str1->v`/`!str2->v`; comparison always
  returned 0, violating strict-weak-ordering for `qsort` (UB).

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
- **Binary search:** `fs2_bsearch`, `fs2_bsearch_ci`,
  `fs2_bsearch_str`, `fs2_bsearch_ci_str` — return index or `-1`.
  Promote canonical `FS2()` key via `findkey` (see Fixed).
- **Substring search (`instr`):** `fs2_instr_pos_nth` (base), `fs2_instr_from`,
  `fs2_instr_nth`, `fs2_instr`, plus `_ci` and `_str` variants.
  Empty pattern matches at every position (Python semantics: `"abc".find("")`
  → `0`, `nth` → `k-1` up to `len+1`).
- **Split-part:** `fs2_split_part(s, delim, n)`, `fs2_split_part_str` —
  n-th part, PostgreSQL `split_part()` semantics. 1-based; negative `n`
  counts from end; `n == 0` returns empty; empty delimiter → whole string
  is single part. Uses `fs2_instr_nth` + `fs2_instr_from` (no full-string
  rescans).
- **Scanners:** `fs2_spn`, `fs2_cspn`, `fs2_nonspace_span` — length of
  matching / non-matching prefix, locale-independent whitespace skip.
  `NULL` buffer (`FS2()`) guarded: return 0, no `NULL + 0` UB.
- **Predicates (text):** `fs2_starts_with` / `fs2_ends_with` / `fs2_contains`
  / `fs2_count` (+ `_str` variants). `fs2_starts_with(s, "")` → true,
  `fs2_contains(s, "")` → true, `fs2_count(s, "")` → 0.
- **Predicates (classes, ASCII / C locale):** `fs2_is_alpha`,
  `fs2_is_digit`, `fs2_is_alnum`, `fs2_is_space`, `fs2_is_ascii`.
  Empty string: all return `false` except `fs2_is_ascii` (`true`,
  vacuously) — Python/Java/PostgreSQL "consists of" semantics.
- **Trim (PostgreSQL style):** `fs2_trim` (= `fs2_btrim`), `fs2_ltrim`,
  `fs2_rtrim` + `_str` variants. Character-set trim; not substring.
- **Case:** `fs2_initcap` (PostgreSQL semantics, non-alnum boundaries),
  `fs2_capitalize` (skip leading non-alnum, upper first alnum, lower rest),
  `fs2_swapcase` (involution: `f(f(x)) == x`). All use `(unsigned char)`
  casts for non-ASCII safety.
- **`fs2_zfill(s, width)`** — zero-pad preserving sign:
  `"-42"` → `"-0042"` (not `"00-42"`), `"+42"` → `"+0042"`. No-op when
  `width <= len`. `_new` twin: `fs2_zfill_new`.
- **`fs2_center(s, width, pad)`** — center in `width` with pattern `pad`.
  Odd extra byte goes **right** (Python `str.center()` rule). Truncates
  when `width < len` (PostgreSQL lpad/rpad convention). Both sides
  restart pattern from `pad[0]` independently. Implemented as
  `fs2_lpad` + `fs2_rpad` after one upfront `fs2_reserve_span`.
  `_new` and `_str` variants.
- **`fs2_squeeze(s, ch)`** — collapse runs of byte `ch` to single byte.
  Only `ch` runs collapse; other runs untouched. `_new` twin.
- **`fs2_normalize_whitespace(s)`** — collapse all whitespace runs to a
  single `' '`, trim leading/trailing. `_new` twin.
- **`fs2_removeall(s, chars)`** — remove all bytes from a set. Single
  256-byte lookup table → O(n+m) (vs naive O(n·m)). `_new` and `_str`
  variants.
- **`fs2_translate(s, from, to)`** — positional byte mapping (PostgreSQL
  `translate()`). `from[i]` → `to[i]`; `from[i]` beyond `to->len` is
  deleted. Duplicates: **first** occurrence wins (reverse iteration over
  `from`). Single `char map[256]` with `'\0'` sentinel (safe: fs2 has no
  embedded NUL). `_new` and `_str` variants.
- **`fs2_concatall(dst, delim, src)`** — join a NULL-terminated array of
  `fs2 *` into `dst`. `dst` is treated as **first element** of the list:
  if non-empty, delimiter is inserted before the first src element.
  Pre-reserves exact size. Variants: `fs2_concatall_str` (C-string array +
  C-string delim), `fs2_concatall_new`, `fs2_concatall_str_new`. Macros:
  `FS2CONCAT`, `FS2CONCATSTR`, `FS2CONCAT_NEW`, `FS2CONCAT_STR_NEW`.
- **Replace:** `fs2_replace`, `fs2_replace_fromstr`, `fs2_replace_cstr`
  — replace all non-overlapping occurrences (`"aaa"` / `"aa"` → `"ba"`).
- **Strip:** `fs2_strip_prefix`, `fs2_strip_suffix` + `_str` variants,
  plus `_new` constructors. Eager abort on non-empty STATIC (see Changed).
- **Repeat:** `fs2_repeat(s, n)` (in-place), `fs2_repeat_new`,
  `fs2_repeat_str_new`.
- **Slice:** `fs2_slice(s, from, len)` — zero-copy `STATIC` view.
- **Capacity management:** `fs2_setsize` (exact grow, no rounding,
  no `FS2_MIN_ALLOC` floor), `fs2_shrink` (exact `len+1`, no-op for
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
    consuming the whole string (numeric types). **Exception:** `tryparse_char`
    strict only rejects leading whitespace (PostgreSQL single-char rule).
  - `fs2_parse_*(s)` — lenient tryparse, raises on failure.
  - Supported: `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `llong`,
    `ullong`, `double`, `char`.
  - All `pos_parse`/`tryparse`/`parse` wrappers chain through `llong` / `ullong`
    (the widest type) and check the target range on the way back.
- **Tech dump (`fs2_tech.c`):** `fs2_techdump(out, s, name)` — full
  content to a stream (no truncation, unlike `fs2_techfprint`).
  `fs2_techdump_file(s, name, path)` — full content to a caller-specified
  file (checks `fclose` — buffered flush can fail after `fwrite` success).
  `fs2_techdump_emergency(s, name)` — auto path
  (`$FS2_DUMP_DIR` / `$TMPDIR` / `/tmp`) with thread-local
  `fs2dump-<name>-<pid>-<epoch>-<seq>.bin`, returns thread-local path,
  logs via `logsimple`. Name sanitized to `[A-Za-z0-9._-]`.
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
- **`_fs2_wrap`**: `NULL` → `FS2EMPTY` (already in 0.2.0, reaffirmed).
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
- **`fs2_setsize`:** exact grow, no `FS2_MIN_ALLOC` floor, no power-of-2
  rounding. Use `fs2_reserve` for amortized growth, `fs2_shrink` for shrink.
- **`fs2_shrink`:** no longer aborts on non-empty STATIC/LOCAL — silent
  no-op (capacity hint, like `std::string::shrink_to_fit`). Only `NULL`
  aborts. Exact `len+1` (no floor).
- **`fs2_free`:** documented idempotency for non-`BODYALLOC` values
  (after `freestr`, state is canonical, second call is no-op). `BODYALLOC`
  values are not idempotent (freeing freed memory is UB).
- **Eager abort for in-place mutators on non-empty STATIC** — extended to
  `fs2_trim_common`, `fs2_strip_prefix`, `fs2_strip_suffix`,
  `fs2_removeall`, `fs2_translate`, `fs2_initcap`, `fs2_capitalize`,
  `fs2_swapcase`, `fs2_squeeze`, `fs2_normalize_whitespace`. Previously
  some were lazy (no-op when pattern didn't match); now `fs2_cant_mutate`
  fires first, even for no-op cases. Rationale: mutating a literal is
  always a bug; better to fail at the call site than on the one-in-a-
  thousand call where the pattern happens to match.
- **`fs2_removeall` guard order** — `fs2_is_empty(chars) ||` was
  evaluated before `fs2_cant_mutate(s)`, so `FS2LITERAL("hello")` with
  empty `chars` silently returned instead of aborting. Reordered:
  `fs2_cant_mutate(s)` first, then pattern check. "Mutators on STATIC =
  always bug", no exceptions.
- **`fs2_concatall` guard** — `fs2_cant_mutate(dst)` was rejecting empty
  `dst` (which is the common case: build from scratch). Changed to
  `fs2_cant_realloc(dst)` so empty `dst` grows and stays.
- **`fs2_capitalize` semantics:** documented as "skip leading non-alnum,
  upper first alnum, lower rest". Differs from Python `str.capitalize()`
  (uppercases literal first byte). Chosen because it's more useful for
  real input (`"  hello"` → `"  Hello"` vs Python `"  hello"`).
- **`fs2_center` extra byte goes right** (Python `str.center()` rule);
  truncates on `width < len` (PG lpad/rpad convention, unlike Python).
- **`fs2_translate` first-wins on duplicate `from`** — `"aa"` + `"xy"`
  maps `'a'` → `'x'`, not `'y'`. Reverse iteration gives earliest-index
  precedence, matching PostgreSQL.
- **C standard:** `gnu11` → `gnu2x` (C23) for digit separators and stricter
  `-Wformat` diagnostics. Requires GCC 14+ / Clang 16+.
- **POSIX extensions:** `-D_GNU_SOURCE` in `Makefile.common` exposes
  `strcasestr` and `strtod_l`.
- **`invfatal` guard order:** exception code first, boolean expr second —
  `invfatal(ERR_*, cond, msg)`. (Several call sites had args swapped;
  fixed below.)

### Fixed

- **`fs2_replace` `tail + 1` off-by-one:** `fs2_selfmove` was called with
  `tail + 1` (move tail + NUL). `fs2_cant_copy` bounds `src` reads by
  `src->len` and rejects reading `v[len]` → `sysraise(ERR_OUT_OF_RANGE)`
  → `SIGINT`. Fixed: `fs2_selfmove(s, p + repl->len, p + pat->len, tail)`
  (no `+1`); `fs2_setlen` writes the new NUL. `fs2_replace` was unusable
  before this fix (crashed on every call).
- **`fs2_rpad` / `fs2_lpad` dead `if (fs2_cant_realloc(...))`:**
  `fs2_cant_realloc` always returns `false` (assert-style) — the `if`
  branch never executed; the whole function was a no-op. Fixed to bare
  `fs2_cant_realloc(s);`.
- **`fs2_lpad` missing `fs2_reserve_span`:** when `s->len == 0`, the
  `fs2_selfmove(s, pad_len, 0, 0)` early-returns via `fs2_cant_copy`
  (len == 0) without reserving, then the fill loop wrote to `s->v[i]`
  with no capacity → SIGSEGV. Fixed: `fs2_reserve_span(s, 0, len)` before
  `fs2_selfmove`.
- **`fs2_nrchr` / `fs2_nrchr_ci` / `fs2_nrchr_nth` / `fs2_nrchr_ci_nth`
  `NULL + 0` UB:** `str->v + off` evaluated before any `str->v == NULL`
  check, formal UB (`C11 §6.5.6p8`). Fixed with `if (str->v == NULL)
  return -1;` at the top. Only `FS2()` (canonical empty) is affected;
  `FS2EMPTY` (`v == ""`) was already safe.
- **`__fs2_chr_raw` / `__fs2_chr_raw_ci` / `__fs2_rchr_raw` /
  `__fs2_rchr_raw_ci`:** all four internal character finders now guard
  `if (p == NULL) return -1;` — protects `FS2()` (canonical empty) from
  `memchr(NULL, ...)` / `NULL + 0` UB in the forward path. Reachable via
  `fs2_chr`, `fs2_nchr`, `fs2_chr_nth`, `fs2_nchr_nth`, and their `_ci`
  variants.
- **`fs2_bsearch` / `fs2_bsearch_ci` empty-key SEGV:** `findkey` was
  computed (promoting `FS2()` → `FS2EMPTY` shape) but `bsearch(key, ...)`
  was called with the original `key` — `strcmp(NULL, ...)` → SEGV for
  `FS2()` key. Fixed: pass `findkey` to `bsearch`.
- **`fs2_instr_pos_nth` empty-pattern for `FS2()`:** `FS2()` (v=NULL)
  with empty pattern was returning `-1` (early return via `v == NULL`),
  but `FS2EMPTY` (v="") with empty pattern returned `0`. Inconsistent.
  Fixed: move empty-pattern check above `v == NULL` check.
- **`fs2_is_alpha` / `fs2_is_digit` / `fs2_is_alnum` / `fs2_is_space`
  on empty:** missing `if (s->len == 0) return false;` — loop ran zero
  times and returned `true` (vacuous). Python/Java/PG return `false` for
  empty; only `fs2_is_ascii` stays `true` (vacuously — all zero bytes
  are ASCII). Fixed.
- **`fs2_split_part` was extern not inline:** `fs2_api.h` declared it
  as `fs2 fs2_split_part(...)` (no `static inline`), but no `.c`
  definition existed → link error `_fs2_split_part`. Fixed: `static
  inline` (it delegates to other inline helpers, no external symbol
  needed).
- **`fs2_translate` missing NULL guards for `from`/`to`:** only `s` was
  checked; `to->len` dereferenced without check → SEGV for `to == NULL`.
  Fixed: `invfatal(s != NULL && from != NULL && to != NULL, ...)`.
- **`fs2_concatall` total sum skipped first element:** `if (delim &&
  iter != src)` skipped the first element entirely from `total`, so
  pre-reserve under-counted. `fs2_cat` amortized growth absorbed it, but
  the pre-reserve optimization was lost. Fixed: `total += (*iter)->len`
  unconditionally, `+ delim_len` only for `iter != src`.
- **`fs2_cat` overflow:** `memcpy(dst, src, len + 1)` wrote 1 byte past
  `src` — fixed to `src->len`.
- **`fs2_rev_catstr` overlap:** `memcpy` → `memmove` for the in-place
  shift.
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
- **`fs2_translate` single `map[256]` with `'\0'` sentinel:** safe
  because `fs2_validate` enforces `strlen(v) == len` (no embedded NUL).
  Delete-marker is `map[c] == '\0'`; identity is `map[c] == c`.
  Reverse-iteration over `from` gives first-wins on duplicates.
- **`fs2_center` via `fs2_lpad` + `fs2_rpad`:** both sides independently
  restart pattern from `pad[0]` — not "continuous" (`"x"` width 9 pad
  `"abc"` → `"abcaXabca"`, not `"abcaXbcab"`). One upfront
  `fs2_reserve_span` avoids double realloc.
- **`fs2_zfill` preserves sign:** `"-42"` zfill 5 → `"-0042"`. Sign
  detection is `s->v[0] == '+' || s->v[0] == '-'` (first byte only;
  no "digit after sign" check — literal Python rule).
- **`fs2_squeeze` uses `s->v[out - 1] == target`** — no separate
  `prev_was` state needed. Because `out <= i` invariant holds and
  `s->v[out - 1]` is the last **written** byte in the compacted prefix,
  which is exactly "previous in output". `out == 0` guarded separately
  (first byte, no run could have started).
- **`fs2_concatall` treats `dst` as first list element:** non-empty
  `dst` → delimiter inserted before first src element too. This is what
  makes `concatall` composable in loops: append rows without tracking
  "is this the first row" at the caller site.
- **`fs2_techdump_emergency` is not async-signal-safe:** uses stdio,
  `getenv`, `snprintf`, thread-locals. Safe from normal error paths,
  not from raw SIGSEGV/SIGABRT handler.
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
  `fs2_clone`, `fs2_movefrom`, `_fs2_wrap` (zero-copy). Full `fs2_heap_*`
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
- **`FS2WRAP` macro** replaced with the `_fs2_wrap` inline function,
  fixing multi-evaluation issues.
- **Primitive / Protected split:** zero-checks in primitive (`fs2_*`),
  validation moved to planned `pfs2_*` *(later dropped — see Unreleased)*.

### Fixed

- **`fs2_rev_catstr` overlap:** `memcpy` → `memmove` for overlapping regions.
- **`fs2_rpad` truncation:** keep the *left* part of the string.
- **`fs2_cpy` on empty string:** SIGSEGV on canonical empty `FS2()`.
- **`rndint` off-by-one:** inclusive bounds.
