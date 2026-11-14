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
import os
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import requests

import db
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
    types_digest_path,
)

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
"""

# ── Git helpers ──────────────────────────────────────────────────────
def git(*args: str, input: str | None = None,
        check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *args], cwd=ROOT, text=True,
        input=input, capture_output=True, check=check)


def git_clean_check() -> tuple[str, str]:
    """Verify we're in a git repo with a clean tree. Return (branch, HEAD sha)."""
    r = git("rev-parse", "--verify", "HEAD", check=False)
    if r.returncode != 0:
        sys.exit("ERROR: not a git repository (or no commits yet)")
    branch = git("rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    if branch == "HEAD":
        sys.exit("ERROR: detached HEAD, checkout a branch first")
    status = git("status", "--porcelain").stdout.strip()
    if status:
        sys.exit("ERROR: working tree is dirty. Commit or stash before "
                 "running with --apply.\n"
                 f"       Uncommitted changes:\n{status}")
    sha = git("rev-parse", "HEAD").stdout.strip()
    return branch, sha


def make_branch_name(label: str) -> str:
    base = f"ai-checker-{label}-fix"
    existing = git("branch", "--list", f"{base}*").stdout.splitlines()
    used: set[int] = set()
    for line in existing:
        name = line.strip().lstrip("* ").strip()
        if name.startswith(base):
            suffix = name[len(base):]
            if suffix.isdigit():
                used.add(int(suffix))
    n = 1
    while n in used:
        n += 1
    return f"{base}{n}"


def git_create_branch(branch: str, from_sha: str) -> None:
    git("checkout", "-b", branch, from_sha)


def git_commit_file(path: Path, msg: str) -> str:
    rel = path.relative_to(ROOT)
    git("add", "--", str(rel))
    r = git("commit", "-F", "-", input=msg, check=False)
    if r.returncode != 0:
        raise RuntimeError(f"git commit failed: {r.stderr.strip()}")
    return git("rev-parse", "--short", "HEAD").stdout.strip()

def make_run_id(label: str) -> str:
    """Return '<label>-fix<N>' with N unused among existing branches.

    Branches from this run are named '<run_id>-<seq>', so we probe
    the same '<label>-fix<N>' prefix when looking for collisions.
    """
    base = f"ai-checker-{label}-fix"
    existing = git("branch", "--list", f"{base}*").stdout.splitlines()
    used: set[int] = set()
    for line in existing:
        name = line.strip().lstrip("* ").strip()
        if not name.startswith(base):
            continue
        suffix = name[len(base):]
        head = suffix.split("-", 1)[0]
        if head.isdigit():
            used.add(int(head))
    n = 1
    while n in used:
        n += 1
    return f"{base}{n}"


def make_fix_branch_name(run_id: str, seq: int) -> str:
    return f"{run_id}-{seq}"


def commit_one_fix(base_sha: str, base_branch: str, branch_name: str,
                   edits: list[tuple[Path, str, str]], msg: str) -> str:
    """Create branch from base_sha, apply all edits, commit, return to base."""
    r = git("checkout", "-b", branch_name, base_sha, check=False)
    if r.returncode != 0:
        raise RuntimeError(f"git checkout -b failed: {r.stderr.strip()}")

    def _cleanup(reason: str) -> None:
        git("reset", "--hard", check=False)
        git("checkout", base_branch, check=False)
        git("branch", "-D", branch_name, check=False)
        raise RuntimeError(reason)

    for path, old, new in edits:
        if not apply_edit(path, old, new):
            _cleanup(f"apply_edit failed on {path.relative_to(ROOT)}")

    for path, _, _ in edits:
        rel = str(path.relative_to(ROOT))
        r = git("add", "--", rel, check=False)
        if r.returncode != 0:
            _cleanup(f"git add failed for {rel}: {r.stderr.strip()}")

    r = git("commit", "-F", "-", input=msg, check=False)
    if r.returncode != 0:
        _cleanup(f"git commit failed: {r.stderr.strip()}")

    sha = git("rev-parse", "--short", "HEAD").stdout.strip()

    r = git("checkout", base_branch, check=False)
    if r.returncode != 0:
        raise RuntimeError(
            f"commit {sha} on {branch_name} succeeded, "
            f"but 'git checkout {base_branch}' failed: {r.stderr.strip()}")
    return sha



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

def build_prompt(finding: dict, project: Path, files: list[Path]) -> str:
    parts = [f"Project: {project.name}",
             f"Section: {finding['section']}",
             "Finding:",
             finding["text"],
             "",
             "Source files:"]
    budget = MAX_SOURCE_CHARS

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

# ── LLM call ─────────────────────────────────────────────────────────
def ask_model(base_url: str, model: str, user: str) -> str:
    url = f"{base_url.rstrip('/')}/v1/chat/completions"
    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": CLASSIFY_SYSTEM},
            {"role": "user",   "content": user},
        ],
        "temperature": 0.0,
        "stream": False,
    }
    r = requests.post(url, json=payload, timeout=600)
    r.raise_for_status()
    return r.json()["choices"][0]["message"]["content"]

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


