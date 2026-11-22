"""View and edit ai-checker tunable params."""
from __future__ import annotations

import argparse
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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--long", action="store_true",
                    help="group by module, show description")
    ap.add_argument("--modules", action="store_true",
                    help="list modules and their descriptions")
    sub = ap.add_subparsers(dest="cmd")
    p_set = sub.add_parser("set")
    p_set.add_argument("module")
    p_set.add_argument("name")
    p_set.add_argument("value", nargs="?", default=None,
                       help="omit to reset to default_value")
    args = ap.parse_args()

    if args.cmd == "set":
        return cmd_set(args.module, args.name, args.value)
    if args.modules:
        return cmd_modules()
    return cmd_list(args.json, args.long)

if __name__ == "__main__":
    raise SystemExit(main())

