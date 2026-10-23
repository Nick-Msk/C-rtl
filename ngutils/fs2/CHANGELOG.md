# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned

- `fs2_api.h` — high-level operations: `append`, `appendstr`,
  `appendmem`, `printf`, `vprintf`, `substr`, `reserve`, `shrink`,
  `clear`, comparison helpers.
- `cfs2.h` — header-only checked wrappers around `fs2_*` using
  `invraisecode` from `checker`; compiles out with `-DNOINVARIANT`.
- Remaining core constructors: `fs2_fromstr`, `fs2_fromstrf`,
  `fs2_clone`, `fs2_movefrom`, `fs2_movefromstr`, `fs2_move`, and all
  `fs2_heap_*` variants.
- Public accessors for the allocation counters (`fs2_alloc_check`,
  `fs2_fprint_checker_cnt`) — mirroring the `fs_*` API from the
  legacy `fs` utility.

## [0.1.0] - 2026-10-13

### Added

- **Struct and flags.** Two orthogonal axes encoded in `fs2_flags`:
  - string location: `FS2_STATIC`, `FS2LOCAL`, `FS2_ALLOC`
    (exactly one is set),
  - body location: `FS2_BODYALLOC` (independent).
  
  Plus state flags `FS2_CONST` (reserved) and `FS2_DEFUNCT` (moved out).
- **Value macros:** `FS2()`, `FS2EMPTY`, `FS2LITERAL(s)`,
  `FS2LOCAL(buf)`, `FS2_DEFUNCT_VAL`.
- **Getters:** `fs2_len`, `fs2_sz`, `fs2_str`, `fs2_cstr`.
- **Predicates:** `fs2_is_static`, `fs2_is_local`, `fs2_is_alloc`,
  `fs2_is_bodyalloc`, `fs2_is_defunct`, `fs2_is_empty`, `fs2_is_null`.
- **Value constructors / destructor:**
  - `fs2_init(size_t sz)` — growable, capacity rounded up to the next
    power of two, floored at `FS2_MIN_ALLOC` (16 bytes).
  - `fs2_free(fs2 *)` — releases the buffer if `ALLOC`, the struct if
    `BODYALLOC`, nothing for `STATIC` / `LOCAL` / `DEFUNCT`. Idempotent
    for value cases.
- **Utilities:** `fs2_setlen`, `fs2_validate`, `fs_techfprint`.
- **Atomic allocation counters** (allocation, free, both for string and
  body): `g_alloc_cnt`, `g_free_cnt`, `g_alloc_body_cnt`,
  `g_free_body_cnt`.
- **Version API:** `fs2_version()`, `fs2_versions()`,
  `FS2_VERSION` macro.
- **30 unit tests** covering macros, getters, predicates, `fs2_init`
  (zero / small / power-of-two / huge-OOM), `fs2_free` for every flag
  combination, `fs2_setlen`, `fs2_is_empty` / `fs2_is_null`,
  `fs2_validate` (OK / NULL / forged invariants / stream output),
  `fs_techfprint` (data / NULL / NULL out).

### Fixed

- `fs2_init(0)` no longer dereferences a NULL buffer.
  `FS2_MIN_ALLOC` (16) floors every allocation, so even a zero-size
  request yields a valid `v == "\0"`.

### Design notes

- This release is the **core** layer: struct, flags, constructors,
  move/copy, adopt, free. Higher-level string operations live in
  `fs2_api.h` (planned).
- Allocation failures route through `sysraise(...)` from `error.h` —
  not `abort()`. An enclosing `TRY()` can catch them; otherwise the
  process raises `SIGINT` and dumps core, which is the intended
  behaviour for an OOM in the core layer.
- The `LOGNPRINTF` helper in `src/fs2.c` is a temporary macro; it will
  move to `common.h` once a second consumer needs it.
