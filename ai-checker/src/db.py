"""SQLite persistence for ai-checker findings.

DB lives at ai-checker/config/check.db (gitignored). All writes are
wrapped in try/except at call sites — ai-check and ai-fix never fail
just because the DB is broken.
"""
from __future__ import annotations

import difflib
import json
import re
import sqlite3
import subprocess
import sys
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path

from common import FILE_REF_RE, OUT_ROOT, ROOT, parse_findings

SCHEMA_VERSION = 15

DB_PATH = Path(__file__).resolve().parent / "config" / "check.db"

SCHEMA_PATH = Path(__file__).resolve().parent / "config" / "check.sql"
_schema_cache: str | None = None

CONFIG_TABLES = ("statuses", "params", "transition_status")


DEFAULTS: dict[tuple[str, str], int | float | bool | str] = {
    ("check", "max_chars"):         80_000,
    ("check", "temperature"):       0.2,
    ("check", "tag_context_chars"): 20_000,
    ("check", "carry_statuses"):    "rejected,no-action",
    ("check", "carry_similarity"):  0.80,
    ("check", "suppress_hint"):     True,
    ("fix",   "max_source_chars"):  100_000,
    ("fix",   "default_limit"):     50,
    ("fix",   "max_diff_lines"):    20,
    ("fix",   "temperature"):       0.0,
    ("fix",   "verify_targets"):    "test-{project},analyze-{project}",
    ("fix",   "verify_on_merge"):   False,
    ("fix",   "verify_on_commit"):  True,
    ("llm",   "url"):               "http://127.0.0.1:1234",
    ("llm",   "model_name"):        "",
    ("track", "mode"):              "off",     # off | dry | live
    ("track", "backend"):           "jira",    # jira | gitea
    ("track", "jira_link"):         "",
    ("track", "jira_project"):      "",
    ("track", "jira_token"):        "",
    ("track", "jira_auth"):         "bearer",  # bearer | basic
    ("track", "jira_user"):         "",
    ("track", "jira_issuetype"):    "Bug",
    ("track", "jira_api"):          "rest",    # rest | plain (json-server)
    ("track", "jira_dry_run"):      True,
    ("track", "gitea_link"):        "",
    ("track", "gitea_owner"):       "",
    ("track", "gitea_repo"):        "",
    ("track", "gitea_token"):       "",
    ("track", "gitea_labels"):      "",
    ("track", "gitea_dry_run"):     True,
}


MODULES: dict[str, str] = {
    "check":  "AI review pipeline: chunk size, model, prompt budget",
    "fix":    "AI fix pipeline: prompt budget, diff limit, default count",
    "db":     "Database: schema, migrations, queries",
    "llm":    "LM Studio endpoint and model selection",
    "common": "Shared constants, regexes, parser, lock",
    "report": "Report header formatting",
    "track":  "Tracker related params"
}

STATUS_ADDED        = "added"
STATUS_PROPOSED     = "proposed-to-fix"
STATUS_ANALYZE      = "analyze-by-dev"
STATUS_FIXED        = "fixed"
STATUS_REJECTED     = "rejected"
STATUS_FAILED       = "failed"
STATUS_STALE        = "stale"
STATUS_NO_ACTION    = "no-action"
STATUS_SUPPRESSED   = "suppressed"
STATUS_MERGED       = "merged"

# name -> (descr, terminal). Insertion order = column order in status.py.
# descr is code-owned documentation (refreshed on every seed).
# terminal is DB-owned policy: edit it in the DB, seeding never resets it.
STATUSES_META: dict[str, tuple[str, bool, bool]] = {
    STATUS_ADDED:      ("found by ai-review, not triaged yet",          False, False),
    STATUS_PROPOSED:   ("validated local patch cached, not committed",  False, False),
    STATUS_ANALYZE:    ("non-local change, needs a human",              False, True),
    STATUS_FIXED:      ("committed to a fix branch, awaiting merge",    False, True),
    STATUS_MERGED:     ("patch merged into the base branch",            True,  True),
    STATUS_REJECTED:   ("rejected by a human",                          True,  True),
    STATUS_FAILED:     ("LLM/validation/commit failed",                 True,  True),
    STATUS_STALE:      ("superseded by a newer project report",         True,  True),
    STATUS_NO_ACTION:  ("classifier verified: not a defect",            True,  True),
    STATUS_SUPPRESSED: ("matches an earlier rejected/no-action finding",True,  True),
}
STATUSES = set(STATUSES_META)      # argparse choices, update_issue_status

