"""AI-assisted code fixer for ngutils — applies only local, self-contained fixes.

Reads a report produced by check.py, asks an LLM to classify each
finding as 'local' (safe in-place fix) or 'needs_review' (requires
human thought), then applies the 'local' ones as ONE GIT COMMIT PER
FIX on a dedicated branch ai-checker-<label>-fix<N>.

Safety rails:
  - dry-run by default; --apply to actually write
  - requires a clean git working tree (no untracked/modified files)
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
import llm
from common import (
    ALLOWED_TOP_DIRS,
    DEFAULT_LIMIT,
    FIX_DIR,
    FORBIDDEN_ROOT_NAMES,
    MAX_SOURCE_CHARS,
    OUT_ROOT,
    ROOT,
    SRC_EXTS,
    acquire_lock,
    discover_projects,
    gather_context_files,
    iter_findings,
    release_lock,
    rotate_reports,
    types_digest_path,
)
from gitutil import (
    commit_one_fix,
    git_clean_check,
    make_fix_branch_name,
    make_run_id,
)
from report import format_header_md
from sysinfo import system_info

MAX_DIFF_LINES = 20

CLASSIFY_SYSTEM = """\
You are a strict C/C++ code-fix validator. You receive a code-review
finding and the source files it refers to. Decide whether the finding
can be fixed by a LOCAL, SELF-CONTAINED edit, or whether fixing it
requires human thought because it would ripple through the codebase.

A fix is LOCAL only if ALL of the following hold:

1. It touches at most TWO files. If two, they must be a paired
   source/header with the same stem in the same directory:
   X.c + X.h, X.cpp + X.hpp, X.cc + X.hpp. Editing a function body
   in X.c AND the matching declaration/comment in X.h is allowed.
2. Within EACH file, edits are confined to a single function body
   (or one #include line, one macro definition, or one comment
   attached to a declaration). Edits spanning multiple function
   bodies in the same file are NON-local.
   A fix is LOCAL even if it changes the on-disk file format (e.g.
a wrong fscanf/sscanf format string in a #define), PROVIDED that:
  - the edit is one line in a single macro or string literal, AND
  - the previous format was provably broken (e.g. the literal did
    not match what the writer produces), so there is nothing to be
    backward-compatible with.
    In this case, do NOT classify as needs_review on "format change"
    grounds alone.
3. It does NOT change any function signature: parameters, return
   type, qualifiers, calling convention. Even if you also update the
   .h file, adding or removing a parameter would require updating
   every caller in other translation units — that is still
   FORBIDDEN.
4. It does NOT add or remove any function, struct, union, enum, or
   global variable.
5. It does NOT change struct/union/enum layout, field type, or order.
6. It does NOT rename or remove a public symbol.
7. It does NOT add a new #include (typo fixes in existing includes are
   allowed).
8. It does NOT require any change in another file to be correct.
9. It is idempotent (applying twice is safe).
10. The diff is at most 20 changed lines (+ and - combined).


Examples of local fixes:
  - missing NULL check before a pointer dereference in the same function
  - missing fclose/free in the same function's error path
  - off-by-one in a loop bound in the same loop
  - uninitialised variable initialised before use
  - wrong constant, format specifier, or operator replaced
  - typo in a string literal or comment

Examples of NON-local fixes (always needs_review):
  - add a parameter to foo(): every caller must be updated
  - rename foo() to bar(): every caller must be updated
  - change foo()'s return type: every caller must be updated
  - extract a helper function: new symbol, new location
  - change struct field type or order: ABI/API break
  - fix a bug that spans multiple functions or files
  - refactor for readability
  - anything you are not 100% certain is local

When in doubt, classify as needs_review. A wrongly-applied fix is
worse than a deferred one.

Output ONLY a JSON object, no prose, no markdown fences:

