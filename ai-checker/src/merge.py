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
from common import acquire_lock, parse_fixlist, release_lock


def _key(r: dict) -> str:
    return f"{r['runid']}/{r['chunk_id']}/{r['local_id']}"

def run(args) -> int:
    rows = db.fixed_issues(args.project, latest_only=args.latest_only)
    if args.key:
        matcher = parse_fixlist(args.key)
        if matcher is None:
            sys.exit(f"ERROR: cannot parse --key {args.key!r}")
        rows = [r for r in rows
                if matcher((r["runid"], r["chunk_id"], r["local_id"]))]
        if not rows:
            print(f"[ai-merge] nothing matches --key {args.key!r}")
            return 0
    if not rows:
        print("nothing awaiting merge")
        return 0

    base = gitutil.git("rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    if base == "HEAD":
        sys.exit("ERROR: detached HEAD, checkout a branch first")
    print(f"[ai-merge] base={base}  {len(rows)} fixed issue(s)")

    targets = [t.strip() for t in
        db.get_param("fix", "verify_targets",
                     "test-{project},analyze-{project}").split(",")
        if t.strip()]
    verify_on = db.get_param("fix", "verify_on_merge", True)

    merged = reconciled = lost = failed = 0
    for r in rows:
        key, fix_sha, br = _key(r), r["fix_commit"], r["fix_branch"]

        merged = reconciled = lost = failed = skipped = 0
        for r in rows:
            key, fix_sha, br = _key(r), r["fix_commit"], r["fix_branch"]
            latest = db.latest_runid_by_project().get(r["project"])
            if latest and r["runid"] != latest:
                print(f"  STALE    {key}  baseline run {r['runid']}, "
                    f"latest {latest} — conflict likely", file=sys.stderr)
                if not args.key:
                    skipped += 1
                    continue

        if not fix_sha or not gitutil.commit_exists(fix_sha):
            print(f"  GONE     {key}  {fix_sha or '(no sha)'} "
                  f"(commit unreachable — never merged, branch deleted)")
            lost += 1
            continue

        if gitutil.is_ancestor(fix_sha):
            head = gitutil.git("rev-parse", "--short", "HEAD").stdout.strip()
            if not args.dry_run:
                db.mark_merged(r["id"], head)
                if not args.keep and not gitutil.discard_fix(
                            r["project"], r["runid"],
                            r["chunk_id"], r["local_id"], br):
                    print(f"  warn: cannot delete branch {br}",
                            file=sys.stderr)
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
            wt = gitutil.ensure_fix_worktree(
                r["project"], r["runid"], r["chunk_id"], r["local_id"],
                br, base_sha=fix_sha)
            verify = (gitutil.make_verifier(r["project"], targets)
                    if verify_on else None)
            sha, _ = gitutil.merge_fix(br, wt=wt, delete=not args.keep, verify=verify)

            
        except RuntimeError as exc:
            print(f"  CONFLICT {key}  {exc}", file=sys.stderr)
            failed += 1
            continue
        db.mark_merged(r["id"], sha)
        print(f"  MERGED   {key}  {sha}")
        merged += 1

    tail = f", {skipped} stale skipped" if skipped else ""
    print(f"\n[ai-merge] {merged} merged, {reconciled} recorded, "
          f"{lost} lost, {failed} conflict(s){tail}")
    return 1 if failed else 0

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true")
    g.add_argument("--project")
    ap.add_argument("--key", default="",
                    help="FIXLIST: '93/1/2', '93/1/2-5', '1/*', '*'")
    ap.add_argument("--sync-only", action="store_true",
                    help="reconcile only, never run git merge")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--keep", action="store_true",
                    help="keep the fix branch after merge (default: delete)")
    ap.add_argument("--latest-only", action="store_true",
                help="only fixes from the newest report per project")
    args = ap.parse_args()

    if args.project:
        known = db.known_projects()
        if args.project not in known:
            sys.exit(f"ERROR: unknown project {args.project!r}; "
                     f"known: {', '.join(known)}")

    lock = acquire_lock()
    try:
        return run(args)
    finally:
        release_lock(lock)


if __name__ == "__main__":
    raise SystemExit(main())
