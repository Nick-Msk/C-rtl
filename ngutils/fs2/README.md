# fs2.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

Core primitives for a growable C string type.

`fs2` is a **faststring**: a small struct that owns a NUL-terminated
buffer together with its length and capacity. It sits between raw
`char *` (no length, manual realloc) and a heavy string library
(always heap, hidden allocations). The trade-off is explicit: the
struct records *who owns what*, and the caller decides.

Depends on `common`, `error`, `log`, `bool`, `checker`.

**Requires C23** (`-std=gnu2x`) and `-D_GNU_SOURCE` for POSIX extensions
(`strcasestr`, `strtod_l`).

## Design: two orthogonal axes

Every `fs2` value is described by **two independent flags**:

| Axis | Values | Meaning |
|---|---|---|
| Where does the string live? | `FS2_STATIC` | read-only, external (literal, view) |
| | `FS2_LOCAL` | writable, caller-owned buffer |
| | `FS2_ALLOC` | heap-owned, growable |
| Where does the struct live? | `FS2_BODYALLOC` | the `fs2` struct itself is `malloc`-ed |
| | *(unset)* | the struct is on the stack (or wherever the caller put it) |

`FS2_STATIC` / `FS2_LOCAL` / `FS2_ALLOC` are **mutually exclusive** —
exactly one is set. `FS2_BODYALLOC` is **independent** — it can combine
with any of the three.

### Full combination table

| Flags | What it means | `fs2_free` releases |
|---|---|---|
| `STATIC` | literal, struct on stack | nothing |
| `STATIC \| BODYALLOC` | literal, struct on heap | `free(struct)` |
| `LOCAL` | caller's buffer, struct on stack | nothing |
| `LOCAL \| BODYALLOC` | caller's buffer, struct on heap | `free(struct)` |
| `ALLOC` | our string, struct on stack | `free(string)` |
| `ALLOC \| BODYALLOC` | everything on heap | `free(string)`, `free(struct)` |

This is what "symmetric API" means in this project: every value
operation has a matching heap-body variant, and both accept any
string-axis.

## Value macros

| Macro | Result |
|---|---|
| `FS2()` | canonical empty (`ALLOC`, `v=NULL`, `sz=0`, `len=0`) |
| `FS2EMPTY` | safe empty (`STATIC`, `v=""`, `sz=1`, `len=0`) |
| `FS2LITERAL(s)` | wrap a compile-time string; `strlen` for free via `sizeof` |
| `FS2LOCAL(buf)` | wrap a caller-owned `char[]` (fixed capacity) |
| `FS2LOCALFILL(name, fmt, ...)` | same, but pre-filled via `snprintf` (VLA) |

**Safe empty vs canonical empty**:
- `FS2()` — no buffer at all. `s.v == NULL`, cannot `printf("%s")`.
- `FS2EMPTY` — has a valid empty string (`""` in `.rodata`), so `printf`, `strcmp`, `s.v[0]` all work. All **constructors** return `FS2EMPTY` for empty results, and **mutators** reset their source to `FS2EMPTY` after a move — so callers never have to NULL-check `v`.

## Naming Conventions

- **Base operations** use the shortest clear name: `fs2_chr`, `fs2_cat`, `fs2_instr`.
- **Modifiers** are appended as suffixes:
  - `_ci` — case-insensitive (`fs2_chr_ci`, `fs2_instr_ci`, `fs2_cmp_ci`).
  - `_nth` — k-th occurrence (`fs2_chr_nth`, `fs2_rchr_nth`, k is 1-based).
  - `_from` — start searching from a specific position (`fs2_instr_from`).
  - `_posn` — return position explicitly (`fs2_instr_pos_nth`).
  - `_str` — C-string variant of a fs2-argument function (`fs2_instr_str`, `fs2_starts_with_str`).
  - `_cstr` — both arguments as C-strings (`fs2_replace_cstr`).
- **Heap constructors** use the `fs2_heap_*` prefix.
- **Stack constructors** use `_new` suffix where necessary to distinguish from in-place mutators (`fs2_substr` vs `fs2_substr_new`).
- **In-place mutators** always take `fs2 *` and return `fs2 *` for chaining.
- **`fs2_flip`** (not `fs2_reverse`) — avoid visual clash with `fs2_reserve`.