def validate_edits(edits: list[dict], project: Path
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
        n = src.count(old)
        if n != 1:
            return None, f"{rel}: old_snippet appears {n} times (need 1)"
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

    if total_added + total_removed > MAX_DIFF_LINES:
        return None, (f"diff too large ({total_added}+{total_removed} "
                      f"> {MAX_DIFF_LINES})")
    return parsed, None

def make_diff(edits: list[tuple[Path, str, str]]) -> str:
    parts: list[str] = []
    for path, old, new in edits:
        rel = path.relative_to(ROOT)
        parts.append("".join(difflib.unified_diff(
            old.splitlines(keepends=True), new.splitlines(keepends=True),
            fromfile=f"a/{rel}", tofile=f"b/{rel}", n=2)))
    return "".join(parts)


def apply_edit(path: Path, old: str, new: str) -> bool:
    try:
        src = path.read_text(encoding="utf-8")
        if src.count(old) != 1:
            return False
        path.write_text(src.replace(old, new, 1), encoding="utf-8")
        return True
    except OSError:
        return False
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
                      explanation: str) -> str:
    first_line = finding["text"].splitlines()[0][:70]
    if not first_line.endswith("."):
        first_line += "."
    body = (f"Section: {finding['section']}\n"
            f"File:    {rel}\n"
            f"Reason:  {reason}\n"
            f"Change:  {explanation}\n")
    return (f"ai-fix: {first_line}\n\n"
            f"{body}\n"
            f"Finding:\n{finding['text']}\n\n"
            f"Applied by ai-checker/fix.py\n")

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

