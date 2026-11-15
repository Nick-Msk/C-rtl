"""View and edit ai-checker tunable params."""
from __future__ import annotations

import argparse
import json
import sys

import db


def cmd_list(as_json: bool) -> int:
    db.seed_defaults()
    rows = db.all_params()
    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0
    if not rows:
        print("(no params)", file=sys.stderr)
        return 0
    print(f"{'category':<8} {'name':<20} {'value':>12} {'default':>12}  dt")
    for r in rows:
        marker = "" if r["value"] == r["default_value"] else " *"
        print(f"{r['category']:<8} {r['name']:<20} "
              f"{r['value']:>12} {r['default_value']:>12}  "
              f"{r['dt']}{marker}")
    return 0


def cmd_set(category: str, name: str, value: str | None) -> int:
    db.seed_defaults()
    if not db.set_param(category, name, value):
        print(f"ERROR: no param {category}.{name}", file=sys.stderr)
        return 1
    verb = "reset to default" if value is None else f"set to {value}"
    print(f"{category}.{name}: {verb}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    sub = ap.add_subparsers(dest="cmd")
    p_set = sub.add_parser("set")
    p_set.add_argument("category")
    p_set.add_argument("name")
    p_set.add_argument("value", nargs="?", default=None,
                       help="omit to reset to default_value")
    args = ap.parse_args()

    if args.cmd == "set":
        return cmd_set(args.category, args.name, args.value)
    return cmd_list(args.json)


if __name__ == "__main__":
    raise SystemExit(main())

