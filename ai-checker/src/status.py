"""Inspect ai-checker/config/check.db. Read-only."""
from __future__ import annotations

import argparse
import json
import sys

import db
import gitutil
import tracker_common
from common import OUT_ROOT, rel_display


def cmd_summary(as_json: bool) -> int:
    by = db.summary_by_project_status()
    runids = db.latest_runid_by_project()
    if as_json:
        out = {}
        for proj, counts in by.items():
            counts["_runid"] = runids.get(proj)
            counts["_status"] = db.project_aggregate_status(counts)
            out[proj] = counts
        print(json.dumps(out, indent=2, ensure_ascii=False))
        return 0
    
    cols = [s["name"] for s in db.all_statuses()]

    
    # Wider first column: 'project(NN)' + aggregate status
    print(f"{'project':<14} {'status':<14}  "
          + "  ".join(f"{c:>14}" for c in cols))
    for proj, counts in by.items():
        rid = runids.get(proj)
        label = f"{proj}({rid})" if rid else proj
        agg = db.project_aggregate_status(counts)
        print(f"{label:<14} {agg:<14}  "
              + "  ".join(f"{counts.get(c, 0):>14}" for c in cols))
    return 0

def cmd_statuses(as_json: bool) -> int:
    rows = db.all_statuses()
    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0
    print(f"{'name':<16} {'terminal':<9} descr")
    for r in rows:
        print(f"{r['name']:<16} {'yes' if r['terminal'] else '-':<9} {r['descr']}")
    return 0

def _print_rows(rows: list[dict]) -> None:
    """Shared renderer for cmd_list and cmd_unmerged."""
    names = [r["fix_branch"] for r in rows
             if r.get("fix_branch") and r["status"] == db.STATUS_FIXED]
    alive = gitutil.branches_exist(names) if names else set()
    for r in rows:
        st = f"{r['status']}{tracker_common.marker(r)}"
        sha = r.get("fix_commit")
        if st in ("fixed", "merged") and sha:
            st = f"{st}({sha})"
        print(f"{r['project']:<6} {r['key']:<14} "
              f"{r['section']:<10} {st:<22} {r['title'][:60]}")
        if r["status"] == db.STATUS_FIXED and r.get("fix_branch"):
            if r["fix_branch"] in alive:
                print(f"      {r['fix_branch']}  {r['fix_commit']}")
            else:
                print(f"      {r['fix_commit']}  (branch deleted)")
            if r.get("fix_subject"):
                print(f"      {r['fix_subject']}")
        if r.get("carried_key"):
            print(f"      ← carried from {r['carried_key']} "
                  f"(score {r['carry_score']:.2f})")
    print("\n^N  pushed to tracker, N = remote issue id    "
          "~  dry-run payload only")

def cmd_list(project: str | None, status: str | None,
             as_json: bool, keys_only: bool,
             all_runs: bool) -> int:
    rows = db.list_issues(project=project, status=status,
                          latest_only=not all_runs)
    if not rows and not (as_json or keys_only):
        print("no issues in db"
              + (f" for project '{project}'" if project else "")
              + (f" with status '{status}'" if status else ""),
              file=sys.stderr)
        if project and (OUT_ROOT / project).is_dir():
            print(f"  reports exist on disk: {OUT_ROOT / project}\n"
                  f"  they are not ingested yet — run "
                  f"'make ai-fix-{project}' (adopts them, no LLM cost)\n"
                  f"  or 'make ai-check-{project}' for a fresh review",
                  file=sys.stderr)
        return 0
    if keys_only:
        for r in rows:
            print(r["key"])
        return 0
    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0
    
    _print_rows(rows)

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

def _key(r: dict) -> str:
    return f"{r['check_runid']}/{r['chunk_id']}/{r['local_id']}"


def _unmerged_rows(project: str | None) -> list[dict]:
    """All non-terminal findings whose commit (if any) is not in HEAD.
    Includes: added, analyze-by-dev, proposed-to-fix, failed, fixed.
    Excludes: merged, rejected, no-action, suppressed, stale."""

    import gitutil
    out = []
    rows = db.list_issues(project=project, latest_only=True)
    for r in rows:
        if r["status"] in (db.STATUS_MERGED, db.STATUS_REJECTED,
                           db.STATUS_NO_ACTION, db.STATUS_SUPPRESSED,
                           db.STATUS_STALE):
            continue
        sha = r.get("fix_commit")
        if sha and gitutil.commit_exists(sha) and gitutil.is_ancestor(sha):
            continue  # already in HEAD
        r = dict(r)
        r["key"] = _key(r)
        out.append(r)
    return out

def cmd_unmerged(project: str | None, as_json: bool) -> int:
    rows = _unmerged_rows(project)
    if not rows:
        print("nothing unmerged")
        return 0
    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0
    _print_rows(rows)
    latest = db.latest_runid_by_project()
    stale = [r["key"] for r in rows
             if latest.get(r["project"]) != r["check_runid"]]
    if stale:
        print(f"\nstale (conflict likely): {', '.join(stale)}")
    # ready = [r for r in rows
    #          if r["status"] in (db.STATUS_FIXED, db.STATUS_PROPOSED)]
    # pending = [r for r in rows
    #            if r["status"] not in (db.STATUS_FIXED, db.STATUS_PROPOSED)]
    # if ready:
    #     by_proj: dict[str, list[str]] = {}
    #     for r in ready:
    #         by_proj.setdefault(r["project"], []).append(r["key"])
    #     for proj, ks in by_proj.items():
    #         print(f"merge:   make ai-merge-{proj} "
    #               f"FIXLIST=\"{','.join(ks)}\"")
    # if pending:
    #     print(f"\npending (need propose/commit): {len(pending)} issue(s)")
    #     for r in pending[:5]:
    #         print(f"  {r['key']}  {r['status']}")
    #     if len(pending) > 5:
    #         print(f"  ... and {len(pending) - 5} more")
    return 0

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--project")
    ap.add_argument("--status", choices=sorted(db.STATUSES))
    ap.add_argument("--statuses", action="store_true",
                    help="show status dictionary (name, descr, terminal)")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--list", action="store_true",
                    help="list individual issues (default: summary)")
    ap.add_argument("--key", action="store_true",
                    help="print one finding key per line "
                         "(directly usable as FIXLIST)")
    ap.add_argument("--unmerged", action="store_true",
                    help="only fixed findings whose fix commit is not in HEAD")
    ap.add_argument("--all-runs", action="store_true",
                    help="include issues from previous (stale) reports")
    ap.add_argument("--runs", action="store_true",
                    help="show recent LLM runs (from llmruns)")
    ap.add_argument("--limit", type=int, default=20)

    args = ap.parse_args()

    if args.statuses:
        return cmd_statuses(args.json)
    if not db.has_any_report():
        rel = rel_display(db.DB_PATH)
        print(f"No data in {rel}.", file=sys.stderr)
        print("Run 'make ai-check' (or 'make ai-check-<project>') first.",
              file=sys.stderr)
        return 1

    if args.runs:
        return cmd_runs(args.limit, args.json)
    if args.unmerged:
        return cmd_unmerged(args.project, args.json)
    if args.list or args.project or args.status or args.key:
        return cmd_list(args.project, args.status, args.json, args.key, args.all_runs)
    return cmd_summary(args.json)

if __name__ == "__main__":
    raise SystemExit(main())


