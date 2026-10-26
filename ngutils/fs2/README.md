# fs2.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

Core primitives for a growable C string type.

`fs2` is a **faststring**: a small struct that owns a NUL-terminated
buffer together with its length and capacity. It sits between raw
`char *` (no length, manual realloc) and a heavy string library
(always heap, hidden allocations). The trade-off is explicit: the
struct records *who owns what*, and the caller decides.

Depends on `common`, `error`, `log`, `bool`, `checker`.

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

Plus two state flags, orthogonal to both:

- `FS2_CONST` — reserved for future const-correctness.
- `FS2_DEFUNCT` — moved out; only `fs2_free` / assignment is valid.

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
operation (`fs2_fromstr`, `fs2_clone`, `fs2_substr_new`, ...) has a
matching heap-body variant (`fs2_heap_fromstr`, `fs2_heap_clone`,
`fs2_heap_substr`, ...), and both accept any string-axis.

## Value macros

| Macro | Result |
|---|---|
| `FS2()` | empty growable (`ALLOC`, no capacity yet) |
| `FS2EMPTY` | empty read-only (`STATIC`, `""`) |
| `FS2LITERAL(s)` | wrap a compile-time string; `strlen` for free via `sizeof` |
| `FS2LOCAL(buf)` | wrap a caller-owned `char[]` (fixed capacity) |
| `FS2LOCALFILL(name, fmt, ...)` | same, but pre-filled via `snprintf` (VLA) |

## Architecture: Primitive vs Protected

`fs2` deliberately splits into **two layers**:

### Primitive layer (`fs2.h`, `fs2_api.h`)

Maximum speed, zero checks. Under `-DNDEBUG` **all** `invraise` calls
compile to nothing. Passing `NULL`, out-of-range indices, or read-only
buffers to mutators is UB — you asked for speed, you got it.

Every `@pre` is documented in the header. If it says "src must not alias
dst", that's a `restrict` contract you break at your own risk.

### Protected layer (`pfs2.h` — *planned*)

Thin wrappers around the primitive layer that validate arguments via
`userraise` (SIGINT) / `sysraise` (OOM). This is what application code
should call. `pfs2_*` never has more overhead than a few branches, and
in release builds (`-DNDEBUG`) it compiles down to the exact same
machine code as the primitive layer.

**Rule of thumb**: primitives assume the caller knows what they're
doing. `pfs2` assumes the caller is human.

## Semantics worth knowing

These behaviours look subtle but are pinned down by tests:

### Canonical empty: `FS2()` has `v == NULL`

Many operations treat `FS2()` (len=0, sz=0, v=NULL) as a valid empty
string. `fs2_cat`, `fs2_substr`, `fs2_in` all handle it. Only
`fs2_setlen(&s, pos)` with `pos > 0` on a NULL buffer is a hard error
(it would mean "write NUL at v[pos]" with no buffer).

### Finders never match `'\0'`

`fs2_chr(&s, '\0')` returns `-1`, not `s->len`. This is `memchr`
semantics — NUL is terminator, not content. Consistent across all
eight `fs2_*chr*` functions.

### `fs2_cpy` clears first, then appends

Equivalent to `fs2_setlen(dst, 0); fs2_cat(dst, src);`. Self-copy
(`fs2_cpy(&s, &s)`) is UB due to `restrict`.

### `fs2_rpad`/`fs2_lpad` follow PostgreSQL

```text
rpad('hi', 5, 'xy') -> 'hixyx'
rpad('hello', 2)    -> 'he'   (truncate right, keep left)
lpad('hi', 5, 'xy') -> 'xyxhi'
lpad('hello', 2)    -> 'he'
```

Empty `pad` does not extend (silently no-op).

### `fs2_wrap(NULL)` is `FS2EMPTY`

Wrapping a NULL C-string gives a valid read-only empty view. No crash,
no special case at every call site.

## API

### Getters

All const-safe, no allocation:

```c
size_t      fs2_len (const fs2 *s);
size_t      fs2_sz  (const fs2 *s);
char       *fs2_str (fs2 *s);
const char *fs2_cstr(const fs2 *s);
```

### Predicates

```c
bool fs2_is_static   (const fs2 *s);
bool fs2_is_local    (const fs2 *s);
bool fs2_is_alloc    (const fs2 *s);
bool fs2_is_bodyalloc(const fs2 *s);
bool fs2_is_empty    (const fs2 *s);   /* len == 0 */
bool fs2_is_null     (const fs2 *s);   /* s == NULL || v == NULL */
```

### Constructors — value

