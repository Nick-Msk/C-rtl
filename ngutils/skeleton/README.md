# skeleton

Template utility. Not built by the dispatcher — copy this directory
to create a new utility, then rename everything named `skeleton`.

## How to make a new utility from this template

```bash
cd ngutils
cp -r skeleton myservice
cd myservice

# Rename files
mv include/skeleton.h    include/myservice.h
mv src/skeleton.c        src/myservice.c
mv test/test_skeleton.c  test/test_myservice.c

# Update PROJNAME
sed -i '' 's/PROJNAME = skeleton/PROJNAME = myservice/' Makefile
# (Linux: sed -i 's/.../.../')

# Update include guards and includes in the three files
# Replace "skeleton" → "myservice", "SKELETON_H" → MYSERVICE_H

# Verify
cd ../..
make list          # should now show myservice
make test-myservice
```

## What's in the box

```
include/   public header
src/       implementation
test/      Criterion tests
Makefile   2 lines: PROJNAME + include ../Makefile.common
```

All build rules (lib, librelease, test, check, analyze, clean)
come from `../Makefile.common`. See `../README.md` for details.

