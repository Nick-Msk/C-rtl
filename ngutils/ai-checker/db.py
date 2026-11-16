"""SQLite persistence for ai-checker findings.

DB lives at ai-checker/config/check.db (gitignored). All writes are
wrapped in try/except at call sites — ai-check and ai-fix never fail
just because the DB is broken.
"""
from __future__ import annotations

import json
import sqlite3
import subprocess
import sys
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path

from common import ROOT

DB_PATH = Path(__file__).resolve().parent / "config" / "check.db"
SCHEMA_VERSION = 8


DEFAULTS: dict[tuple[str, str], int | float | bool | str] = {
    ("check", "max_chars"):        80_000,
    ("check", "temperature"):      0.2,
    ("fix",   "max_source_chars"): 100_000,
    ("fix",   "default_limit"):     50,
    ("fix",   "max_diff_lines"):    20,
    ("fix",   "temperature"):      0.0,
    ("llm",   "url"):              "http://127.0.0.1:1234",
    ("llm",   "model_name"):       "",
}

MODULES: dict[str, str] = {
    "check":  "AI review pipeline: chunk size, model, prompt budget",
    "fix":    "AI fix pipeline: prompt budget, diff limit, default count",
    "db":     "Database: schema, migrations, queries",
    "llm":    "LM Studio endpoint and model selection",
    "common": "Shared constants, regexes, parser, lock",
    "report": "Report header formatting",
}

SCHEMA = """
CREATE TABLE IF NOT EXISTS base_commit (
    sha         TEXT PRIMARY KEY,
    short_sha   TEXT NOT NULL,
    branch      TEXT,
    first_dt    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS llmruns (
    runid       INTEGER PRIMARY KEY AUTOINCREMENT,
    kind        TEXT NOT NULL CHECK (kind IN ('check','fix')),
    url         TEXT NOT NULL,
    name        TEXT,
    start_time  TEXT NOT NULL,
    end_time    TEXT,
    status      TEXT NOT NULL DEFAULT 'running'
                CHECK (status IN ('running','ok','failed')),
    in_tokens   INTEGER NOT NULL DEFAULT 0,
    out_tokens  INTEGER NOT NULL DEFAULT 0,
    requests    INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_llmruns_start ON llmruns(start_time);

CREATE TABLE IF NOT EXISTS report (
    run_id      INTEGER REFERENCES llmruns(runid),
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
    status      TEXT NOT NULL DEFAULT 'added'
                CHECK (status IN ('added','proposed-to-fix',
                                          'analyze-by-dev','fixed',
                                          'rejected','failed','stale')),
    reason      TEXT,
    direction   TEXT,
    fix_branch  TEXT,
    fix_commit  TEXT,
    fix_subject TEXT,
    dt          TEXT NOT NULL,
    UNIQUE (report_id, chunk_id, local_id)
);

CREATE INDEX IF NOT EXISTS idx_issue_status ON issue(status);
CREATE INDEX IF NOT EXISTS idx_issue_report ON issue(report_id);

CREATE TABLE proposal_commit (
    issue_id        INTEGER NOT NULL REFERENCES issue(id) ON DELETE CASCADE,
    seq             INTEGER NOT NULL DEFAULT 1,
    file            TEXT NOT NULL,
    edits_json      TEXT,
    old_snippet     TEXT NOT NULL,
    new_snippet     TEXT NOT NULL,
    explanation     TEXT NOT NULL DEFAULT '',
    commit_msg      TEXT NOT NULL,
    reason          TEXT NOT NULL DEFAULT '',
    file_sha256     TEXT NOT NULL,
    commit_subject  TEXT
    dt              TEXT NOT NULL,
    PRIMARY KEY (issue_id, seq)
);

CREATE TABLE IF NOT EXISTS modules (
    name  TEXT PRIMARY KEY,
    descr TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS params (
    module        TEXT NOT NULL REFERENCES modules(name),
    name          TEXT NOT NULL,
    value         TEXT NOT NULL,
    default_value TEXT NOT NULL,
    dt            TEXT NOT NULL,
    PRIMARY KEY (category, name)
);

CREATE INDEX idx_proposal_commit_dt ON proposal_commit(dt);
"""

STATUS_ADDED    = "added"
STATUS_PROPOSED = "proposed-to-fix"
STATUS_ANALYZE  = "analyze-by-dev"
STATUS_FIXED    = "fixed"
STATUS_REJECTED = "rejected"
STATUS_FAILED   = "failed"
STATUS_STALE    = "stale"
STATUSES = {STATUS_ADDED, STATUS_PROPOSED, STATUS_ANALYZE,
            STATUS_FIXED, STATUS_REJECTED, STATUS_FAILED, STATUS_STALE}


