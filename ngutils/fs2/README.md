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
| `FS2()` | empty growable (`ALLOC`, `v=NULL`, no capacity) |
| `FS2EMPTY` | empty read-only (`STATIC`, `v=""`) |
| `FS2LITERAL(s)` | wrap a compile-time string; `strlen` for free via `sizeof` |
| `FS2LOCAL(buf)` | wrap a caller-owned `char[]` (fixed capacity) |
| `FS2LOCALFILL(name, fmt, ...)` | same, but pre-filled via `snprintf` (VLA) |

## Naming Conventions

- **Base operations** use the shortest clear name: `fs2_chr`, `fs2_cat`, `fs2_instr`.
- **Modifiers** are appended as suffixes:
  - `_ci` — case-insensitive (`fs2_chr_ci`, `fs2_instr_ci`).
  - `_nth` — k-th occurrence (`fs2_chr_nth`, k is 1-based).
  - `_from` — start searching from a specific position (`fs2_instr_from`).
  - `_posn` — return position explicitly (`fs2_instr_posn`).
- **Heap constructors** use the `fs2_heap_*` prefix.
- **Stack constructors** use `_new` suffix where necessary to distinguish from in-place mutators (`fs2_substr` vs `fs2_substr_new`).

## Architecture: Primitive vs Protected

`fs2` deliberately splits into **two layers**:

### Primitive layer (`fs2.h`, `fs2_api.h`)
Maximum speed, zero checks. Under `-DNDEBUG` **all** `invraise` calls
compile to nothing. Passing `NULL`, out-of-range indices, or read-only
buffers to mutators is UB — you asked for speed, you got it. Every
`@pre` is documented in the header.

### Protected layer (`pfs2.h` — *planned*)
Thin wrappers around the primitive layer that validate arguments via
`userraise` (SIGINT) / `sysraise` (OOM). `pfs2_*` compiles down to the
exact same machine code as the primitive layer in release builds.

**Rule of thumb**: primitives assume the caller knows what they're
doing. `pfs2` assumes the caller is human.

## Semantics worth knowing

- **Canonical empty**: `FS2()` has `v == NULL`. Many operations treat this as a valid empty string. Only `fs2_setlen(&s, pos)` with `pos > 0` on a NULL buffer is a hard error.
- **Finders never match `'\0'`**: `fs2_chr(&s, '\0')` returns `-1`. NUL is a terminator, not content (consistent with `memchr`).
- **`fs2_cpy` clears first, then appends**: Equivalent to `fs2_setlen(dst, 0); fs2_cat(dst, src);`.
- **`fs2_rpad`/`fs2_lpad` follow PostgreSQL**: `rpad('hello', 2) -> 'he'` (truncates from the right, keeps left). Empty `pad` does not extend.
- **`fs2_wrap(NULL)` is `FS2EMPTY`**: Wrapping NULL gives a valid read-only empty view.
- **Overlapping matches**: `fs2_instr_posn("aaa", "aa")` finds matches at 0 and 1 (like `grep -o`).
- **Parsers & Locales**: `fs2_parse_double` uses `strtod` and respects the current `LC_NUMERIC` locale. Call `setlocale(LC_NUMERIC, "C")` if you need locale-independent parsing.

## API

### Constructors (Value & Heap)
Return an `fs2` by value (or `fs2 *` for heap). Free with `fs2_free`.
- `fs2_init`, `fs2_fromstr`, `fs2_fromstrf`, `fs2_clone`, `fs2_wrap`
- Substrings: `fs2_substr_new`, `fs2_left_new`, `fs2_right_new`, `fs2_slice` (view, zero-copy)
- Transformations: `fs2_toupper_new`, `fs2_tolower_new`, `fs2_lpad_new`, `fs2_rpad_new`
- Generators: `fs2_genrnd_new`
- **Generic serializer**: `fs2_from_new(val)` (macro, uses `_Generic`, supports int, long, double, char, etc.)
- All of the above have `fs2_heap_*` twins.

### Mutation (In-place)
- Append: `fs2_cat`, `fs2_catstr`, `fs2_rev_catstr` (prepend)
- Overwrite: `fs2_cpy`, `fs2_cpystr`
- Substrings: `fs2_substr`, `fs2_left`, `fs2_right`
- Padding: `fs2_lpad`, `fs2_rpad`, `fs2_lpadstr`, `fs2_rpadstr`
- Case: `fs2_toupper`, `fs2_tolower`
- Generate: `fs2_genrnd`

### Formatting & Serialization
- `fs2_sprintf(dst, fmt, ...)` — write from pos 0, updates len.
- `fs2_sprintf_position(dst, pos, fmt, ...)` — write at pos, does NOT update len.
- `fs2_sprintf_concat(dst, fmt, ...)` — append formatted text, updates len.
- **Generic macros**: `fs2_serialize(dst, val)`, `fs2_serialize_concat(dst, val)`.

### Finders — Characters
All return `ptrdiff_t` (offset or `-1`).
- `fs2_chr`, `fs2_nchr` (limited), `fs2_rchr` (reverse), `fs2_nrchr`
- `_ci` suffix for case-insensitive (`fs2_chr_ci`, etc.)
- `_nth` suffix for k-th occurrence (`fs2_chr_nth`, `fs2_rchr_nth`)

