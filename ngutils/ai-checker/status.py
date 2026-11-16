"""Inspect ai-checker/config/check.db. Read-only."""
from __future__ import annotations

import argparse
import json
import sys

import db
import gitutil


def cmd_summary(as_json: bool) -> int:
    by = db.summary_by_project_status()
    if as_json:
        print(json.dumps(by, indent=2, ensure_ascii=False))
        return 0
    cols = ["added", "proposed-to-fix", "analyze-by-dev",
            "fixed", "rejected", "failed", "stale"]
    missing = db.STATUSES - set(cols)
    extra = set(cols) - db.STATUSES
    if missing or extra:
        print(f"warn: column list out of sync with db.STATUSES "
              f"(missing={missing}, extra={extra})", file=sys.stderr)
    print(f"{'project':<10} " + " ".join(f"{c:>16}" for c in cols))
    for proj, counts in by.items():
        print(f"{proj:<10} " + " ".join(
            f"{counts.get(c, 0):>16}" for c in cols))
    return 0


def cmd_list(project: str | None, status: str | None,
             as_json: bool, keys_only: bool,
             all_runs: bool) -> int:
    rows = db.list_issues(project=project, status=status,
                          latest_only=not all_runs)

    if keys_only:
        for r in rows:
            print(r["key"])
        return 0

    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0

    names = [r["fix_branch"] for r in rows if r.get("fix_branch")]
    alive = gitutil.branches_exist(names) if names else set()

    for r in rows:
        status = r["status"]
        if status == "fixed" and r.get("fix_commit"):
            status = f"fixed({r['fix_commit']})"
        print(f"{r['project']:<6} {r['key']:<14} "
              f"{r['section']:<10} {status:<16} {r['title'][:60]}")
        if status != r["status"]:
            continue  # fixed, sha already in status — no extra lines
        if r.get("fix_branch"):
            if r["fix_branch"] in alive:
                print(f"      {r['fix_branch']}  {r['fix_commit']}")
            else:
                print(f"      {r['fix_commit']}  (branch deleted)")
            if r.get("fix_subject"):
                print(f"      {r['fix_subject']}")
    return 0

def cmd_runs(limit: int, as_json: bool) -> int:
    rows = db.llmruns_list(limit=limit)
    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0
    print(f"{'runid':<6} {'kind':<6} {'status':<8} "
          f"{'started':<20} {'dur':<8} {'in':>8} {'out':>8} {'req':>4}  name")
    for r in rows:
        started = (r["start_time"] or "")[:19]
        if r["end_time"]:
            try:
                from datetime import datetime
                t0 = datetime.strptime(r["start_time"], "%Y-%m-%dT%H:%M:%S")
                t1 = datetime.strptime(r["end_time"], "%Y-%m-%dT%H:%M:%S")
                dur = f"{int((t1 - t0).total_seconds())}s"
            except (ValueError, TypeError):
                dur = "?"
        else:
            dur = "..."
        print(f"{r['runid']:<6} {r['kind']:<6} {r['status']:<8} "
              f"{started:<20} {dur:<8} "
              f"{r['in_tokens']:>8} {r['out_tokens']:>8} "
              f"{r['requests']:>4}  {r['name'] or ''}")
    return 0

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--project")
    ap.add_argument("--status", choices=sorted(db.STATUSES))
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--list", action="store_true",
                    help="list individual issues (default: summary)")
    ap.add_argument("--key", action="store_true",
                    help="print one finding key per line "
                         "(directly usable as FIXLIST)")
    ap.add_argument("--all-runs", action="store_true",
                    help="include issues from previous (stale) reports")
    ap.add_argument("--runs", action="store_true",
                    help="show recent LLM runs (from llmruns)")
    ap.add_argument("--limit", type=int, default=20)

    args = ap.parse_args()

    if not db.has_any_report():
        rel = db.DB_PATH.relative_to(db.ROOT)
        print(f"No data in {rel}.", file=sys.stderr)
        print("Run 'make ai-check' (or 'make ai-check-<project>') first.",
              file=sys.stderr)
        return 1

    if args.runs:
        return cmd_runs(args.limit, args.json)
    if args.list or args.project or args.status or args.key:
        return cmd_list(args.project, args.status, args.json, args.key, args.all_runs)
    return cmd_summary(args.json)

if __name__ == "__main__":
    raise SystemExit(main())