TRACKER_MAP: dict[tuple[str, str], str] = {
    ("jira", STATUS_ADDED):      "To Do",
    ("jira", STATUS_PROPOSED):   "In Progress",
    ("jira", STATUS_ANALYZE):    "In Progress",
    ("jira", STATUS_FIXED):      "In Review",
    ("jira", STATUS_MERGED):     "Done",
    ("jira", STATUS_REJECTED):   "Done",
    ("jira", STATUS_NO_ACTION):  "Done",
    ("jira", STATUS_FAILED):     "To Do",
    ("jira", STATUS_STALE):      "Done",
    ("jira", STATUS_SUPPRESSED): "Done",
    ("gitea", STATUS_ADDED):     "open",
    ("gitea", STATUS_PROPOSED):  "open",
    ("gitea", STATUS_ANALYZE):   "open",
    ("gitea", STATUS_FIXED):     "open",
    ("gitea", STATUS_MERGED):    "closed",
    ("gitea", STATUS_REJECTED):  "closed",
    ("gitea", STATUS_NO_ACTION): "closed",
    ("gitea", STATUS_FAILED):    "open",
    ("gitea", STATUS_STALE):     "closed",
    ("gitea", STATUS_SUPPRESSED): "closed",
}


def _seed_tracker_mapping(con: sqlite3.Connection) -> None:
    for (backend, status), remote in TRACKER_MAP.items():
        con.execute("INSERT OR IGNORE INTO tracker_mapping "
                    "(backend, status, remote_status) VALUES (?, ?, ?)",
                    (backend, status, remote))

_RESOLVED_SQL = "status IN (SELECT name FROM statuses WHERE resolved = 1)"

_HERE   = Path(__file__).resolve().parent        # .../ai-checker/src
_CONFIG = _HERE.parent / "config"                # .../ai-checker/config
DB_PATH    = _CONFIG / "check.db"
SCHEMA_PATH = _CONFIG / "check.sql"

_schema_cache: str | None = None

def _schema_sql() -> str:
    """Load schema from config/check.sql. Cached after first read.

    Schema lives in a separate .sql file so it can be viewed/diffed
    as plain SQL, opened in sqlite3/dbeaver/pgAdmin, syntax-highlighted
    by editors — without the triple-quoted Python string noise.
    """
    global _schema_cache
    if _schema_cache is None:
        _schema_cache = SCHEMA_PATH.read_text(encoding="utf-8")
    return _schema_cache

def _now_iso() -> str:
    return datetime.now().strftime("%Y-%m-%dT%H:%M:%S")


def ensure_report(report_path: Path | str, *, project: str,
                  kind: str = "full", model: str | None = None) -> int | None:
    """Make sure an on-disk report has a row in the DB; adopt it if not.

    Reports outlive the DB: 'make clean' keeps them, a schema bump may
    recreate check.db. fix.py reads findings from the .md, so without
    a report row every status write silently matches zero rows.

    Adoption costs no LLM calls: findings are re-parsed from the very
    same markdown check.py wrote, with the same parser, so ids (and
    therefore FIXLIST keys) are identical. Never touches an existing
    report row — ingest_report's DELETE+reinsert is NOT used here.

    Returns report.run_id, or None if the report could not be adopted.
    """
    rel = _rel(report_path)
    if report_id_by_path(report_path) is not None:
        return report_runid(report_path)

    p = Path(report_path)
    try:
        findings = parse_findings(p.read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"[ai-checker] db: cannot adopt {rel}: {exc}", file=sys.stderr)
        return None

    # rotate_reports() may have moved an already-ingested report into
    # hist/. Re-point the existing row instead of duplicating it.
    with connect() as con:
        row = con.execute(
            "SELECT id, run_id FROM report WHERE project = ? AND path LIKE ?",
            (project, "%/" + p.name)).fetchone()
        if row:
            con.execute("UPDATE report SET path = ? WHERE id = ?", (rel, row["id"]))
            return row["run_id"]

    started = datetime.fromtimestamp(p.stat().st_mtime)
    ts = started.strftime("%Y-%m-%dT%H:%M:%S")
    with connect() as con:
        # issue.runid is NOT NULL REFERENCES llmruns — a real row is
        # required. url='(adopted)' + zero tokens keep ai-runs honest.
        runid = con.execute(
            "INSERT INTO llmruns (kind, url, name, start_time, end_time, "
            "  status, in_tokens, out_tokens, requests) "
            "VALUES ('check', '(adopted)', ?, ?, ?, 'ok', 0, 0, 0)",
            (model, ts, ts)).lastrowid

    ingest_report(project=project, report_path=p, model=model,
                  kind=kind, started=started, duration_s=None,
                  findings=findings, runid=runid)
    print(f"[ai-checker] db: adopted {rel} "
          f"({len(findings)} issue(s), runid={runid})",
          file=sys.stderr)
    return runid

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

def resolved_finding_keys(report_path: Path | str,
                          reopen: frozenset[str] = frozenset()
                          ) -> set[tuple[int, int]]:
    """Keys of resolved findings. `reopen` excludes specific statuses
    from the resolved set, so propose mode can re-process
    analyze-by-dev / no-action while merged/rejected stay closed."""
    rel = _rel(report_path)
    q = ("SELECT chunk_id, local_id, status FROM issue "
         "WHERE report_id = (SELECT id FROM report WHERE path = ?) "
         f"AND {_RESOLVED_SQL}")
    with connect() as con:
        rows = con.execute(q, (rel,)).fetchall()
    return {(r["chunk_id"], r["local_id"]) for r in rows
            if r["status"] not in reopen}