def _handle_finding(idx: int, total: int, finding: dict,
                    project: Path, report_path: Path, files: list[Path],
                    args, model: str,
                    ctx: dict,
                    applied: list[dict], skipped: list[str],
                    needs_review: list[dict]) -> None:
    
    try:
        _c, _l = db.issue_key(finding)
        key = f"{_c}/{_l}"
    except (ValueError, TypeError):
        key = finding.get("id", "?")
    print(f"\n=== Finding {key}  ({idx}/{total})  "
          f"[{finding['section']}] ===")
    
    print(f"  {finding['text'][:200].replace(chr(10), ' ')}...")
    if not files:
        print("  SKIP: no source files to work with")
        skipped.append(finding["text"][:80])
        return

    pending = None
    if not args.refetch:
        try:
            pending = db.load_proposal(report_path, finding)
        except Exception as exc:  # noqa: BLE001
            print(f"  warn: proposal load failed: {exc}", file=sys.stderr)

    if pending is not None:
        if args.apply:
            # Проверить file_sha256 — единственная быстрая проверка
            try:
                cur = hashlib.sha256(
                    (ROOT / pending["file"]).read_text(
                        encoding="utf-8").encode()).hexdigest()
            except OSError as e:
                print(f"  STALE proposal: cannot read {pending['file']}: {e}")
                print(f"  rerun 'make ai-fix-log AI_FIX_REFETCH=1 "
                      f"FIXLIST=\"{finding.get('id', '?')}\"' to reclassify")
                # db.clear_proposal(report_path, finding)
                _db_set(report_path, finding, db.STATUS_FAILED,
                        reason=f"stale: OS {e}")
                return
            if cur != pending["file_sha256"]:
                print(f"  STALE proposal: {pending['file']} changed since "
                      f"dry-run ({pending['dt']})")
                print(f"  rerun 'make ai-fix-log AI_FIX_REFETCH=1 "
                      f"FIXLIST=\"{finding.get('id', '?')}\"' to reclassify")
                # db.clear_proposal(report_path, finding)
                _db_set(report_path, finding, db.STATUS_FAILED,
                        reason="stale: file changed")
                return

        if args.apply:
            for e in pending["edits"]:
                try:
                    cur = hashlib.sha256(
                        (ROOT / e["file"]).read_text(
                            encoding="utf-8").encode()).hexdigest()
                except OSError as exc:
                    print(f"  STALE proposal: cannot read {e['file']}: {exc}")
                    _db_set(report_path, finding, db.STATUS_FAILED,
                            reason=f"stale: {exc}")
                    return
                if cur != e.get("file_sha256"):
                    print(f"  STALE proposal: {e['file']} changed since "
                          f"dry-run ({pending['dt']})")
                    print(f"  rerun 'make ai-fix-log AI_FIX_REFETCH=1 "
                          f"FIXLIST=\"{key}\"' to reclassify")
                    _db_set(report_path, finding, db.STATUS_FAILED,
                            reason="stale: file changed")
                    return
        parsed, err = validate_edits(pending["edits"], project)
        if err:
            print(f"  STALE proposal ({err})")
            print(f"  rerun 'make ai-fix-log AI_FIX_REFETCH=1 "
                              f"FIXLIST=\"{finding.get('id', '?')}\"' to reclassify")
            _db_set(report_path, finding, db.STATUS_FAILED,
                    reason=f"stale: {err}")
            return

        reason = pending["reason"]
        explanation = "; ".join(e.get("explanation", "")
                                for e in pending["edits"] if e.get("explanation"))
        diff = make_diff([(p, e["old_snippet"], e["new_snippet"])
                          for p, e in parsed])
        print(f"  LOCAL (cached {pending['dt']}): {explanation}")
        for line in diff.splitlines():
            print(f"    {line}")

        if not args.apply:
            print(f"  [dry-run] cached proposal from {pending['dt']}")
            ctx["ready"] += 1
            return

        if not args.yes:
            try:
                ans = input("  Apply and commit? [y/N/q] ").strip().lower()
            except EOFError:
                ans = "n"
            if ans == "q":
                sys.exit(0)
            if ans != "y":
                print("  skipped by user")
                skipped.append(finding.get("id", "?") + ": user")
                # db.clear_proposal(report_path, finding)
                _db_set(report_path, finding, db.STATUS_REJECTED,
                        reason="user rejected")
                return

                ctx["seq"] += 1
        branch = make_fix_branch_name(ctx["run_id"], ctx["seq"])
        commit_edits = [(p, e["old_snippet"], e["new_snippet"])
                        for p, e in parsed]
        try:
            sha = commit_one_fix(ctx["base_sha"], ctx["base_branch"],
                                 branch, commit_edits, pending["commit_msg"])
        except RuntimeError as exc:
            print(f"  SKIP: {exc}")
            skipped.append(f"{key}: {exc}")
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
        })
        # db.clear_proposal(report_path, finding)
        _db_set(report_path, finding, db.STATUS_FIXED,
                reason=reason, fix_branch=branch, fix_commit=sha)
        return

    try:
        raw = ask_model(args.base_url, model,
                        build_prompt(finding, project, files))
    except requests.RequestException as e:
        print(f"  SKIP: LLM request failed: {e}")
        skipped.append(finding["text"][:80])
        _db_set(report_path, finding, db.STATUS_FAILED, reason=f"LLM: {e}")
        
        return

    raw = raw.strip()
    if raw.startswith("```"):
        raw = re.sub(r"^```[a-z]*\n?", "", raw)
        raw = re.sub(r"\n?```$", "", raw)
    try:
        verdict = json.loads(raw)
    except json.JSONDecodeError as e:
        print(f"  SKIP: invalid JSON: {raw[:120]!r}")
        skipped.append(finding["text"][:80])
        _db_set(report_path, finding, db.STATUS_FAILED, reason=f"bad JSON {e}");

        return

    cls = verdict.get("class", "needs_review")
    reason = verdict.get("reason", "")

    if cls != "local":
        print(f"  NEEDS REVIEW: {reason}")
        if verdict.get("direction"):
            print(f"    direction: {verdict['direction']}")
        needs_review.append(finding | {"verdict": verdict})
        _db_set(report_path, finding, db.STATUS_ANALYZE, reason=reason, direction=verdict.get("direction"))

        return

    raw_edits = verdict.get("edits")
    if not raw_edits:
        single = verdict.get("edit")
        raw_edits = [single] if single else []
    parsed, err = validate_edits(raw_edits, project)
    if err:
        print(f"  SKIP (invalid edit): {err}")
        skipped.append(f"{key}: {err}")
        _db_set(report_path, finding, db.STATUS_FAILED,
                reason=f"invalid edit: {err}")
        return

    explanations = [e.get("explanation", "") for _, e in parsed]
    explanation = "; ".join(x for x in explanations if x)
    diff = make_diff([(p, e["old_snippet"], e["new_snippet"])
                      for p, e in parsed])
    print(f"  LOCAL: {explanation}")
    for line in diff.splitlines():
        print(f"    {line}")

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
            "file_sha256": sha256,
        })

    rels = [e["file"] for e in proposal_edits]
    commit_msg = _build_commit_msg(finding, ", ".join(rels),
                                    reason, explanation)
    try:
        db.save_proposal(
            report_path, finding,
            edits=proposal_edits,
            commit_msg=commit_msg,
            reason=reason,
        )
    except Exception as exc:  # noqa: BLE001
        print(f"  warn: proposal save failed: {exc}", file=sys.stderr)

    _db_set(report_path, finding, db.STATUS_PROPOSED, reason=reason)
    ctx["ready"] +=1
    
    if not args.apply:
        print("  [dry-run] not applied (saved as proposal)")
        return

    if not args.yes:
        try:
            ans = input("  Apply and commit? [y/N/q] ").strip().lower()
        except EOFError:
            ans = "n"
        if ans == "q":
            sys.exit(0)
        if ans != "y":
            print("  skipped by user")
            skipped.append(finding["text"][:80])
            _db_set(report_path, finding,
                        db.STATUS_REJECTED,
                        reason="user rejected")
            
            return

        ctx["seq"] += 1
        branch = make_fix_branch_name(ctx["run_id"], ctx["seq"])
        commit_edits = [(p, e["old_snippet"], e["new_snippet"])
                        for p, e in parsed]
        try:
            sha = commit_one_fix(ctx["base_sha"], ctx["base_branch"], branch,
                                commit_edits, commit_msg)
        except RuntimeError as exc:
            print(f"  SKIP: {exc}")
            skipped.append(f"{key}: {exc}")
            return

        print(f"  COMMITTED {sha}  {branch}  {', '.join(rels)}")
        applied.append({
            "id": finding.get("id", "?"),
            "section": finding["section"],
            "sha": sha,
            "branch": branch,
            "file": ", ".join(rels),
            "summary": finding["text"].splitlines()[0][:70],
        })
        _db_set(report_path, finding, db.STATUS_FIXED,
                reason=reason, fix_branch=branch, fix_commit=sha)
        _db_set(report_path, finding, db.STATUS_FIXED,
                reason=reason, fix_branch=branch, fix_commit=sha)
        return
    _db_set(report_path, finding, db.STATUS_FIXED, reason=reason, 
            fix_branch=branch, fix_commit=sha)