Return an `fs2` by value. Free with `fs2_free` when done.

| Function | Purpose |
|---|---|
| `fs2 fs2_init(size_t sz)` | growable, capacity `sz` (rounded up, min 16) |
| `fs2 fs2_fromstr(const char *src)` | copy of a C-string |
| `fs2 fs2_fromstrf(const char *fmt, ...)` | printf-formatted copy |
| `fs2 fs2_clone(const fs2 *src)` | deep copy of any `fs2` |
| `fs2 fs2_movefrom(fs2 *src)` | steal; `*src` becomes `DEFUNCT` |
| `fs2 fs2_movefromstr(const char **from)` | steal a heap C-string |
| `fs2 fs2_wrap(const char *str)` | zero-copy STATIC view (NULL -> empty) |
| `fs2 fs2_substr_new(const fs2 *s, size_t from, size_t len)` | substring, clamped |
| `fs2 fs2_left_new(const fs2 *s, size_t len)` | prefix |
| `fs2 fs2_right_new(const fs2 *s, size_t len)` | suffix |
| `fs2 fs2_toupper_new(const fs2 *s)` | clone + uppercase |
| `fs2 fs2_tolower_new(const fs2 *s)` | clone + lowercase |
| `fs2 fs2_genrnd_new(size_t len, char type)` | random string (`'a'`/`'A'`/`'0'`) |
| `fs2 fs2_lpad_new(const fs2 *s, size_t len, const fs2 *pad)` | left pad (clone + pad) |
| `fs2 fs2_rpad_new(const fs2 *s, size_t len, const fs2 *pad)` | right pad |

### Constructors — heap

Same operations, but the `fs2` struct itself is `malloc`-ed. Free with
`fs2_free` (releases buffer, struct, or both per flags).

Every `fs2_heap_*` twin of the value constructors exists:
`fs2_heap_init`, `fs2_heap_fromstr`, `fs2_heap_fromstrf`,
`fs2_heap_clone`, `fs2_heap_movefrom`, `fs2_heap_movefromstr`,
`fs2_heap_substr`, `fs2_heap_left`, `fs2_heap_right`,
`fs2_heap_toupper`, `fs2_heap_tolower`, `fs2_heap_genrnd`,
`fs2_heap_lpad`, `fs2_heap_rpad`.

### Mutation — append / overwrite

| Function | Purpose |
|---|---|
| `fs2 *fs2_cat(fs2 *dst, const fs2 *src)` | append `src` |
| `fs2 *fs2_catstr(fs2 *dst, const char *src)` | append C-string |
| `fs2 *fs2_rev_catstr(fs2 *dst, const char *beginner)` | prepend C-string |
| `fs2 *fs2_cpy(fs2 *dst, const fs2 *src)` | overwrite with `src` |
| `fs2 *fs2_cpystr(fs2 *dst, const char *src)` | overwrite with C-string (NULL -> clear) |

### Mutation — substrings

| Function | Purpose |
|---|---|
| `fs2 *fs2_substr(fs2 *s, size_t from, size_t len)` | in-place; clamped |
| `fs2 *fs2_left(fs2 *s, size_t len)` | keep first `len` bytes |
| `fs2 *fs2_right(fs2 *s, size_t len)` | keep last `len` bytes |

### Mutation — padding / case / random

| Function | Purpose |
|---|---|
| `fs2 *fs2_lpad(fs2 *s, size_t len, const fs2 *pad)` | left pad (Postgres) |
| `fs2 *fs2_rpad(fs2 *s, size_t len, const fs2 *pad)` | right pad (Postgres) |
| `fs2 *fs2_lpadstr(fs2 *s, size_t len, const char *pad)` | C-string variant |
| `fs2 *fs2_rpadstr(fs2 *s, size_t len, const char *pad)` | C-string variant |
| `fs2 *fs2_toupper(fs2 *s)` | uppercase in place (UTF-8 safe) |
| `fs2 *fs2_tolower(fs2 *s)` | lowercase in place (UTF-8 safe) |
| `fs2 *fs2_genrnd(fs2 *s, size_t len, char type)` | random string |

### Finders — characters

All return `ptrdiff_t` (offset or `-1`). Never match `'\0'`.

| Function | Search | Case | Limit |
|---|---|---|---|
| `fs2_chr` | first | sensitive | – |
| `fs2_nchr` | first | sensitive | first `lim` |
| `fs2_ichr` | first | insensitive | – |
| `fs2_nichr` | first | insensitive | first `lim` |
| `fs2_rchr` | last | sensitive | – |
| `fs2_nrchr` | last | sensitive | last `lim` |
| `fs2_irchr` | last | insensitive | – |
| `fs2_nirchr` | last | insensitive | last `lim` |