def resolved_finding_ids(report_path: Path | str) -> set[str]:
    """Тот же предикат, что и resolved_finding_keys (раньше расходились)."""
    rel = _rel(report_path)
    with connect() as con:
        rows = con.execute(
            "SELECT finding_id FROM issue "
            "WHERE report_id = (SELECT id FROM report WHERE path = ?) "
            f"AND {_RESOLVED_SQL}", (rel,)).fetchall()
    return {r["finding_id"] for r in rows}

def _rel(path: Path | str) -> str:
    """Store report path relative to OUT_ROOT (tool data dir), not ROOT.
    Reports live OUTSIDE the reviewed repo now."""

    p = Path(path)
    try:
        return str(p.resolve().relative_to(OUT_ROOT))
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
            con.executescript(_schema_sql())
            _seed_statuses(con)
            import db_status
            db_status.seed(con)
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
                  runid: int | None = None,
                  baseline_branch: str | None = None) -> None:
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
            "(project, path, base_sha, model, kind, started, "
            " duration_s, run_id, baseline_branch, dt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (project, rel, sha, model, kind,
             started.strftime("%Y-%m-%dT%H:%M:%S"), duration_s,
             runid, baseline_branch, now))
        report_id = cur.lastrowid

        skipped_dup = 0
        for f in findings:
            fid = f.get("id", "")
            if not fid:
                continue
            try:
                chunk_id, local_id = issue_key(f)
            except (ValueError, TypeError):
                continue
            title = f["text"].splitlines()[0][:120]
            try:
                con.execute(
                    "INSERT INTO issue "
                    "(report_id, runid, finding_id, chunk_id, local_id, "
                    " section, title, text, status, dt) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                    (report_id, runid or 0, fid, chunk_id, local_id,
                     f["section"], title, f["text"], STATUS_ADDED, now))
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

    Non-'fixed' issues in older reports of the same project become
    'stale'; their fix_* references are cleared. 'fixed' issues are
    terminal and preserved (they represent already-merged commits).

    Returns (n_stale_issues, branches_to_check) — the SECOND element
    includes fix_branch from BOTH stale and fixed issues, because
    a fixed issue's branch may still be lying around in git even
    after merge. The caller (check.py) filters merged-vs-unmerged
    via gitutil.merged_branches() and deletes only merged ones.
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

            # Collect ALL fix_branch refs from older reports, including
            # those of 'fixed' issues (merged earlier but branch may
            # still exist). Caller filters merged vs unmerged.
            for r in con.execute(
                    f"SELECT DISTINCT fix_branch FROM issue WHERE "
                    f"  report_id IN ({ph_old}) "
                    f"  AND fix_branch IS NOT NULL",
                    (*older,)).fetchall():
                if r["fix_branch"]:
                    branches.append(r["fix_branch"])

            # Mark stale, clear fix_* refs
            import db_status
            cur = con.execute(
                f"UPDATE issue SET status = ?, "
                f"  fix_branch = NULL, fix_commit = NULL, "
                f"  fix_subject = NULL, dt = ? WHERE "
                f"  report_id IN ({ph_old}) "
                f"  AND status IN (SELECT status FROM transition_status "
                f"                 WHERE actor = ? AND new_status = ? "
                f"                   AND allowed = 'y')",
                (STATUS_STALE, now, *older, db_status.ACT_STALE, STATUS_STALE))
            total += cur.rowcount

    return total, branches