def _run(args, model: str, jobs: list[tuple[str, Path, Path]]) -> int:
    base_branch = base_sha = None
    if args.apply:
        base_branch, base_sha = git_clean_check()

    FIX_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    created_all: list[tuple[str, str, list[dict]]] = []

    for label, report_path, project in jobs:
        print(f"\n--- {label}: {report_path.relative_to(ROOT)} ---")        
        
        findings = iter_findings(report_path, 0)
        only = _parse_fixlist(args.only)
        if only is not None:
            selected: list[dict] = []
            for f in findings:
                try:
                    key = db.issue_key(f)
                except (ValueError, TypeError):
                    continue
                if only(key):
                    selected.append(f)
            if not selected:
                print(f"  warn: FIXLIST matched nothing: {args.only!r}",
                      file=sys.stderr)
                print("  no findings match FIXLIST")
                continue
            findings = selected
        elif args.limit:
            findings = findings[:args.limit]

        if not findings:
            print("  no findings to process")
            continue

        run_id = (make_run_id(label) if args.apply
                          else f"ai-checker-{label}-fix<N>")
        ctx = {
            "run_id": run_id,
            "base_sha": base_sha,
            "base_branch": base_branch,
            "seq": 0,
            "ready": 0
        }

        # Skip findings already resolved in a previous run, unless
        # --reprocess is given. Only makes sense with --apply: dry-run
        # should show everything.
        if args.apply and not args.reprocess:
            try:
                resolved = db.resolved_finding_keys(report_path)
            except Exception as exc:  # noqa: BLE001
                print(f"  warn: db query failed: {exc}", file=sys.stderr)
                resolved = set()
            if resolved:
                before = len(findings)
                kept: list[dict] = []
                for f in findings:
                    try:
                        key = db.issue_key(f)
                    except (ValueError, TypeError):
                        kept.append(f)
                        continue
                    if key not in resolved:
                        kept.append(f)
                findings = kept
                print(f"  skip {before - len(findings)} already-resolved "
                      f"(use --reprocess to include)", file=sys.stderr)
                if not findings:
                    print("  nothing left after resolved filter")
                    continue

        applied: list[dict] = []
        skipped: list[str] = []
        needs_review: list[dict] = []

        t0 = time.perf_counter()
        for idx, finding in enumerate(findings, 1):
            files = gather_context_files(finding, project)
            _handle_finding(idx, len(findings), finding, project, 
                            report_path, files,
                            args, model, ctx,
                            applied, skipped, needs_review)
        dt = time.perf_counter() - t0

        print(f"\n[ai-fixer] {label}: "
              f"{len(applied)} applied, "
              f"{ctx.get('ready', 0)} ready (not committed), "
              f"{len(skipped)} skipped, "
              f"{len(needs_review)} needs review, {dt:.1f}s")

        if applied:
            print(f"[ai-fixer] {len(applied)} fix branches "
                  f"(run_id={run_id}):")
            for c in applied:
                print(f"  {c['branch']:<32}  id={c['id']:<6}  "
                      f"{c['summary']}")
            first = applied[0]
            print(f"\n[ai-fixer] review one:   git log --oneline "
                  f"{first['branch']}")
            print(f"[ai-fixer] inspect one:  git show {first['sha']}")
            print(f"[ai-fixer] merge one:    git checkout {base_branch} && "
                  f"git merge --no-ff {first['branch']}")
            print(f"[ai-fixer] reject one:   git branch -D {first['branch']}")
            print(f"[ai-fixer] reject all:   git branch -D "
                  f"{' '.join(c['branch'] for c in applied)}")

            log_path = FIX_DIR / f"{ts}_{label}.json"
            log_path.write_text(json.dumps({
                "project": label,
                "base_branch": base_branch,
                "base_sha": base_sha,
                "run_id": run_id,
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
            print(f"[ai-fixer] json log:    {log_path.relative_to(ROOT)}")

            created_all.append((label, run_id, applied))

        if needs_review:
            print(f"\n=== Needs human thought ({len(needs_review)}) ===")
            for i, item in enumerate(needs_review, 1):
                v = item["verdict"]
                print(f"[{i}] id={item.get('id', '?')}  "
                      f"{item['text'].splitlines()[0][:100]}")
                if v.get("reason"):
                    print(f"    why: {v['reason']}")
                if v.get("direction"):
                    print(f"    direction: {v['direction']}")

    if created_all:
        print("\n[ai-fixer] all branches from this run:")
        for label, _run_id, applied in created_all:
            for c in applied:
                print(f"  {c['branch']:<32}  {label:<8}  id={c['id']}")
        print(f"[ai-fixer] return to base:  git checkout "
              f"{base_branch or 'master'}")

    return 0

def main() -> int:
    ap = argparse.ArgumentParser(
        description="Apply local-only fixes from an ai-checker report.")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--all", action="store_true",
                   help="latest report of every project (default)")
    g.add_argument("--project", help="latest report of one project")
    g.add_argument("--file", help="latest _files/ report for a source file")
    g.add_argument("--report", help="explicit report path")
    ap.add_argument("--model", default=os.environ.get("QWEN_MODEL", ""))
    ap.add_argument("--base-url",
                    default=os.environ.get("LMSTUDIO_URL",
                                           "http://127.0.0.1:1234"))
    ap.add_argument("--apply", action="store_true",
                    help="write edits + commit to a fix branch "
                         "(default: dry-run)")
    ap.add_argument("--yes", action="store_true",
                    help="skip per-fix confirmation (use with --apply)")
    ap.add_argument("--limit", type=int, default=DEFAULT_LIMIT,
                    help=f"max findings when --only is not set "
                         f"(default {DEFAULT_LIMIT}, 0 = unlimited)")
    ap.add_argument("--only", default="",
                    help="FIXLIST: '1/1, 1/3-5, 1/1001-1011, 1/*, *'. "
                         "chunk/local; local>=1000 = Мелочи. default: all")
    ap.add_argument("--reprocess", action="store_true",
                    help="re-process findings already marked "
                         "fixed/rejected in the DB")
    ap.add_argument("--refetch", action="store_true",
                    help="ignore cached proposal; re-query the LLM")
    args = ap.parse_args()

    try:
        r = requests.get(f"{args.base_url.rstrip('/')}/v1/models", timeout=10)
        r.raise_for_status()
        models = [m["id"] for m in r.json().get("data", [])]
    except requests.RequestException as e:
        sys.exit(f"ERROR: cannot reach LM Studio at {args.base_url}: {e}")

    if not models:
        sys.exit("ERROR: LM Studio reports no loaded models")

    model = args.model or next(
        (m for m in models if "qwen" in m.lower()), models[0])
    print(f"[ai-fixer] endpoint={args.base_url} model={model}", file=sys.stderr)

    jobs = _resolve_jobs(args)
    lock_fd = acquire_lock()
    try:
        return _run(args, model, jobs)
    finally:
        release_lock(lock_fd)


if __name__ == "__main__":
    raise SystemExit(main())