### Finders — Substrings (Instr)
`pattern` is a `const fs2 *`.
- `fs2_instr(str, pattern)` — first occurrence.
- `fs2_instr_from(str, pattern, pos)` — first from `pos`.
- `fs2_instr_nth(str, pattern, nth)` — nth occurrence from start.
- `fs2_instr_posn(str, pattern, pos, nth)` — base function (all params explicit).
- `_ci` suffix for insensitive (`fs2_instr_ci`, `fs2_instr_posn_ci`, etc.).

### Finders — Sets & Spans
- `fs2_spn(s, pos, accept)` — length of prefix containing only `accept`.
- `fs2_cspn(s, pos, reject)` — length of prefix containing none of `reject`.
- `fs2_nonspace_span(s, pos)` — token until whitespace (`" \t\n\r\v\f"`).
- `fs2_in(s, list)`, `fs2_notin(s, list)` — search NULL-terminated arrays (with `_ci` variants and `FS2_IN` macros).

### Comparators
- `fs2_cmp`, `fs2_ncmp`, `fs2_cmp_ci`, `fs2_ncmp_ci`
- `fs2_cmpstr` (fs2 vs c-string)
- `fs2_cmp_wrap`, `fs2_cmp_ci_wrap` (for `qsort`/`bsearch` via `void *`)
- Binary search: `fs2_in_bsearch`, `fs2_in_bsearch_ci` (returns index or `-1`).

### Parsers
Three levels of abstraction:
1. **`fs2_pos_parse_*(s, pos, &newpos, &out)`** — raw scanner, starts at `pos`, returns `bool`.
2. **`fs2_tryparse_*(s, &out, strict)`** — parses from 0, `strict` requires consuming the whole string.
3. **`fs2_parse_*(s)`** — calls `tryparse` in lenient mode, throws `userraise` on failure.

Supported types: `int`, `long`, `llong`, `uint`, `ulong`, `ullong`, `short`, `ushort`, `double`, `char`.

### Move & Destructor
- `fs2_move(to, from)` — steals `from` into `to`, marks `from` DEFUNCT.
- `fs2_free(s)` — releases buffer if `ALLOC`, struct if `BODYALLOC`. Idempotent.

## Internal helpers
Used by the API to prevent raw `memcpy` footguns.
```c
fs2 *fs2_memcpy    (fs2 *dst, size_t dstpos, const fs2 *src, size_t srcpos, size_t len);
fs2 *fs2_memcpy_str(fs2 *dst, size_t dstpos, const char *src, size_t len);
fs2 *fs2_memmove   (fs2 *dst, size_t dstpos, const fs2 *src, size_t srcpos, size_t len);
fs2 *fs2_selfmove  (fs2 *dst, size_t to, size_t from, size_t len);
void fs2_substr_bounds(const fs2 *s, size_t *from, size_t *len);
```

## Status

**0.2.0** — the primitive layer is feature-complete. What's in:
- Struct, flags, macros, validation.
- Full constructor/mutator suite (append, prepend, slice, pad, case).
- Character & substring finders (forward, reverse, ci, nth).
- Span scanning (`spn`, `cspn`, `nonspace_span`).
- Array finders and binary search.
- Generic serializers (`_Generic`) and serializers for all base types.
- Parsers for all base types with strict/lenient modes.
- Formatting (`sprintf`, `sprintf_concat`).

**Not yet implemented**:
- **`pfs2.h` / `cfs2.h`** — protected / checked layers.
- **`fs2_iter`** — stateful string iterator (tokenizer).
- **`fs2_tok.h`** — specialized lexer for quotes/escapes.
- **`fs2_regex` / `fs2_glob`** — separate libraries.

## Notes
- **`invraise` is debug-only.** It expands to nothing under `-DNDEBUG`. `userraise` and `sysraise` survive in release.
- **OOM is not fatal-by-abort.** Allocation failures route through `sysraise(...)`, allowing `TRY()` to catch them.
- **Minimum allocation** is `FS2_MIN_ALLOC` (16 bytes).
- **`restrict` is everywhere it matters.** `fs2_cat(&s, &s)` is UB. Clone first.
- **UTF-8 is bytes, not characters.** `fs2_toupper`/`fs2_tolower` use `tolower((unsigned char)c)` which only affects ASCII.
- **CI workaround**: release build uses `-Wno-error=format-truncation` for GCC 13+ due to a false positive in `_FORTIFY_SOURCE=3` (GCC Bugzilla #102218).

## Development
Uses [Criterion](https://criterion.readthedocs.io/) for unit tests. Runs under ASan+UBSan on Linux (Docker) and native on macOS.
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
│   ├── fs2.h           struct, flags, macros, validation
│   └── fs2_api.h       higher-level operations
├── src/
│   ├── fs2.c           primitives
│   └── fs2_api.c       higher-level impl
├── test/
│   ├── test_fs2.c      primitive tests
│   ├── test_fs2_api.c  API tests (including composites)
│   └── test_fs2.h      shared helpers
├── build/              artifacts (gitignored)
├── logs/               test logs (gitignored)
├── Makefile
└── CHANGELOG.md
```

## License
See the top-level `LICENSE` file.

