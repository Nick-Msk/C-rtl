"""SQLite persistence for ai-checker findings.

DB lives at ai-checker/config/check.db (gitignored). All writes are
wrapped in try/except at call sites — ai-check and ai-fix never fail
just because the DB is broken.
"""
from __future__ import annotations

import sqlite3
import subprocess
import sys
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path

from common import ROOT

DB_PATH = Path(__file__).resolve().parent / "config" / "check.db"
SCHEMA_VERSION = 1

SCHEMA = """
CREATE TABLE IF NOT EXISTS base_commit (
    sha         TEXT PRIMARY KEY,
    short_sha   TEXT NOT NULL,
    branch      TEXT,
    first_dt    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS report (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project     TEXT NOT NULL,
    path        TEXT NOT NULL UNIQUE,
    base_sha    TEXT NOT NULL REFERENCES base_commit(sha),
    model       TEXT,
    kind        TEXT NOT NULL,
    started     TEXT NOT NULL,
    duration_s  REAL,
    dt          TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS issue (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    report_id   INTEGER NOT NULL REFERENCES report(id) ON DELETE CASCADE,
    finding_id  TEXT NOT NULL,
    chunk_id    INTEGER NOT NULL,
    local_id    INTEGER NOT NULL,
    section     TEXT NOT NULL,
    title       TEXT NOT NULL,
    text        TEXT NOT NULL,
    status      TEXT NOT NULL DEFAULT 'proposed-to-fix'
                CHECK (status IN ('proposed-to-fix','analyze-by-dev',
                                  'fixed','rejected','failed')),
    reason      TEXT,
    direction   TEXT,
    fix_branch  TEXT,
    fix_commit  TEXT,
    dt          TEXT NOT NULL,
    UNIQUE (report_id, chunk_id, local_id)
);

CREATE INDEX IF NOT EXISTS idx_issue_status ON issue(status);
CREATE INDEX IF NOT EXISTS idx_issue_report ON issue(report_id);
"""

STATUS_PROPOSED = "proposed-to-fix"
STATUS_ANALYZE  = "analyze-by-dev"
STATUS_FIXED    = "fixed"
STATUS_REJECTED = "rejected"
STATUS_FAILED   = "failed"
STATUSES = {STATUS_PROPOSED, STATUS_ANALYZE, STATUS_FIXED,
            STATUS_REJECTED, STATUS_FAILED}


def _now_iso() -> str:
    return datetime.now().strftime("%Y-%m-%dT%H:%M:%S")



TABLE_LOCAL_BASE = 1000


def issue_key(finding: dict) -> tuple[int, int]:
    """Return (chunk_id, local_id) for a finding dict.

    '1/10'      (header) -> (1, 10)
    '16' chunk1 (table)  -> (1, 1016)
    '16' chunk2 (table)  -> (2, 1016)

    Headers carry their chunk in the id, so chunk=parse, local=parse.
    Tables/lists carry only the row number; we keep the report's
    current chunk and offset the local id past TABLE_LOCAL_BASE so
    rows never collide with header locals of the same chunk.
    """
    fid = str(finding.get("id", ""))
    chunk = int(finding.get("chunk", 1))
    kind = finding.get("kind", "header")
    if "/" in fid:
        a, b = fid.split("/", 1)
        return int(a), int(b)
    local = int(fid) if fid.isdigit() else 0
    if kind in ("table", "list"):
        return chunk, TABLE_LOCAL_BASE + local
    return chunk, local


def split_finding_id(fid: str) -> tuple[int, int]:
    """Legacy: '1/10' -> (1, 10); bare '16' -> (1, 1016).

    Bare ids cannot carry their chunk; this fallback assumes chunk 1.
    Prefer passing the finding dict to update_issue_status/get_status.
    """
    if "/" in fid:
        a, b = fid.split("/", 1)
        return int(a), int(b)
    return 1, TABLE_LOCAL_BASE + int(fid)

def resolved_finding_keys(report_path: Path | str) -> set[tuple[int, int]]:
    """Return {(chunk_id, local_id)} with status fixed/rejected."""
    rel = _rel(report_path)
    with connect() as con:
        rows = con.execute(
            "SELECT chunk_id, local_id FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND status IN (?, ?)",
            (rel, STATUS_FIXED, STATUS_REJECTED)).fetchall()
    return {(r["chunk_id"], r["local_id"]) for r in rows}

def _rel(path: Path | str) -> str:
    p = Path(path)
    try:
        return str(p.resolve().relative_to(ROOT))
    except ValueError:
        return str(p)