def _now_iso() -> str:
    return datetime.now().strftime("%Y-%m-%dT%H:%M:%S")

def has_any_report() -> bool:
    """True if check.db exists and contains at least one report row.

    Does not create the DB if it is missing; does not raise on a
    half-built or corrupt file — callers get False and decide.
    """
    if not DB_PATH.is_file():
        return False
    try:
        with connect() as con:
            n = con.execute("SELECT COUNT(*) FROM report").fetchone()[0]
            return n > 0
    except Exception:  # noqa: BLE001
        return False

TABLE_LOCAL_BASE = 1000

def finding_key_str(finding) -> str:
    """Canonical 'chunk/local_id' for display and FIXLIST.

    Accepts a dict (from parse_findings) or a raw str id.
    """
    if isinstance(finding, dict):
        chunk, local = issue_key(finding)
    else:
        chunk, local = split_finding_id(str(finding))
    return f"{chunk}/{local}"

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
    """Keys to skip on next --apply: needs-review (already classified),
    fixed, rejected. Use --reprocess to override."""
    rel = _rel(report_path)
    with connect() as con:
        rows = con.execute(
            "SELECT chunk_id, local_id FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND status IN (?, ?, ?)",
            (rel, STATUS_ANALYZE, STATUS_FIXED, STATUS_REJECTED, STATUS_STALE)).fetchall()
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

def report_id_by_path(report_path: Path | str) -> int | None:
    rel = _rel(report_path)
    with connect() as con:
        row = con.execute(
            "SELECT id FROM report WHERE path = ?", (rel,)).fetchone()
    return row["id"] if row else None

def ingest_report(*, project: str, report_path: Path, model: str,
                  kind: str, started: datetime, duration_s: float,
                  findings: list[dict],
                  runid: int | None = None) -> None:
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
            "(project, path, base_sha, model, kind, started, duration_s, "
            " run_id, dt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (project, rel, sha, model, kind,
             started.strftime("%Y-%m-%dT%H:%M:%S"), duration_s,
             runid, now))
        report_id = cur.lastrowid
        skipped_dup = 0
        for f in findings:
            fid = f.get("id", "")
            try:
                chunk_id, local_id = split_finding_id(fid)
            except (ValueError, AttributeError):
                continue
            title = f["text"].splitlines()[0][:120]
            try:
                con.execute(
                    "INSERT INTO issue "
                    "(report_id, finding_id, chunk_id, local_id, section, "
                    " title, text, status, dt) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (report_id, fid, chunk_id, local_id, f["section"],
                    title, f["text"], STATUS_ADDED, now))
            except sqlite3.IntegrityError:
                skipped_dup += 1
        if skipped_dup:
            print(f"  ERROR: {skipped_dup} duplicate finding(s) "
                  f"skipped during ingest", file=sys.stderr)

def _key_for(finding) -> tuple[str, int, int]:
    """Return (fid_text, chunk_id, local_id) from a finding dict or str."""
    if isinstance(finding, dict):
        fid = str(finding.get("id", ""))
        chunk, local = issue_key(finding)
        return fid, chunk, local
    fid = str(finding)
    chunk, local = split_finding_id(fid)
    return fid, chunk, local

