"""AI-assisted code fixer for ngutils — applies only local, self-contained fixes.

Reads a report produced by check.py, asks an LLM to classify each
finding as 'local' (safe in-place fix) or 'needs_review' (requires
human thought), then applies the 'local' ones as ONE GIT COMMIT PER
FIX on a dedicated branch ai-checker-<label>-fix<N>.

Safety rails:
  - dry-run by default; --apply to actually write
  - edits only C/C++ sources/headers and tests
  - refuses fixes that would ripple: signature change, new function,
    struct/union/enum layout change, new #include, edits spanning
    more than one function body, diff > 20 lines
  - per-fix confirmation unless --yes is given
  - same flock(2) as check.py: ai-check and ai-fix are mutually exclusive
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path

import db
import db_status
import emulate
import llm
from common import (
    DEFAULT_LIMIT,
    FIX_DIR,
    FORBIDDEN_ROOT_NAMES,
    MAX_SOURCE_CHARS,
    OUT_ROOT,
    ROOT,
    SRC_EXTS,
    acquire_lock,
    allowed_top_dirs,
    discover_projects,
    gather_context_files,
    iter_findings,
    rel_display,
    release_lock,
    rotate_reports,
    types_digest_path,
)
from gitutil import (
    blob_text,
    commit_one_fix,
    commit_patch,
    ensure_baseline_branch,
    head_sha,
    make_fix_branch_name,
    make_verifier,
    remote_web_url,
)
from report import (
    add_chunk_usage,
    format_header_md,
    measure,
    merge_usage,
    new_usage,
    usage_line,
    volume_str,
)
from sysinfo import system_info

MAX_DIFF_LINES = 20

_LOCAL_RULES_STRICT = """
 At most 2 files. Two only if they are a paired source/header in the same
directory with the same stem: X.c + X.h, X.cpp/.cc/.cxx + X.hpp.
Never 3 files, never two unrelated files, never the same file twice.
 Within each file the edit stays inside ONE function body, or one #include
line, or one macro definition, or one comment attached to a declaration.
 No signature change (parameters, return type, qualifiers); no function,
struct, union, enum or global added or removed; no struct layout or field
type change; no public symbol renamed or removed. Updating the matching
.h does NOT make a signature change local - callers still break.
 No new #include. Fixing a typo in an existing include is fine.
 Correct without touching any other file, and idempotent.
 Total changed lines (added + removed, summed over all entries and
multiplied by count) <= @MAX_DIFF@.
If the finding names several functions or files, or says "all callers",
"everywhere", "across the codebase" -> needs_review. Never emit a partial fix.
"""

_LOCAL_RULES_FREE = """
 PROPOSE mode: locality and size restrictions are LIFTED. There is NO
line-count limit and NO file-count limit here. You may propose edits
spanning several files, touching signatures, adding helpers, or adding
#include lines — whatever a correct fix requires. Do not shrink a fix
to fit a diff budget; do not refuse because a change is large or
non-local. The patch is a proposal for a human to review before merging,
never an automatic merge. Aim for correctness, not brevity.
"""

_CLASS_MODE_STRICT = """Pick exactly one class and answer with JSON only.

CLASSES
local        - a real defect, fixable by a self-contained edit (rules below).
needs_review - a real defect, but the fix ripples: a human must design it.
no_action    - NOT a defect: the premise is factually wrong, the concern is
already handled in the code or docs, or the finding itself
says the code is fine. Verify against the sources first and
name what you checked in "reason". Never use needs_review
for these.
"""

_CLASS_MODE_PROPOSE = """You are in PROPOSE mode. Answer with class "local" and provide "edits"
for EVERY finding. Never answer needs_review or no_action in this mode.
If you believe the code is already correct, still propose the single most
defensible micro-improvement (a defensive check, a safer comparison, a
clarifying guard). The result is a proposal for a human, not a verdict.
"""

CLASSIFY_SYSTEM = """
You are a strict C/C++ fix validator for a small systems library.
You receive ONE code-review finding and the source files it refers to.
@CLASS_MODE@
LOCAL - all rules must hold, otherwise needs_review:
@LOCAL_RULES@
Typical local fixes: missing NULL check, missing free/fclose on an error path,
off-by-one, uninitialised variable, wrong constant or format specifier, typo.
A one-line change to a provably broken format string in a macro or literal IS
local: there is no working behaviour to stay compatible with, so do not reject
it as a "format change".
When in doubt -> needs_review. A wrongly applied fix is worse than a deferred
one.
Use "no_action" when the finding is NOT a defect: the premise is
factually wrong, the concern is already addressed in the code or
docs, or the finding itself says the code is fine. Do NOT use
needs_review for these — needs_review means a real defect whose
fix is non-local. "reason" must state what you verified.

OUTPUT - one JSON object, no prose, no markdown fences:
{
  "class": "local" | "needs_review" | "no_action",
  "reason": "what you verified, <= 200 chars",
  "commit_subject": "local only: English imperative, <= 60 chars, no 'fix:' prefix, no trailing period",
  "direction": "needs_review only: what a human should do",
  "edits": [
    {
      "file": "path relative to repo root, one of the sources provided",
      "old_snippet": "EXACT verbatim copy from the file, 1-3 lines, including ALL whitespace",
      "new_snippet": "replacement, same indentation",
      "explanation": "one line",
      "count": 1
    }
  ]
}

EDITS - present if and only if class == "local":
  - "count" is how many times old_snippet occurs in that file and must match
    exactly (default 1). Use count > 1 only for identical copy-pasted text
    taking the identical replacement. Different replacements needed in the
    same file -> needs_review, not several entries.
  - new_snippet must differ from old_snippet. Change only what the finding
    requires: no reformatting, no new comments, no extra blank lines.
  - Editable: <project>/src/, <project>/include/, <project>/test/ only.
    _types.h, _machine.json and anything under build/ are context, never edit.
