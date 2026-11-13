"""Inspect ai-checker/config/check.db. Read-only."""
from __future__ import annotations

import argparse
import json
import sys

import db


def cmd_summary(as_json: bool) -> int:
    with db.connect() as con:
        rows = con.execute(
            "SELECT r.project, i.status, COUNT(*) AS n "
            "FROM issue i JOIN report r ON r.id = i.report_id "
            "GROUP BY r.project, i.status "
            "ORDER BY r.project, i.status").fetchall()
    by: dict[str, dict[str, int]] = {}
    for r in rows:
        by.setdefault(r["project"], {})[r["status"]] = r["n"]
    if as_json:
        print(json.dumps(by, indent=2, ensure_ascii=False))
        return 0
    cols = ["proposed-to-fix", "analyze-by-dev", "fixed",
            "rejected", "failed"]
    print(f"{'project':<10} " + " ".join(f"{c:>16}" for c in cols))
    for proj, counts in by.items():
        print(f"{proj:<10} " + " ".join(
            f"{counts.get(c, 0):>16}" for c in cols))
    return 0


def cmd_list(project: str | None, status: str | None, as_json: bool) -> int:
    q = ("SELECT r.project, i.finding_id, i.status, i.title, "
         "i.fix_branch, i.fix_commit, i.dt "
         "FROM issue i JOIN report r ON r.id = i.report_id WHERE 1=1 ")
    args: list = []
    if project:
        q += "AND r.project = ? "
        args.append(project)
    if status:
        q += "AND i.status = ? "
        args.append(status)
    q += "ORDER BY r.id DESC, i.chunk_id, i.local_id"
    with db.connect() as con:
        rows = con.execute(q, args).fetchall()
    if as_json:
        print(json.dumps([dict(r) for r in rows], indent=2,
                         ensure_ascii=False))
        return 0
    for r in rows:
        tail = ""
        if r["fix_branch"]:
            tail = f"  {r['fix_branch']}  {r['fix_commit']}"
        print(f"{r['project']:<6} {r['finding_id']:<6} "
              f"{r['status']:<16} {r['title'][:60]}{tail}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--project")
    ap.add_argument("--status", choices=sorted(db.STATUSES))
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--list", action="store_true",
                    help="list individual issues (default: summary)")
    args = ap.parse_args()
    if args.list or args.project or args.status:
        return cmd_list(args.project, args.status, args.json)
    return cmd_summary(args.json)


if __name__ == "__main__":
    raise SystemExit(main())