def close_period(runid: int) -> tuple[int, list[str]]:
    """Close previous periods for projects touched by this run.

    For each project that has a report in this run, all OTHER
    reports of the same project are superseded. Non-'fixed' issues
    in those older reports become 'stale', their cached proposals
    are deleted, and their fix_branch references are cleared.

    'fixed' issues are terminal and preserved (they represent
    already-merged commits).

    Returns (n_stale_issues, branches_to_delete). The caller is
    responsible for actually deleting the branches — db.py does no
    git operations.
    """
    now = _now_iso()
    branches: list[str] = []
    total = 0

    with connect() as con:
        projects = [r["project"] for r in con.execute(
            "SELECT DISTINCT project FROM report WHERE run_id = ?",
            (runid,)).fetchall()]

        for project in projects:
            this_ids = [r["id"] for r in con.execute(
                "SELECT id FROM report WHERE run_id = ? AND project = ?",
                (runid, project)).fetchall()]
            if not this_ids:
                continue

            ph_this = ",".join("?" * len(this_ids))
            older = [r["id"] for r in con.execute(
                f"SELECT id FROM report WHERE project = ? "
                f"  AND id NOT IN ({ph_this})",
                (project, *this_ids)).fetchall()]
            if not older:
                continue

            ph_old = ",".join("?" * len(older))

            # Collect branches before clearing them
            for r in con.execute(
                    f"SELECT DISTINCT fix_branch FROM issue WHERE "
                    f"  report_id IN ({ph_old}) "
                    f"  AND fix_branch IS NOT NULL "
                    f"  AND status != ?",
                    (*older, STATUS_FIXED)).fetchall():
                if r["fix_branch"]:
                    branches.append(r["fix_branch"])

            # Drop cached proposals
            con.execute(
                f"DELETE FROM proposal_commit WHERE issue_id IN ("
                f"  SELECT id FROM issue WHERE "
                f"    report_id IN ({ph_old}) "
                f"    AND status != ?)",
                (*older, STATUS_FIXED))

            # Mark stale, clear fix_* refs
            cur = con.execute(
                f"UPDATE issue SET status = ?, "
                f"  fix_branch = NULL, fix_commit = NULL, "
                f"  fix_subject = NULL, dt = ? WHERE "
                f"  report_id IN ({ph_old}) "
                f"  AND status != ?",
                (STATUS_STALE, now, *older, STATUS_FIXED))
            total += cur.rowcount

    return total, branches

def update_issue_status(report_path, finding, status, *,
                        reason=None, direction=None,
                        fix_branch=None, fix_commit=None,
                        fix_subject=None) -> bool:
    """Update one issue. `finding` is a dict (preferred) or str id."""
    if status not in STATUSES:
        raise ValueError(f"unknown status {status!r}")
    try:
        _fid, chunk_id, local_id = _key_for(finding)
    except (ValueError, TypeError):
        return False
    rel = _rel(report_path)
    with connect() as con:
        cur = con.execute(
            "UPDATE issue SET "
            "  status = ?, dt = ?, "
            "  reason = COALESCE(?, reason), "
            "  direction = COALESCE(?, direction), "
            "  fix_branch = COALESCE(?, fix_branch), "
            "  fix_commit = COALESCE(?, fix_commit), "
            "  fix_subject = COALESCE(?, fix_subject) "
            "WHERE report_id = (SELECT id FROM report WHERE path = ?) "
            "  AND chunk_id = ? AND local_id = ?",
            (status, _now_iso(), reason, direction,
             fix_branch, fix_commit, fix_subject,
             rel, chunk_id, local_id))
        return cur.rowcount > 0

def resolved_finding_ids(report_path: Path | str) -> set[str]:
    """Return finding ids whose status is 'fixed' or 'rejected'."""
    rel = _rel(report_path)
    with connect() as con:
        rows = con.execute(
            "SELECT finding_id FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND status IN (?, ?)",
            (rel, STATUS_ANALYZE, STATUS_FIXED, STATUS_REJECTED)).fetchall()
    return {r["finding_id"] for r in rows}


def get_status(report_path: Path | str, finding) -> str | None:
    try:
        _fid, chunk_id, local_id = _key_for(finding)
    except (ValueError, TypeError):
        return None
    rel = _rel(report_path)
    with connect() as con:
        row = con.execute(
            "SELECT status FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND chunk_id = ? AND local_id = ?",
            (rel, chunk_id, local_id)).fetchone()
    return row["status"] if row else None

def save_proposal(report_path, finding, *,
                  edits, commit_msg, reason,
                  commit_subject: str = "",
                  seq: int = 1) -> bool:
    """Cache 1-2 validated local-fix edits.

    `edits` = [{file, old_snippet, new_snippet, explanation, file_sha256}].
    """
    if not edits:
        return False
    try:
        _fid, chunk_id, local_id = _key_for(finding)
    except (ValueError, TypeError):
        return False
    rel = _rel(report_path)
    first = edits[0]
    with connect() as con:
        row = con.execute(
            "SELECT id FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND chunk_id = ? AND local_id = ?",
            (rel, chunk_id, local_id)).fetchone()
        if not row:
            return False
        con.execute(
            "INSERT OR REPLACE INTO proposal_commit "
            "(issue_id, seq, file, old_snippet, new_snippet, explanation, "
            " commit_msg, commit_subject, reason, file_sha256, "
            " edits_json, dt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (row["id"], seq,
             first["file"], first["old_snippet"], first["new_snippet"],
             first.get("explanation", ""),
             commit_msg, commit_subject, reason,
             first.get("file_sha256", ""),
             json.dumps(edits, ensure_ascii=False),
             _now_iso()))
        return True


