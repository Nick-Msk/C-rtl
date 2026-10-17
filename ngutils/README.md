# ngutils

A small collection of independent C utility headers. Each utility is
versioned and released on its own; you can use any subset of them, or
link the whole set as a single archive.

## Utilities

| Utility | Version | Description |
|---|---|---|
| [`bool`](bool/)       | 0.2.0 | `bool` ↔ string conversion (`bool_str`, `bool_tryparse`, `bool_parsedef`) |
| [`log`](log/)         | 0.1.0 | File-based logging with per-module levels and indent-aware macros |
| [`error`](error/)     | 0.2.0 | Per-thread error stack and TRY/CATCH exceptions (`err_raise`, `userraise`, `sysraise`) |
| [`checker`](checker/) | 0.1.0 | Assert-style invariants (`inv`, `invraise`, `inv2`) and runtime validators (`check_arg`) |
| [`common`](common/)   | 0.1.0 | Cross-utility helpers: `MIN`/`MAX`, `try_parse_*`, `round_up_2`, `pchar_cmp` |

Each utility has its own `README.md`, `CHANGELOG.md`, and version API
(`bool_version()`, `log_version()`). They are not required to be used
together — `error` depends on `log`, but `bool` and `log` are
standalone.

## Using a single utility

Copy the utility's `include/` and `src/` into your project, or link its
standalone archive:

```
cc -Ipath/to/ngutils/bool/include \
   main.c \
   -Lpath/to/ngutils/bool/build/debug -lbool \
   -o main
```

Only the symbols you use get pulled in from the archive — no extra
overhead for unused utilities.

If the utility has `REQUIRES` (declared in its `Makefile`), you also
need those siblings' archives. For example, `checker` requires `log`:

```
cc -Ipath/to/ngutils/checker/include \
   -Ipath/to/ngutils/log/include \
   main.c \
   -Lpath/to/ngutils/log/build/debug     -llog \
   -Lpath/to/ngutils/checker/build/debug -lchecker \
   -o main
```

The `REQUIRES` line in each utility's Makefile is authoritative —
check it if linking fails with unresolved symbols.

## Using the whole set

Build everything once and link against the combined archive:

```
cd ngutils
make lib            # → build/debug/libngutils.a (and per-utility .a)
```

```
cc -Ipath/to/ngutils/bool/include \
   -Ipath/to/ngutils/log/include \
   -Ipath/to/ngutils/error/include \
   main.c \
   -Lpath/to/ngutils/build/debug -lngutils \
   -o main
```

Or install to a prefix and use standard `-I` / `-L` paths:

```
make install PREFIX=$HOME/.local
cc -I$HOME/.local/include main.c -L$HOME/.local/lib -lngutils -o main
```

## Building

Each utility is standalone: `cd <utility> && make test`. The dispatcher
at the top level runs the same across all of them:

```
make all              # lib + test + librelease in every utility
make test             # run all unit tests
make lib              # per-utility .a + build/debug/libngutils.a
make librelease       # release archives in build/release/
make check            # clang-tidy on all utilities
make analyze          # clang static analyzer on library sources
make analyze-test     # clang static analyzer on tests (noisier)
make install          # to $(PREFIX), default $HOME/.local
make docs             # doxygen HTML → docs/html/index.html
make build-docker     # build the Linux dev image
make test-docker      # run tests inside a Linux container
make clean            # remove all build artifacts
make clean-docker     # remove /tmp/ngutils-docker-build
```

Run a single target on a single utility:

```
make test-bool
make lib-log
make check-error
```

## How the build works

- **`Makefile.common`** holds all shared rules. Each utility's `Makefile`
  is 2–6 lines: `PROJNAME` plus `include ../Makefile.common`.
- **`REQUIRES`** in a utility's `Makefile` declares sibling dependencies.
  For example, `error/Makefile` has `REQUIRES = log bool`. The build then
  adds `-I../log/include -I../bool/include` and links `liblog.a`,
  `libbool.a` automatically via sub-make.
