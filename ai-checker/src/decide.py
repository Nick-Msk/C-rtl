"""Record a human decision on issues, selected by FIXLIST.

Uses the SAME key filter as ai-fix (common.parse_fixlist), so
'93/1/2' means exactly what ai-status prints.
"""
from __future__ import annotations

import argparse
import sys

import db
import db_status
import gitutil
from common import parse_fixlist


def _discard_git(r: dict) -> None:
    """A rejected fix must not leave a branch + worktree behind: git
    refuses `branch -D` while a worktree holds the branch."""
    wt = gitutil.fix_worktree_path(r["project"], r["check_runid"],
                                   r["chunk_id"], r["local_id"])
    gitutil.remove_fix_worktree(wt)      # harvest logs, then unmount
    if gitutil.delete_branch(r["fix_branch"], force=True):
        print(f"         discarded {r['fix_branch']} + worktree")
    else:
        print(f"  warn: cannot delete branch {r['fix_branch']}",
              file=sys.stderr)

def run(args) -> int:
    # Full keys are globally unique: an operation must see ALL runs.
    # latest_only is a VIEW default, not an operation scope — otherwise
    # ancient 'fixed' issues are invisible here while ai-merge queues them.

    rows = db.list_issues(project=args.project, latest_only=False)

    matcher = parse_fixlist(args.only)
    if matcher is not None:
        rows = [r for r in rows
                if matcher((r["check_runid"], r["chunk_id"], r["local_id"]))]
    if not rows:
        print("no issues match"
              + (f" FIXLIST={args.only!r}" if args.only else "")
              + (f" in project '{args.project}'" if args.project else ""),
              file=sys.stderr)
        return 1

    # Bulk guard: '*' across every project is almost never intended.
    if len(rows) > 1 and not args.project and not args.all_projects:
        print(f"refusing to change {len(rows)} issues across ALL projects; "
              f"pass --project NAME or --all-projects", file=sys.stderr)
        return 1

    rc = 0
    for r in rows:
        # A hand-merged fix must be RECORDED, not rejected: marking it
        # rejected would lie about history, and -D would drop the label.
        if (args.status == db.STATUS_REJECTED 
                and r["fix_commit"]
                and gitutil.commit_exists(r["fix_commit"])
                and gitutil.is_ancestor(r["fix_commit"])):
            print(f"{r['project']:<8} {r['key']:<14} already in HEAD — "
                  f"run ai-merge to record it", file=sys.stderr)
            rc = 1
            continue
        with db.connect() as con:
            ok, msg = db_status.decide(con, r["check_runid"], r["chunk_id"],
                                       r["local_id"], args.status, args.reason)
        if not ok:
            print(f"{r['project']:<8} {r['key']:<14} {msg}", file=sys.stderr)
            rc = 1
            continue
        print(f"{r['project']:<8} {r['key']:<14} {msg}")
        if args.status == db.STATUS_REJECTED and r["fix_branch"]:
            _discard_git(r)
    return rc


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--status", required=True,
                    choices=["rejected", "no-action", "added"])
    ap.add_argument("--only", default="", help="FIXLIST, same syntax as ai-fix")
    ap.add_argument("--project", help="restrict to one project")
    ap.add_argument("--reason", default="")
    ap.add_argument("--all-projects", action="store_true",
                    help="allow a bulk change across every project")
    ap.add_argument("keys", nargs="*",
                    help="positional full keys (shorthand for --only)")
    args = ap.parse_args()
    if args.keys and not args.only:
        args.only = ",".join(args.keys)     # backward compat: decide.py 93/1/2
    if not args.only:
        ap.error("need FIXLIST (--only) or positional keys")
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