def load_proposal(report_path: Path | str, finding) -> dict | None:
    """Load newest proposal; `edits` is a list of 1-2 dicts."""
    try:
        _fid, chunk_id, local_id = _key_for(finding)
    except (ValueError, TypeError):
        return None
    rel = _rel(report_path)
    with connect() as con:
        row = con.execute(
            "SELECT p.seq, p.commit_msg, p.commit_subject, p.reason, p.dt, "
            "       p.edits_json, p.file, p.old_snippet, p.new_snippet, "
            "       p.explanation, p.file_sha256 "
            "FROM proposal_commit p JOIN issue i ON i.id = p.issue_id "
            "JOIN report r ON r.id = i.report_id "
            "WHERE r.path = ? AND i.chunk_id = ? AND i.local_id = ? "
            "ORDER BY p.seq DESC LIMIT 1",
            (rel, chunk_id, local_id)).fetchone()
    if not row:
        return None
    edits: list[dict]
    if row["edits_json"]:
        try:
            edits = json.loads(row["edits_json"])
        except json.JSONDecodeError:
            edits = []
    else:
        edits = []
    if not edits:
        # legacy single-file proposal
        edits = [{
            "file": row["file"],
            "old_snippet": row["old_snippet"],
            "new_snippet": row["new_snippet"],
            "explanation": row["explanation"] or "",
            "file_sha256": row["file_sha256"] or "",
        }]
    return {"seq": row["seq"],
            "commit_msg": row["commit_msg"] or "",
            "commit_subject": row["commit_subject"] or "",
            "reason": row["reason"] or "",
            "dt": row["dt"] or "",
            "edits": edits}


def clear_proposal(report_path: Path | str, finding,
                   seq: int | None = None) -> int:
    """Delete one proposal (seq given) or all for an issue. Returns count."""
    try:
        _fid, chunk_id, local_id = _key_for(finding)
    except (ValueError, TypeError):
        return 0
    rel = _rel(report_path)
    with connect() as con:
        if seq is None:
            cur = con.execute(
                "DELETE FROM proposal_commit WHERE issue_id = ("
                "  SELECT id FROM issue WHERE "
                "    report_id = (SELECT id FROM report WHERE path = ?) "
                "    AND chunk_id = ? AND local_id = ?)",
                (rel, chunk_id, local_id))
        else:
            cur = con.execute(
                "DELETE FROM proposal_commit WHERE seq = ? AND issue_id = ("
                "  SELECT id FROM issue WHERE "
                "    report_id = (SELECT id FROM report WHERE path = ?) "
                "    AND chunk_id = ? AND local_id = ?)",
                (seq, rel, chunk_id, local_id))
        return cur.rowcount

def _seed_modules(con: sqlite3.Connection) -> None:
    """Insert missing modules. Idempotent."""
    for name, descr in MODULES.items():
        con.execute(
            "INSERT OR IGNORE INTO modules (name, descr) VALUES (?, ?)",
            (name, descr))

def _seed_params(con: sqlite3.Connection) -> None:
    """Insert missing default params. Idempotent."""
    now = _now_iso()
    for (cat, name), default in DEFAULTS.items():
        con.execute(
            "INSERT OR IGNORE INTO params "
            "(module, name, value, default_value, dt) "
            "VALUES (?, ?, ?, ?, ?)",
            (cat, name, str(default), str(default), now))

def seed_defaults() -> None:
    """Public entry point: ensure all known params exist.

    Best-effort: a failure here must not abort a run — callers always
    have their in-code defaults as a fallback. We log once, so a
    persistent problem (corrupt DB, read-only mount) is visible.
    """
    try:
        with connect() as con:
            _seed_modules(con)
            _seed_params(con)
    except Exception as exc:  # noqa: BLE001
        print(f"[ai-checker] warn: seed_defaults failed: {exc}",
              file=sys.stderr)


def get_param(category: str, name: str, default):
    """Return typed value from params. Falls back to `default`."""
    try:
        with connect() as con:
            row = con.execute(
                "SELECT value FROM params "
                "WHERE module = ? AND name = ?",
                (category, name)).fetchone()
        if not row:
            return default
        raw = row["value"]
        if isinstance(default, bool):
            return raw.lower() in ("1", "true", "yes", "y", "on")
        if isinstance(default, int):
            return int(raw)
        if isinstance(default, float):
            return float(raw)
        return raw
    except Exception:  # noqa: BLE001
        return default