Nth-occurrence variants (`_nth` suffix, `k` is 1-based):

```text
fs2_chr_nth(s, c, k)         fs2_rchr_nth(s, c, k)
fs2_nchr_nth(s, c, k, lim)   fs2_nrchr_nth(s, c, k, lim)
fs2_ichr_nth(s, c, k)        fs2_irchr_nth(s, c, k)
fs2_nichr_nth(s, c, k, lim)  fs2_nirchr_nth(s, c, k, lim)
```

Invariant: `fs2_chr_nth(s, c, 1) == fs2_chr(s, c)`.

### Finders — arrays

Search a NULL-terminated array of C-strings (full scan):

```c
bool fs2_in      (const fs2 *s, const char *restrict strs[]);
bool fs2_notin   (const fs2 *s, const char *restrict strs[]);
bool fs2_iin     (const fs2 *s, const char *restrict strs[]);   // case-insens
bool fs2_not_iin (const fs2 *s, const char *restrict strs[]);
```

Variadic macros for ergonomics:

```c
FS2_IN     (&s, "apple", "banana", "cherry")
FS2_IIN    (&s, "apple", "banana", "cherry")
FS2_NOT_IN (&s, "apple", "banana")
FS2_NOT_IIN(&s, "apple", "banana")
```

### Formatting

| Function | Purpose |
|---|---|
| `long fs2_sprintf(fs2 *s, const char *fmt, ...)` | write from pos 0, updates len |
| `long fs2_sprintf_position(fs2 *s, size_t pos, const char *fmt, ...)` | raw writer, no len update |
| `long fs2_vsprintf_position(fs2 *s, size_t pos, const char *fmt, va_list ap)` | va_list variant |

`fs2_sprintf` returns the number of characters written, or negative on
format error (len is left untouched in that case).

### Move assignment

```c
fs2 *fs2_move(fs2 *to, fs2 *from);
```

`to <- from`, then `from -> DEFUNCT`. Frees whatever `to` held before.
Safe on same pointer (`to == from` -> no-op).

### Destructor

```c
void fs2_free(fs2 *s);
```

- No-op on `STATIC`, `LOCAL`, `DEFUNCT`.
- Frees the buffer if `ALLOC`.
- Frees the struct if `BODYALLOC`.
- Idempotent for value cases.

### Utilities

| Function | Purpose |
|---|---|
| `fs2 *fs2_setlen(fs2 *s, size_t pos)` | set `len = pos`, write NUL at `v[pos]` |
| `fs2 *fs2_clear(fs2 *s)` | shorthand for `fs2_setlen(s, 0)` |
| `bool fs2_validate(FILE *out, const fs2 *s)` | check invariants; log to `out` + log |
| `long fs_techfprint(FILE *out, const fs2 *s, const char *name)` | debug dump |

## Internal helpers (used by the API, exposed for advanced users)

Do not call these from application code unless you know why. They exist
to keep `fs2_api.c` free of raw `memcpy`/`memmove` footguns.

```c
// Copy between strings (src must not alias dst — restrict).
fs2 *fs2_memcpy    (fs2 *restrict dst, size_t dstpos,
                    const fs2 *restrict src, size_t srcpos, size_t len);

// Copy from C-string.
fs2 *fs2_memcpy_str(fs2 *restrict dst, size_t dstpos,
                    const char *restrict src, size_t len);

// Move within or between strings (handles realloc + overlap).
// src == NULL means "move inside dst".
fs2 *fs2_memmove   (fs2 *dst, size_t dstpos,
                    const fs2 *src, size_t srcpos, size_t len);

// Sugar for in-place moves.
fs2 *fs2_selfmove  (fs2 *dst, size_t to, size_t from, size_t len);

// Clamp from/len to a valid range for substring operations.
void fs2_substr_bounds(const fs2 *s, size_t *from, size_t *len);
```

## Usage

```c
#include <fs2.h>
#include <fs2_api.h>

int main(void) {
    fs2 greeting = FS2LITERAL("hello");
    fs2 buffer = fs2_fromstr("world");

    fs2_catstr(&buffer, "!");
    // buffer == "world!"

    fs2_cat(&buffer, &greeting);
    // buffer == "world!hello"

    fs2_rev_catstr(&buffer, ">>> ");
    // buffer == ">>> world!hello"

    fs2_substr(&buffer, 4, 5);
    // buffer == "world"

    fs2_free(&buffer);
    return 0;
}
```

