"""Print finding keys eligible for fix/propose, for shell completion.

Usage: fixlist_candidates.py [project]

Output: one key per line, '<runid>/<chunk>/<local>', sorted.
Statuses: added, proposed-to-fix, analyze-by-dev, no-action.
Never fails: completion must not break the shell, so errors go to
stderr and the exit code is always 0.
"""
from __future__ import annotations

import argparse
import sys

import db

ELIGIBLE = {db.STATUS_ADDED, db.STATUS_PROPOSED,
            db.STATUS_ANALYZE, db.STATUS_NO_ACTION}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("project", nargs="?", help="restrict to one project")
    args = ap.parse_args()
    try:
        rows = db.list_issues(project=args.project, latest_only=True)
    except Exception as exc:  # noqa: BLE001
        print(f"warn: {exc}", file=sys.stderr)
        return 0
    keys = sorted(
        f"{r['check_runid']}/{r['chunk_id']}/{r['local_id']}"
        for r in rows if r["status"] in ELIGIBLE)
    if keys:
        print("\n".join(keys))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

