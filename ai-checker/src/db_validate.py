"""Configuration validation, kept out of db.py.

One snapshot of the config is loaded once; every validator is a
standalone function registered with a DESCRIPTION of what it checks
and why. validate_params() runs them all and splits the results into
critical (abort the run) and warning (log only).

Return contract (unchanged for db.seed_defaults):
    (all_messages, critical_messages)
"""
from __future__ import annotations

import sqlite3
from dataclasses import dataclass
from typing import Callable

CRITICAL = "critical"
WARNING = "warning"


@dataclass
class Issue:
    severity: str
    message: str


def crit(msg: str) -> Issue:
    return Issue(CRITICAL, msg)


def warn(msg: str) -> Issue:
    return Issue(WARNING, msg)


@dataclass
class Config:
    """Single read-only snapshot; validators never touch the DB."""
    params: dict[tuple[str, str], str]
    trackers: list[dict] | None      # None = table missing/unreadable

    def get(self, module: str, name: str, cast=str):
        v = self.params.get((module, name))
        if v is None:
            return None
        try:
            return cast(v)
        except (ValueError, TypeError):
            return None


@dataclass
class Validator:
    name: str
    description: str
    check: Callable[[Config], list[Issue]]


VALIDATORS: list[Validator] = []


def validator(name: str, description: str):
    def deco(fn: Callable[[Config], list[Issue]]):
        VALIDATORS.append(Validator(name, description, fn))
        return fn
    return deco


# ── validators ──────────────────────────────────────────────────────

@validator("tag_budget",
           "check.tag_context_chars must fit inside check.max_chars, "
           "otherwise the type/tag block cannot be packed into a chunk "
           "and every chunk overflows the prompt budget.")
def _tag_budget(cfg: Config) -> list[Issue]:
    tag = cfg.get("check", "tag_context_chars", int)
    max_c = cfg.get("check", "max_chars", int)
    if tag is not None and max_c is not None and tag > max_c:
        return [crit(
            f"check.tag_context_chars ({tag:,}) > check.max_chars ({max_c:,}); "
            f"tags will not fit in chunks. "
            f"Fix: raise check.max_chars or lower check.tag_context_chars")]
    return []


@validator("carry_similarity",
           "check.carry_similarity is a 0..1 similarity threshold; outside "
           "that range carry_decisions either suppresses everything or nothing.")
def _carry_similarity(cfg: Config) -> list[Issue]:
    sim = cfg.get("check", "carry_similarity", float)
    if sim is not None and not (0.0 <= sim <= 1.0):
        return [warn(f"check.carry_similarity ({sim}) must be in [0, 1]")]
    return []


@validator("llm_url",
           "llm.url must be an http(s) endpoint; a bare host or typo makes "
           "every LLM request fail at connect time.")
def _llm_url(cfg: Config) -> list[Issue]:
    url = cfg.get("llm", "url", str)
    if url and not url.startswith(("http://", "https://")):
        return [warn(f"llm.url ({url!r}) must start with http:// or https://")]
    return []


@validator("temperature",
           "check/fix temperature should stay in [0, 2]; values outside are "
           "rejected by most OpenAI-compatible servers or produce garbage.")
def _temperature(cfg: Config) -> list[Issue]:
    out = []
    for module in ("check", "fix"):
        t = cfg.get(module, "temperature", float)
        if t is not None and not (0.0 <= t <= 2.0):
            out.append(warn(f"{module}.temperature ({t}) should be in [0, 2]"))
    return out


@validator("fix_budgets",
           "fix.max_source_chars / max_diff_lines must be positive and "
           "default_limit non-negative; zero/negative budgets disable the "
           "fixer or make its rails meaningless.")
def _fix_budgets(cfg: Config) -> list[Issue]:
    out = []
    src = cfg.get("fix", "max_source_chars", int)
    if src is not None and src <= 0:
        out.append(warn(f"fix.max_source_chars ({src}) must be > 0"))
    diff = cfg.get("fix", "max_diff_lines", int)
    if diff is not None and diff <= 0:
        out.append(warn(f"fix.max_diff_lines ({diff}) must be > 0"))
    lim = cfg.get("fix", "default_limit", int)
    if lim is not None and lim < 0:
        out.append(warn(f"fix.default_limit ({lim}) must be >= 0"))
    return out


@validator("trackers_single",
           "At most one tracker backend may be enabled; with several enabled "
           "the integration cannot decide where to push tickets.")
def _trackers_single(cfg: Config) -> list[Issue]:
    if cfg.trackers is None:
        return [warn("trackers table unavailable (migration not applied?)")]
    enabled = [r["tracker"] for r in cfg.trackers if r["enabled"]]
    if len(enabled) > 1:
        return [crit(
            f"trackers: {len(enabled)} backends enabled ({', '.join(enabled)}); "
            f"at most one allowed. Disable extras: "
            f"UPDATE trackers SET enabled = 0 WHERE tracker IN (...)")]
    return []


@validator("tracker_mode_backend",
           "When track.mode is dry/live, track.backend must name an existing, "
           "enabled tracker; otherwise push/close silently target nothing.")
def _tracker_mode_backend(cfg: Config) -> list[Issue]:
    mode = cfg.get("track", "mode", str)
    backend = cfg.get("track", "backend", str)
    if mode not in ("dry", "live"):
        return []
    if not backend:
        return [crit(f"track.mode={mode} but track.backend is empty; "
                     f"set track.backend to an enabled tracker name")]
    if cfg.trackers is None:
        return [warn(f"cannot verify track.backend={backend!r}: "
                     f"trackers table unavailable")]
    row = next((r for r in cfg.trackers if r["tracker"] == backend), None)
    if row is None:
        return [crit(f"track.backend={backend!r} not found in trackers table")]
    if not row["enabled"]:
        return [crit(f"track.backend={backend!r} is disabled in trackers table")]
    return []


# ── orchestration ───────────────────────────────────────────────────

def _load() -> Config:
    import db                      # lazy: db imports us at module load
    with db.connect() as con:
        rows = con.execute(
            "SELECT module, name, value FROM params").fetchall()
        params = {(r["module"], r["name"]): r["value"] for r in rows}
        try:
            trackers = [dict(r) for r in con.execute(
                "SELECT tracker, enabled FROM trackers").fetchall()]
        except sqlite3.Error:
            trackers = None
    return Config(params=params, trackers=trackers)


def validate_params() -> tuple[list[str], list[str]]:
    """Run every validator over ONE config snapshot.
    Returns (all_messages, critical_messages)."""
    try:
        cfg = _load()
    except Exception as exc:  # noqa: BLE001
        msg = f"cannot read config: {exc}"
        return [msg], []
    issues: list[Issue] = []
    for v in VALIDATORS:
        issues.extend(v.check(cfg))
    critical = [i.message for i in issues if i.severity == CRITICAL]
    warnings = [i.message for i in issues if i.severity == WARNING]
    return critical + warnings, critical


def describe() -> list[dict]:
    """For ai-config: what each validator checks and why."""
    return [{"name": v.name, "description": v.description}
            for v in VALIDATORS]
