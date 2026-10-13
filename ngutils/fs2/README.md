# fs2

Placeholder utility. Replace this description with what fs2
actually does.

## Layout

```
include/   public header
src/       implementation
test/      Criterion tests
Makefile   PROJNAME + include ../Makefile.common
```

All build rules (`lib`, `librelease`, `test`, `check`, `analyze`,
`clean`) come from `../Makefile.common`. See `../README.md`.

## Version

Bump in `include/fs2.h`:

```c
#define FS2_VERSION_MAJOR 0
#define FS2_VERSION_MINOR 1
#define FS2_VERSION_PATCH 0
```

Add the previous release to the `versions[]` array in `src/fs2.c`.

