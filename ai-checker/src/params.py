"""View and edit ai-checker tunable params."""
from __future__ import annotations

import argparse
import json
import sys
import db
import db_status
import re
from collections import Counter
from datetime import datetime
from pathlib import Path

from common import (
    AI_ROOT, 
    rel_display
)

DEFAULT_SAVE = AI_ROOT / "config" / "ai-config-save.sql"


def _q(v) -> str:
    return "'" + str(v).replace("'", "''") + "'"


def _flat(s) -> str:
    """descr is context-only; keep one row per line for readable diffs."""
    return re.sub(r"\s+", " ", str(s or "")).strip()


def render_config_sql(snap: dict) -> str:
    now = datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
    out = [
        "-- ai-checker configuration dump.",
        f"-- saved {now}   schema v{db.SCHEMA_VERSION}   from {db.DB_PATH}",
        "--",
        "-- RESTORED : params.value, statuses.terminal/resolved,",
        "--            transition_status.allowed",
        "-- CONTEXT  : descr, default_value — code-owned, refreshed by seed,",
        "--            editing them here has NO effect.",
        "--",
        "-- Apply: make ai-config-restore FILENAME=<this file>",
        "--    or: sqlite3 config/check.db < <this file>",
        "",
    ]

    out.append(f"-- ── statuses ({len(snap['statuses'])}) " + "─" * 30)
    for r in snap["statuses"]:
        out.append(
            f"INSERT INTO statuses (name,descr,terminal,resolved) VALUES "
            f"({_q(r['name'])},{_q(_flat(r['descr']))},"
            f"{int(r['terminal'] or 0)},{int(r['resolved'] or 0)})")
        out.append("  ON CONFLICT(name) DO UPDATE SET "
                   "terminal=excluded.terminal, resolved=excluded.resolved;")

    changed = sum(1 for r in snap["params"]
                  if r["value"] != r["default_value"])
    out += ["", f"-- ── params ({len(snap['params'])}, {changed} changed) "
                + "─" * 20]
    for r in snap["params"]:
        note = ("" if r["value"] == r["default_value"]
                else f"   -- changed (default {r['default_value']})")
        out.append(
            f"INSERT INTO params (module,name,value,default_value,dt) VALUES "
            f"({_q(r['module'])},{_q(r['name'])},{_q(r['value'])},"
            f"{_q(r['default_value'])},{_q(r['dt'])})")
        out.append("  ON CONFLICT(module,name) DO UPDATE SET "
                   f"value=excluded.value, dt=excluded.dt;{note}")

    tr = snap["transition_status"]
    off = sum(1 for r in tr if r["allowed"] != "y")
    out += ["", f"-- ── transition_status ({len(tr)}, {off} disabled) " + "─" * 12]
    for r in tr:
        note = "" if r["allowed"] == "y" else "   -- DISABLED"
        out.append(
            f"INSERT INTO transition_status "
            f"(status,actor,new_status,allowed,descr) VALUES "
            f"({_q(r['status'])},{_q(r['actor'])},{_q(r['new_status'])},"
            f"{_q(r['allowed'])},{_q(_flat(r['descr']))})")
        out.append("  ON CONFLICT(status,actor,new_status) DO UPDATE SET "
                   f"allowed=excluded.allowed;{note}")
    return "\n".join(out) + "\n"

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

def _resolve_file(path: str | None) -> Path:
    if not path:
        return DEFAULT_SAVE
    p = Path(path).expanduser()
    return (p if p.is_absolute() else Path.cwd() / p).resolve()


def cmd_save(path: str | None) -> int:
    db.seed_defaults()
    p = _resolve_file(path)
    snap = db.config_snapshot()
    try:
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(render_config_sql(snap), encoding="utf-8")
    except OSError as exc:
        print(f"ERROR: cannot write {p}: {exc}", file=sys.stderr)
        return 1
    changed = sum(1 for r in snap["params"]
                  if r["value"] != r["default_value"])
    off = sum(1 for r in snap["transition_status"] if r["allowed"] != "y")
    print(f"[ai-config] saved -> {rel_display(p)}")
    print(f"  {len(snap['params'])} param(s) ({changed} changed), "
          f"{len(snap['statuses'])} status flag(s), "
          f"{len(snap['transition_status'])} transition(s) ({off} disabled)")
    return 0