- **Per-project layout**: sources in `src/`, headers in `include/`, tests
  in `test/`. Build artifacts land under `build/debug/` (with `-g -O0`)
  and `build/release/` (with `-O2 -DNDEBUG`).
- **`test-docker`** mounts a temporary directory over each project's
  `build/`, so Linux artifacts never touch the host build tree and
  incremental in-container builds work.

## Layout

```
.
├── Makefile.common     shared build rules
├── Makefile            dispatcher
├── bool/               utility: bool ↔ string
├── log/                utility: file-based logging
├── error/              utility: error stack and exceptions
├── skeleton/           template (excluded from builds)
├── Dockerfile.dev      Linux dev/CI environment
├── Doxyfile            API documentation config
├── CHANGELOG.md        umbrella changelog
├── DISCLAIMER.md
├── README.md
└── LICENSE
```

## Versioning

Each utility carries its own version:

- `BOOL_VERSION` / `bool_version()` / `bool_versions()`
- `LOG_VERSION` / `log_version()` / `log_versions()`
- `error` currently has no version API.

`*_versions()[0]` is always the current version, older releases follow.

`ngutils` as a whole does not have a version number. The umbrella
`CHANGELOG.md` uses release dates instead, and records what utilities
and infrastructure exist at that point. The individual utility
versions are the ones you should track.

## Development

Requirements:

- A C compiler (`cc`, `gcc`, or `clang`)
- GNU Make
- [Criterion](https://criterion.readthedocs.io/) for unit tests
- Optional: `clang-tidy`, `scan-build` for `make check` / `make analyze`

On macOS:

```
brew install criterion pkg-config
brew install llvm cppcheck          # optional
```

See each utility's `README.md` for API details and examples.

## Test fixture (`test_common.h`)

`test_common.h` provides a shared fixture that most projects use:

```c
#include "test_common.h"
TestSuite(myproj, .init = test_setup, .fini = test_teardown);
```

`test_setup` / `test_teardown` handle:

- `logs/` creation,
- per-test log files (`log_<projname>_<testname>_<pid>.log`),
- stderr muting during the test run.

After the suite, `Makefile.common` merges per-test logs into
`logs/result/all.log`.

### Why some projects still use their own fixture

`test_common.h` includes `log.h` (for `log_init`, `log_mkdir`). That
creates a dependency from the test layer of a project onto `log`.
For projects that **are** `log`, or whose library is not allowed to
depend on `log` even at test time, this would be circular or simply
wrong.

Status:

| Project | Fixture | Reason |
|---|---|---|
| `bool` | local (own `setup`/`teardown`) | library must not depend on `log`; pulling `log` into `bool` tests, even via `TEST_REQUIRES`, makes `bool` non-standalone |
| `log`  | local (own `setup`/`teardown`) | circular dependency (`common` → `log`, `log` → `common`) |
| `checker` | `test_setup` (migrating) | `log` is already a real dependency of the checker library |
| `error` | `test_setup` (migrating) | needs `err_clean`/`err_resetenv` on top; keep its own wrapper calling `test_setup` |
| `common` | under question | `log` is already a real dependency of the `common` library; if `log` moves away from `common`, the fixture can be shared |
| `fs2` | `test_setup` | every test dep is already a lib dep |

Projects with the local fixture produce no `logs/` output — `cat`
in `Makefile.common` sees no `log_<proj>_*.log` files and creates an
empty `logs/result/all.log`. That is expected, not a bug.

### If you migrate a project

1. Delete the local `setup`/`teardown` from `test_<proj>.c`.
2. Replace `TestSuite(proj, .init = setup, .fini = teardown);`
   with `TestSuite(proj, .init = test_setup, .fini = test_teardown);`.
3. Add whatever the project needs **on top** — either call
   `test_setup()` first from a local wrapper, or accept that the
   project has no special setup.
4. Ensure `common` is reachable: `REQUIRES` or `TEST_REQUIRES` must
   contain `common`, and `log` (`test_common.h` includes it).

## Disclaimer

See [DISCLAIMER.md](DISCLAIMER.md).

## License

See [LICENSE](LICENSE).

