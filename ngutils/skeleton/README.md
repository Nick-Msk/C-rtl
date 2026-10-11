# skeleton

Placeholder utility. Replace this description with what skeleton
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

Bump in `include/skeleton.h`:

```c
#define SKELETON_VERSION_MAJOR 0
#define SKELETON_VERSION_MINOR 1
#define SKELETON_VERSION_PATCH 0
```

Add the previous release to the `versions[]` array in `src/skeleton.c`.

