"""Status state machine: actors + legal transitions.

DDL lives in config/check.sql (actors, transition_status). This module
owns the reference DATA and the queries deriving eligibility / moves.
No CREATE TABLE here. `actor` = a concrete operation, not a command
alias. ai-check is the ORIGIN (inserts 'added'), so it has no actor.
"""
from __future__ import annotations

import sqlite3
import sys

import db

ACT_CLASSIFY = "classify"
ACT_COMMIT   = "commit"
ACT_PROPOSE  = "propose"
ACT_MERGE    = "merge"
ACT_REJECT   = "reject"
ACT_CARRY    = "carry"
ACT_STALE    = "stale"
ACT_DECIDE   = "decide"

# name -> descr (какой python-метод / шаг это делает)
ACTORS: dict[str, str] = {
    ACT_CLASSIFY: "fix.py _classify_with_llm: dry-run классификация находки",
    ACT_COMMIT:   "fix.py _apply_or_report --apply: коммит кэшированного патча",
    ACT_PROPOSE:  "fix.py --propose: форс-патч поверх отказа классификатора",
    ACT_MERGE:    "db.mark_merged / ai-merge: влитие fixed-ветки",
    ACT_REJECT:   "decide.py / ai-reject: человек отклоняет находку",
    ACT_CARRY:    "db.carry_decisions: подавление повтора прошлого решения",
    ACT_STALE:    "db.close_period: вытеснение более свежим отчётом",
    ACT_DECIDE:   "decide by developer"
}

# (source, actor, target, descr) — descr поясняет суть перехода человеку
TRANSITIONS: list[tuple[str, str, str, str]] = [
    # classify (dry-run)
    (db.STATUS_ADDED,    ACT_CLASSIFY, db.STATUS_PROPOSED,  "validated local patch cached"),
    (db.STATUS_ADDED,    ACT_CLASSIFY, db.STATUS_ANALYZE,   "real defect, non-local fix"),
    (db.STATUS_ADDED,    ACT_CLASSIFY, db.STATUS_NO_ACTION, "verified not a defect"),
    (db.STATUS_ADDED,    ACT_CLASSIFY, db.STATUS_FAILED,    "LLM/validation error"),
    (db.STATUS_PROPOSED, ACT_CLASSIFY, db.STATUS_PROPOSED,  "re-validated cached proposal"),
    (db.STATUS_PROPOSED, ACT_CLASSIFY, db.STATUS_ANALYZE,   "refetch -> non-local"),
    (db.STATUS_PROPOSED, ACT_CLASSIFY, db.STATUS_NO_ACTION, "refetch -> not a defect"),
    (db.STATUS_PROPOSED, ACT_CLASSIFY, db.STATUS_FAILED,    "cached proposal went stale"),
    # commit (apply)
    (db.STATUS_PROPOSED, ACT_COMMIT, db.STATUS_FIXED,  "successful commit of a proposal"),
    (db.STATUS_PROPOSED, ACT_COMMIT, db.STATUS_FAILED, "commit failed"),
    # propose (force over refusal) — FIXED достигается шагом commit, не тут
    (db.STATUS_ANALYZE,   ACT_PROPOSE, db.STATUS_PROPOSED, "forced patch for a non-local finding"),
    (db.STATUS_ANALYZE,   ACT_PROPOSE, db.STATUS_FAILED,   "forced proposal failed validation"),
    (db.STATUS_NO_ACTION, ACT_PROPOSE, db.STATUS_PROPOSED, "forced patch for a not-a-defect finding"),
    (db.STATUS_NO_ACTION, ACT_PROPOSE, db.STATUS_FAILED,   "forced proposal failed validation"),
    (db.STATUS_FAILED,    ACT_PROPOSE, db.STATUS_PROPOSED, "forced failed to reanalyze"),
    # merge
    (db.STATUS_FIXED, ACT_MERGE, db.STATUS_MERGED, "patch merged into base"),
    # reject (origin-agnostic)
    (db.STATUS_ADDED,     ACT_REJECT, db.STATUS_REJECTED, "human rejected"),
    (db.STATUS_PROPOSED,  ACT_REJECT, db.STATUS_REJECTED, "human rejected the proposal"),
    (db.STATUS_ANALYZE,   ACT_REJECT, db.STATUS_REJECTED, "human rejected"),
    (db.STATUS_FIXED,     ACT_REJECT, db.STATUS_REJECTED, "human rejected the committed patch"),
    (db.STATUS_NO_ACTION, ACT_REJECT, db.STATUS_REJECTED, "human rejected"),
    (db.STATUS_FAILED,    ACT_REJECT, db.STATUS_REJECTED, "human closed a failed finding"),
    # carry / stale
    (db.STATUS_ADDED,    ACT_CARRY, db.STATUS_SUPPRESSED, "matches an earlier rejected/no-action"),
    (db.STATUS_ADDED,    ACT_STALE, db.STATUS_STALE, "superseded by newer report"),
    (db.STATUS_PROPOSED, ACT_STALE, db.STATUS_STALE, "superseded by newer report"),
    # decide
    (db.STATUS_ADDED,     ACT_DECIDE, db.STATUS_NO_ACTION, "human: not a defect"),
    (db.STATUS_PROPOSED,  ACT_DECIDE, db.STATUS_NO_ACTION, "human: not a defect"),
    (db.STATUS_ANALYZE,   ACT_DECIDE, db.STATUS_NO_ACTION, "human: not a defect"),
    (db.STATUS_FAILED,    ACT_DECIDE, db.STATUS_NO_ACTION, "human: not a defect"),
    (db.STATUS_REJECTED,  ACT_DECIDE, db.STATUS_ADDED, "human reopen"),
    (db.STATUS_NO_ACTION, ACT_DECIDE, db.STATUS_ADDED, "human reopen"),
    (db.STATUS_STALE,     ACT_DECIDE, db.STATUS_ADDED, "human reopen"),
    (db.STATUS_SUPPRESSED,ACT_DECIDE, db.STATUS_ADDED, "human reopen"),
    (db.STATUS_FAILED,    ACT_DECIDE, db.STATUS_ADDED, "human reopen"),
]