"""

def latest_report_for_project(proj: str) -> Path | None:
    d = OUT_ROOT / proj
    if not d.is_dir():
        return None
    reports = sorted(d.glob("*.md"), key=lambda p: p.stat().st_mtime)
    if reports:
        return reports[-1]
    hist = d / "hist"
    if hist.is_dir():
        old = sorted(hist.glob("*.md"), key=lambda p: p.stat().st_mtime)
        if old:
            return old[-1]
    return None


def latest_report_for_file(stem: str) -> Path | None:
    d = OUT_ROOT / "_files"
    if not d.is_dir():
        return None
    reports = sorted(d.glob(f"*_{stem}*.md"))
    return reports[-1] if reports else None

def build_prompt(finding: dict, project: Path, files: list[Path],
                 budget: int = MAX_SOURCE_CHARS) -> str:
    parts = [f"Project: {project.name}",
             f"Section: {finding['section']}",
             "Finding:",
             finding["text"],
             "",
             "Source files:"]

    # Prepend the pre-extracted type digest. This is what check.py
    # showed the reviewing model; the classifier must see it too.
    types_path = types_digest_path(project.name)
    if types_path.is_file():
        try:
            types_text = types_path.read_text(encoding="utf-8")
            block = ("### File: _types.h "
                     "(extracted type declarations from all headers)\n"
                     f"```c\n{types_text}\n```\n")
            parts.append(block)
            budget -= len(block)
        except OSError:
            pass

    for f in files:
        if budget <= 0:
            break
        try:
            text = f.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        # Add line numbers for better context
        # numbered = "\n".join(f"{i+1:4d}: {line}" for i, line in enumerate(text.splitlines()))
        # block = f"### {f.relative_to(ROOT)}\n```\n{numbered}\n```\n"
        block = f"### {f.relative_to(ROOT)}\n```\n{text}\n```\n"
        if len(block) > budget:
            block = block[:budget] + "\n... (truncated)\n```\n"
        parts.append(block)
        budget -= len(block)
    return "\n".join(parts)

# ── Edit validation ─────────────────────────────────────────────────
FUNC_HEADER_RE = re.compile(
    r'^[\t ]*(?:static\s+|inline\s+|extern\s+|const\s+)*'
    r'[A-Za-z_][\w\s*]*?\b([A-Za-z_]\w*)\s*\(([^)]*)\)\s*\{?\s*$')
CTRL_KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof",
                 "do", "else", "case", "goto"}


def extract_function_header(line: str) -> str | None:
    s = line.strip()
    if not s or s.startswith(("#", "//", "/*")):
        return None
    m = FUNC_HEADER_RE.match(s)
    if not m:
        return None
    name = m.group(1)
    if name in CTRL_KEYWORDS:
        return None
    if s.endswith(";"):
        return None
    return f"{name}({m.group(2)})"


def is_forbidden(rel: str) -> bool:
    parts = Path(rel).parts
    if not parts:
        return True
    if parts[0] not in allowed_top_dirs():
        return True
    if Path(rel).name in FORBIDDEN_ROOT_NAMES:
        return True
    return Path(rel).suffix.lower() not in SRC_EXTS


def diff_line_counts(old: str, new: str) -> tuple[int, int]:
    added = removed = 0
    for line in difflib.unified_diff(
            old.splitlines(), new.splitlines(), n=0, lineterm=""):
        if line.startswith("+") and not line.startswith("+++"):
            added += 1
        elif line.startswith("-") and not line.startswith("---"):
            removed += 1
    return added, removed

def _is_paired(a: Path, b: Path) -> bool:
    """X.c + X.h, X.cpp + X.hpp, X.cc + X.hpp, X.cxx + X.hpp — same dir, same stem."""
    if a.parent != b.parent or a.stem != b.stem:
        return False
    pair = {a.suffix.lower(), b.suffix.lower()}
    return pair in ({".c", ".h"}, {".cpp", ".hpp"},
                    {".cc", ".hpp"}, {".cxx", ".hpp"})


def validate_edits_propose(edits: list[dict], project: Path
                           ) -> tuple[list[tuple[Path, dict]] | None, str | None]:
    """Propose mode: NO locality/applicability rails. The human reviews
    the branch before merge, so keep the model's edits as-is. We only
    resolve paths and require the file to exist and old/new non-empty —
    the bare minimum for _apply_edit to run. count, old!=new, signature,
    pairing, diff-size, duplicate-file are NOT checked here."""
    if not edits:
        return None, "no edits"
    parsed: list[tuple[Path, dict]] = []
    for e in edits:
        rel = e.get("file", "")
        old = e.get("old_snippet", "")
        new = e.get("new_snippet", "")
        if not rel or not old or not new:
            return None, "missing file/old_snippet/new_snippet"
        p = (ROOT / rel).resolve()
        try:
            p.relative_to(project.resolve())
        except ValueError:
            return None, f"outside project: {rel}"
        if not p.is_file():
            return None, f"file not found: {rel}"
        
        try:
            src = p.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError) as exc:
            return None, f"cannot read {rel}: {exc}"
        if old not in src:
            return None, (f"{rel}: old_snippet mismatch: "
                          f"{_snippet_mismatch_detail(old, src)}")
        
        parsed.append((p, e))
    return parsed, None

def validate_edits(edits: list[dict], project: Path,
                   max_diff_lines: int = MAX_DIFF_LINES
                   ) -> tuple[list[tuple[Path, dict]] | None, str | None]:
    """Strict validation for the automatic fix path: 1 or 2 edits,
    two only as a paired .c/.h. Propose mode does NOT use this —
    it has validate_edits_propose() with no locality rails.
    """
    if not edits:
        return None, "no edits"
    if len(edits) > 2:
        return None, f"too many files ({len(edits)}, max 2)"
    parsed: list[tuple[Path, dict]] = []
    seen: set[Path] = set()
    for e in edits:
        rel = e.get("file", "")
        old = e.get("old_snippet", "")
        new = e.get("new_snippet", "")
        if not rel or not old or not new:
            return None, "missing file/old_snippet/new_snippet"
        if is_forbidden(rel):
            return None, f"forbidden path: {rel}"
        p = (ROOT / rel).resolve()
        try:
            p.relative_to(project.resolve())
        except ValueError:
            return None, f"outside project: {rel}"
        if not p.is_file():
            return None, f"file not found: {rel}"
        if p in seen:
            return None, f"file appears twice: {rel}"
        seen.add(p)
        try:
            src = p.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError) as exc:
            return None, f"cannot read {rel}: {exc}"
        expected = int(e.get("count", 1))
        if expected < 1:
            return None, f"{rel}: count={expected}, must be >= 1"
        n = src.count(old)
        if n != expected:
            return None, (f"{rel}: old_snippet appears {n} times, "
                          f"expected {expected}")
        if old == new:
            return None, f"{rel}: old_snippet == new_snippet"
        old_h = {h for line in old.splitlines()
                 if (h := extract_function_header(line))}
        new_h = {h for line in new.splitlines()
                 if (h := extract_function_header(line))}
        added = new_h - old_h
        if added:
            return None, f"{rel}: adds function {sorted(added)}"
        removed = old_h - new_h
        if removed:
            return None, (f"{rel}: changes/removes function signature "
                          f"{sorted(removed)}")
        parsed.append((p, e))
    if len(parsed) == 2 and not _is_paired(parsed[0][0], parsed[1][0]):
        return None, "two files must be a paired X.c + X.h (same dir, same stem)"
    weighted = 0
    for _p, e in parsed:
        a, r = diff_line_counts(e["old_snippet"], e["new_snippet"])
        weighted += (a + r) * int(e.get("count", 1))
    if weighted > max_diff_lines:
        return None, (f"diff too large ({weighted} weighted lines "
                      f"> {max_diff_lines})")
    return parsed, None


def make_diff(edits: list[tuple[Path, str, str, int]]) -> str:
    """[(path, old, new, count)]. One diff per entry; 'count' shown
    as a comment after the file header when > 1."""
    parts: list[str] = []
    for path, old, new, count in edits:
        rel = path.relative_to(ROOT)
        header = f"a/{rel}"
        if count > 1:
            header += f"  (replace all {count} occurrence(s))"
        parts.append("\n".join(difflib.unified_diff(
            old.splitlines(), new.splitlines(),
            fromfile=header, tofile=f"b/{rel}", n=2, lineterm="")) + "\n")
    return "".join(parts)

def _ensure_baseline(report_path: Path, project: Path) -> None:
    """Materialize the check report into the DB before any status read."""

    kind = ("file" if report_path.parent.name == "_files"
            else "notest" if report_path.stem.endswith("_notest") else "full")
    try:
        db.ensure_report(report_path, project=project.name, kind=kind)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: cannot adopt report into db: {exc}", file=sys.stderr)
    
# ── Main ─────────────────────────────────────────────────────────────
def _resolve_jobs(args) -> list[tuple[str, Path, Path]]:
    if args.report:
        rp = Path(args.report)
        if not rp.is_absolute():
            rp = (Path.cwd() / rp).resolve()
        if not rp.is_file():
            sys.exit(f"ERROR: report not found: {rp}")
        proj = rp.parent.parent if rp.parent.name == "_files" else rp.parent
        return [(rp.stem, rp, proj)]
    if args.file:
        f = Path(args.file)
        if not f.is_absolute():
            f = (Path.cwd() / f).resolve()
        if not f.is_file():
            sys.exit(f"ERROR: file not found: {f}")
        rp = latest_report_for_file(f.stem)
        if not rp:
            sys.exit(f"ERROR: no report in {OUT_ROOT / '_files'} "
                     f"for '{f.stem}'. Run 'make ai-check-file' first.")
        return [(f"files-{f.stem}", rp, f.parent.parent)]
    projects = discover_projects()
    if args.project:
        projects = [p for p in projects if p.name == args.project]
        if not projects:
            sys.exit(f"ERROR: unknown project '{args.project}'")
    out: list[tuple[str, Path, Path]] = []
    for p in projects:
        rp = latest_report_for_project(p.name)
        if rp:
            out.append((p.name, rp, p))
    if not out:
        sys.exit(f"ERROR: no reports in {OUT_ROOT}. Run 'make ai-check' first.")
    return out

def _build_commit_msg(finding: dict, rel: str, reason: str,
                      explanation: str, *,
                      commit_subject: str = "",
                      project_name: str = "",
                      branch: str = "",
                      local_id: int = 0) -> str:
    """fix(ngutils/<proj>): <english subject> (<branch>/<local_id>).

    When branch and local_id are both set, the reference is appended
    to the subject line — one line, greppable from git log.
    """
    subject = (commit_subject or explanation).splitlines()[0].strip()
    subject = subject.rstrip(" .,;:!?()[]")
    if len(subject) > 60:
        subject = subject[:57].rstrip(" .,;:!?()[]") + "..."
    if not subject:
        subject = "auto-fix"
    scope = f"ngutils/{project_name}" if project_name else "ngutils"

    header = f"fix({scope}): {subject}"
    if branch:
        # Branch name already contains the finding ref (<run_id>/<chunk>-<local>),
        # so no extra /<local_id> — that would duplicate it.
        header += f" ({branch})"

    body = (f"Section: {finding['section']}\n"
            f"Finding: {finding.get('id', '?')}\n"
            f"File:    {rel}\n"
            f"Reason:  {reason}\n"
            f"Change:  {explanation}\n")
    return (f"{header}\n\n"
            f"{body}\n"
            f"Full finding:\n{finding['text']}\n\n"
            f"Applied by ai-checker/fix.py\n")

def _db_set_if_not_fixed(report_path: Path, finding: dict, status: str, *,
                         actor: str, **kw) -> None:
    _db_set(report_path, finding, status, actor=actor, **kw)

def _db_set(report_path: Path, finding: dict, status: str, *,
            actor: str, **kw) -> None:
    """Пишет статус ТОЛЬКО если (cur -[actor]-> status) легален по
    transition_status. нелегальный переход — warning, без записи."""
    try:
        cur = db.get_status(report_path, finding) or db.STATUS_ADDED
    except Exception:  # noqa: BLE001
        cur = db.STATUS_ADDED

    if cur == status:
        # state of machine remains the same
        return

    if not db_status.can_transition(cur, actor, status):
        print(f"  warn: illegal transition {cur} -[{actor}]-> {status} "
              f"({db.finding_key_str(finding)}); write skipped",
              file=sys.stderr)
        return
    try:
        ok = db.update_issue_status(report_path, finding, status, **kw)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: db update failed: {exc}", file=sys.stderr)
        return
    if not ok:
        print(f"  warn: db update matched no issue "
              f"({db.finding_key_str(finding)} in {report_path.name}); "
              f"report not ingested?", file=sys.stderr)
    if status in (db.STATUS_FAILED, db.STATUS_ANALYZE, db.STATUS_NO_ACTION):
        try:
            import tracker_common
            iid = db.issue_id_by_key(report_path, finding)
            if iid is not None:
                tracker_common.comment_status(
                    iid, status, kw.get("reason", ""))
        except Exception as exc:  # noqa: BLE001
            print(f"  warn: tracker comment failed: {exc}",
                  file=sys.stderr)

def _clear_proposal(report_path: Path, finding: dict) -> None:
    """Best-effort proposal cleanup. Currently unused in _handle_finding
    (proposals are kept as history); reserved for admin tools."""
    try:
        db.clear_proposal(report_path, finding)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: proposal clear failed: {exc}", file=sys.stderr)

def _print_finding_header(finding: dict, idx: int, total: int,
                          runid: int) -> str:
    """Return (full_key, chunk_key). full_key = '<runid>/<chunk>/<local>',
    chunk_key = '<chunk>/<local>' — both shown for FIXLIST convenience."""
    try:
        c, l = db.issue_key(finding)
        chunk_key = f"{c}/{l}"
    except (ValueError, TypeError):
        chunk_key = finding.get("id", "?")
        c, l = 0, 0
    full_key = f"{runid}/{chunk_key}"
    print(f"\n=== Finding {full_key}  ({idx}/{total})  "
          f"[{finding['section']}] ===")
    print(f"  {finding['text'][:200].replace(chr(10), ' ')}...")
    return full_key

def _resolve_edits(finding: dict, full_key: str, project: Path,
                   report_path: Path, files: list[Path],
                   args, model: str, ctx: dict,
                   needs_review: list[dict], skipped: list[str]):
    """Return outcome dict, or None if this finding was already
    recorded (skip / failed / needs_review)."""

    if not (args.refetch or args.reprocess or getattr(args, "propose", False)):
        pending = _load_pending(report_path, finding)
        if pending is not None:
            return _validate_cached(pending, finding, full_key,
                                    project, report_path, args, ctx, skipped)
        
    return _classify_with_llm(finding, full_key, project, report_path, files,
                               args, model, ctx, needs_review, skipped)

def _resolve_cached_edits(edits: list[dict], project: Path
                          ) -> tuple[list[tuple[Path, dict]] | None, str | None]:
    """Resolve cached proposal edits to paths. NO rule validation —
    the patch was already validated when proposed (strict rules for
    classify, no rails for propose). Commit trusts the status; we only
    resolve paths and check the file exists so _apply_edit can read it.
    """

    parsed: list[tuple[Path, dict]] = []
    for e in edits:
        rel = e.get("file", "")
        if not rel:
            return None, "missing file"
        p = (ROOT / rel).resolve()
        try:
            p.relative_to(project.resolve())
        except ValueError:
            return None, f"outside project: {rel}"
        if not p.is_file():
            return None, f"file not found: {rel}"
        parsed.append((p, e))
    return parsed, None

def _load_pending(report_path: Path, finding: dict) -> dict | None:
    try:
        return db.load_proposal(report_path, finding)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: proposal load failed: {exc}", file=sys.stderr)
        return None

def _validate_cached(pending: dict, finding: dict, full_key: str,
                     project: Path, report_path: Path,
                     args, ctx: dict, skipped: list[str]):
    """Verify cached proposal is still valid; return outcome or None."""
    actor = db_status.fix_actor(args)        # classify или commit
    if args.apply:
        # 1. File freshness: SHA-256 against what propose saw
        for e in pending["edits"]:
            msg, reason = _check_file_freshness(e, pending["dt"], full_key)
            if msg is not None:
                print(msg)
                skipped.append(f"{full_key}: {reason}")
                _db_set_if_not_fixed(report_path, finding, db.STATUS_FAILED,
                                     actor=actor, reason=reason)
                return None
        
        # 2. NEW: snippet presence at baseline (catches whitespace
        #    mismatches the freshness check cannot see — propose reads
        #    working tree, commit applies against ctx["base_sha"])
        for e in pending["edits"]:
            p = (ROOT / e["file"]).resolve()
            blob = blob_text(ctx["base_sha"], p)
            if blob is None:
                reason = (f"stale: cannot read {e['file']} at baseline "
                          f"{ctx['base_sha'][:8]}")
                print(f"  STALE proposal ({reason})")
                skipped.append(f"{full_key}: {reason}")
                _db_set_if_not_fixed(report_path, finding, db.STATUS_FAILED,
                                     actor=actor, reason=reason)
                return None
            if e["old_snippet"] not in blob:
                reason = (f"stale: snippet mismatch at baseline "
                          f"{ctx['base_sha'][:8]}: "
                          f"{_snippet_mismatch_detail(e['old_snippet'], blob)}")
                print(f"  STALE proposal ({reason})")
                print(f"  rerun 'make ai-fix-propose-{project.name} "
                      f"FIXLIST=\"{full_key}\" AI_FIX_REFETCH=1'")
                skipped.append(f"{full_key}: {reason}")
                _db_set_if_not_fixed(report_path, finding, db.STATUS_FAILED,
                                     actor=actor, reason=reason)
                return None
    
    parsed, err = _resolve_cached_edits(pending["edits"], 
                                        project)
    
    if err:
        print(f"  STALE proposal ({err})")
        print(f"  rerun 'make ai-fix-log AI_FIX_REFETCH=1 "
              f"FIXLIST=\"{full_key}\"' to reclassify")
        skipped.append(f"{full_key}: stale: {err}")
        _db_set_if_not_fixed(report_path, finding, db.STATUS_FAILED,
                             actor=actor, reason=f"stale: {err}")
        return None

    explanation = "; ".join(
        e.get("explanation", "") for _, e in parsed
        if e.get("explanation"))
    rels = [str(p.relative_to(ROOT)) for p, _ in parsed]
    commit_subject = pending.get("commit_subject", "")
    commit_msg = _build_commit_msg(
        finding, ", ".join(rels), pending["reason"], explanation,
        commit_subject=commit_subject,
        project_name=project.name)
    return {
        "edits": parsed,
        "reason": pending["reason"],
        "explanation": explanation,
        "commit_msg": commit_msg,
        "commit_subject": commit_subject,
        "source": "cached",
        "dt": pending["dt"],
    }

def _check_file_freshness(e: dict, dt: str, key: str):
    """Return (message, reason_short) if file changed, else (None, None)."""
    path = ROOT / e["file"]
    try:
        cur = hashlib.sha256(
            path.read_text(encoding="utf-8").encode()).hexdigest()
    except OSError as exc:
        return (f"  STALE proposal: cannot read {e['file']}: {exc}",
                f"stale: {exc}")
    if cur != e.get("file_sha256"):
        return ((f"  STALE proposal: {e['file']} changed since "
                f"dry-run ({dt})\n"
                f"  rerun 'make ai-fix-log AI_FIX_REFETCH=1 "
                f"FIXLIST=\"{key}\"' to reclassify"),
                "stale: file changed")
    return None, None

def _snippet_mismatch_detail(old: str, blob: str) -> str:
    """First divergence between old_snippet and file content:
    line no, column no, expected vs closest actual line, both in
    repr() so tabs/spaces are visible."""
    blob_lines = blob.splitlines()
    for i, ln in enumerate(old.splitlines()):
        if ln in blob_lines:
            continue
        close = difflib.get_close_matches(ln, blob_lines, n=1, cutoff=0.5)
        if not close:
            return (f"line {i+1} not found, nothing similar; "
                    f"expected {ln!r}")
        got = close[0]
        pos = next((k for k in range(min(len(ln), len(got)))
                    if ln[k] != got[k]), min(len(ln), len(got)))
        return (f"line {i+1}, column {pos+1}: expected "
                f"{ln[pos:pos+16]!r}... vs file {got[pos:pos+16]!r}...; "
                f"full expected {ln!r} / full file {got!r}")
    return ("all lines exist individually but not consecutively "
            "(order or interleaved lines differ)")

def _classify_with_llm(finding, full_key, project, report_path, files,
                       args, model, ctx, needs_review, skipped):
    actor = (db_status.ACT_PROPOSE if ctx.get("propose")
             else db_status.ACT_CLASSIFY)
    verdict, err = _ask_classifier(finding, project, files, args, model, ctx)
    if err is not None:
        print(f"  SKIP: {err}")
        skipped.append(f"{full_key}: {err}")
        _db_set(report_path, finding, db.STATUS_FAILED, actor=actor, reason=err)
        return None
    cls = verdict.get("class", "needs_review")
    reason = verdict.get("reason", "")
    # propose: модель обязана дать local; отказалась — статус НЕ трогаем
    if ctx.get("propose") and cls != "local":
        print(f"  PROPOSE: model refused (class={cls}): {reason}")
        skipped.append(f"{full_key}: propose refused (class={cls})")
        return None
    if cls == "no_action":
        print(f"  NO ACTION: {reason}")
        _db_set(report_path, finding, db.STATUS_NO_ACTION, actor=actor, reason=reason)
        return None
    if cls != "local":
        print(f"  NEEDS REVIEW: {reason}")
        if verdict.get("direction"):
            print(f"    direction: {verdict['direction']}")
        needs_review.append(finding | {"verdict": verdict})
        _db_set(report_path, finding, db.STATUS_ANALYZE, actor=actor,
                reason=reason, direction=verdict.get("direction"))
        return None
    raw_edits = verdict.get("edits")
    if not raw_edits:
        single = verdict.get("edit")
        raw_edits = [single] if single else []

    if ctx.get("propose"):
        parsed, verr = validate_edits_propose(raw_edits, project)
    else:
        parsed, verr = validate_edits(raw_edits, project,
                                      max_diff_lines=ctx["max_diff_lines"])
        
    if verr:
        print(f"  SKIP (invalid edit): {verr}")
        skipped.append(f"{full_key}: {verr}")
        _db_set(report_path, finding, db.STATUS_FAILED, actor=actor,
                reason=f"invalid edit: {verr}")
        return None

    if ctx.get("propose") and len(parsed) > 2:
        print(f"  ⚠ propose: {len(parsed)} files — non-local patch; "
              f"verify build and callers before merge")
    
    explanation = "; ".join(e.get("explanation", "") for _, e in parsed
                            if e.get("explanation"))
    commit_subject = verdict.get("commit_subject", "")
    rels = [str(p.relative_to(ROOT)) for p, _ in parsed]
    commit_msg = _build_commit_msg(finding, ", ".join(rels), reason, explanation,
                                   commit_subject=commit_subject,
                                   project_name=project.name)
    _save_proposal(report_path, finding, parsed, commit_msg, reason,
                   runid=ctx["check_runid"], fix_runid=ctx["fix_runid"],
                   commit_subject=commit_subject)
    _db_set(report_path, finding, db.STATUS_PROPOSED, actor=actor, reason=reason)
    return {"edits": parsed, "reason": reason, "explanation": explanation,
            "commit_msg": commit_msg, "commit_subject": commit_subject,
            "source": "llm", "dt": None}

def _ask_classifier(finding: dict, project: Path, files: list[Path],
                    args, model: str, ctx: dict):
    """Return (verdict_dict, None) or (None, error_string)."""

    if emulate.enabled():
        try:
            return emulate.classifier_verdict(project.name,
                                              finding["id"]), None
        except emulate.MissingFixture as exc:
            return None, str(exc)

    chunk_usage: dict = {}

    system = classify_system(ctx)
    user_content = build_prompt(finding, project, files, budget=ctx["max_source"])
    c, l = measure(system, user_content)
    ctx["usage"]["chars"] += c
    ctx["usage"]["lines"] += l
    messages = [{"role": "system", "content": system},
                {"role": "user",   "content": user_content}]
    
    try:
        raw = llm.chat_once(args.base_url, model, messages,
                            temperature=ctx["fix_temp"],
                            usage_out=chunk_usage)
    except llm.RequestException as exc:
        return None, f"LLM: {exc}"

    for k in ("prompt_tokens", "completion_tokens", "total_tokens"):
        # ctx["usage"][k] += chunk_usage.get(k, 0)
        add_chunk_usage(ctx["usage"], chunk_usage)  
    ctx["usage"]["requests"] += 1

    raw = raw.strip()
    if raw.startswith("```"):
        raw = re.sub(r"^```[a-z]*\n?", "", raw)
        raw = re.sub(r"\n?```$", "", raw)
    try:
        return json.loads(raw), None
    except json.JSONDecodeError as exc:
        return None, f"bad JSON: {exc}"

def _save_proposal(report_path: Path, finding: dict,
                   parsed: list[tuple[Path, dict]],
                   commit_msg: str, reason: str,
                   runid: int, fix_runid: int,
                   commit_subject: str = "") -> None:
    proposal_edits = []
    for p, e in parsed:
        try:
            sha256 = hashlib.sha256(
                p.read_text(encoding="utf-8").encode()).hexdigest()
        except OSError:
            sha256 = ""
        proposal_edits.append({
            "file": str(p.relative_to(ROOT)),
            "old_snippet": e["old_snippet"],
            "new_snippet": e["new_snippet"],
            "explanation": e.get("explanation", ""),
            "count": int(e.get("count", 1)),
            "file_sha256": sha256,
        })
    try:
        db.save_proposal(report_path, finding,
                         edits=proposal_edits,
                         commit_msg=commit_msg, reason=reason,
                         runid=runid, fix_runid=fix_runid,
                         commit_subject=commit_subject)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: proposal save failed: {exc}", file=sys.stderr)

def _apply_or_report(outcome: dict, finding: dict, full_key: str,
                     project: Path, report_path: Path,
                     args, ctx: dict,
                     applied: list[dict], skipped: list[str]) -> None:
    """Show diff; in --apply mode: confirm, commit, record status.

    This is the ONLY place where dry-run/commit/reject logic lives,
    so cached and LLM outcomes cannot diverge.
    """
    parsed = outcome["edits"]
    reason = outcome["reason"]
    explanation = outcome["explanation"]
    commit_msg = outcome["commit_msg"]

    header = (f"  LOCAL (cached {outcome['dt']}): {explanation}"
              if outcome["source"] == "cached"
              else f"  LOCAL: {explanation}")

    diff = make_diff([
        (p, e["old_snippet"], e["new_snippet"], int(e.get("count", 1)))
        for p, e in parsed
    ])
    print(header)
    for line in diff.splitlines():
        print(f"    {line}")

    if not args.apply:
        if outcome["source"] == "cached":
            print(f"  [dry-run] cached proposal from {outcome['dt']}")
        else:
            print("  [dry-run] not applied (saved as proposal)")
        _notify_tracker_proposal(report_path, finding, full_key,
                                 explanation, diff)
        ctx["ready"] += 1
        ctx["proposed"].append(full_key)
        return

    # Resolve ticket id (if any) for the branch name.
    # Best-effort: if db query fails or no link exists, ticket_id stays None.
    ticket_id = None
    try:
        iid = db.issue_id_by_key(report_path, finding)
        if iid is not None:
            link = db.remote_link(iid)
            if link and not link["dry"] and link["remote_id"]:
                ticket_id = int(link["remote_id"])
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: ticket id lookup failed: {exc}", file=sys.stderr)

    ctx["seq"] += 1
    try:
        chunk_id, local_id = db.issue_key(finding)
    except (ValueError, TypeError):
        chunk_id, local_id = 0, 0
    branch = make_fix_branch_name(
                        project.name, ctx["fix_runid"], ctx["seq"],
                        chunk_id, local_id,
                        check_runid=ctx.get("check_runid", 0),
                        ticket_id=ticket_id)
    
    commit_edits = [(p, e["old_snippet"], e["new_snippet"])
                    for p, e in parsed]
    files = [str(p.relative_to(ROOT)) for p, _ in parsed]

    commit_msg = _build_commit_msg(
        finding, ", ".join(files), reason, explanation,
        commit_subject=outcome.get("commit_subject", ""),
        project_name=project.name,
        branch=branch, local_id=local_id)
    subject_line = commit_msg.split("\n\n", 1)[0]

    verify = (make_verifier(project.name, ctx["verify_targets"],
                            log_dir=FIX_DIR / "logs")
              if ctx.get("verify_on_commit") and ctx.get("verify_targets")
              else None)

    try:
        sha, _ = commit_one_fix(project.name, ctx["check_runid"],
                         chunk_id, local_id,
                         ctx["base_sha"], branch,
                         commit_edits, commit_msg, verify=verify)
    except RuntimeError as exc:
        print(f"  SKIP: {exc}")
        skipped.append(f"{full_key}: {exc}")
        _db_set(report_path, finding, db.STATUS_FAILED,
                actor=db_status.ACT_COMMIT,
                reason=f"commit/verify failed: {str(exc).splitlines()[0]}")
        return

    files = [str(p.relative_to(ROOT)) for p, _ in parsed]
    print(f"  COMMITTED {sha}  {branch}  {', '.join(files)}")
    applied.append({
        "id": finding.get("id", "?"),
        "section": finding["section"],
        "sha": sha,
        "branch": branch,
        "file": ", ".join(files),
        "summary": finding["text"].splitlines()[0][:70],
        "edits": [{
            "file": str(p.relative_to(ROOT)),
            "explanation": e.get("explanation", ""),
            "count": int(e.get("count", 1)),
        } for p, e in parsed],
    })
    _db_set(report_path, 
            finding, 
            db.STATUS_FIXED, 
            actor=db_status.ACT_COMMIT,
            reason=reason, 
            fix_branch=branch, 
            fix_commit=sha,
            fix_subject=subject_line, 
            fix_runid=ctx["fix_runid"])
    _notify_tracker_commit(report_path, 
                           finding, 
                           full_key, 
                           sha, 
                           branch,
                           files, 
                           reason, 
                           explanation, 
                           diff)

def _handle_finding(idx: int, total: int, finding: dict,
                    project: Path, report_path: Path, files: list[Path],
                    args, model: str, ctx: dict,
                    applied: list[dict], skipped: list[str],
                    needs_review: list[dict]) -> None:
    """Classify one finding and either report it or commit a fix.

    Pipeline:
      _print_finding_header  -> display
      _resolve_edits         -> cached proposal or LLM (returns outcome)
      _apply_or_report       -> dry-run print / apply commit / reject
    """

    full_key = _print_finding_header(finding, idx, total, ctx.get("check_runid", 0))
    if not files:
        print("  SKIP: no source files to work with")
        skipped.append(finding.get("id", "?") + ": no files")
        return

    outcome = _resolve_edits(finding, full_key, project, report_path, files,
                             args, model, ctx, needs_review, skipped)
    if outcome is None:
        return

    _apply_or_report(outcome, finding, full_key, project, report_path,
                     args, ctx, applied, skipped)
    

def _write_fix_report(label: str, source_report: Path, started: datetime,
                      duration: float, ctx: dict, args,
                      applied: list[dict], skipped: list[str],
                      needs_review: list[dict], sysinfo: dict) -> Path:
    """Write a fix-run report under build/ai-checker/<label>/fix/."""
    out_dir = OUT_ROOT / label / "fix"
    out_dir.mkdir(parents=True, exist_ok=True)
    ts = started.strftime("%Y-%m-%d_%H-%M-%S")
    suffix = "_commit" if args.apply else ""
    out_path = out_dir / f"{ctx['fix_runid']}_{ts}{suffix}.md"

    header = format_header_md(
        f"AI fix report: {label}",
        model=ctx.get("model", ""),
        base_url=args.base_url,
        started=started,
        duration=duration,
        step_durs=ctx.get("finding_durs", []),
        usage=ctx["usage"],
        sysinfo=sysinfo,
    )

    mode = "apply" if args.apply else "dry-run"
    body: list[str] = [
        f"**Source report:** `{rel_display(source_report)}`\n",
        f"**Mode:** {mode}\n",
        f"**FIXLIST:** `{args.only or '*'}`\n\n",
        "## Summary\n\n",
        f"- applied: {len(applied)}\n",
        f"- ready (validated, not committed): {ctx.get('ready', 0)}\n",
        f"- skipped: {len(skipped)}\n",
        f"- needs review: {len(needs_review)}\n\n",
    ]

    if applied:
        body.append("## Applied\n\n")
        for c in applied:
            body.append(f"- **{c['id']}** → `{c['branch']}` "
                        f"(`{c['sha']}`): {c['summary']}\n")
            for e in c.get("edits", []):
                cnt = e.get("count", 1)
                cnt_s = f" (x{cnt})" if cnt > 1 else ""
                body.append(f"  - `{e['file']}`{cnt_s}: "
                            f"{e.get('explanation', '')}\n")
        body.append("\n")

    if ctx.get("ready", 0) and not args.apply:
        body.append("## Ready (validated, not committed)\n\n")
        body.append("These findings were classified as `local` and "
                    "validated; run `make ai-fix-commit-<proj>` "
                    "(or `AI_FIX_REFETCH=1` to reclassify) to apply.\n\n")

    if needs_review:
        body.append("## Needs human thought\n\n")
        for n in needs_review:
            v = n["verdict"]
            head = n["text"].splitlines()[0][:100]
            body.append(f"### {n.get('id', '?')}: {head}\n\n")
            if v.get("reason"):
                body.append(f"**Why:** {v['reason']}\n\n")
            if v.get("direction"):
                body.append(f"**Direction:** {v['direction']}\n\n")
            body.append(f"<details><summary>Full finding</summary>\n\n"
                        f"{n['text']}\n\n</details>\n\n")

    if skipped:
        body.append("## Skipped\n\n")
        for s in skipped:
            body.append(f"- {s}\n")
        body.append("\n")

    out_path.write_text(header + "".join(body), encoding="utf-8")

    moved = rotate_reports(out_dir)
    if moved:
        print(f"[ai-fixer] rotated {len(moved)} older fix report(s) "
              f"-> {rel_display(out_dir)}/hist/", file=sys.stderr)
    return out_path

def _prepare_run(args) -> dict:
    """One-time setup: base branch, machine snapshot, accumulator state."""
    base_branch = base_sha = None

    FIX_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

    sysinfo = system_info()
    try:
        (OUT_ROOT / "_machine.json").write_text(
            json.dumps(sysinfo, indent=2, ensure_ascii=False),
            encoding="utf-8")
    except OSError as exc:
        print(f"[ai-fixer] warn: cannot write _machine.json: {exc}",
              file=sys.stderr)

    return {
        "base_branch": base_branch,
        "base_sha": base_sha,
        "ts": ts,
        "sysinfo": sysinfo,
        "all_usage": new_usage(),
        "created_all": [],
        "projects_done": 0
    }

def _issue_key_or_none(finding: dict) -> tuple[int, int] | None:
    try:
        return db.issue_key(finding)
    except (ValueError, TypeError):
        return None


def _matches_fixlist(finding: dict, matcher, runid: int) -> bool:
    k = _issue_key_or_none(finding)
    return k is not None and matcher((runid, k[0], k[1]))


def _load_resolved(report_path: Path,
                   reopen: frozenset[str] = frozenset()) -> set[tuple[int, int]]:
    try:
        return db.resolved_finding_keys(report_path, reopen=reopen)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: db query failed: {exc}", file=sys.stderr)
        return set()
    

def _load_proposed(report_path: Path) -> set[tuple[int, int]]:
    try:
        return db.proposed_finding_keys(report_path)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: db query failed: {exc}", file=sys.stderr)
        return set()
    

def _filter_findings(report_path: Path, args) -> list[dict]:
    """FIXLIST (key filter) + --limit + eligibility из transition_status.
    Знание о статусах здесь больше не живёт."""

    findings = iter_findings(report_path, 0)
    
    only = db.fixlist_matcher(args.only, project=args.project or None)
    if only is not None:
        rid = db.report_runid(report_path) or 0
        selected = [f for f in findings if _matches_fixlist(f, only, rid)]
        if not selected:
            print(f"  warn: FIXLIST matched nothing: {args.only!r}",
                    file=sys.stderr)
            sample = [k for f in findings[:5]
                if (k := _issue_key_or_none(f))]
            if sample:
                hint = ", ".join(f"{rid}/{c}/{l}" for c, l in sample)
                print(f"  keys in this report look like: {hint} ...  "
                      f"(forms: R/C/L, R/C/L-L, R/C/*, R/*/*, C/L, C/*, *)",
                      file=sys.stderr)
            print("  no findings match FIXLIST")
            return []
        findings = selected
    elif args.limit:
        findings = findings[:args.limit]
    if not findings:
        print("  no findings to process")
        return []

    actor = db_status.fix_actor(args)
    eligible = db_status.eligible_statuses(actor)
    if getattr(args, "force_propose", False):   # support --force-propose
        eligible = set(eligible) | {db.STATUS_ANALYZE, db.STATUS_NO_ACTION}

    statuses = db_status.report_statuses(report_path)

    kept: list[dict] = []
    skipped: list[str] = []
    for f in findings:
        k = _issue_key_or_none(f)
        st = statuses.get(k, db.STATUS_ADDED) if k else db.STATUS_ADDED
        if st in eligible:
            kept.append(f)
        else:
            skipped.append(st)

    if skipped:
        from collections import Counter
        detail = ", ".join(f"{s}×{n}" for s, n in sorted(Counter(skipped).items()))
        print(f"  skip {len(skipped)} not eligible for '{actor}' ({detail})",
              file=sys.stderr)
    if not kept:
        print(f"  nothing eligible for '{actor}'")
    return kept

def _build_ctx(label: str, args, model: str, state: dict) -> dict:
    """Per-project accumulator + params from DB."""

    db.seed_defaults()
    max_source = db.get_param("fix", "max_source_chars", MAX_SOURCE_CHARS)
    max_diff = db.get_param("fix", "max_diff_lines", MAX_DIFF_LINES)
    fix_temp = db.get_param("fix", "temperature", 0.0)
    verify_targets = [t.strip() for t in db.get_param(
        "fix", "verify_targets",
        "test-{project},analyze-{project}").split(",") if t.strip()]
    verify_on_commit = db.get_param("fix", "verify_on_commit", True)

    return {
        "run_id": state["fix_runid"],
        "fix_runid": state["fix_runid"],
        "check_runid": 0,  # fill in в _process_project
        "base_sha": state["base_sha"],
        "base_branch": state["base_branch"],
        "seq": 0,
        "ready": 0,
        "model": model,
        "usage": new_usage(),
        "finding_durs": [],
        "max_source": max_source,
        "max_diff_lines": max_diff,
        "fix_temp": fix_temp,
        "propose": getattr(args, "propose", False),
        "proposed": [],
        "verify_targets": verify_targets,
        "verify_on_commit": verify_on_commit,
    }

def _process_project(label: str, report_path: Path, project: Path,
                     findings: list[dict], args, model: str,
                     ctx: dict, state: dict) -> None:
    """Iterate findings, then print summary and persist outputs."""

    state["projects_done"] += 1
    merge_usage(state["all_usage"], ctx["usage"])
    applied: list[dict] = []
    skipped: list[str] = []
    needs_review: list[dict] = []

    started = datetime.now()
    t0 = time.perf_counter()

    try:
        report_runid = db.report_runid(report_path)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: cannot resolve report.run_id: {exc}",
              file=sys.stderr)
        report_runid = 0
    ctx["check_runid"] = report_runid or 0

    info = db.report_base(report_path) or {}
    refresh = bool(getattr(args, "refetch", False)
                   or getattr(args, "propose", False))
    if refresh:
        base_sha = head_sha()
        db.update_report_base(report_path, base_sha)
    else:
        base_sha = info.get("base_sha") or head_sha()

    rid = info.get("run_id") or ctx["check_runid"]
    baseline = ensure_baseline_branch(project.name, rid, base_sha, reset=refresh)
    ctx["base_sha"], ctx["base_branch"], ctx["check_runid"] = base_sha, baseline, rid

    for idx, finding in enumerate(findings, 1):
        t_finding = time.perf_counter()
        files = gather_context_files(finding, project)
        _handle_finding(idx, len(findings), finding, project,
                        report_path, files, args, model, ctx,
                        applied, skipped, needs_review)
        ctx["finding_durs"].append(time.perf_counter() - t_finding)
    dt = time.perf_counter() - t0

    merge_usage(state["all_usage"], ctx["usage"])

    report_out = _write_fix_report(label, report_path, started, dt,
                                    ctx, args, applied, skipped,
                                    needs_review, state["sysinfo"])
    _print_project_summary(label, ctx, applied, skipped, needs_review,
                           dt, report_out)

    if not args.apply and ctx.get("proposed"):
        _print_dry_run_hint(label, ctx)

    if applied:
        _print_applied_branches(applied, ctx["base_branch"], ctx["run_id"])
        log_path = _write_json_log(label, state["ts"], ctx, dt,
                                   applied, skipped, needs_review)
        print(f"[ai-fixer] json log:    {rel_display(log_path)}")
        state["created_all"].append((label, ctx["run_id"], applied))

    if needs_review:
        _print_needs_review(needs_review)

def _print_project_summary(label: str, ctx: dict,
                           applied: list[dict], skipped: list[str],
                           needs_review: list[dict], dt: float,
                           report_out: Path) -> None:
    print(f"\n[ai-fixer] {label}:  "
          f"{len(applied)} applied,  "
          f"{ctx['ready']} ready (not committed),  "
          f"{len(skipped)} skipped,  "
          f"{len(needs_review)} needs review, {dt:.1f}s,  "
          f"{volume_str(ctx['usage'])},  "
          f"{ctx['usage']['prompt_tokens']:,} in / "
          f"{ctx['usage']['completion_tokens']:,} out")
    print(f"[ai-fixer] report: {rel_display(report_out)}")


def _print_dry_run_hint(label: str, ctx: dict) -> None:
    ids = ",".join(ctx["proposed"])
    n = len(ctx["proposed"])
    print(f"\n[ai-fixer] {n} fix(es) ready to commit for {label}:")
    print(f"    make ai-fix-commit-{label} FIXLIST=\"{ids}\"")
    print("  or commit all proposed (no FIXLIST):")
    print(f"    make ai-fix-commit-{label}")
    print("  discard and reclassify with LLM:")
    print("    make ai-fix-log FIXLIST=\"<id>\" AI_FIX_REFETCH=1")


def _print_applied_branches(applied: list[dict], base_branch: str,
                            run_id: int) -> None:
    print(f"\n[ai-fixer] {len(applied)} fix branch(es) (fix_runid={run_id}):")

    for c in applied:
        print(f"\n  {c['branch']}")
        print(f"    id:       {c['id']}")
        print(f"    summary:  {c['summary']}")
        print(f"    inspect:  git show {c['sha']}")
        print(f"    merge:    git merge --no-ff {c['branch']}")
        print(f"    reject:   git branch -D {c['branch']}")

    shas = " ".join(c["sha"] for c in applied)
    branches = " ".join(c["branch"] for c in applied)
    merge_lines = "\n    ".join(
        f"git merge --no-ff {c['branch']}" for c in applied)
    print(f"\n[ai-fixer] from '{base_branch}', inspect all at once:")
    print(f"    git show {shas}")
    print(f"\n[ai-fixer] from '{base_branch}', merge in order:")
    print(f"    {merge_lines}")
    print("\n[ai-fixer] reject all:")
    print(f"    git branch -D {branches}")


def _print_needs_review(needs_review: list[dict]) -> None:
    print(f"\n=== Needs human thought ({len(needs_review)}) ===")
    for i, item in enumerate(needs_review, 1):
        v = item["verdict"]
        print(f"[{i}] id={item.get('id', '?')}  "
              f"{item['text'].splitlines()[0][:100]}")
        if v.get("reason"):
            print(f"    why: {v['reason']}")
        if v.get("direction"):
            print(f"    direction: {v['direction']}")


def _write_json_log(label: str, ts: str, ctx: dict, dt: float,
                    applied: list[dict], skipped: list[str],
                    needs_review: list[dict]) -> Path:
    log_path = FIX_DIR / f"{ctx['fix_runid']}_{ts}_{label}.json"
    log_path.write_text(json.dumps({
        "project": label,
        "base_branch": ctx["base_branch"],
        "base_sha": ctx["base_sha"],
        "fix_runid": ctx["fix_runid"],
        "check_runid": ctx.get("check_runid", 0),
        "started": ts,
        "duration_sec": round(dt, 1),
        "fixes": applied,
        "skipped": skipped,
        "needs_review": [
            {"id": n.get("id"), "section": n["section"],
             "text": n["text"],
             "reason": n["verdict"].get("reason"),
             "direction": n["verdict"].get("direction")}
            for n in needs_review
        ],
    }, indent=2, ensure_ascii=False), encoding="utf-8")
    return log_path

def _print_final_summary(state: dict) -> None:
    if state["created_all"]:
        print("\n[ai-fixer] all branches from this run:")
        for label, _run_id, applied in state["created_all"]:
            for c in applied:
                print(f"  {c['branch']:<32}  {label:<8}  id={c['id']}")
        print(f"[ai-fixer] return to base:  git checkout "
              f"{state['base_branch'] or 'master'}")

    # Per-project lines already carry the numbers; a run total adds
    # information only when several projects were aggregated.
    if state.get("projects_done", 0) > 1:
        print("\n" + usage_line("ai-fixer", state["all_usage"]))

def _run(args, jobs: list[tuple[str, Path, Path]], runid: int) -> tuple[int, dict]:
    state = _prepare_run(args)
    state["fix_runid"] = runid
    state["model"] = None
    actor = db_status.fix_actor(args)
    needs_llm = actor in (db_status.ACT_CLASSIFY, db_status.ACT_PROPOSE)

    for label, report_path, project in jobs:
        print(f"\n--- {label}: {report_path.relative_to(OUT_ROOT)} ---")
        _ensure_baseline(report_path, project)
        findings = _filter_findings(report_path, args)
        if not findings:
            continue                      # <-- LLM ещё не трогали
        
        if needs_llm and state["model"] is None:
            if emulate.enabled():
                state["model"] = "emulated"
            else:
                llm.check_health(args.base_url)
                state["model"] = llm.resolve_model(args.base_url, args.model)
            db.llmrun_set_model(runid, state["model"])
            print(f"[ai-fixer] endpoint={args.base_url} "
                  f"model={state['model']}", file=sys.stderr)
            
        model = state["model"] or "(cached)"
        ctx = _build_ctx(label, args, model, state)
        _process_project(label, report_path, project, findings,
                         args, model, ctx, state)

    _print_final_summary(state)
    return 0, state["all_usage"]

def classify_system(ctx: dict) -> str:
    """Render CLASSIFY_SYSTEM (+ propose override) with live max_diff_lines."""

    if ctx.get("propose"):
        return (CLASSIFY_SYSTEM
                .replace("@CLASS_MODE@", _CLASS_MODE_PROPOSE)
                .replace("@LOCAL_RULES@", _LOCAL_RULES_FREE))
    else:
        return (CLASSIFY_SYSTEM
            .replace("@CLASS_MODE@", _CLASS_MODE_STRICT)
            .replace("@LOCAL_RULES@", _LOCAL_RULES_STRICT)
            .replace("@MAX_DIFF@", str(ctx["max_diff_lines"]) ) )

def _notify_tracker_commit(report_path, finding, full_key, sha, branch,
                           files, reason, explanation, diff) -> None:
    try:
        import tracker_common
        iid = db.issue_id_by_key(report_path, finding)
        if iid is None:
            return
        web = remote_web_url()
        sha_md = f"[`{sha}`]({web}/commit/{sha})" if web else f"`{sha}`"
        patch = commit_patch(sha) or diff   # fallback: proposal, если git недоступен
        
        body = (f"**ai-checker: committed**  `{full_key}`\n\n"
            f"status → fixed\n"
            f"commit {sha_md} on `{branch}`\n"
            f"files: {', '.join(files)}\n\n"
            f"**Reason:** {reason}\n"
            f"**Change:** {explanation}\n\n"
            f"```diff\n{patch}\n```\n")
        tracker_common.comment_ticket(iid, body, new_status=db.STATUS_FIXED)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: tracker comment failed: {exc}", file=sys.stderr)

def _notify_tracker_proposal(report_path, finding, full_key,
                             explanation, diff) -> None:
    try:
        import tracker_common
        iid = db.issue_id_by_key(report_path, finding)
        if iid is None:
            return
        body = (f"**ai-checker: fix proposed automatically**  "
                f"`{full_key}`\n\n"
                f"status → proposed-to-fix (not committed yet)\n"
                f"**Change:** {explanation}\n\n"
                f"```diff\n{diff}\n```\n")
        tracker_common.comment_ticket(iid, body,
                                      new_status=db.STATUS_PROPOSED)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: tracker comment failed: {exc}", file=sys.stderr)

def main() -> int:
    ap = argparse.ArgumentParser(
        description="Apply local-only fixes from an ai-checker report.")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--all", action="store_true",
                   help="latest report of every project (default)")
    g.add_argument("--project", help="latest report of one project")
    g.add_argument("--file", help="latest _files/ report for a source file")
    g.add_argument("--report", help="explicit report path")

    ap.add_argument("--model", default="")
    ap.add_argument("--base-url", default="")
    
    ap.add_argument("--apply", action="store_true",
                    help="write edits + commit to a fix branch "
                         "(default: dry-run)")
    
    db.seed_defaults()
    default_limit = db.get_param("fix", "default_limit", DEFAULT_LIMIT)
    ap.add_argument("--limit", type=int, default=default_limit,
                    help=f"max findings when --only is not set "
                         f"(default {default_limit}, 0 = unlimited)")
    ap.add_argument("--only", default="",
                    help="FIXLIST: '1/1, 1/3-5, 1/1001-1011, 1/*, *'. "
                         "chunk/local; local>=1000 = Мелочи. default: all")
    ap.add_argument("--reprocess", action="store_true",
                    help="re-process findings already in a terminal "
                         "status (or analyze-by-dev) in the DB")
    ap.add_argument("--propose", action="store_true",
                    help="force a patch proposal even for analyze-by-dev/"
                         "no-action findings (override classifier refusal)")
    ap.add_argument("--force-propose", action="store_true",
                    help="with --propose: also reopen analyze-by-dev and "
                         "no-action findings (failed stays manual)")
    ap.add_argument("--refetch", action="store_true",
                    help="ignore cached proposal; re-query the LLM")
    
    args = ap.parse_args()
    args.base_url = llm.resolve_url(args.base_url)   # без сети
    jobs = _resolve_jobs(args)
    lock_fd = acquire_lock()
    try:
        actor = db_status.fix_actor(args)
        name = "" if actor != db_status.ACT_COMMIT else "(cached)"
        runid = db.llmrun_start(kind="fix", url=args.base_url, name=name)
        print(f"[ai-fixer] runid={runid}", file=sys.stderr)
        try:
            rc, usage = _run(args, jobs, runid)
        except BaseException:
            db.llmrun_finish(runid, status="failed")
            raise
        db.llmrun_finish(runid, status="ok",
                         in_tokens=usage["prompt_tokens"],
                         out_tokens=usage["completion_tokens"],
                         requests=usage["requests"])
        # Sync statuses after all fixes
        for proj_name in db.known_projects():
            try:
                import tracker_common
                tracker_common.sync_statuses(proj_name)
            except Exception as exc:  # noqa: BLE001
                print(f"[ai-fixer] warn: tracker sync {proj_name}: {exc}",
                        file=sys.stderr)
        return rc
    finally:
        release_lock(lock_fd)


if __name__ == "__main__":
    raise SystemExit(main())