def cmd_restore(path: str | None) -> int:
    p = _resolve_file(path)
    if not p.is_file():
        print(f"ERROR: no such file: {p}", file=sys.stderr)
        return 1
    text = p.read_text(encoding="utf-8")
    counts = Counter(t.lower() for t in
                     re.findall(r"INSERT\s+INTO\s+(\w+)", text, re.I))
    if not counts:
        print(f"ERROR: no INSERT statements in {p}", file=sys.stderr)
        return 1
    extra = set(counts) - set(db.CONFIG_TABLES)
    if extra:
        print(f"  warn: file also touches: {', '.join(sorted(extra))}",
              file=sys.stderr)
    try:
        db.restore_config_sql(text)
    except Exception as exc:  # noqa: BLE001
        print(f"ERROR: restore failed, rolled back: {exc}", file=sys.stderr)
        return 1
    print(f"[ai-config] restored <- {rel_display(p)}")
    for t in db.CONFIG_TABLES:
        print(f"  {t:<18} {counts.get(t, 0)} row(s)")
    # AFTER apply, not before: seed_defaults() runs validate_params(),
    # which exits on a critical error — restoring a GOOD config over a
    # currently-bad one must not be blocked by the bad one.
    db.seed_defaults()
    off = [r for r in db_status.all_transitions() if r["allowed"] != "y"]
    if off:
        print(f"  note: {len(off)} transition(s) DISABLED "
              f"— see make ai-config-transitions")
    return 0

def cmd_reset_transitions() -> int:
    n = db_status.reset_allowed()
    print(f"{n} transition(s) restored to 'y'")
    return 0


def cmd_actor_switch(name: str, on: bool) -> int:
    n = db_status.set_actor_allowed(name, on)
    if not n:
        print(f"unknown actor {name!r}", file=sys.stderr)
        return 1
    print(f"{name}: {n} transition(s) -> {'y' if on else 'n'}")
    return 0


def cmd_set_allowed(args) -> int:
    if not (args.status and args.actor and args.new_status):
        print("ERROR: --set-allowed needs --status, --actor and --new-status",
              file=sys.stderr)
        print("usage: params.py --set-allowed y|n --status S "
              "--actor A --new-status N", file=sys.stderr)
        return 1
    ok = db_status.set_allowed(args.status, args.actor, args.new_status,
                               args.set_allowed == "y")
    if not ok:
        print(f"no such transition: {args.status} -[{args.actor}]-> "
              f"{args.new_status}", file=sys.stderr)
        return 1
    print(f"{args.status} -[{args.actor}]-> {args.new_status} "
          f"= {args.set_allowed}")
    return 0


def cmd_transitions(args) -> int:
    print_actors()
    print_transitions(args.actor, args.status, args.enabled_only)
    return 0


def run(args) -> int:
    if args.save is not None:
        return cmd_save(args.save)
    if args.restore is not None:
        return cmd_restore(args.restore)
    if args.reset_transitions:
        return cmd_reset_transitions()
    if args.actor_off or args.actor_on:
        return cmd_actor_switch(args.actor_off or args.actor_on,
                                bool(args.actor_on))
    if args.set_allowed:
        return cmd_set_allowed(args)
    if args.transitions:
        return cmd_transitions(args)
    if args.cmd == "set":
        return cmd_set(args.module, args.name, args.value)
    if args.modules:
        return cmd_modules()
    return cmd_list(args.json, args.long)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--long", action="store_true",
                    help="group by module, show description")
    ap.add_argument("--actor", help="filter transitions by actor")
    ap.add_argument("--status", help="filter transitions by source status")
    ap.add_argument("--new-status")
    ap.add_argument("--enabled-only", action="store_true")

    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--modules", action="store_true",
                      help="list modules and their descriptions")
    mode.add_argument("--transitions", action="store_true",
                      help="show the status machine instead of params")
    mode.add_argument("--set-allowed", choices=["y", "n"],
                      help="flip one transition (needs --status/--actor/--new-status)")
    mode.add_argument("--actor-off", help="disable ALL transitions of an actor")
    mode.add_argument("--actor-on", help="enable ALL transitions of an actor")
    mode.add_argument("--reset-transitions", action="store_true",
                      help="restore every allowed='y'")
    mode.add_argument("--save", nargs="?", const=str(DEFAULT_SAVE),
                      metavar="FILE", help="dump config as SQL inserts")
    mode.add_argument("--restore", nargs="?", const=str(DEFAULT_SAVE),
                      metavar="FILE", help="apply a config dump")

    sub = ap.add_subparsers(dest="cmd")
    p_set = sub.add_parser("set")
    p_set.add_argument("module")
    p_set.add_argument("name")
    p_set.add_argument("value", nargs="?", default=None,
                       help="omit to reset to default_value")
    return run(ap.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