def seed(con: sqlite3.Connection) -> None:
    """descr — code-owned (refreshed). allowed — DB-owned (never reset)."""
    for name, descr in ACTORS.items():
        con.execute(
            "INSERT INTO actors (name, descr) VALUES (?, ?) "
            "ON CONFLICT(name) DO UPDATE SET descr = excluded.descr",
            (name, descr))
    for src, actor, dst, descr in TRANSITIONS:
        con.execute(
            "INSERT INTO transition_status (status, actor, new_status, descr) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(status, actor, new_status) "
            "DO UPDATE SET descr = excluded.descr",   # allowed не трогаем
            (src, actor, dst, descr))


def fix_actor(args) -> str:
    """Какой actor открывает пайплайн для данного вызова fix.py."""
    if getattr(args, "propose", False):
        return ACT_PROPOSE
    if args.apply:
        return ACT_COMMIT
    return ACT_CLASSIFY


def eligible_statuses(actor: str) -> frozenset[str]:
    with db.connect() as con:
        rows = con.execute(
            "SELECT DISTINCT status FROM transition_status "
            "WHERE actor = ? AND allowed = 'y'", (actor,)).fetchall()
    return frozenset(r["status"] for r in rows)


def report_statuses(report_path) -> dict[tuple[int, int], str]:
    rel = db._rel(report_path)
    with db.connect() as con:
        rows = con.execute(
            "SELECT chunk_id, local_id, status FROM issue "
            "WHERE report_id = (SELECT id FROM report WHERE path = ?)",
            (rel,)).fetchall()
    return {(r["chunk_id"], r["local_id"]): r["status"] for r in rows}

def can_transition_con(con, src: str, actor: str, dst: str) -> bool:
    """Connection-local: для писателей, уже держащих транзакцию.
    Не открывает новое соединение (без вложенности/локов)."""
    
    return con.execute(
        "SELECT 1 FROM transition_status "
        "WHERE status=? AND actor=? AND new_status=? AND allowed='y'",
        (src, actor, dst)).fetchone() is not None


def can_transition(src: str, actor: str, dst: str) -> bool:
    with db.connect() as con:
        return can_transition_con(con, src, actor, dst)


def transition(report_path, finding, actor: str, new_status: str, **kw) -> bool:
    """Валидированный писатель одной находки: get_status -> can_transition
    -> update. Соединения последовательные (flock исключает гонки).
    Для массовых писателей используй гейт / WHERE-подзапрос, не это."""

    cur = db.get_status(report_path, finding) or db.STATUS_ADDED
    if not can_transition(cur, actor, new_status):
        print(f"  warn: illegal transition {cur} -[{actor}]-> {new_status} "
              f"({db.finding_key_str(finding)}); write skipped",
              file=sys.stderr)
        return False
    return db.update_issue_status(report_path, finding, new_status, **kw)

DECIDE_ACTOR = {
    db.STATUS_REJECTED:  ACT_REJECT,
    db.STATUS_NO_ACTION: ACT_DECIDE,
    db.STATUS_ADDED:     ACT_DECIDE,
}

def decide(con, runid: int, chunk: int, local: int,
           new_status: str, reason: str = "") -> tuple[bool, str]:
    """Валидированный UPDATE по ключу на соединении вызывающего.
    Возвращает (ok, message). reason пустой -> не трогаем прежний."""
    actor = DECIDE_ACTOR.get(new_status)
    if actor is None:
        return False, f"no actor for target {new_status!r}"
    row = con.execute(
        "SELECT status FROM issue "
        "WHERE runid=? AND chunk_id=? AND local_id=?",
        (runid, chunk, local)).fetchone()
    if row is None:
        return False, "not found"
    cur = row["status"]
    if not can_transition_con(con, cur, actor, new_status):
        return False, f"illegal transition {cur} -[{actor}]-> {new_status}"
    con.execute(
        "UPDATE issue SET status=?, dt=?, "
        "  reason=COALESCE(NULLIF(?,''), reason) "
        "WHERE runid=? AND chunk_id=? AND local_id=?",
        (new_status, db._now_iso(), reason, runid, chunk, local))
    return True, f"{cur} -> {new_status}"