@contextmanager
def connect():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(DB_PATH)
    con.execute("PRAGMA foreign_keys = ON")
    con.execute("PRAGMA journal_mode = WAL")
    con.row_factory = sqlite3.Row
    try:
        v = con.execute("PRAGMA user_version").fetchone()[0]
        if v == 0:
            con.executescript(SCHEMA)
            con.execute(f"PRAGMA user_version = {SCHEMA_VERSION}")
        elif v != SCHEMA_VERSION:
            raise RuntimeError(
                f"check.db schema v{v}, expected v{SCHEMA_VERSION}; "
                f"delete {DB_PATH} to recreate (data will be lost)"
            )
        yield con
        con.commit()
    except Exception:
        con.rollback()
        raise
    finally:
        con.close()


def current_commit() -> tuple[str | None, str | None]:
    """Return (sha40, branch). None on non-git checkout."""
    try:
        sha = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True,
            stderr=subprocess.DEVNULL, timeout=2).strip()
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=ROOT,
            text=True, stderr=subprocess.DEVNULL, timeout=2).strip()
        if sha and sha != "HEAD":
            return sha, branch
    except (OSError, subprocess.SubprocessError):
        pass
    return None, None


def ingest_report(*, project: str, report_path: Path, model: str,
                  kind: str, started: datetime, duration_s: float,
                  findings: list[dict]) -> None:
    """Insert base_commit, report and all findings with default status.

    Idempotent w.r.t. report.path: if a report with the same path was
    already ingested, it is deleted (cascade) and re-inserted.
    """
    sha, branch = current_commit()
    if sha is None:
        print("[ai-checker] db: not a git repo, skipping ingest",
              file=sys.stderr)
        return
    now = _now_iso()
    rel = _rel(report_path)

    with connect() as con:
        con.execute(
            "INSERT OR IGNORE INTO base_commit "
            "(sha, short_sha, branch, first_dt) VALUES (?, ?, ?, ?)",
            (sha, sha[:7], branch, now))
        con.execute("DELETE FROM report WHERE path = ?", (rel,))
        cur = con.execute(
            "INSERT INTO report "
            "(project, path, base_sha, model, kind, started, duration_s, dt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            (project, rel, sha, model, kind,
             started.strftime("%Y-%m-%dT%H:%M:%S"), duration_s, now))
        report_id = cur.lastrowid
        for f in findings:
            fid = f.get("id", "")
            try:
                chunk_id, local_id = split_finding_id(fid)
            except (ValueError, AttributeError):
                continue
            title = f["text"].splitlines()[0][:120]
            con.execute(
                "INSERT INTO issue "
                "(report_id, finding_id, chunk_id, local_id, section, "
                " title, text, status, dt) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                (report_id, fid, chunk_id, local_id, f["section"],
                 title, f["text"], STATUS_PROPOSED, now))


def update_issue_status(report_path: Path | str, finding_id: str,
                        status: str, *, reason: str | None = None,
                        direction: str | None = None,
                        fix_branch: str | None = None,
                        fix_commit: str | None = None) -> bool:
    """Update one issue. Returns True if the row exists, False otherwise."""
    if status not in STATUSES:
        raise ValueError(f"unknown status {status!r}")
    try:
        chunk_id, local_id = split_finding_id(finding_id)
    except (ValueError, AttributeError):
        return False
    rel = _rel(report_path)
    with connect() as con:
        cur = con.execute(
            "UPDATE issue SET "
            "  status = ?, dt = ?, "
            "  reason = COALESCE(?, reason), "
            "  direction = COALESCE(?, direction), "
            "  fix_branch = COALESCE(?, fix_branch), "
            "  fix_commit = COALESCE(?, fix_commit) "
            "WHERE report_id = (SELECT id FROM report WHERE path = ?) "
            "  AND chunk_id = ? AND local_id = ?",
            (status, _now_iso(), reason, direction,
             fix_branch, fix_commit, rel, chunk_id, local_id))
        return cur.rowcount > 0


def resolved_finding_ids(report_path: Path | str) -> set[str]:
    """Return finding ids whose status is 'fixed' or 'rejected'."""
    rel = _rel(report_path)
    with connect() as con:
        rows = con.execute(
            "SELECT finding_id FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND status IN (?, ?)",
            (rel, STATUS_FIXED, STATUS_REJECTED)).fetchall()
    return {r["finding_id"] for r in rows}


def get_status(report_path: Path | str, finding_id: str) -> str | None:
    try:
        chunk_id, local_id = split_finding_id(finding_id)
    except (ValueError, AttributeError):
        return None
    rel = _rel(report_path)
    with connect() as con:
        row = con.execute(
            "SELECT status FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND chunk_id = ? AND local_id = ?",
            (rel, chunk_id, local_id)).fetchone()
    return row["status"] if row else None