## Three contracts

### 1. In-place mutators abort on non-empty STATIC

`fs2_toupper`, `fs2_trim`, `fs2_replace`, `fs2_removeall`, `fs2_translate`,
`fs2_initcap`, `fs2_capitalize`, `fs2_swapcase`, `fs2_squeeze`,
`fs2_normalize_whitespace`, `fs2_strip_prefix`, `fs2_strip_suffix`,
`fs2_flip`, `fs2_sort`, `fs2_initcap` — all call `fs2_cant_mutate(s)`
eagerly. Non-empty `FS2_STATIC` → `SIGABRT` regardless of arguments, even if
the operation would be a no-op. Empty (`FS2()`, `FS2EMPTY`, `FS2LITERAL("")`)
returns unchanged.

```c
fs2 s = FS2LITERAL("hello");
fs2_toupper(&s);                        // SIGABRT — cannot mutate .rodata
fs2_trim(&s, fs2_static_or(&empty));    // SIGABRT — even for no-match pattern
```

Growable mutators (`fs2_rpad`, `fs2_lpad`, `fs2_center`, `fs2_zfill`,
`fs2_genrnd`, `fs2_concatall`) use `fs2_cant_realloc` — same eager abort on
non-empty STATIC, but empty inputs are promoted to `ALLOC`.

### 2. `_new` constructors never mutate their source

