#!/bin/sh
# cloneproj — create a new utility from skeleton/.
#
# Usage:
#   scripts/cloneproj <name>
#   make cloneproj NAME=<name>
#
# Copies skeleton/ to <name>/, renames files, and rewrites every
# occurrence of "skeleton" → <name> and "SKELETON" → <NAME>.

set -eu

if [ $# -ne 1 ]; then
    echo "usage: $0 <name>" >&2
    exit 1
fi

name="$1"

case "$name" in
    [a-z]*) ;;
    *) echo "error: name must start with a lowercase letter" >&2; exit 1 ;;
esac

case "$name" in
    *[!a-z0-9_]*) echo "error: name may only contain [a-z0-9_]" >&2; exit 1 ;;
esac

script_dir="$(cd "$(dirname "$0")" && pwd)"
root_dir="$(cd "$script_dir/.." && pwd)"
src="$root_dir/skeleton"
dst="$root_dir/$name"

[ -d "$src" ] || { echo "error: skeleton not found at $src" >&2; exit 1; }
[ ! -e "$dst" ] || { echo "error: $dst already exists" >&2; exit 1; }

upper=$(printf '%s' "$name" | tr '[:lower:]' '[:upper:]')

echo "Creating $name from skeleton..."

# 1. Copy, drop build artifacts and vcs metadata
cp -R "$src" "$dst"
rm -rf "$dst/build" "$dst/.git" 2>/dev/null || true

# 2. Rename files
for pair in \
    "include/skeleton.h:include/$name.h" \
    "src/skeleton.c:src/$name.c" \
    "test/test_skeleton.c:test/test_$name.c"
do
    from="${pair%%:*}"
    to="${pair##*:}"
    [ -f "$dst/$from" ] && mv "$dst/$from" "$dst/$to"
done

# 3. Rewrite contents (perl is on macOS and Linux)
for f in \
    "$dst/Makefile" \
    "$dst/README.md" \
    "$dst/CHANGELOG.md" \
    "$dst/include/$name.h" \
    "$dst/src/$name.c" \
    "$dst/test/test_$name.c"
do
    [ -f "$f" ] || continue
    perl -pi -e "s/skeleton/$name/g; s/SKELETON/$upper/g" "$f"
done

echo "Created: $dst"
echo
echo "Next:"
echo "  cd $name && make test"
echo "  cd .. && make test-$name"

