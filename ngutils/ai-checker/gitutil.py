"""List and delete ai-checker fix branches.

Safety model:
  - By default, only branches already merged into HEAD are deleted
    (git branch -d). Unmerged branches are listed but preserved.
  - --force / FORCE=1 flips to git branch -D (delete regardless).
  - --dry-run / DRY_RUN=1 prints the plan without touching git.
"""
from __future__ import annotations

import argparse
import subprocess
import sys

from common import ROOT


def _git(*args: str, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=ROOT, text=True,
                          capture_output=True, check=check)


def _list(prefix: str) -> tuple[list[str], list[str]]:
    """Return (merged, unmerged) branches whose short name starts
    with prefix. Excludes the current branch (git refuses to delete it
    anyway, but we skip it for cleanliness of output)."""
    head = _git("rev-parse", "--abbrev-ref", "HEAD", check=False)
    current = head.stdout.strip() if head.returncode == 0 else ""

    def collect(flag: str) -> list[str]:
        r = _git("branch", flag, "--format=%(refname:short)", check=False)
        if r.returncode != 0:
            return []
        return [b for b in r.stdout.splitlines()
                if b and b != current and b.startswith(prefix)]

    if current == "HEAD":
        print("warn: detached HEAD; all branches treated as unmerged",
              file=sys.stderr)
        return [], collect("--no-merged") + collect("")
    return collect("--merged"), collect("--no-merged")


def _print_list(label: str, branches: list[str]) -> None:
    if not branches:
        return
    print(f"\n{label} ({len(branches)}):")
    for b in branches:
        print(f"  {b}")


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Delete ai-checker fix branches")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--project",
                   help="only branches for this project")
    g.add_argument("--all", action="store_true",
                   help="all ai-checker-* branches (default)")
    ap.add_argument("--force", action="store_true",
                    help="also delete unmerged branches (-D instead of -d)")
    ap.add_argument("--dry-run", action="store_true",
                    help="print what would be deleted, do nothing")
    args = ap.parse_args()

    prefix = f"ai-checker-{args.project}-" if args.project else "ai-checker-"

    merged, unmerged = _list(prefix)
    if not merged and not unmerged:
        print(f"no branches match '{prefix}*'")
        return 0

    print(f"pattern: '{prefix}*'")
    _print_list("merged (safe to delete)", merged)
    _print_list("unmerged (preserved unless FORCE=1)", unmerged)

    to_delete = merged + (unmerged if args.force else [])
    if not to_delete:
        print("\nnothing to delete (use FORCE=1 to also delete unmerged)")
        return 0

    if args.dry_run:
        print(f"\n[dry-run] would delete {len(to_delete)} branch(es)")
        return 0

    flag = "-D" if args.force else "-d"
    deleted = 0
    failed = 0
    print()
    for b in to_delete:
        r = _git("branch", flag, b, check=False)
        if r.returncode != 0:
            print(f"  FAILED:  {b}: {r.stderr.strip()}", file=sys.stderr)
            failed += 1
        else:
            print(f"  deleted: {b}")
            deleted += 1

    print(f"\n{deleted} deleted, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())