def set_param(module: str, name: str, value) -> bool:
    """Update value. If `value` is None, reset to default_value."""
    try:
        with connect() as con:
            if value is None:
                cur = con.execute(
                    "UPDATE params SET value = default_value, dt = ? "
                    "WHERE module = ? AND name = ?",
                    (_now_iso(), module, name))
            else:
                cur = con.execute(
                    "UPDATE params SET value = ?, dt = ? "
                    "WHERE module = ? AND name = ?",
                    (str(value), _now_iso(), module, name))
            return cur.rowcount > 0
    except Exception:  # noqa: BLE001
        return False

def all_params() -> list[dict]:
    with connect() as con:
        rows = con.execute(
            "SELECT p.module, m.descr, p.name, p.value, "
            "       p.default_value, p.dt "
            "FROM params p JOIN modules m ON m.name = p.module "
            "ORDER BY p.module, p.name").fetchall()
    return [dict(r) for r in rows]


def all_modules() -> list[dict]:
    with connect() as con:
        rows = con.execute(
            "SELECT name, descr FROM modules ORDER BY name").fetchall()
    return [dict(r) for r in rows]

def summary_by_project_status() -> dict[str, dict[str, int]]:
    """Returns {project: {status: count}} across all ingested issues."""
    with connect() as con:
        rows = con.execute(
            "SELECT r.project, i.status, COUNT(*) AS n "
            "FROM issue i JOIN report r ON r.id = i.report_id "
            "GROUP BY r.project, i.status "
            "ORDER BY r.project, i.status").fetchall()
    out: dict[str, dict[str, int]] = {}
    for r in rows:
        out.setdefault(r["project"], {})[r["status"]] = r["n"]
    return out


def list_issues(project: str | None = None,
                status: str | None = None,
                latest_only: bool = True) -> list[dict]:
    """Return issue rows as plain dicts, newest report first.

    latest_only=True (default) restricts to the most recent report
    per project, so superseded ('stale') runs don't drown the view.
    Each dict has an extra 'key' = '<chunk_id>/<local_id>'.
    """
    q = ("SELECT r.project, i.chunk_id, i.local_id, i.section, "
         "i.finding_id, i.status, i.title, i.fix_branch, i.fix_commit, "
         "i.fix_subject, i.dt "
         "FROM issue i JOIN report r ON r.id = i.report_id WHERE 1=1 ")
    params: list = []
    if project:
        q += "AND r.project = ? "
        params.append(project)
    if status:
        if latest_only:
            q += ("AND r.id = (SELECT MAX(id) FROM report "
                "WHERE project = r.project) ")
        params.append(status)
    q += "ORDER BY r.id DESC, i.chunk_id, i.local_id"
    with connect() as con:
        rows = con.execute(q, params).fetchall()
    out = []
    for r in rows:
        d = dict(r)
        d["key"] = f"{r['chunk_id']}/{r['local_id']}"
        out.append(d)
    return out

def llmrun_start(kind: str, url: str, name: str = "") -> int:
    """Insert a new 'running' row, return runid."""
    if kind not in ("check", "fix"):
        raise ValueError(f"unknown kind {kind!r}")
    with connect() as con:
        cur = con.execute(
            "INSERT INTO llmruns (kind, url, name, start_time, status) "
            "VALUES (?, ?, ?, ?, 'running')",
            (kind, url, name or None, _now_iso()))
        return cur.lastrowid


def llmrun_finish(runid: int, status: str = "ok", *,
                  in_tokens: int = 0, out_tokens: int = 0,
                  requests: int = 0) -> bool:
    """Mark run as finished. status: 'ok' | 'failed'."""
    if status not in ("ok", "failed"):
        raise ValueError(f"bad finish status {status!r}")
    with connect() as con:
        cur = con.execute(
            "UPDATE llmruns SET status = ?, end_time = ?, "
            "  in_tokens = ?, out_tokens = ?, requests = ? "
            "WHERE runid = ?",
            (status, _now_iso(), in_tokens, out_tokens, requests, runid))
        return cur.rowcount > 0


def llmruns_list(limit: int = 20) -> list[dict]:
    with connect() as con:
        rows = con.execute(
            "SELECT runid, kind, url, name, start_time, end_time, "
            "       status, in_tokens, out_tokens, requests "
            "FROM llmruns ORDER BY runid DESC LIMIT ?",
            (limit,)).fetchall()
    return [dict(r) for r in rows]