Compile with:

```text
cc -std=gnu11 \
   -Ipath/to/fs2/include \
   -Ipath/to/common/include \
   -Ipath/to/error/include \
   -Ipath/to/log/include \
   -Ipath/to/bool/include \
   -Ipath/to/checker/include \
   main.c \
   -Lpath/to/build/debug -lfs2 -lerror -llog -lcommon -lbool -lchecker \
   -o main
```

`REQUIRES` in `fs2/Makefile` maps to the `-I` / `-l` chain automatically
when building inside `ngutils`.

## Status

**0.1.0** — the primitive layer is feature-complete for common
string work. What's in:

- struct, flags, all value macros,
- getters, predicates, validation, tech print,
- all constructors (value + heap),
- append / overwrite (`cat`, `cpy`, and string variants),
- substrings (`substr`, `left`, `right`),
- padding (`lpad`, `rpad`, Postgres semantics),
- case conversion (UTF-8 safe),
- random generator (3 modes),
- character finders (8 base + 8 nth-occurrence),
- array finders (`in`, `notin`, insensitive, variadic macros),
- formatting (`sprintf` family with position),
- internal `memcpy`/`memmove` wrappers.

**Not yet implemented**:

- **Comparators** (`fs2_cmp`, `fs2_icmp`, `fs2_ncmp`) — needed before
  `bsearch` and sorting.
- **Binary search finders** (`fs2_in_bsearch`, etc.) — depends on
  comparators.
- **Substring search** (`fs2_strstr`, `fs2_istrstr`).
- **Sorting** (`fs2_sort` with custom comparator).
- **Protected layer** (`pfs2.h`, `pfs2.c`) — wraps every primitive with
  `userraise`/`sysraise` checks.
- **Checked header** (`cfs2.h`) — header-only guards that compile out
  under `-DNOINVARIANT` (alternative naming for the same idea).

## Notes

- **`fs2` is the unchecked layer.** No NULL checks in release builds, no
  overflow checks, no "are you sure" questions. If you want guards,
  include `cfs2.h`/`pfs2.h` (planned) or call `fs2_validate` yourself at
  strategic points.
- **`invraise` is debug-only.** It expands to nothing under `-DNDEBUG`.
  `userraise` and `sysraise` survive in release (they signal real user
  errors or OOM), and `err_raise` is `__attribute__((noreturn))`.
- **OOM is not fatal-by-abort.** Allocation failures route through
  `sysraise(...)`, so an enclosing `TRY()` can catch them, or the
  process raises `SIGINT` and produces a core dump.
- **Minimum allocation** is `FS2_MIN_ALLOC` (16 bytes). Growth uses a
  power-of-two policy (`calcnewsize(SIZE_POWER2, ...)`).
- **`restrict` is everywhere it matters.** `fs2_cat(&s, &s)` and
  `fs2_cpy(&s, &s)` are UB. If you need to append a string to itself,
  clone it first.
- **Byte counters** (`g_alloc_cnt`, `g_free_cnt`, `g_alloc_body_cnt`,
  `g_free_body_cnt`) are `atomic_int`, exposed via
  `fs2_checkleak(...)` for test harnesses.
- **UTF-8 is bytes, not characters.** `fs2_toupper`/`fs2_tolower` use
  `tolower((unsigned char)c)` which only affects ASCII in the C locale.
  Multi-byte UTF-8 is preserved verbatim (no corruption, no
  transformation).

## Development

Uses [Criterion](https://criterion.readthedocs.io/) for unit tests.
Runs under ASan+UBSan on Linux (Docker) and native on macOS.

```text
make test             # build and run tests
make lib              # debug library
make librelease       # release library
make check            # clang-tidy
make analyze          # clang static analyzer
make clean
```

CI (`.github/workflows/ci.yml`) runs a matrix:

| Job | Compiler | Sanitizer |
|---|---|---|
| `test (clang)` | clang | – |
| `test (gcc)` | gcc | – |
| `sanitize (lsan)` | clang | ASan + LeakSanitizer |
| `sanitize (ubsan)` | clang | ASan + UBSan |

Sanitizer jobs are clang-only — reference implementation, tighter
diagnostics than GCC's port.

**Known CI workaround**: release build passes
`-Wno-error=format-truncation` to work around a false positive in
GCC 13+ `_FORTIFY_SOURCE=3` builds (GCC Bugzilla #102218).

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
├── Makefile            PROJNAME + REQUIRES
├── README.md
└── CHANGELOG.md
```

## License

See the top-level `LICENSE` file.

