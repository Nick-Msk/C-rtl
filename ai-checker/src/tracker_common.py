"""Bug-tracker integration: contract, dispatch, orchestration.

track.mode     off | dry | live   -- global gate
track.backend  jira | gitea       -- implementation selector
track.<name>_* -- owned by that backend, never read here

Effective dry = (mode == 'dry') or the backend's own <name>_dry_run.
Param defaults live in db.DEFAULTS: db must not import trackers (cycle).
"""
from __future__ import annotations

import argparse
import importlib
import json
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import db
from common import OUT_ROOT, discover_projects, rel_display

MODES = ("off", "dry", "live")


@dataclass
class Ticket:
    """One finding, normalized — backend-agnostic."""
    issue_id: int
    key: str            # '166/1/1001'
    project: str
    section: str
    title: str
    text: str
    status: str
    remote_status: str = ""


@dataclass
class PushResult:
    remote_id: str = ""       # 'PROJ-42' / '17'
    remote_url: str = ""
    remote_status: str = ""
    dry: bool = False
    error: str = ""


class Tracker:
    """Contract every backend implements."""
    name = ""
    closed_status = "closed"     # token written back to issue_remote

    def configure(self) -> str | None:
        """Read OWN params; return an error string, or None if usable."""
        raise NotImplementedError

    def force_dry(self) -> bool:
        """Backend-local dry pin (<name>_dry_run)."""
        raise NotImplementedError

    def build(self, t: Ticket) -> dict:
        """Backend-native create payload — also the dry-run artifact."""
        raise NotImplementedError

    def create(self, t: Ticket, payload: dict) -> PushResult:
        """POST payload; fill remote_id/remote_url, or error."""
        raise NotImplementedError

    def ensure_project(self, project: str) -> str | None:
        """Make sure the remote container for this ai-checker project
        exists (Gitea: a project board). Default: no-op. Return error
        string, or None."""
        return None

    def close(self, link: dict, comment: str) -> str | None:
        """Close the remote ticket and append `comment`.
        Return an error string, or None on success."""
        raise NotImplementedError