`fs2_*_new` return an `fs2` by value; source `const fs2 *` is untouched, and
the result buffer is **always** `ALLOC` (never `BODYALLOC`, never `STATIC`).
Even on no-op paths (pattern doesn't match, empty pattern) the result is a
**clone**, not a share.

```c
fs2 src = fs2_fromstr("hello");
fs2 up  = fs2_toupper_new(&src);        // up  = "HELLO", src = "hello"
fs2 st  = fs2_strip_prefix_new(&src, &nonmatching);  // st = clone(src)
```

### 3. `_str` wrappers are lenient on NULL

`fs2_*_str` / `fs2_*_cstr` / `fs2_*_fromstr` accept `const char *` and pass
it through `_fs2_wrap(p)` which maps `NULL` → `FS2EMPTY`. Non-`_str`
functions taking `const fs2 *` **abort** on `NULL` via `invfatal`.

```c
fs2_catstr(&dst, NULL);                 // no-op
fs2_instr_str(&s, NULL);                // == fs2_instr_str(&s, "") == 0
fs2_instr(&s, NULL);                    // SIGABRT — programmer error
```

## Architecture: Protected (`fs2.*`) vs Bare (`ufs2.*` — planned)

**Current `fs2.*` / `fs2_api.*` is the Protected layer.** Every public
function validates its preconditions. Passing a bad argument aborts the
process (SIGABRT) with a diagnostic — the bug surfaces *here*, not as
silent memory corruption 100 lines later.

The Bare layer (`ufs2.*`, planned) will mirror the same API with all
guards stripped — for hot loops where inputs are already validated.

### Protected (`fs2.*`, `fs2_api.*`) — **current**

Three error tiers, each with its own signal:

| Macro | Meaning | Signal | Catchable via `TRY()`? |
|---|---|---|---|
| `invfatal(ERR_*, cond, msg)` | **Contract violation** (NULL param, bad flag, broken invariant) — programmer bug | `SIGABRT` (via `abort()`) | **No** |
| `userraise(errcode, msg)` | **Runtime error** (bad format string, recoverable) | `SIGINT` (via `siglongjmp` or `raise(SIGINT)`) | **Yes** |
| `sysraise(errcode, msg)` | **System error** (OOM) | `SIGINT` (same as `userraise`) | **Yes** |

`invfatal` **always fires** (unlike `invraise`, which is debug-only).
Catching it via `TRY()` is intentionally impossible — a broken invariant
means the process state is undefined, continuation is UB.

Tests use `.signal = SIGABRT` for contract violations and
`.signal = SIGINT` for runtime errors / OOM.

### Bare (`ufs2.*` — planned)

Same data layout, same API. `ufs2_*` = `fs2_*` with every `invfatal`
removed. `NULL`, out-of-range indices, read-only buffers — all UB, no
signal, maximum speed.

**Rule of thumb**: application code calls `fs2_*`; library authors who
have already validated inputs (or who operate in a hot loop after a
single up-front check) call `ufs2_*`.

## Semantics worth knowing

- **Canonical empty**: `FS2()` has `v == NULL, sz == 0, len == 0`. Some operations treat this as a valid empty string; grow-functions auto-promote it to `FS2EMPTY` internally (see `fs2_promote_empty`). Only `fs2_setlen(&s, pos)` with `pos > 0` on a NULL buffer is a hard error.
- **Safe empty**: `FS2EMPTY` has `v == ""` so `printf("%s", s.v)` is always safe. All empty-results from constructors return `FS2EMPTY`.
- **`fs2_move` resets `src` to `FS2EMPTY`** (not `FS2()`): after `fs2_move(&a, &b)`, `b.v` is `""` — readable, printable. To reuse `b`, call `fs2_reserve` (auto-promotes).
- **Finders never match `'\0'`**: `fs2_chr(&s, '\0')` returns `-1`. NUL is a terminator, not content (consistent with `memchr`).
- **`fs2_instr_pos_nth` matches empty patterns** (Python semantics): `fs2_instr("abc", "")` returns `0`, `fs2_instr_nth("abc", "", k)` returns `k-1` for `k <= len+1`.
- **`fs2_cpy` clears first, then appends**: equivalent to `fs2_setlen(dst, 0); fs2_cat(dst, src);`.
- **`fs2_rpad`/`fs2_lpad`/`fs2_center` follow PostgreSQL**: truncate when target length < current length. Empty `pad` does not extend. `center` puts the odd extra byte on the **right**.
- **`fs2_trim`/`ltrim`/`rtrim`** — character-set trim, PostgreSQL-style.
- **`fs2_strip_prefix`/`fs2_strip_suffix`** — single-occurrence strip, not recursive. `"aaa"` with `"aa"` → `"a"` (one strip).
- **`fs2_replace`/`fs2_removeall`/`fs2_count`/`fs2_translate`** — **non-overlapping** (Python `str.replace` / `str.count` semantics). `"aaa"` / `"aa"` → 1 match, not 2.
- **`fs2_instr_nth`** — **overlapping** matches count separately (loop with `+1` step). `"aaa"` / `"aa"` with `nth=2` → 1, not `-1`.
- **`fs2_split_part` negative `n`** counts from the end (Python `split()[-1]` style). `n == 0` returns empty.
- **`fs2_translate` first-wins on duplicate `from`**: `"aa"` + `"xy"` maps `'a'` → `'x'`, not `'y'` (PostgreSQL semantics).
- **`fs2_zfill` preserves sign**: `"-42"` zfill 5 → `"-0042"`, not `"00-42"`. `fs2_lpad` does **not** do this — use `zfill` for numeric strings.
- **`fs2_is_alpha`/`digit`/`alnum`/`space`** return `false` for empty string (Python semantics). `fs2_is_ascii` returns `true` for empty (vacuously).
- **ctype is ASCII-only in C locale**: `fs2_toupper`, `fs2_initcap`, `fs2_swapcase`, `fs2_is_*` use `<ctype.h>`. Bytes ≥ 0x80 are non-alpha in C locale. Non-C locales may diverge.
- **`_fs2_wrap(NULL)` is `FS2EMPTY`**: wrapping NULL gives a valid read-only empty view.
- **`fs2_setsize`/`fs2_reserve` are grow-only**; `fs2_shrink` is the only shrinker. `reserve` rounds to power-of-2, `setsize` is exact, `shrink` reallocs to `len+1`.
- **`fs2_parse_double` respects `LC_NUMERIC`**: call `setlocale(LC_NUMERIC, "C")` if you need locale-independent parsing.
- **`fs2_tryparse_char` strict ≠ numeric strict**: numeric `strict` requires full consumption; `char` strict only rejects leading whitespace (PostgreSQL behavior).
- **`fs2_concatall` semantic**: `dst` is treated as the first element of the list. If `dst` is non-empty, the delimiter is inserted before the **first** src element too. `fs2_concatall(FS2(), "|", {a, b})` → `"a|b"`; `fs2_concatall("x", "|", {a, b})` → `"x|a|b"`.

## API

### Constructors (Value & Heap)
Return `fs2` by value (or `fs2 *` for heap). Free with `fs2_free`.

- `fs2_init(sz)`, `fs2_fromstr(s)`, `fs2_fromstrf(fmt, ...)`, `fs2_clone(src)`, `_fs2_wrap(s)`
- `fs2_adopt(char **)`, `fs2_adoptn(char **, len)` — take ownership of heap C-string
- `fs2_movefrom(fs2 *src)`, `fs2_movefromstr(const char **, fl)` — steal from another source
- Substrings: `fs2_substr_new`, `fs2_left_new`, `fs2_right_new`, `fs2_slice` (zero-copy view)
- Transformations: `fs2_toupper_new`, `fs2_tolower_new`, `fs2_swapcase_new`, `fs2_initcap_new`, `fs2_capitalize_new`
- Padding: `fs2_lpad_new`, `fs2_rpad_new`, `fs2_center_new`, `fs2_zfill_new`
- Trim: `fs2_trim_new`, `fs2_ltrim_new`, `fs2_rtrim_new`
- Replace: `fs2_replace_new`, `fs2_replace_fromstr_new`, `fs2_removeall_new`, `fs2_translate_new`
- Squeeze: `fs2_squeeze_new`, `fs2_normalize_whitespace_new`
- Strip: `fs2_strip_prefix_new`, `fs2_strip_suffix_new`
- Repeat: `fs2_repeat_new`
- Generators: `fs2_genrnd_new`
- Search: `fs2_split_part` (PG-style n-th part, negative `n` from end)
- **Generic serializer**: `fs2_from_new(val)` (macro, `_Generic`)
- All of the above have `fs2_heap_*` twins (`fs2_heap_init`, `fs2_heap_fromstr`, …) for the **basic** constructors only (`init`, `fromstr`, `fromstrf`, `clone`, `movefromstr`). Higher-level `_new` results live on the stack; use `fs2_stacktoheap` to lift them if needed.

### Capacity management
- `fs2_reserve(s, need)` — grow to power-of-2 ≥ `need`, floor 16.
- `fs2_setsize(s, need)` — grow to exact `need` (no rounding, no floor).
- `fs2_shrink(s)` — realloc to `len+1` (exact, no floor). No-op for STATIC/LOCAL — capacity hint semantics.
- `fs2_promote_empty(s)` — canonical `FS2()` → `FS2EMPTY` shape (used internally by grow paths).

### Mutation (In-place)
- Append: `fs2_cat`, `fs2_catstr`, `fs2_rev_catstr` (prepend), `fs2_concatall` (join with delim; `dst` = first element)
- Overwrite: `fs2_cpy`, `fs2_cpystr`
- Substrings: `fs2_substr`, `fs2_left`, `fs2_right`
- Padding: `fs2_lpad`, `fs2_rpad`, `fs2_center`, `fs2_zfill` + `_str` variants
- Trim: `fs2_trim` (= `fs2_btrim`), `fs2_ltrim`, `fs2_rtrim`, + `_str` variants
- Replace: `fs2_replace`, `fs2_replace_fromstr`, `fs2_replace_cstr`
- Remove: `fs2_removeall` (byte set), `fs2_translate` (positional map + delete)
- Squeeze: `fs2_squeeze` (single-char runs), `fs2_normalize_whitespace` (collapse + trim)
- Strip: `fs2_strip_prefix`, `fs2_strip_suffix` + `_str` variants
- Case: `fs2_toupper`, `fs2_tolower`, `fs2_swapcase`, `fs2_initcap`, `fs2_capitalize`
- Repeat: `fs2_repeat(s, n)`
- Order: `fs2_flip` (reverse), `fs2_sort(s, asc)`, `fs2_partialsort(s, from, to, asc)`
- Generate: `fs2_genrnd`
- Exchange: `fs2_swap(a, b)` (value swap), `fs2_exch(a, b)` (buffer-identity swap)

### Predicates
- `fs2_is_empty(s)`, `fs2_is_null(s)`
- `fs2_is_static`, `fs2_is_local`, `fs2_is_alloc`, `fs2_is_bodyalloc`
- `fs2_eq_empty(s)` — strict `FS2EMPTY` shape check
- `fs2_is_canon(s)` — strict `FS2()` shape check
- `fs2_validate(out, s)` — full invariant check (used by tests)
- Text classes (ASCII, C locale): `fs2_is_alpha`, `fs2_is_digit`, `fs2_is_alnum`, `fs2_is_space`, `fs2_is_ascii`

### Formatting & Serialization
- `fs2_sprintf(dst, fmt, ...)` — write from pos 0, updates len.
- `fs2_sprintf_position(dst, pos, fmt, ...)` — write at pos, does NOT update len.
- `fs2_sprintf_concat(dst, fmt, ...)` — append formatted text, updates len.
- **Generic macros**: `fs2_serialize(dst, val)`, `fs2_serialize_concat(dst, val)`, `fs2_from_new(val)`, `fs2_heap_from(val)` — `_Generic` dispatch over `char`, `short`, `int`, `long`, `long long`, unsigned variants, `float`, `double`, pointers.

### Finders — Characters
All return `ptrdiff_t` (offset or `-1`). Never match `'\0'`.

- `fs2_chr`, `fs2_nchr` (limited), `fs2_rchr` (reverse), `fs2_nrchr`
- `_ci` suffix for case-insensitive: `fs2_chr_ci`, `fs2_nchr_ci`, `fs2_rchr_ci`, `fs2_nrchr_ci`
- `_nth` suffix for k-th occurrence: `fs2_chr_nth`, `fs2_rchr_nth` (+ `_ci` variants)

### Finders — Substrings (`instr`)
`pattern` is a `const fs2 *`. The `_str` variants take a C-string.

- `fs2_instr(str, pattern)` — first occurrence
- `fs2_instr_from(str, pattern, pos)` — first from `pos`
- `fs2_instr_nth(str, pattern, nth)` — nth occurrence from start
- `fs2_instr_pos_nth(str, pattern, pos, nth)` — base function (all params explicit)
- `_ci` suffix for insensitive: `fs2_instr_ci`, `fs2_instr_pos_ci_nth`, …
- `_str` suffix for C-string pattern: `fs2_instr_str`, `fs2_instr_posn_str`, …
- **Split**: `fs2_split_part(s, delim, n)`, `fs2_split_part_str(s, delim, n)` — nth part, PG semantics

### Finders — Sets, Spans, Arrays
- `fs2_spn(s, pos, accept)` — length of prefix containing only `accept`.
- `fs2_cspn(s, pos, reject)` — length of prefix containing none of `reject`.
- `fs2_nonspace_span(s, pos)` — token until whitespace (`" \t\n\r\v\f"`).
- `fs2_in(s, list)`, `fs2_not_in(s, list)` + `_ci` variants — search NULL-terminated arrays.
- `FS2_IN`, `FS2_IN_CI`, `FS2_NOT_IN`, `FS2_NOT_IN_CI` — variadic macros.
- Binary search: `fs2_bsearch`, `fs2_bsearch_ci`, `fs2_bsearch_str`, `fs2_bsearch_ci_str` — return index or `-1`.

### Comparators
- `fs2_cmp`, `fs2_ncmp`, `fs2_cmp_ci`, `fs2_ncmp_ci`
- `fs2_cmpstr` (fs2 vs C-string)
- `fs2_cmp_wrap`, `fs2_cmp_ci_wrap` — `void *` wrappers for `qsort`/`bsearch`

### Predicates for text
- `fs2_starts_with(s, prefix)`, `fs2_ends_with(s, suffix)` + `_str` variants
- `fs2_contains(s, needle)` + `_str`
- `fs2_count(s, needle)` + `_str` — non-overlapping occurrence count

### Parsers
Three levels of abstraction:

1. **`fs2_pos_parse_*(s, pos, &newpos, &out)`** — raw scanner, starts at `pos`, returns `bool`.
2. **`fs2_tryparse_*(s, &out, strict)`** — parses from 0, `strict` requires consuming the whole string (numeric types).
3. **`fs2_parse_*(s)`** — calls `tryparse` in lenient mode, raises on failure.

Supported types: `short`, `ushort`, `int`, `uint`, `long`, `ulong`, `llong`, `ullong`, `double`, `char`.

### Move & Conversion
- `fs2_move(dst, src)` — steal `src` into `dst`, reset `src` to `FS2EMPTY` (safe empty).
- `fs2_movefrom(src)` — value-returning variant.
- `fs2_heaptostack(&h)` — heap struct → stack value, `h = NULL`.
- `fs2_stacktoheap(&s)` — stack value → heap struct, `s` reset to `FS2EMPTY`.

### Destructor
- `fs2_free(s)` — releases buffer if `ALLOC`, struct if `BODYALLOC`. Idempotent for non-`BODYALLOC`.

### Utilities
- `fs2_setlen(s, pos)` — set logical length, write NUL at `pos`.
- `fs2_clear(s)` — shorthand for `fs2_setlen(s, 0)`.
- `fs2_validate(out, s)` — check all invariants.
- `fs2_techfprint(out, s, name)` — one-line technical dump (truncated).
- `fs2_techdump(out, s, name)` — full content dump to a stream.
- `fs2_techdump_file(s, name, path)` — full content to a caller-specified path.
- `fs2_techdump_emergency(s, name)` — auto path (`$FS2_DUMP_DIR` / `$TMPDIR` / `/tmp`) + `fs2dump-<name>-<pid>-<epoch>-<seq>.bin` + `logsimple` of resulting path; returns thread-local path string.

## Internal helpers
Used by the API to prevent raw `memcpy`/`memmove` footguns.

```c
fs2 *fs2_memcpy     (fs2 *dst, size_t dstpos, const fs2 *src, size_t srcpos, size_t len);
fs2 *fs2_memcpy_str (fs2 *dst, size_t dstpos, const char *src, size_t len);
fs2 *fs2_memmove    (fs2 *dst, size_t dstpos, const fs2 *src, size_t srcpos, size_t len);
fs2 *fs2_selfmove   (fs2 *dst, size_t to, size_t from, size_t len);
void fs2_substr_bounds(const fs2 *s, size_t *from, size_t *len);
bool fs2_cant_mutate(const fs2 *s);
bool fs2_cant_realloc(fs2 *s);
```

## Status

**0.3.0** — feature-complete protected API. What's in:

- Struct, flags, macros, validation.
- Full constructor/mutator suite: append, prepend, concatall, slice, pad (lpad/rpad/center/zfill), trim, case (upper/lower/swapcase/initcap/capitalize), replace, removeall, translate, squeeze, normalize_whitespace, strip, repeat, flip, sort, adopt, move.
- Character & substring finders (forward, reverse, ci, nth).
- Span scanning (`spn`, `cspn`, `nonspace_span`).
- Array finders, binary search, comparators.
- Predicates: `starts_with`, `ends_with`, `contains`, `count`, `eq_empty`, `is_canon`, `is_alpha`/`digit`/`alnum`/`space`/`ascii`.
- Generic serializers (`_Generic`) + `fs2_sprintf_concat` + `fs2_serialize` macros.
- Parsers for 10 base types with strict/lenient modes.
- Capacity management: `reserve` (power-of-2), `setsize` (exact), `shrink`, `promote_empty`.
- Stack↔heap converters: `fs2_stacktoheap`, `fs2_heaptostack`.
- Emergency tech dump with automatic path selection.
- `invfatal` (abort-on-contract-bug) with `.signal = SIGABRT` tests.

**Not yet implemented**:

- **`ufs2.h` / `ufs2.c`** — bare/unprotected layer: `fs2_*` without `invfatal` guards. For hot paths after up-front validation. Naming convention: `ufs2_*` prefix; `.h`/`.c` parallel to `fs2.*`.
- **`fs2array`** — dynamic array of `fs2`. Foundation for `fs2_split` (returns `fs2array`) and other multi-result operations.
- **`fs2_iter`** — stateful string iterator (tokenizer).
- **`fs2_tok.h`** — specialized lexer for quotes/escapes.
- **`fs2_regex` / `fs2_glob`** — separate libraries.
- **`fs2_io`** — `FILE*`-based file I/O (readfile, writefile, fgetline).

## Notes

- **`invfatal` always fires (unlike `invraise`).** It aborts with `SIGABRT` on contract violations. This is **not** catchable by `TRY()` — a broken invariant means the process state is undefined.
- **`userraise` / `sysraise` are catchable** via `TRY()` (`SIGINT`). Use these for runtime errors (bad format, OOM) that a caller might recover from.
- **`fs2_*_str` wrappers** call `_fs2_wrap(p)` (NULL → empty view). **Non-`_str`** functions taking `const fs2 *` abort on NULL.
- **Mutators eagerly abort on non-empty STATIC.** No exceptions for no-op patterns — programmatic error is caught at the call site.
- **`_new` constructors always clone.** Even on no-op paths (pattern doesn't match, empty pattern), the result is an independent `ALLOC` buffer.
- **`restrict` is everywhere it matters.** `fs2_cat(&s, &s)` is UB. Clone first.
- **UTF-8 is bytes, not characters.** `fs2_toupper`/`fs2_tolower`/`fs2_initcap`/`fs2_swapcase` use `tolower((unsigned char)c)` which only affects ASCII in C locale. `fs2_is_*` predicates likewise ASCII-only in C locale.
- **`fs2_free` uses `#ifndef __clang_analyzer__` guard around `free(s->v)`** — `scan-build` can't prove `v` is malloc'd (no data-flow from `flags` to `v`). The runtime path is proven safe: all callers guarantee `fs2_is_alloc(s)`.
- **`fs2_techdump_emergency` is not async-signal-safe.** Uses stdio, `getenv`, `snprintf`, thread-locals. Safe from normal error paths, NOT from a raw SIGSEGV/SIGABRT handler.
- **Minimum allocation** is `FS2_MIN_ALLOC` (16 bytes). Applies to grow paths only; `fs2_shrink` reallocs to exact `len+1`.
- **`fs2_heap_*` for higher-level constructors was removed.** Use `fs2_stacktoheap(&fs2_*_new(...))` directly. Only basic heap constructors remain (`_init`, `_fromstr`, `_fromstrf`, `_clone`, `_movefromstr`).
- **`fs2_static()` was considered and rejected.** A compound-literal view helper would eliminate `_str` wrappers but introduces block-scope lifetime pitfalls. `FS2LITERAL("x")` covers inline use; `_str` wrappers cover runtime C-strings.
- **CI workaround**: release build uses `-Wno-error=format-truncation` for GCC 13+ due to a false positive in `_FORTIFY_SOURCE=3` (GCC Bugzilla #102218).

## Development

Uses [Criterion](https://criterion.readthedocs.io/) for unit tests.
Runs under ASan+UBSan on Linux (Docker) and native on macOS.

```text
make test             # build and run tests
make lib              # debug library
make librelease       # release library
make check            # clang-tidy
make analyze          # clang static analyzer
```

### Layout

```text
.
├── include/
│   ├── fs2.h           struct, flags, macros, validation, capacity, move
│   ├── fs2_api.h       higher-level operations (string manipulation, finders, parsers)
│   └── fs2_internal_utils.h   view wrappers (_fs2_wrap, _fs2_vsprintf_position)
├── src/
│   ├── fs2.c           primitives (validate, free, move, convert, techfprint)
│   ├── fs2_api.c       higher-level impl
│   └── fs2_tech.c      techdump, techdump_file, techdump_emergency
├── test/
│   ├── test_fs2.c                      primitives tests
│   ├── test_fs2_api.c                  core API tests
│   ├── test_fs2_api_any_class.c        fs2_in / fs2_anyof / membership
│   ├── test_fs2_api_instr.c            substring finders
│   ├── test_fs2_api_modificators.c     mutation suite (case/trim/pad/replace/...)
│   ├── test_fs2_api_parse.c            parsers
│   ├── test_fs2_api_serialization.c    _Generic serializers
│   ├── test_fs2_api_sort_search.c      bsearch / qsort wrappers
│   ├── test_fs2_tech.c                 techdump
│   └── test_fs2.h                      shared helpers
├── build/              artifacts (gitignored)
├── logs/               test logs (gitignored, `logs/result/all.log` for consolidated)
├── Makefile
└── CHANGELOG.md
```

## License

See the top-level `LICENSE` file.