{
  "class": "local" | "needs_review",
  "reason": "short reason (<= 200 chars)",
  "commit_subject": "short English imperative, <= 60 chars, no 'fix:' prefix, no trailing period",
  "direction": "for needs_review: suggested direction for a human",
  "edits": [
    {
      "file": "relative path from repo root",
      "old_snippet": "exact text from the source, with 1-3 lines of context",
      "new_snippet": "replacement text",
      "explanation": "one line"
    }
  ]
}

"edits" is present iff class == "local". It is a list of 1 or 2 entries.
Rules for each entry in "edits":
- "file" must be one of the source files provided.
- "old_snippet" must appear exactly ONCE in that file.
- "new_snippet" must differ and preserve indentation.
- Do NOT reformat surrounding code, add comments, or add blank lines.
- Do NOT touch anything outside the single function body (or the
  single comment block attached to a declaration).
- "count" (optional, default 1): the exact number of times old_snippet
  appears in that file and must be replaced. Use count > 1 ONLY when
  the SAME replacement applies to copy-pasted occurrences of the SAME
  text (e.g. an identical @param doc-comment in N functions). The model
  MUST count exactly; validate_edits will reject if the count does not
  match.

Rules for the list itself:
- If there is ONE entry, that file is the only file changed.
- If there are TWO entries, the files MUST be a paired source/header
  with the same stem in the same directory: X.c + X.h, X.cpp + X.hpp,
  X.cc + X.hpp, X.cxx + X.hpp. Typical case: change the function
  body or comment in X.c AND the matching declaration or comment
  in X.h.
- Never propose edits to three or more files.
- Never propose edits to two unrelated files (e.g. log.c + fs2.c).
- Total added+removed lines across ALL entries must be <= 20.
- "commit_subject" is REQUIRED when class == "local". It must be in
English, imperative mood, no "fix(...):" prefix — the prefix is
added automatically. Example: "replace 0 with NULL for pointer
assignment".

If the finding mentions multiple functions, multiple files, or
phrases like "in A, B, and C", "all callers", "everywhere",
"across the codebase", classify as needs_review. A partial fix that
touches only one of the mentioned functions is misleading — do not
emit one.

Never propose edits to:
  - any file outside <project>/src/, <project>/include/, <project>/test/
  - _types.h, _machine.json, or anything under build/
Those files are context-only.

If the finding requires adding a new #include (e.g. <stdbool.h>),
classify as needs_review: a missing include can ripple across
translation units.

If a finding requires the same textual replacement in multiple
places, output ONE entry per file with count = N, and list the file
once. If different contexts in the same file need DIFFERENT
replacements, that is not uniform — list separate entries or
classify as needs_review.
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
    if parts[0] not in ALLOWED_TOP_DIRS:
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


def validate_edits(edits: list[dict], project: Path,
                   max_diff_lines: int = MAX_DIFF_LINES
                   ) -> tuple[list[tuple[Path, dict]] | None, str | None]:
    """Validate 1 or 2 edits. Two edits must be a paired .c/.h.

    Returns ([(abs_path, edit), ...], None) on success.
    """
    if not edits:
        return None, "no edits"
    if len(edits) > 2:
        return None, f"too many files ({len(edits)}, max 2)"

    parsed: list[tuple[Path, dict]] = []
    seen: set[Path] = set()
    total_added = total_removed = 0

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

        old_h: set[str] = set()
        new_h: set[str] = set()
        for line in old.splitlines():
            h = extract_function_header(line)
            if h:
                old_h.add(h)
        for line in new.splitlines():
            h = extract_function_header(line)
            if h:
                new_h.add(h)
        added = new_h - old_h
        if added:
            return None, f"{rel}: adds function {sorted(added)}"
        removed = old_h - new_h
        if removed:
            return None, f"{rel}: changes/removes function signature {sorted(removed)}"

        a, r = diff_line_counts(old, new)
        total_added += a
        total_removed += r
        parsed.append((p, e))

    if len(parsed) == 2 and not _is_paired(parsed[0][0], parsed[1][0]):
        return None, "two files must be a paired X.c + X.h (same dir, same stem)"

    weighted = 0
    for p, e in parsed:
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
        parts.append("".join(difflib.unified_diff(
            old.splitlines(keepends=True), new.splitlines(keepends=True),
            fromfile=header, tofile=f"b/{rel}", n=2)))
    return "".join(parts)

    
