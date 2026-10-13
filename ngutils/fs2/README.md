# fs2.h

[![CI](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml/badge.svg)](https://github.com/Nick-Msk/C-rtl/actions/workflows/ci.yml)

Core primitives for a growable C string type.

`fs2` is a **faststring**: a small struct that owns a NUL-terminated
buffer together with its length and capacity. It sits between raw
`char *` (no length, manual realloc) and a heavy string library
(always heap, hidden allocations). The trade-off is explicit: the
struct records *who owns what*, and the caller decides.

Depends on `common`, `error`, `log`, `bool`.

## Design: two orthogonal axes

Every `fs2` value is described by **two independent flags**:

| Axis | Values | Meaning |
|---|---|---|
| Where does the string live? | `FS2_STATIC` | read-only, external (literal) |
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
| `DEFUNCT` | moved out | nothing |
| `DEFUNCT \| BODYALLOC` | moved out, struct on heap | `free(struct)` |

This is what "symmetric API" means in this project: every value
operation (`fs2_fromstr`, `fs2_clone`, `fs2_movefrom`, ...) has a
matching heap-body variant (`fs2_heap_fromstr`, `fs2_heap_clone`,
`fs2_heap_movefrom`, ...), and both accept any string-axis.

## Value macros

| Macro | Result |
|---|---|
| `FS2()` | empty growable (`ALLOC`, no capacity yet) |
| `FS2_EMPTY` | empty read-only (`STATIC`, `""`) |
| `FS2_LITERAL(s)` | wrap a compile-time string; `strlen` for free via `sizeof` |
| `FS2_LOCAL(buf)` | wrap a caller-owned `char[]` (fixed capacity) |
| `FS2_DEFUNCT_VAL` | moved-out marker; only free / assignment valid |

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
bool fs2_is_defunct  (const fs2 *s);
bool fs2_is_empty    (const fs2 *s);   /* len == 0 */
bool fs2_is_null     (const fs2 *s);   /* s == NULL || v == NULL */
```

### Constructors — value

Return an `fs2` by value. Free with `fs2_free` when done.

| Function | Purpose |
|---|---|
| `fs2 fs2_init(size_t sz)` | growable, capacity `sz` (rounded up, minimum 16) |
| `fs2 fs2_fromstr(const char *src)` | copy of a C-string |
| `fs2 fs2_fromstrf(const char *fmt, ...)` | printf-formatted copy |
| `fs2 fs2_clone(const fs2 *src)` | deep copy of any `fs2` |
| `fs2 fs2_movefrom(fs2 *src)` | steal; `*src` becomes `DEFUNCT` |
| `fs2 fs2_movefromstr(const char **from)` | steal a heap C-string; `*from` → `NULL` |

### Constructors — heap

Same operations, but the `fs2` struct itself is `malloc`-ed. Free with
`fs2_free` (which releases the buffer, the struct, or both, per flags).

| Function | Purpose |
|---|---|
| `fs2 *fs2_heap(void)` | empty, struct on heap |
| `fs2 *fs2_heap_init(size_t sz)` | growable, struct on heap |
| `fs2 *fs2_heap_fromstr(const char *src)` | copy, struct on heap |
| `fs2 *fs2_heap_fromstrf(const char *fmt, ...)` | formatted, struct on heap |
| `fs2 *fs2_heap_clone(const fs2 *src)` | deep copy, struct on heap |
| `fs2 *fs2_heap_movefrom(fs2 *src)` | steal + push body to heap |
| `fs2 *fs2_heap_movefromstr(const char **from)` | steal C-string, struct on heap |

### Move assignment

```c
fs2 *fs2_move(fs2 *to, fs2 *from);
```

`to ← from`, then `from → DEFUNCT`. Frees whatever `to` held before.
Safe on same pointer (`to == from` → no-op).

### Destructor

```c
void fs2_free(fs2 *s);
```

- No-op on `STATIC`, `LOCAL`, `DEFUNCT`.
- Frees the buffer if `ALLOC`.
- Frees the struct if `BODYALLOC`.
- Idempotent for value cases (safe to call twice on `ALLOC` that's
  already been reset).

### Utilities

| Function | Purpose |
|---|---|
| `fs2 *fs2_setlen(fs2 *s, size_t pos)` | set `len = pos`, write NUL at `v[pos]` |
| `bool fs2_validate(FILE *out, const fs2 *s)` | check invariants; log to `out` + log |
| `long fs_techfprint(FILE *out, const fs2 *s, const char *name)` | debug dump: flags, len, sz, first N bytes |

## Usage

```c
#include <fs2.h>

int main(void) {
    /* A literal — zero allocation, safe to pass around. */
    fs2 greeting = FS2_LITERAL("hello");

    /* A growable empty string. */
    fs2 buffer = FS2();

    /* Later: capacity management, appends, etc. — see fs2_api.h
     * (not yet written; see "Status" below). */

    fs2_free(&buffer);
    /* greeting needs no free: it is STATIC. */
    return 0;
}
```

Compile with:

```
cc -std=gnu11 \
   -Ipath/to/fs2/include \
   -Ipath/to/common/include \
   -Ipath/to/error/include \
   -Ipath/to/log/include \
   main.c \
   -Lpath/to/.../build/debug -lfs2 -lerror -llog -lcommon \
   -o main
```

`REQUIRES` in `fs2/Makefile` maps to the `-I` / `-l` chain automatically
when building inside `ngutils`.

## Status

This is **0.1.0** — the *core* layer only. What's in:

- struct, flags, all value macros,
- getters and predicates,
- `fs2_init`, `fs2_free`,
- `fs2_setlen`, `fs2_validate`, `fs_techfprint`,
- per-operation allocation counters (atomic).

What's **declared but not yet implemented** (link will fail if used):

- `fs2_fromstr`, `fs2_fromstrf`, `fs2_clone`,
- `fs2_movefrom`, `fs2_movefromstr`, `fs2_move`,
- all `fs2_heap_*` constructors,
- `fs2_reserve`, `fs2_shrink`, `fs2_clear` (planned in `fs2_api.h`).

Planned files:

- `fs2_api.h` — higher-level operations: `append`, `printf`, `substr`,
  `reserve`, comparison helpers.
- `cfs2.h` — header-only *checked* wrappers: every `cfs2_*` validates
  its arguments via `invraisecode` from `checker`, then delegates to the
  matching `fs2_*`. Compiles out under `-DNOINVARIANT`.

## Notes

- **`fs2` is the unchecked layer.** No NULL checks, no overflow checks,
  no "are you sure" questions. If you want guards, include `cfs2.h`
  (planned) or call `fs2_validate` yourself at strategic points.
- **OOM is not fatal-by-abort.** Allocation failures route through
  `sysraise(...)` from `error.h`, so an enclosing `TRY()` can catch
  them, or the process raises `SIGINT` and produces a core dump.
- **Minimum allocation** is `FS2_MIN_ALLOC` (16 bytes). Small requests
  are rounded up; growth uses power-of-two policy
  (`calcnewsize(SIZE_POWER2, ...)` from `common.h`).
- **Byte counters** (`g_alloc_cnt`, `g_free_cnt`, `g_alloc_body_cnt`,
  `g_free_body_cnt`) are `atomic_int` and are exposed via checker
  helpers planned alongside `fs2_api.h`.

## Development

Uses [Criterion](https://criterion.readthedocs.io/) for unit tests.

```
make test       # build and run
make lib        # debug library
make librelease # release library
make check      # clang-tidy
make analyze    # clang static analyzer
make clean
```

### Layout

```
.
├── include/fs2.h       public header
├── src/fs2.c           implementation
├── test/test_fs2.c     unit tests
├── build/              artifacts (gitignored)
├── Makefile            PROJNAME + REQUIRES
├── README.md
└── CHANGELOG.md
```

## License

See the top-level `LICENSE` file.

