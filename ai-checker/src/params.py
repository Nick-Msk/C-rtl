"""View and edit ai-checker tunable params."""
from __future__ import annotations

import argparse
import db_status
import json
import sys

import db


def cmd_list(as_json: bool, long: bool) -> int:
    db.seed_defaults()
    rows = db.all_params()
    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0
    if not rows:
        print("(no params)", file=sys.stderr)
        return 0

    if long:
        cur_mod = None
        for r in rows:
            if r["module"] != cur_mod:
                cur_mod = r["module"]
                print(f"\n[{cur_mod}] {r['descr']}")
            marker = "" if r["value"] == r["default_value"] else " *"
            print(f"  {r['name']:<20} {r['value']:>12}  "
                  f"(default {r['default_value']}){marker}")
        return 0

    print(f"{'module':<8} {'name':<20} {'value':>12} {'default':>12}  dt")
    for r in rows:
        marker = "" if r["value"] == r["default_value"] else " *"
        print(f"{r['module']:<8} {r['name']:<20} "
              f"{r['value']:>12} {r['default_value']:>12}  "
              f"{r['dt']}{marker}")
    return 0


def cmd_modules() -> int:
    db.seed_defaults()
    rows = db.all_modules()
    for r in rows:
        print(f"{r['name']:<8} {r['descr']}")
    return 0


def cmd_set(module: str, name: str, value: str | None) -> int:
    db.seed_defaults()
    if not db.set_param(module, name, value):
        print(f"ERROR: no param {module}.{name}", file=sys.stderr)
        print("       run 'python params.py --modules' to see "
              "known modules", file=sys.stderr)
        return 1
    verb = "reset to default" if value is None else f"set to {value}"
    print(f"{module}.{name}: {verb}")
    return 0


def print_actors() -> None:
    rows = db_status.all_actors()
    if not rows:
        print("\n(no actors seeded — run any ai-* command once)")
        return
    print("\n=== actors ===")
    print(f"{'actor':<10} {'on/total':<9} descr")
    for r in rows:
        flag = "" if r["enabled"] else "  ⚠ ALL OFF"
        print(f"{r['name']:<10} {str(r['enabled']) + '/' + str(r['total']):<9} "
              f"{r['descr'] or ''}{flag}")


def print_transitions(actor=None, status=None, enabled_only=False) -> None:
    rows = db_status.all_transitions(actor, status, enabled_only)
    if not rows:
        print("\n(no transitions match)")
        return
    print(f"\n=== transition_status ({len(rows)}) ===")
    print(f"{'':<2} {'status':<16} {'actor':<10} {'new_status':<16} descr")
    for r in rows:
        mark = "  " if r["allowed"] == "y" else "✗ "
        print(f"{mark:<2} {r['status']:<16} {r['actor']:<10} "
              f"{r['new_status']:<16} {r['descr'] or ''}")
    off = sum(1 for r in rows if r["allowed"] != "y")
    if off:
        print(f"\n{off} transition(s) DISABLED — eligibility is narrowed. "
              f"Restore: make ai-config-reset-transitions")
    # Eligibility is what fix.py actually filters on — show the effect.
    print("\n=== eligible source statuses per actor ===")
    for a in db_status.all_actors():
        el = sorted(db_status.eligible_statuses(a["name"]))
        print(f"  {a['name']:<10} {', '.join(el) or '(none — actor disabled)'}")

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--long", action="store_true",
                    help="group by module, show description")
    ap.add_argument("--modules", action="store_true",
                    help="list modules and their descriptions")
    ap.add_argument("--transitions", action="store_true",
                    help="show the status machine instead of params")
    ap.add_argument("--actor", help="filter transitions by actor")
    ap.add_argument("--status", help="filter transitions by source status")
    ap.add_argument("--enabled-only", action="store_true")
    ap.add_argument("--set-allowed", choices=["y", "n"],
                    help="flip one transition (needs --status --actor --new-status)")
    ap.add_argument("--new-status")
    ap.add_argument("--actor-off", help="disable ALL transitions of an actor")
    ap.add_argument("--actor-on", help="enable ALL transitions of an actor")
    ap.add_argument("--reset-transitions", action="store_true",
                    help="restore every allowed='y'")
    
    sub = ap.add_subparsers(dest="cmd")

    p_set = sub.add_parser("set")
    p_set.add_argument("module")
    p_set.add_argument("name")
    p_set.add_argument("value", nargs="?", default=None,
                       help="omit to reset to default_value")
    args = ap.parse_args()

    if args.reset_transitions:
        n = db_status.reset_allowed()
        print(f"{n} transition(s) restored to 'y'")
        return 0
    if args.actor_off or args.actor_on:
        name = args.actor_off or args.actor_on
        n = db_status.set_actor_allowed(name, bool(args.actor_on))
        if not n:
            print(f"unknown actor {name!r}", file=sys.stderr)
            return 1
        print(f"{name}: {n} transition(s) -> "
              f"{'y' if args.actor_on else 'n'}")
        return 0
    if args.set_allowed:
        if not (args.status and args.actor and args.new_status):
            ap.error("--set-allowed needs --status, --actor and --new-status")
        ok = db_status.set_allowed(args.status, args.actor, args.new_status,
                                   args.set_allowed == "y")
        if not ok:
            print(f"no such transition: {args.status} -[{args.actor}]-> "
                  f"{args.new_status}", file=sys.stderr)
            return 1
        print(f"{args.status} -[{args.actor}]-> {args.new_status} "
              f"= {args.set_allowed}")
        return 0
    if args.transitions:
        print_actors()
        print_transitions(args.actor, args.status, args.enabled_only)
        return 0

    if args.cmd == "set":
        return cmd_set(args.module, args.name, args.value)
    if args.modules:
        return cmd_modules()
    return cmd_list(args.json, args.long)

if __name__ == "__main__":
    raise SystemExit(main())