def _parse_fixlist(raw: str):
    """Return matcher(key: tuple[int,int]) -> bool, or None.

    Accepted (comma/space separated), all ids are 'chunk/local_id':
      1/1           exact
      1/1-5         local 1..5 in chunk 1
      1/1001-1011   Мелочи 1..11 in chunk 1
      1/1-*         local 1..∞ in chunk 1
      1/*           everything in chunk 1
      *             everything
    """
    if not raw or not raw.strip():
        return None

    tokens = [t for t in re.split(r"[,\s]+", raw.strip()) if t]
    exact: set[tuple[int, int]] = set()
    wildcards: set[int] = set()
    ranges: list[tuple[int, int, int]] = []
    match_all = False

    for tok in tokens:
        if tok == "*":
            match_all = True
            continue
        m = re.match(r"^(\d+)/(\*|\d+)(?:-(\d+|\*))?$", tok)
        if not m:
            continue
        chunk = int(m.group(1))
        a = m.group(2)
        b = m.group(3)
        if a == "*":
            wildcards.add(chunk)
        elif b is None:
            exact.add((chunk, int(a)))
        else:
            lo = int(a)
            hi = 10**9 if b == "*" else int(b)
            ranges.append((chunk, lo, hi))

    def matcher(key: tuple[int, int]) -> bool:
        if match_all:
            return True
        if key in exact:
            return True
        if key[0] in wildcards:
            return True
        for c, lo, hi in ranges:
            if key[0] == c and lo <= key[1] <= hi:
                return True
        return False

    return matcher

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

_TERMINAL = {db.STATUS_FIXED, db.STATUS_REJECTED}

def _db_set_if_not_fixed(report_path: Path, finding: dict,
                         status: str, **kw) -> None:
    """_db_set, but never downgrades fixed/rejected to something else."""
    try:
        cur = db.get_status(report_path, finding)
    except Exception:  # noqa: BLE001
        cur = None
    if cur in _TERMINAL:
        return
    _db_set(report_path, finding, status, **kw)

def _db_set(report_path: Path, finding: dict, status: str,
            **kw) -> None:
    try:
        db.update_issue_status(report_path, finding, status, **kw)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: db update failed: {exc}", file=sys.stderr)

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
    if not (args.refetch or args.reprocess):
        pending = _load_pending(report_path, finding)
        if pending is not None:
            return _validate_cached(pending, finding, full_key,
                                    project, report_path, args, ctx, skipped)

    return _classify_with_llm(finding, full_key, project, report_path, files,
                              args, model, ctx, needs_review, skipped)

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
    if args.apply:
        for e in pending["edits"]:
            msg, reason = _check_file_freshness(e, pending["dt"], full_key)
            if msg is not None:
                print(msg)
                skipped.append(f"{full_key}: {reason}")
                _db_set_if_not_fixed(report_path, finding, db.STATUS_FAILED,
                                     reason=reason)
                return None

    parsed, err = validate_edits(pending["edits"], project,
                                 max_diff_lines=ctx["max_diff_lines"])
    if err:
        print(f"  STALE proposal ({err})")
        print(f"  rerun 'make ai-fix-log AI_FIX_REFETCH=1 "
              f"FIXLIST=\"{full_key}\"' to reclassify")
        skipped.append(f"{full_key}: stale: {err}")
        _db_set_if_not_fixed(report_path, finding, db.STATUS_FAILED,
                             reason=f"stale: {err}")
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