def update_issue_status(report_path, 
                        finding, 
                        status, *,
                        reason=None, 
                        direction=None,
                        fix_branch=None, 
                        fix_commit=None,
                        fix_subject=None,
                        fix_runid=None) -> bool:
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
            "  fix_subject = COALESCE(?, fix_subject), "
            " fix_runid = COALESCE(?, fix_runid) "
            "WHERE report_id = (SELECT id FROM report WHERE path = ?) "
            "  AND chunk_id = ? AND local_id = ?",
            (status, _now_iso(), reason, direction,
             fix_branch, fix_commit, fix_subject, fix_runid,
             rel, chunk_id, local_id))
        return cur.rowcount > 0

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
                  runid: int,              # check-runid (from issue)
                  fix_runid: int,           # fix-runid (from llmruns)
                  commit_subject: str = "",
                  seq: int | None = None) -> bool:
    """Cache 1-2 validated local-fix edits.

    `runid` is the check-run id (issue.runid). `fix_runid` is the
    current fix run id. `seq` auto-increments per (runid, issue_id)
    when omitted, so re-running ai-fix in a new fix-run preserves
    the previous proposal as history.
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
        issue_id = row["id"]

        if seq is None:
            r = con.execute(
                "SELECT COALESCE(MAX(seq), 0) + 1 FROM proposal_commit "
                "WHERE runid = ? AND issue_id = ?",
                (runid, issue_id)).fetchone()
            seq = r[0]

        con.execute(
            "INSERT OR REPLACE INTO proposal_commit "
            "(runid, issue_id, seq, fix_runid, file, old_snippet, "
            " new_snippet, explanation, commit_msg, commit_subject, "
            " reason, file_sha256, edits_json, dt) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (runid, issue_id, seq, fix_runid,
             first["file"], first["old_snippet"], first["new_snippet"],
             first.get("explanation", ""),
             commit_msg, commit_subject, reason,
             first.get("file_sha256", ""),
             json.dumps(edits, ensure_ascii=False),
             _now_iso()))
        return True

def latest_runid_by_project() -> dict[str, int]:
    """Most recent check-runid per project (from report.run_id).

    Used by 'make ai-status' to label each row as 'project(N)' so the
    numbers are tied to a concrete run. Stale reports do not contribute —
    MAX picks the newest report per project.
    """
    with connect() as con:
        rows = con.execute(
            "SELECT project, MAX(run_id) AS rid "
            "FROM report GROUP BY project"
        ).fetchall()
    return {r["project"]: r["rid"] for r in rows if r["rid"]}

def report_runid(report_path: Path | str) -> int | None:
    """Return report.run_id for this report, or None (legacy / not ingested)."""

    rel = _rel(report_path)
    with connect() as con:  # обычный connect
        row = con.execute(
            "SELECT run_id FROM report WHERE path = ?", (rel,)).fetchone()
    return row["run_id"] if row else None

def load_proposal(report_path: Path | str, finding) -> dict | None:
    """Load the newest proposal for an issue (max seq)."""
    try:
        _fid, chunk_id, local_id = _key_for(finding)
    except (ValueError, TypeError):
        return None
    rel = _rel(report_path)
    with connect() as con:
        row = con.execute(
            "SELECT p.runid, p.seq, p.fix_runid, p.commit_msg, "
            "       p.commit_subject, p.reason, p.dt, p.edits_json, "
            "       p.file, p.old_snippet, p.new_snippet, "
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
    return {"runid": row["runid"], "seq": row["seq"],
            "fix_runid": row["fix_runid"],
            "commit_msg": row["commit_msg"] or "",
            "commit_subject": row["commit_subject"] or "",
            "reason": row["reason"] or "",
            "dt": row["dt"] or "",
            "edits": edits}

def issue_runid(report_path: Path | str, finding) -> int | None:
    """Return issue.runid (check-runid) for this finding, or None."""
    try:
        _fid, chunk_id, local_id = _key_for(finding)
    except (ValueError, TypeError):
        return None
    rel = _rel(report_path)
    with connect() as con:
        row = con.execute(
            "SELECT runid FROM issue WHERE "
            "report_id = (SELECT id FROM report WHERE path = ?) "
            "AND chunk_id = ? AND local_id = ?",
            (rel, chunk_id, local_id)).fetchone()
    return row["runid"] if row else None

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

def _seed_statuses(con: sqlite3.Connection) -> None:
    """Ensure all known statuses exist; refresh descr, preserve terminal.

    descr comes from code (docs must follow the code, e.g. after a
    translation change). terminal is local policy — a user may flip
    'failed' to 0 to allow retries, and seeding must not undo that.
    """
    for name, (descr, terminal, resolved) in STATUSES_META.items():
        con.execute(
            "INSERT INTO statuses (name, descr, terminal) "
            "VALUES (?, ?, ?) "
            "ON CONFLICT(name) DO UPDATE SET descr = excluded.descr",
            (name, descr, int(terminal)))

def _seed_modules(con: sqlite3.Connection) -> None:
    """Insert missing modules. Idempotent."""
    for name, descr in MODULES.items():
        con.execute(
            "INSERT OR IGNORE INTO modules (name, descr) VALUES (?, ?)",
            (name, descr))

def _param_str(value) -> str:
    """Canonical storage form: booleans as '1'/'0' (never 'True'/'False'),
    so value and default_value render identically in ai-config."""
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value)


def _seed_params(con: sqlite3.Connection) -> None:
    """Insert missing params; normalize legacy bool spellings. Idempotent.

    default_value is code-owned -> always refreshed to canonical.
    A user-set value keeps its meaning: only legacy 'true'/'false' are
    rewritten to '1'/'0'; an explicit '1'/'0' is left untouched.
    """
    now = _now_iso()
    for (cat, name), default in DEFAULTS.items():
        sv = _param_str(default)
        con.execute(
            "INSERT OR IGNORE INTO params "
            "(module, name, value, default_value, dt) "
            "VALUES (?, ?, ?, ?, ?)",
            (cat, name, sv, sv, now))
        if isinstance(default, bool):
            con.execute(
                "UPDATE params SET default_value = ? "
                "WHERE module = ? AND name = ?",
                (sv, cat, name))
            con.execute(
                "UPDATE params SET value = '1' "
                "WHERE module = ? AND name = ? AND lower(value) = 'true'",
                (cat, name))
            con.execute(
                "UPDATE params SET value = '0' "
                "WHERE module = ? AND name = ? AND lower(value) = 'false'",
                (cat, name))

def seed_defaults() -> None:
    """Public entry point: ensure all known params exist.

    Best-effort: a failure here must not abort a run — callers always
    have their in-code defaults as a fallback. We log once, so a
    persistent problem (corrupt DB, read-only mount) is visible.
    """
    try:
        with connect() as con:
            _seed_statuses(con)
            _seed_tracker_mapping(con)
            _seed_modules(con)
            _seed_params(con)
            # state machine values
            import db_status
            db_status.seed(con)
    except Exception as exc:  # noqa: BLE001
        print(f"[ai-checker] warn: seed_defaults failed: {exc}",
              file=sys.stderr)
        
    # Validate params AFTER seeding (otherwise validate_params may
    # read an empty table on first run)
    errors, critical = validate_params()
    if critical:
        for e in critical:
            print(f"[ai-checker] ERROR: {e}", file=sys.stderr)
        sys.exit("Parameter validation failed")
    if errors:
        for e in errors:
            print(f"[ai-checker] warn: {e}", file=sys.stderr)

def all_statuses() -> list[dict]:
    """Справочник в порядке вставки — для status.py."""
    with connect() as con:
        return [dict(r) for r in con.execute(
            "SELECT name, descr, terminal FROM statuses ORDER BY rowid")]


_terminal_cache: frozenset[str] | None = None


def terminal_statuses() -> frozenset[str]:
    """Конечные статусы из БД. Кэш на процесс; при битой БД — fallback
    на STATUSES_META (БД не должна ронять пайплайн)."""
    global _terminal_cache
    if _terminal_cache is None:
        fallback = frozenset(n for n, (_, t, _r) in STATUSES_META.items() if t)
        try:
            with connect() as con:
                rows = con.execute(
                    "SELECT name FROM statuses WHERE terminal = 1").fetchall()
            _terminal_cache = frozenset(r["name"] for r in rows) or fallback
        except Exception:  # noqa: BLE001
            return fallback        # ошибку не кэшируем
    return _terminal_cache

def resolved_statuses() -> frozenset[str]:
    """Конечные статусы из БД. Кэш на процесс; при битой БД — fallback
    на STATUSES_META (БД не должна ронять пайплайн)."""
    global _terminal_cache
    if _terminal_cache is None:
        fallback = frozenset(n for n, (_, t, r) in STATUSES_META.items() if t)
        try:
            with connect() as con:
                rows = con.execute(
                    "SELECT name FROM statuses WHERE resolved = 1").fetchall()
            _terminal_cache = frozenset(r["name"] for r in rows) or fallback
        except Exception:  # noqa: BLE001
            return fallback        # ошибку не кэшируем
    return _terminal_cache


def is_terminal(status: str | None) -> bool:
    return status is not None and status in terminal_statuses()

def _db_set_if_not_fixed(status: str | None) -> bool:
    return status is not None and status not in resolved_statuses()

def fixed_issues(project: str | None = None,
                 latest_only: bool = False) -> list[dict]:
    """Issues awaiting merge, oldest first (merge in commit order)."""

    q = ("SELECT i.id, i.runid, i.chunk_id, i.local_id, i.section, "
         "       i.title, i.fix_branch, i.fix_commit, r.project "
         "FROM issue i JOIN report r ON r.id = i.report_id "
         "WHERE i.status = ? AND i.fix_branch IS NOT NULL ")
    params: list = [STATUS_FIXED]
    if project:
        q += "AND r.project = ? "
        params.append(project)
    if latest_only:
        q += ("AND r.id = (SELECT MAX(id) FROM report "
              "WHERE project = r.project) ")
    q += "ORDER BY r.project, i.runid, i.chunk_id, i.local_id"
    with connect() as con:
        return [dict(r) for r in con.execute(q, params)]


def mark_merged(issue_id: int, merge_commit: str) -> bool:
    """fixed -> merged. Guarded by status so re-runs are idempotent."""

    import db_status
    if not db_status.can_transition(STATUS_FIXED, db_status.ACT_MERGE,
                                    STATUS_MERGED):
        print("  warn: merge transition disabled in transition_status",
              file=sys.stderr)
        return False
    now = _now_iso()
    with connect() as con:
        cur = con.execute(
            "UPDATE issue SET status = ?, merge_commit = ?, merged_dt = ?, "
            " fix_branch = NULL,  dt = ? WHERE id = ? AND status = ?",
            (STATUS_MERGED, merge_commit, now, now, issue_id, STATUS_FIXED))
    return cur.rowcount > 0

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

def validate_params() -> list[str]:
    """Validate parameter constraints. Returns list of errors (empty = ok).
    
    Critical errors (tag_context_chars > max_chars) should abort the run;
    others are warnings. Caller decides severity.
    """
    errors: list[str] = []
    critical: list[str] = []
    
    try:
        with connect() as con:
            rows = con.execute(
                "SELECT module, name, value FROM params").fetchall()
            params = {(r["module"], r["name"]): r["value"] for r in rows}
    except Exception as exc:  # noqa: BLE001
        return [f"cannot read params: {exc}"]
    
    def get(module: str, name: str, cast=str):
        v = params.get((module, name))
        if v is None:
            return None
        try:
            return cast(v)
        except (ValueError, TypeError):
            return None
    
    # ── Critical: tags must fit in chunk budget ──
    tag = get("check", "tag_context_chars", int)
    max_c = get("check", "max_chars", int)
    if tag is not None and max_c is not None and tag > max_c:
        critical.append(
            f"check.tag_context_chars ({tag:,}) > check.max_chars ({max_c:,}); "
            f"tags will not fit in chunks. "
            f"Fix: raise check.max_chars or lower check.tag_context_chars")
    
    # ── Warnings ──
    # carry_similarity ∈ [0, 1]
    sim = get("check", "carry_similarity", float)
    if sim is not None and not (0.0 <= sim <= 1.0):
        errors.append(f"check.carry_similarity ({sim}) must be in [0, 1]")
    
    # url starts with http:// or https://
    url = get("llm", "url", str)
    if url and not url.startswith(("http://", "https://")):
        errors.append(f"llm.url ({url!r}) must start with http:// or https://")
    
    # temperature ∈ [0, 2]
    for module in ("check", "fix"):
        temp = get(module, "temperature", float)
        if temp is not None and not (0.0 <= temp <= 2.0):
            errors.append(f"{module}.temperature ({temp}) should be in [0, 2]")
    
    # max_source_chars > 0
    src = get("fix", "max_source_chars", int)
    if src is not None and src <= 0:
        errors.append(f"fix.max_source_chars ({src}) must be > 0")
    
    # max_diff_lines > 0
    diff = get("fix", "max_diff_lines", int)
    if diff is not None and diff <= 0:
        errors.append(f"fix.max_diff_lines ({diff}) must be > 0")
    
    # default_limit >= 0
    lim = get("fix", "default_limit", int)
    if lim is not None and lim < 0:
        errors.append(f"fix.default_limit ({lim}) must be >= 0")
    
    return critical + errors, critical

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

def summary_by_project_status(latest_only: bool = True) -> dict[str, dict[str, int]]:
    """Returns {project: {status: count}} for the latest report per project."""
    where = ("WHERE r.id = (SELECT MAX(id) FROM report WHERE project = r.project) "
             if latest_only else "")
    with connect() as con:
        rows = con.execute(
            "SELECT r.project, i.status, COUNT(*) AS n "
            "FROM issue i JOIN report r ON r.id = i.report_id "
            f"{where}"
            "GROUP BY r.project, i.status "
            "ORDER BY r.project, i.status").fetchall()
    out: dict[str, dict[str, int]] = {}
    for r in rows:
        out.setdefault(r["project"], {})[r["status"]] = r["n"]
    return out


def project_aggregate_status(counts: dict[str, int]) -> str:
    """Single status for a project based on its issue counts.

    Priority (highest first):
      needs-review   — any issue needs a human (analyze-by-dev, failed)
      in-progress    — has proposed-to-fix or raw 'added'
      solved         — only terminal (fixed/rejected/stale/no-action)
      empty          — no issues at all
    """
    if not counts:
        return "empty"
    attention = counts.get(STATUS_ANALYZE, 0) + counts.get(STATUS_FAILED, 0)
    active    = counts.get(STATUS_ADDED, 0) + counts.get(STATUS_PROPOSED, 0)
    awaiting  = counts.get(STATUS_FIXED, 0)
    term      = terminal_statuses()
    terminal = sum(n for s, n in counts.items() if s in term)

    if attention:
        return "needs-review"
    if active:
        return "in-progress"
    if awaiting:  
        return "ready-to-merge"
    if terminal:
        return "solved"
    return "empty"

def finding_full_key(finding: dict, runid: int) -> str:
    """'<runid>/<chunk>/<local>' — globally unique across runs."""
    chunk, local = issue_key(finding)
    return f"{runid}/{chunk}/{local}"

def list_issues(project: str | None = None,
                status: str | None = None,
                latest_only: bool = True) -> list[dict]:
    """Return issue rows as plain dicts, newest report first.

    latest_only=True (default) restricts to the most recent report
    per project, so superseded ('stale') runs don't drown the view.
    Each dict has an extra 'key' = '<chunk_id>/<local_id>'.
    """
    q = ("SELECT r.project, r.id AS report_id, i.id, "
         "r.started AS report_started, i.runid AS check_runid, "
         "i.chunk_id, i.local_id, i.section, i.finding_id, i.status, "
         "i.title, i.text, i.fix_branch, i.fix_commit, i.fix_subject, i.dt, "
         "i.carried_from, i.carry_score, "
         "rm.backend AS remote_backend, rm.remote_id, rm.remote_url, "
         "rm.dry AS remote_dry, "
         "(SELECT s.runid || '/' || s.chunk_id || '/' || s.local_id "
         " FROM issue s WHERE s.id = i.carried_from) AS carried_key "
         "FROM issue i JOIN report r ON r.id = i.report_id "
         "LEFT JOIN issue_remote rm ON rm.issue_id = i.id "
         "  AND rm.rowid = (SELECT r2.rowid FROM issue_remote r2 "
         "                  WHERE r2.issue_id = i.id "
         "                  ORDER BY r2.dry ASC, r2.rowid DESC LIMIT 1) "
         "WHERE 1=1 ")
    params: list = []

    if project:
        q += "AND r.project = ? "
        params.append(project)
    if status:
        q += "AND i.status = ? "
        params.append(status)
    if latest_only:
        q += ("AND r.id = (SELECT MAX(id) FROM report "
              "WHERE project = r.project) ")
        
    q += "ORDER BY r.id DESC, i.chunk_id, i.local_id"
    with connect() as con:
        rows = con.execute(q, params).fetchall()
    out = []
    for r in rows:
        d = dict(r)
        d["chunk_key"] = f"{r['chunk_id']}/{r['local_id']}"
        d["key"] = f"{r['check_runid']}/{r['chunk_id']}/{r['local_id']}"
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

_CODE_SPAN_RE = re.compile(r"`([^`]+)`")
_IDENT_RE = re.compile(r"[A-Za-z_]\w*")


def _carry_set() -> set[str]:
    raw = get_param("check", "carry_statuses", "rejected,no-action")
    s = {x.strip() for x in raw.split(",") if x.strip()}
    return s | {STATUS_SUPPRESSED} if s else set()


def _norm(text: str) -> str:
    return re.sub(r"\s+", " ", re.sub(r"[`*_#>|]", "", text.lower())).strip()


def carry_score(a: str, b: str) -> float:
    """0..1 similarity: half token-Jaccard on code anchors, half text ratio.

    Anchors = file refs + identifiers inside `backticked` spans. Prose
    wording drifts between runs; code anchors do not.
    """
    def anchors(t):
        files = set(FILE_REF_RE.findall(t))
        ids = {m for span in _CODE_SPAN_RE.findall(t)
               for m in _IDENT_RE.findall(span)}
        return files | ids
    ta, tb = anchors(a), anchors(b)
    jac = len(ta & tb) / len(ta | tb) if (ta | tb) else 0.0
    seq = difflib.SequenceMatcher(None, _norm(a), _norm(b)).ratio()
    return 0.5 * jac + 0.5 * seq


def standing_decisions(project: str, limit: int = 20) -> list[dict]:
    """Currently-standing rejections, newest per decision-root, for the hint."""
    carry = _carry_set()
    if not carry:
        return []
    with connect() as con:
        rows = con.execute(
            "SELECT i.title, i.status, "
            "  COALESCE(i.carried_from, i.id) AS root "
            "FROM issue i JOIN report r ON r.id = i.report_id "
            "WHERE r.project = ? ORDER BY i.id DESC", (project,)).fetchall()
    seen: set[int] = set()
    out: list[dict] = []
    for r in rows:
        if r["root"] in seen:
            continue
        seen.add(r["root"])
        if r["status"] in carry:
            out.append({"title": r["title"]})
            if len(out) >= limit:
                break
    return out

def carry_decisions(project: str, report_path: Path | str) -> list[dict]:
    """Suppress fresh 'added' issues matching a standing decision.
    
    Returns list of suppressed issues with keys:
      {"key": "runid/chunk/local", "title": "...", "carried_from": "...", "score": 0.85}
    """
    carry = _carry_set()
    if not carry:
        return []
    min_score = get_param("check", "carry_similarity", 0.80)
    rel = _rel(report_path)
    now = _now_iso()
    suppressed: list[dict] = []
    
    with connect() as con:
        r = con.execute("SELECT id, run_id FROM report WHERE path = ?",
                        (rel,)).fetchone()
        if not r:
            return []
        rid, runid = r["id"], r["run_id"]

        import db_status
        if not db_status.can_transition_con(
                con, STATUS_ADDED, db_status.ACT_CARRY, STATUS_SUPPRESSED):
            return [] 
        
        fresh = con.execute(
            "SELECT id, chunk_id, local_id, title, text FROM issue "
            "WHERE report_id = ? AND status = 'added'", (rid,)).fetchall()
        history = con.execute(
            "SELECT i.id, i.runid, i.chunk_id, i.local_id, i.text, i.status "
            "FROM issue i JOIN report r ON r.id = i.report_id "
            "WHERE r.project = ? AND i.report_id != ? "
            "ORDER BY i.id DESC", (project, rid)).fetchall()
        
        for f in fresh:
            for o in history:
                s = carry_score(f["text"], o["text"])
                if s < min_score:
                    continue
                if o["status"] in carry:
                    con.execute(
                        "UPDATE issue SET status = ?, carried_from = ?, "
                        "  carry_score = ?, dt = ?, "
                        "  reason = COALESCE(reason, ?) WHERE id = ?",
                        (STATUS_SUPPRESSED, o["id"], round(s, 3), now,
                         f"carried from run {o['runid']} (score {s:.2f})",
                         f["id"]))
                    suppressed.append({
                        "key": f"{runid}/{f['chunk_id']}/{f['local_id']}",
                        "title": f["title"],
                        "carried_from": f"{o['runid']}/{o['chunk_id']}/{o['local_id']}",
                        "score": s,
                    })
                    break
    return suppressed

def proposed_finding_keys(report_path: Path | str) -> set[tuple[int, int]]:
    """Keys of findings with status='proposed-to-fix' in this report."""
    rel = _rel(report_path)
    with connect() as con:
        rows = con.execute(
            "SELECT chunk_id, local_id FROM issue "
            "WHERE report_id = (SELECT id FROM report WHERE path = ?) "
            "AND status = ?", (rel, STATUS_PROPOSED)).fetchall()
    return {(r["chunk_id"], r["local_id"]) for r in rows}

def llmrun_set_model(runid: int, name: str) -> bool:
    with connect() as con:
        cur = con.execute("UPDATE llmruns SET name = ? WHERE runid = ?",
                          (name, runid))
    return cur.rowcount > 0

def report_base(report_path) -> dict | None:
    """base_sha + baseline_branch + run_id + project for a report."""
    with connect() as con:
        r = con.execute(
            "SELECT project, run_id, base_sha, baseline_branch "
            "FROM report WHERE path = ?", (_rel(report_path),)).fetchone()
    return dict(r) if r else None


def open_baselines() -> list[dict]:
    """Baselines still needed: some actor can act on at least one issue.
    Eligibility comes from transition_status, not a hardcoded list."""
    with connect() as con:
        rows = con.execute(
            "SELECT DISTINCT r.project, r.run_id, r.baseline_branch "
            "FROM report r JOIN issue i ON i.report_id = r.id "
            "WHERE r.baseline_branch IS NOT NULL AND i.status IN "
            "  (SELECT status FROM transition_status "
            "   WHERE actor IN ('classify','commit','propose','merge') "
            "     AND allowed = 'y')").fetchall()
    return [dict(r) for r in rows]

def all_baselines() -> list[dict]:
    """All reports that still have a pinned baseline branch."""
    with connect() as con:
        rows = con.execute(
            "SELECT DISTINCT project, run_id, baseline_branch "
            "FROM report "
            "WHERE baseline_branch IS NOT NULL AND run_id IS NOT NULL"
        ).fetchall()
    return [dict(r) for r in rows]

def update_report_base(report_path, sha: str) -> None:
    """Advance report.base_sha to `sha` (used on --refetch / --propose)."""
    rel = _rel(report_path)
    now = _now_iso()
    _, branch = current_commit()
    with connect() as con:
        con.execute(
            "INSERT OR IGNORE INTO base_commit "
            "(sha, short_sha, branch, first_dt) VALUES (?, ?, ?, ?)",
            (sha, sha[:7], branch or "HEAD", now))
        con.execute(
            "UPDATE report SET base_sha = ? WHERE path = ?",
            (sha, rel))

def config_snapshot() -> dict[str, list[dict]]:
    """DB-owned configuration for ai-config-save. descr/default_value
    come along for readability only — restore never applies them."""
    import db_status
    with connect() as con:
        statuses = [dict(r) for r in con.execute(
            "SELECT name, descr, terminal, resolved "
            "FROM statuses ORDER BY rowid")]
        params = [dict(r) for r in con.execute(
            "SELECT module, name, value, default_value, dt "
            "FROM params ORDER BY module, name")]
    return {"statuses": statuses, "params": params,
            "transition_status": db_status.all_transitions()}


def restore_config_sql(text: str) -> None:
    """Apply a dump as ONE transaction: a half-applied config is worse
    than none. executescript() commits any pending txn first, so the
    explicit BEGIN/COMMIT is what makes it atomic; on error connect()
    rolls the whole thing back.

    The file is executed as SQL — same trust level as config/check.sql.
    That is deliberate: a developer may hand-edit it."""

    with connect() as con:
        con.executescript("BEGIN;\n" + text + "\nCOMMIT;")

def mark_pushed(issue_id: int, backend: str, *, remote_id: str = "",
                remote_url: str = "", remote_status: str = "",
                dry: bool = False) -> bool:
    with connect() as con:
        con.execute(
            "INSERT INTO issue_remote (issue_id, backend, remote_id, "
            "  remote_url, remote_status, dry, dt) VALUES (?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(issue_id, backend) DO UPDATE SET "
            "  remote_id = excluded.remote_id, remote_url = excluded.remote_url, "
            "  remote_status = excluded.remote_status, dry = excluded.dry, "
            "  dt = excluded.dt",
            (issue_id, backend, remote_id, remote_url, remote_status,
             int(bool(dry)), _now_iso()))
    return True


def remote_links(project: str, backend: str) -> dict[int, dict]:
    """issue_id -> link row, for idempotency of repeated pushes."""
    with connect() as con:
        rows = con.execute(
            "SELECT rm.* FROM issue_remote rm "
            "JOIN issue i ON i.id = rm.issue_id "
            "JOIN report r ON r.id = i.report_id "
            "WHERE r.project = ? AND rm.backend = ?",
            (project, backend)).fetchall()
    return {r["issue_id"]: dict(r) for r in rows}