def _request(method: str, url: str, payload: dict | None,
             headers: dict | None = None, timeout: int = 15) -> tuple[object, str]:
    hdr = {"Accept": "application/json"}
    if payload is not None:
        hdr["Content-Type"] = "application/json"
    hdr.update(headers or {})
    data = json.dumps(payload).encode("utf-8") if payload is not None else None
    req = urllib.request.Request(url, data=data, headers=hdr, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            body = resp.read().decode("utf-8", "replace")
        return (json.loads(body) if body.strip() else {}), ""
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")[:300]
        return {}, f"HTTP {exc.code} {exc.reason}: {detail}"
    except (urllib.error.URLError, OSError, ValueError) as exc:
        return {}, str(exc)


def post_json(url, payload, headers=None, timeout=15):
    return _request("POST", url, payload, headers, timeout)


def patch_json(url, payload, headers=None, timeout=15):
    return _request("PATCH", url, payload, headers, timeout)


def get_json(url, headers=None, timeout=15):
    return _request("GET", url, None, headers, timeout)

def load(name: str) -> Tracker:
    return importlib.import_module(f"tracker_{name}").tracker()

def remote_status(backend: str, status: str) -> str:
    with db.connect() as con:
        row = con.execute(
            "SELECT remote_status FROM tracker_mapping "
            "WHERE backend = ? AND status = ?", (backend, status)).fetchone()
    return row["remote_status"] if row else ""


def artifact_path(project: str, backend: str) -> Path:
    return OUT_ROOT / project / f"tracker-{backend}.json"

def close_old_tickets(keep_runid: int) -> int:
    """A new baseline supersedes the previous period: force-close every
    live remote ticket of older runs, with a superseded-by comment.
    Grouped by the backend the ticket actually lives in."""
    mode = str(db.get_param("track", "mode", "off")).strip().lower()
    if mode not in ("dry", "live"):
        return 0
    closed = failed = 0
    for project in db.projects_for_run(keep_runid):
        rows = db.remote_tickets_to_close(project, keep_runid)
        by_backend: dict[str, list[dict]] = {}
        for r in rows:
            by_backend.setdefault(r["backend"], []).append(r)
        for name, links in by_backend.items():
            try:
                be = load(name)
            except (ImportError, AttributeError) as exc:
                print(f"[tracker] unknown backend {name!r}: {exc}",
                      file=sys.stderr)
                continue
            err = be.configure()
            if err:
                print(f"[tracker] {name}: {err}", file=sys.stderr)
                continue
            dry = mode == "dry" or be.force_dry()
            for link in links:
                comment = (f"Closed by new ai-check-{project} run "
                           f"(runid={keep_runid}); superseded by the new "
                           f"review baseline.")
                if dry:
                    print(f"[tracker] WOULD CLOSE {name}:{link['remote_id']} "
                          f"({link['key']})")
                    continue
                err = be.close(link, comment)
                if err:
                    print(f"[tracker] FAILED close {name}:{link['remote_id']}: "
                          f"{err}", file=sys.stderr)
                    failed += 1
                    continue
                db.mark_remote_closed(link["issue_id"], name, be.closed_status)
                print(f"[tracker] CLOSED {name}:{link['remote_id']} "
                      f"({link['key']})")
                closed += 1
    if closed or failed:
        print(f"[tracker] {closed} ticket(s) closed, {failed} failed")
    return 1 if failed else 0

def push_project(project: str, *, force: bool = False,
                 dry_override: bool = False) -> int:
    mode = str(db.get_param("track", "mode", "off")).strip().lower()
    if mode not in ("dry", "live"):
        return 0
    name = str(db.get_param("track", "backend", "jira")).strip().lower()
    try:
        be = load(name)
    except (ImportError, AttributeError) as exc:
        print(f"[tracker] unknown backend {name!r}: {exc}", file=sys.stderr)
        return 1
    
    err = be.configure()
    if err:
        print(f"[tracker] {name}: {err}", file=sys.stderr)
        return 1
    
    err = be.ensure_project(project)
    if err:
        print(f"[tracker] {name}: ensure_project({project}): {err}",
              file=sys.stderr)
        return 1
    
    dry = dry_override or mode == "dry" or be.force_dry()

    rows = [r for r in db.list_issues(project=project, latest_only=True)
            if r["status"] not in (db.STATUS_SUPPRESSED, db.STATUS_STALE)]
    linked = db.remote_links(project, name)

    entries: list[dict] = []
    pushed = skipped = failed = 0
    for r in rows:
        prev = linked.get(r["id"])
        # A live link is final; a dry row must be re-pushable in live mode.
        if not force and prev and not prev["dry"] and prev["remote_id"]:
            skipped += 1
            continue
        t = Ticket(r["id"], r["key"], project, r["section"], r["title"],
                   r.get("text") or r["title"], r["status"],
                   remote_status(name, r["status"]))
        payload = be.build(t)
        res = (PushResult(dry=True, remote_status=t.remote_status) if dry
               else be.create(t, payload))
        entries.append({"key": t.key, "local_status": t.status,
                        "remote_status": t.remote_status, "dry": res.dry,
                        "remote_id": res.remote_id,
                        "remote_url": res.remote_url,
                        "error": res.error, "payload": payload})
        if res.error:
            print(f"[tracker] FAILED {t.key}: {res.error}", file=sys.stderr)
            failed += 1
            continue
        db.mark_pushed(t.issue_id, name, remote_id=res.remote_id,
                       remote_url=res.remote_url,
                       remote_status=res.remote_status or t.remote_status,
                       dry=res.dry)
        pushed += 1
        print(f"[tracker] {'DRY' if res.dry else 'PUSHED':<6} {t.key}  "
              f"{name}:{res.remote_id or '(payload only)'}")

    out = artifact_path(project, name)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(
        {"project": project, "backend": name, "mode": mode, "dry": dry,
         "dt": datetime.now().strftime("%Y-%m-%dT%H:%M:%S"),
         "issues": entries}, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[tracker] {project}: {pushed} {'rendered' if dry else 'pushed'}, "
          f"{skipped} already linked, {failed} failed -> {rel_display(out)}")
    return 1 if failed else 0

def marker(r: dict) -> str:
    """Status suffix for ai-status:
       '^<remote_id>'  pushed (gitea: issue number, jira: PROJ-42)
       '~'             dry-run payload only, no ticket exists
       ''              never pushed
    """
    if not r.get("remote_backend"):
        return ""
    if r.get("remote_dry"):
        return "~"
    rid = str(r.get("remote_id") or "")
    return f"^{rid}" if rid else "^"

def main() -> int:
    ap = argparse.ArgumentParser(description="Push findings to the bug tracker")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--project")
    g.add_argument("--all", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--force", action="store_true",
                    help="re-push even if already linked")
    args = ap.parse_args()
    names = ([args.project] if args.project
             else [p.name for p in discover_projects()])
    rc = 0
    for n in names:
        rc |= push_project(n, force=args.force, dry_override=args.dry_run)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