def _classify_with_llm(finding: dict, full_key: str, project: Path,
                       report_path: Path, files: list[Path],
                       args, model: str, ctx: dict,
                       needs_review: list[dict], skipped: list[str]):
    """Ask the model; return outcome for LOCAL, else record and None."""
    verdict, err = _ask_classifier(finding, project, files, args, model, ctx)
    if err is not None:
        print(f"  SKIP: {err}")
        skipped.append(f"{full_key}: {err}")
        _db_set(report_path, finding, db.STATUS_FAILED, reason=err)
        return None

    cls = verdict.get("class", "needs_review")
    reason = verdict.get("reason", "")

    if cls != "local":
        print(f"  NEEDS REVIEW: {reason}")
        if verdict.get("direction"):
            print(f"    direction: {verdict['direction']}")
        needs_review.append(finding | {"verdict": verdict})
        _db_set(report_path, finding, db.STATUS_ANALYZE,
                reason=reason, direction=verdict.get("direction"))
        return None

    raw_edits = verdict.get("edits")
    if not raw_edits:
        single = verdict.get("edit")
        raw_edits = [single] if single else []

    parsed, verr = validate_edits(raw_edits, project,
                                  max_diff_lines=ctx["max_diff_lines"])
    if verr:
        print(f"  SKIP (invalid edit): {verr}")
        skipped.append(f"{full_key}: {verr}")
        _db_set(report_path, finding, db.STATUS_FAILED,
                reason=f"invalid edit: {verr}")
        return None

    explanation = "; ".join(
        e.get("explanation", "") for _, e in parsed
        if e.get("explanation"))
    
    commit_subject = verdict.get("commit_subject", "")
    rels = [str(p.relative_to(ROOT)) for p, _ in parsed]
    commit_msg = _build_commit_msg(
        finding, ", ".join(rels), reason, explanation,
        commit_subject=commit_subject,
        project_name=project.name)

    _save_proposal(report_path, finding, parsed, commit_msg, reason,
                   runid=ctx["check_runid"],
                   fix_runid=ctx["fix_runid"],
                   commit_subject=commit_subject)
    _db_set(report_path, finding, db.STATUS_PROPOSED, reason=reason)

    return {
        "edits": parsed,
        "reason": reason,
        "explanation": explanation,
        "commit_msg": commit_msg,
        "commit_subject": commit_subject,
        "source": "llm",
        "dt": None,
    }

def _ask_classifier(finding: dict, project: Path, files: list[Path],
                    args, model: str, ctx: dict):
    """Return (verdict_dict, None) or (None, error_string)."""

    chunk_usage: dict = {}
    messages = [
        {"role": "system", "content": CLASSIFY_SYSTEM},
        {"role": "user",   "content": build_prompt(finding, project, files,
                                                   budget=ctx["max_source"])},
    ]
    try:
        raw = llm.chat_once(args.base_url, model, messages,
                            temperature=ctx["fix_temp"],
                            usage_out=chunk_usage)
    except llm.RequestException as exc:
        return None, f"LLM: {exc}"

    for k in ("prompt_tokens", "completion_tokens", "total_tokens"):
        ctx["usage"][k] += chunk_usage.get(k, 0)
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
        ctx["ready"] += 1
        ctx["proposed"].append(full_key)
        return

    ctx["seq"] += 1
    try:
        chunk_id, local_id = db.issue_key(finding)
    except (ValueError, TypeError):
        chunk_id, local_id = 0, 0
    branch = make_fix_branch_name(ctx["run_id"], ctx["seq"],
                                  chunk_id, local_id)
    commit_edits = [(p, e["old_snippet"], e["new_snippet"])
                    for p, e in parsed]
    files = [str(p.relative_to(ROOT)) for p, _ in parsed]

    commit_msg = _build_commit_msg(
        finding, ", ".join(files), reason, explanation,
        commit_subject=outcome.get("commit_subject", ""),
        project_name=project.name,
        branch=branch, local_id=local_id)
    subject_line = commit_msg.split("\n\n", 1)[0]

    try:
        sha = commit_one_fix(ctx["base_sha"], ctx["base_branch"], branch,
                             commit_edits, commit_msg)
    except RuntimeError as exc:
        print(f"  SKIP: {exc}")
        skipped.append(f"{full_key}: {exc}")
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
    _db_set(report_path, finding, db.STATUS_FIXED,
            reason=reason, fix_branch=branch, fix_commit=sha,
            fix_subject=subject_line)

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
    out_path = out_dir / f"{ts}{suffix}.md"

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
        f"**Source report:** `{source_report.relative_to(ROOT)}`\n",
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
              f"-> {out_dir.relative_to(ROOT)}/hist/", file=sys.stderr)
    return out_path

