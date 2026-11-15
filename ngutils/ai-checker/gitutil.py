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
    """Return (merged, unmerged) branches matching '<prefix>*'.

    Uses for-each-ref (all branches) filtered in Python by prefix,
    then git merge-base --is-ancestor to classify merged vs unmerged.
    Both are portable across git versions; avoids the fragile
    --list/--merged/--format combination that returns empty output
    on some macOS git builds.
    """
    current_r = _git("rev-parse", "--abbrev-ref", "HEAD", check=False)
    current = current_r.stdout.strip() if current_r.returncode == 0 else ""
    if current == "HEAD":
        print("warn: detached HEAD; all branches treated as unmerged",
              file=sys.stderr)
        current = ""

    r = _git("for-each-ref", "--format=%(refname:short)",
             "refs/heads/", check=False)
    if r.returncode != 0:
        print(f"warn: for-each-ref failed: {r.stderr.strip()}",
              file=sys.stderr)
        return [], []

    branches = [b.strip() for b in r.stdout.splitlines() if b.strip()]
    branches = [b for b in branches
                if b != current and b.startswith(prefix)]

    merged: list[str] = []
    unmerged: list[str] = []
    for b in branches:
        mb = _git("merge-base", "--is-ancestor", b, "HEAD", check=False)
        if mb.returncode == 0:
            merged.append(b)
        else:
            unmerged.append(b)
    return merged, unmerged


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

