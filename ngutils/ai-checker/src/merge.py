"""Close the loop: merge ai-checker fix branches, record 'merged'.

Two phases, always in this order:
  reconcile — branch already an ancestor of HEAD -> record merged
  merge     — branch not yet in HEAD -> git merge --no-ff, then record
Reconciliation runs first so a hand-merged branch is never re-merged.
"""
from __future__ import annotations

import argparse
import sys

import db
import gitutil
from common import acquire_lock, release_lock


def _key(r: dict) -> str:
    return f"{r['runid']}/{r['chunk_id']}/{r['local_id']}"

def run(args) -> int:
    rows = db.fixed_issues(args.project)
    if args.key:
        rows = [r for r in rows if _key(r) in set(args.key.split())]
    if not rows:
        print("nothing awaiting merge")
        return 0

    base, _ = gitutil.git_clean_check()
    print(f"[ai-merge] base={base}  {len(rows)} fixed issue(s)")

    merged = reconciled = lost = failed = 0
    for r in rows:
        key, fix_sha, br = _key(r), r["fix_commit"], r["fix_branch"]

        if not fix_sha or not gitutil.commit_exists(fix_sha):
            print(f"  GONE     {key}  {fix_sha or '(no sha)'} "
                  f"(commit unreachable — never merged, branch deleted)")
            lost += 1
            continue

        if gitutil.is_ancestor(fix_sha):
            head = gitutil.git("rev-parse", "--short", "HEAD").stdout.strip()
            if not args.dry_run:
                db.mark_merged(r["id"], head)
                if not args.keep:
                    gitutil.delete_branch(br)      # -d, refuses if unmerged
            print(f"  RECORDED {key}  {fix_sha} already in {base}")
            reconciled += 1
            continue

        # Коммит жив, но не в HEAD — мержить нечего веткой, мержим sha.
        if args.sync_only:
            print(f"  PENDING  {key}  {fix_sha} (not in {base})")
            continue
        if args.dry_run:
            print(f"  WOULD    {key}  git merge --no-ff {fix_sha}")
            continue
        if not gitutil.branch_exists(br):
            gitutil.git("branch", br, fix_sha, check=False)   # ярлык из sha
        try:
            sha, _ = gitutil.merge_fix(br, delete=not args.keep)
        except RuntimeError as exc:
            print(f"  CONFLICT {key}  {exc}", file=sys.stderr)
            failed += 1
            break
        db.mark_merged(r["id"], sha)
        print(f"  MERGED   {key}  {sha}")
        merged += 1

    print(f"\n[ai-merge] {merged} merged, {reconciled} recorded, "
          f"{lost} lost, {failed} conflict(s)")
    return 1 if failed else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true")
    g.add_argument("--project")
    ap.add_argument("--key", default="", help="only these runid/chunk/local")
    ap.add_argument("--sync-only", action="store_true",
                    help="reconcile only, never run git merge")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--keep", action="store_true",
                    help="keep the fix branch after merge (default: delete)")
    args = ap.parse_args()

    lock = acquire_lock()
    try:
        return run(args)
    finally:
        release_lock(lock)


if __name__ == "__main__":
    raise SystemExit(main())
