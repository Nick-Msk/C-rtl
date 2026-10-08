# ngutils

A small collection of independent C utility headers. Each utility is
versioned and released on its own; you can use any subset of them, or
link the whole set as a single archive.

## Utilities

| Utility | Version | Description |
|---|---|---|
| [`bool`](bool/) | 0.2.0 | `bool` ↔ string conversion (`bool_str`, `bool_tryparse`, `bool_parsedef`) |
| [`log`](log/)   | 0.1.0 | File-based logging with per-module levels and indent-aware macros |

Each utility has its own `README.md`, `CHANGELOG.md`, and version API
(`bool_version()`, `log_version()`). They do not depend on each other
and can be adopted individually.

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

## Using the whole set

Build everything once and link against the combined archive:

```
cd ngutils
make lib            # → build/debug/libngutils.a (and per-utility .a)
```

```
cc -Ipath/to/ngutils/bool/include \
   -Ipath/to/ngutils/log/include \
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
make analyze          # clang static analyzer (scan-build)
make install          # to $(PREFIX), default $HOME/.local
make docs             # doxygen HTML → docs/html/index.html
make build-docker     # build the Linux dev image
make test-docker      # run tests inside a Linux container
make clean            # remove all build artifacts
```

Run a single target on a single utility:

```
make test-bool
make lib-log
make check-bool
```

## Layout

```
.
├── Makefile.common     shared build rules (included by each utility)
├── Makefile            dispatcher
├── bool/               utility: bool ↔ string
│   ├── include/bool.h
│   ├── src/bool.c
│   └── test/test_bool.c
├── log/                utility: file-based logging
│   ├── include/log.h
│   ├── src/log.c
│   └── test/test_log.c
├── Dockerfile.dev      Linux dev/CI environment
├── Doxyfile            API documentation config
├── CHANGELOG.md        umbrella changelog
├── README.md
└── LICENSE
```

## Versioning

Each utility carries its own version:

- `BOOL_VERSION` / `bool_version()` / `bool_versions()`
- `LOG_VERSION` / `log_version()` / `log_versions()`

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

## License

See [LICENSE](LICENSE).