def _prepare_run(args) -> dict:
    """One-time setup: base branch, machine snapshot, accumulator state."""
    base_branch = base_sha = None
    if args.apply:
        base_branch, base_sha = git_clean_check()

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
        "all_usage": {"prompt_tokens": 0, "completion_tokens": 0,
                      "total_tokens": 0, "requests": 0},
        "created_all": [],
    }

def _issue_key_or_none(finding: dict) -> tuple[int, int] | None:
    try:
        return db.issue_key(finding)
    except (ValueError, TypeError):
        return None


def _matches_fixlist(finding: dict, matcher) -> bool:
    k = _issue_key_or_none(finding)
    return k is not None and matcher(k)


def _load_resolved(report_path: Path) -> set[tuple[int, int]]:
    try:
        return db.resolved_finding_keys(report_path)
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: db query failed: {exc}", file=sys.stderr)
        return set()


def _filter_findings(report_path: Path, args) -> list[dict]:
    """Apply FIXLIST / --limit / resolved-filter. Empty list = skip project."""
    findings = iter_findings(report_path, 0)

    only = _parse_fixlist(args.only)
    if only is not None:
        selected = [f for f in findings if _matches_fixlist(f, only)]
        if not selected:
            print(f"  warn: FIXLIST matched nothing: {args.only!r}",
                  file=sys.stderr)
            print("  no findings match FIXLIST")
            return []
        findings = selected
    elif args.limit:
        findings = findings[:args.limit]

    if not findings:
        print("  no findings to process")
        return []

    if args.reprocess:
        return findings

    resolved = _load_resolved(report_path)
    if not resolved:
        return findings

    kept: list[dict] = []
    for f in findings:
        k = _issue_key_or_none(f)
        if k is None or k not in resolved:
            kept.append(f)

    n = len(findings) - len(kept)
    print(f"  skip {n} already-resolved "
          f"(fixed/rejected/analyze-by-dev); "
          f"use --reprocess to include", file=sys.stderr)
    if not kept:
        print("  nothing left after resolved filter")
    return kept

def _build_ctx(label: str, args, model: str, state: dict) -> dict:
    """Per-project accumulator + params from DB."""
    run_id = (make_run_id(label) if args.apply
              else f"ai-checker-{label}-fix<N>")

    db.seed_defaults()
    max_source = db.get_param("fix", "max_source_chars", MAX_SOURCE_CHARS)
    max_diff = db.get_param("fix", "max_diff_lines", MAX_DIFF_LINES)
    fix_temp = db.get_param("fix", "temperature", 0.0)

    return {
        "run_id": run_id,
        "fix_runid": state["fix_runid"],
        "check_runid": 0,  # fill in в _process_project
        "base_sha": state["base_sha"],
        "base_branch": state["base_branch"],
        "seq": 0,
        "ready": 0,
        "model": model,
        "usage": {"prompt_tokens": 0, "completion_tokens": 0,
                  "total_tokens": 0, "requests": 0},
        "finding_durs": [],
        "max_source": max_source,
        "max_diff_lines": max_diff,
        "fix_temp": fix_temp,
        "proposed": [],
    }

def _process_project(label: str, report_path: Path, project: Path,
                     findings: list[dict], args, model: str,
                     ctx: dict, state: dict) -> None:
    """Iterate findings, then print summary and persist outputs."""
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

    for idx, finding in enumerate(findings, 1):
        t_finding = time.perf_counter()
        files = gather_context_files(finding, project)
        _handle_finding(idx, len(findings), finding, project,
                        report_path, files, args, model, ctx,
                        applied, skipped, needs_review)
        ctx["finding_durs"].append(time.perf_counter() - t_finding)
    dt = time.perf_counter() - t0

    for k in state["all_usage"]:
        state["all_usage"][k] += ctx["usage"].get(k, 0)

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
        print(f"[ai-fixer] json log:    {log_path.relative_to(ROOT)}")
        state["created_all"].append((label, ctx["run_id"], applied))

    if needs_review:
        _print_needs_review(needs_review)

def _print_project_summary(label: str, ctx: dict,
                           applied: list[dict], skipped: list[str],
                           needs_review: list[dict], dt: float,
                           report_out: Path) -> None:
    print(f"\n[ai-fixer] {label}: "
          f"{len(applied)} applied, "
          f"{ctx['ready']} ready (not committed), "
          f"{len(skipped)} skipped, "
          f"{len(needs_review)} needs review, {dt:.1f}s, "
          f"{ctx['usage']['prompt_tokens']:,} in / "
          f"{ctx['usage']['completion_tokens']:,} out")
    print(f"[ai-fixer] report: {report_out.relative_to(ROOT)}")


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
                            run_id: str) -> None:
    print(f"\n[ai-fixer] {len(applied)} fix branch(es) (run_id={run_id}):")
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
    log_path = FIX_DIR / f"{ts}_{label}.json"
    log_path.write_text(json.dumps({
        "project": label,
        "base_branch": ctx["base_branch"],
        "base_sha": ctx["base_sha"],
        "run_id": ctx["run_id"],
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

    u = state["all_usage"]
    if u["requests"]:
        print(f"\n[ai-fixer] total across projects: "
              f"{u['prompt_tokens']:,} in / "
              f"{u['completion_tokens']:,} out "
              f"({u['requests']} request(s))")

def _run(args, model: str, 
         jobs: list[tuple[str, Path, Path]],
         runid: int) -> tuple[int, dict]:

    state = _prepare_run(args)
    state["fix_runid"] = runid

    for label, report_path, project in jobs:
        print(f"\n--- {label}: {report_path.relative_to(ROOT)} ---")
        findings = _filter_findings(report_path, args)
        if not findings:
            continue
        ctx = _build_ctx(label, args, model, state)
        _process_project(label, report_path, project, findings,
                         args, model, ctx, state)

    _print_final_summary(state)
    
    return 0, state["all_usage"]

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
                    help="re-process findings already marked "
                         "fixed/rejected in the DB")
    ap.add_argument("--refetch", action="store_true",
                    help="ignore cached proposal; re-query the LLM")
    args = ap.parse_args()

    args.base_url = llm.resolve_url(args.base_url)
    llm.check_health(args.base_url)
    try:
        model = llm.resolve_model(args.base_url, args.model)
    except llm.LLMError as e:
        sys.exit(f"ERROR: {e}")
    print(f"[ai-fixer] endpoint={args.base_url} model={model}",
          file=sys.stderr)

    jobs = _resolve_jobs(args)
    lock_fd = acquire_lock()
    try:
        runid = db.llmrun_start(kind="fix", url=args.base_url,
                                name=model)
        print(f"[ai-fixer] runid={runid}", file=sys.stderr)

        try:
            rc, usage = _run(args, model, jobs, runid)
        except BaseException:
            db.llmrun_finish(runid, status="failed")
            raise

        db.llmrun_finish(runid, status="ok",
                         in_tokens=usage["prompt_tokens"],
                         out_tokens=usage["completion_tokens"],
                         requests=usage["requests"])
        return rc
    finally:
        release_lock(lock_fd)


if __name__ == "__main__":
    raise SystemExit(main())

