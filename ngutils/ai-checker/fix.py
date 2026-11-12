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
import json
import os
import re
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

import requests

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OUT_ROOT = ROOT / "build" / "ai-checker"
FIX_DIR = OUT_ROOT / "fixes"
LOCK_PATH = OUT_ROOT / ".lock"

SRC_EXTS = {".h", ".hpp", ".c", ".cc", ".cpp", ".cxx"}
MAX_FINDING_CHARS = 8_000
MAX_SOURCE_CHARS = 40_000
MAX_DIFF_LINES = 20
DEFAULT_LIMIT = 20

# Only these directories may contain editable files.
ALLOWED_TOP_DIRS = {"bool", "checker", "common", "error", "fs2", "log",
                    "path", "skeleton"}
# Anything at repo root with these suffixes/names is off-limits.
FORBIDDEN_ROOT_NAMES = {"Makefile", "Makefile.common", "Doxyfile",
                        "Dockerfile.dev", "README.md", "CHANGELOG.md",
                        "DISCLAIMER.md", "LICENSE", ".gitignore"}

CLASSIFY_SYSTEM = """\
You are a strict C/C++ code-fix validator. You receive a code-review
finding and the source files it refers to. Decide whether the finding
can be fixed by a LOCAL, SELF-CONTAINED edit, or whether fixing it
requires human thought because it would ripple through the codebase.

A fix is LOCAL only if ALL of the following hold:

1. It touches exactly ONE file.
2. It is confined to ONE function body (or one #include line, or one
   macro definition). Anything that spans function boundaries is
   NON-local.
3. It does NOT change any function signature: parameters, return type,
   qualifiers, calling convention. Adding or removing a parameter is
   FORBIDDEN, because every caller would then need updating.
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
  "direction": "for needs_review: suggested direction for a human (<= 300 chars)",
  "edit": {
    "file": "relative path from repo root",
    "old_snippet": "exact text from the source, with 1-3 lines of context",
    "new_snippet": "replacement text",
    "explanation": "one line"
  }
}

"edit" is present iff class == "local". Rules for "edit":
- "file" must be one of the source files provided.
- "old_snippet" must appear exactly ONCE in that file.
- "new_snippet" must differ and preserve indentation.
- Do NOT reformat surrounding code, add comments, or add blank lines.
- Do NOT touch anything outside the single function body.
"""

# ── Single-instance lock (shared with check.py) ─────────────────────
def acquire_lock():
    try:
        import fcntl
    except ImportError:
        print("[ai-fixer] warn: no fcntl module, "
              "concurrent-run protection disabled", file=sys.stderr)
        return None
    LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
    fd = LOCK_PATH.open("w")
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        try:
            other = LOCK_PATH.read_text().strip() or "?"
        except OSError:
            other = "?"
        fd.close()
        sys.exit(f"ERROR: another ai-checker/ai-fixer is already running "
                 f"(pid {other}).\n"
                 f"       Lock: {LOCK_PATH}\n"
                 f"       Wait for it to finish, or if truly stuck: kill {other}")
    fd.seek(0)
    fd.truncate()
    fd.write(f"{os.getpid()}\n")
    fd.flush()
    return fd


def release_lock(fd):
    if fd is None:
        return
    try:
        import fcntl
        fcntl.flock(fd, fcntl.LOCK_UN)
    except (ImportError, OSError):
        pass
    finally:
        fd.close()

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

# ── Report discovery ─────────────────────────────────────────────────
def discover_projects() -> list[Path]:
    out: list[Path] = []
    for p in sorted(ROOT.iterdir()):
        if p.name not in ALLOWED_TOP_DIRS:
            continue
        if (p / "include").is_dir() or (p / "src").is_dir():
            out.append(p)
    return out


def latest_report_for_project(proj: str) -> Path | None:
    d = OUT_ROOT / proj
    if not d.is_dir():
        return None
    reports = sorted(d.glob("*.md"))
    return reports[-1] if reports else None


def latest_report_for_file(stem: str) -> Path | None:
    d = OUT_ROOT / "_files"
    if not d.is_dir():
        return None
    reports = sorted(d.glob(f"*_{stem}*.md"))
    return reports[-1] if reports else None

# ── Finding extraction ───────────────────────────────────────────────
def parse_findings(report_text: str, include_minor: bool) -> list[dict]:
    targets = {"Критично", "Важно"}
    if include_minor:
        targets.add("Мелочи")
    findings: list[dict] = []
    sections = re.split(r"^## ", report_text, flags=re.MULTILINE)
    for sec in sections:
        m = re.match(r"^(\S+)", sec)
        if not m or m.group(1) not in targets:
            continue
        body = sec[m.end():]
        items = re.split(r"^(?:\d+\.|[-*])\s+", body, flags=re.MULTILINE)
        for item in items[1:]:
            text = item.strip()
            if len(text) >= 20:
                findings.append({"section": m.group(1), "text": text})
    return findings


FILE_REF_RE = re.compile(r"\b([\w./-]+\.(?:c|h|hpp|cc|cpp|cxx))(?::\d+)?")


def resolve_path(token: str, project: Path) -> Path | None:
    candidates = [
        ROOT / token,
        project / token,
        project / "src" / token,
        project / "include" / token,
        project / "test" / token,
    ]
    if "/" in token:
        candidates.append(project / Path(*Path(token).parts[-2:]))
    for c in candidates:
        try:
            c = c.resolve()
            c.relative_to(ROOT)
            if c.is_file() and c.suffix.lower() in SRC_EXTS:
                return c
        except (ValueError, OSError):
            continue
    return None


def gather_context_files(finding: dict, project: Path) -> list[Path]:
    hits: list[Path] = []
    seen: set[Path] = set()
    for token in FILE_REF_RE.findall(finding["text"]):
        p = resolve_path(token, project)
        if p and p not in seen:
            seen.add(p)
            hits.append(p)
    if hits:
        return hits
    files: list[Path] = []
    for sub in ("include", "src", "test"):
        d = project / sub
        if not d.is_dir():
            continue
        for f in sorted(d.rglob("*")):
            if f.is_file() and f.suffix.lower() in SRC_EXTS:
                files.append(f)
    return files


def build_prompt(finding: dict, project: Path, files: list[Path]) -> str:
    parts = [f"Project: {project.name}",
             f"Section: {finding['section']}",
             "Finding:",
             finding["text"],
             "",
             "Source files:"]
    budget = MAX_SOURCE_CHARS
    for f in files:
        try:
            text = f.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        block = f"### {f.relative_to(ROOT)}\n```\n{text}\n```\n"
        if len(block) > budget:
            block = block[:budget] + "\n... (truncated)\n```\n"
        parts.append(block)
        budget -= len(block)
        if budget <= 0:
            break
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


def validate_edit(edit: dict, project: Path) -> tuple[Path | None, str | None]:
    rel = edit.get("file", "")
    old = edit.get("old_snippet", "")
    new = edit.get("new_snippet", "")
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
    try:
        src = p.read_text(encoding="utf-8")
    except (UnicodeDecodeError, OSError) as e:
        return None, f"cannot read {rel}: {e}"
    if src.count(old) != 1:
        return None, f"old_snippet appears {src.count(old)} times (need 1)"
    if old == new:
        return None, "old_snippet == new_snippet"

    # Cascade / ripple checks.
    old_headers: set[str] = set()
    new_headers: set[str] = set()
    for line in old.splitlines():
        h = extract_function_header(line)
        if h:
            old_headers.add(h)
    for line in new.splitlines():
        h = extract_function_header(line)
        if h:
            new_headers.add(h)
    added_headers = new_headers - old_headers
    if added_headers:
        return None, f"adds function: {sorted(added_headers)}"
    removed_headers = old_headers - new_headers
    if removed_headers:
        return None, f"changes/removes function signature: {sorted(removed_headers)}"

    added, removed = diff_line_counts(old, new)
    if added + removed > MAX_DIFF_LINES:
        return None, f"diff too large ({added}+{removed} > {MAX_DIFF_LINES})"
    return p, None


def make_diff(path: Path, old: str, new: str) -> str:
    rel = path.relative_to(ROOT)
    return "".join(difflib.unified_diff(
        old.splitlines(keepends=True), new.splitlines(keepends=True),
        fromfile=f"a/{rel}", tofile=f"b/{rel}", n=2))


def apply_edit(path: Path, old: str, new: str) -> bool:
    try:
        src = path.read_text(encoding="utf-8")
        if src.count(old) != 1:
            return False
        path.write_text(src.replace(old, new, 1), encoding="utf-8")
        return True
    except OSError:
        return False

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


def _iter_findings(report_path: Path, include_minor: bool, limit: int):
    text = report_path.read_text(encoding="utf-8")
    findings = parse_findings(text, include_minor)
    if limit and len(findings) > limit:
        print(f"[ai-fixer] warning: {len(findings)} findings, "
              f"capped to {limit} (use --limit 0 for all)", file=sys.stderr)
        findings = findings[:limit]
    return findings


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


def _handle_finding(idx: int, total: int, finding: dict,
                    project: Path, files: list[Path],
                    args, model: str,
                    fix_state: dict,
                    applied: list[dict], skipped: list[str],
                    needs_review: list[dict]) -> None:
    print(f"\n=== Finding {idx}/{total}  [{finding['section']}] ===")
    print(f"  {finding['text'][:200].replace(chr(10), ' ')}...")
    if not files:
        print("  SKIP: no source files to work with")
        skipped.append(finding["text"][:80])
        return

    try:
        raw = ask_model(args.base_url, model,
                        build_prompt(finding, project, files))
    except requests.RequestException as e:
        print(f"  SKIP: LLM request failed: {e}")
        skipped.append(finding["text"][:80])
        return

    raw = raw.strip()
    if raw.startswith("```"):
        raw = re.sub(r"^```[a-z]*\n?", "", raw)
        raw = re.sub(r"\n?```$", "", raw)
    try:
        verdict = json.loads(raw)
    except json.JSONDecodeError:
        print(f"  SKIP: invalid JSON: {raw[:120]!r}")
        skipped.append(finding["text"][:80])
        return

    cls = verdict.get("class", "needs_review")
    reason = verdict.get("reason", "")

    if cls != "local":
        print(f"  NEEDS REVIEW: {reason}")
        if verdict.get("direction"):
            print(f"    direction: {verdict['direction']}")
        needs_review.append(finding | {"verdict": verdict})
        return

    edit = verdict.get("edit") or {}
    path, err = validate_edit(edit, project)
    if err:
        print(f"  SKIP (invalid edit): {err}")
        skipped.append(finding["text"][:80])
        return

    diff = make_diff(path, edit["old_snippet"], edit["new_snippet"])
    explanation = edit.get("explanation", "")
    print(f"  LOCAL: {explanation}")
    for line in diff.splitlines():
        print(f"    {line}")

    if not args.apply:
        print("  [dry-run] not applied")
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
            return

    # Lazy branch creation on the first applied fix of this run.
    if fix_state["branch"] is None:
        branch = make_branch_name(fix_state["label"])
        git_create_branch(branch, fix_state["base_sha"])
        fix_state["branch"] = branch
        print(f"  (created branch: {branch})")

    if not apply_edit(path, edit["old_snippet"], edit["new_snippet"]):
        print("  SKIP: file changed under us (previous fix shifted context?)")
        skipped.append(finding["text"][:80])
        return

    rel = str(path.relative_to(ROOT))
    msg = _build_commit_msg(finding, rel, reason, explanation)
    try:
        sha = git_commit_file(path, msg)
    except RuntimeError as e:
        # Roll back the file write so the tree stays clean.
        path.write_text(
            path.read_text(encoding="utf-8").replace(
                edit["new_snippet"], edit["old_snippet"], 1),
            encoding="utf-8")
        print(f"  SKIP: {e}")
        skipped.append(finding["text"][:80])
        return

    print(f"  COMMITTED {sha}  {rel}")
    applied.append({"sha": sha, "file": rel,
                    "summary": finding["text"].splitlines()[0][:70]})


def _run(args, model: str, jobs: list[tuple[str, Path, Path]]) -> int:
    base_branch = None
    base_sha = None
    if args.apply:
        base_branch, base_sha = git_clean_check()

    FIX_DIR.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    all_branches: list[tuple[str, str, list[dict]]] = []

    for label, report_path, project in jobs:
        print(f"\n--- {label}: {report_path.relative_to(ROOT)} ---")
        applied: list[dict] = []
        skipped: list[str] = []
        needs_review: list[dict] = []
        fix_state = {"label": label, "base_sha": base_sha, "branch": None}

        findings = _iter_findings(report_path, args.include_minor, args.limit)
        if not findings:
            print("  no findings to process")
            continue

        t0 = time.perf_counter()
        for idx, finding in enumerate(findings, 1):
            files = gather_context_files(finding, project)
            _handle_finding(idx, len(findings), finding, project, files,
                            args, model, fix_state,
                            applied, skipped, needs_review)
        dt = time.perf_counter() - t0

        print(f"\n[ai-fixer] {label}: "
              f"{len(applied)} applied, {len(skipped)} skipped, "
              f"{len(needs_review)} needs review, {dt:.1f}s")

        if applied and fix_state["branch"]:
            branch = fix_state["branch"]
            print(f"[ai-fixer] branch: {branch}")
            print("[ai-fixer] commits:")
            for c in applied:
                print(f"  {c['sha']}  {c['summary']}")
            print(f"[ai-fixer] review:  git log --oneline {base_branch}..{branch}")
            print(f"[ai-fixer] diff:    git diff {base_sha}..{branch}")
            print("[ai-fixer] keep one: git cherry-pick <sha>")
            print(f"[ai-fixer] reject:  git branch -D {branch}")
            print(f"[ai-fixer] back:    git checkout {base_branch}")

            # Save a JSON log of what happened.
            (FIX_DIR / f"{ts}_{label}.json").write_text(json.dumps({
                "project": label,
                "base_branch": base_branch,
                "base_sha": base_sha,
                "fix_branch": branch,
                "started": ts,
                "duration_sec": round(dt, 1),
                "commits": applied,
                "skipped": skipped,
                "needs_review": [
                    {"section": n["section"], "text": n["text"],
                     "reason": n["verdict"].get("reason"),
                     "direction": n["verdict"].get("direction")}
                    for n in needs_review
                ],
            }, indent=2, ensure_ascii=False), encoding="utf-8")

            all_branches.append((label, branch, applied))

        if needs_review:
            print(f"\n=== Needs human thought ({len(needs_review)}) ===")
            for i, item in enumerate(needs_review, 1):
                v = item["verdict"]
                first = item["text"].splitlines()[0][:100]
                print(f"[{i}] {first}")
                if v.get("reason"):
                    print(f"    why: {v['reason']}")
                if v.get("direction"):
                    print(f"    direction: {v['direction']}")

    # Return to base branch if we moved.
    if args.apply and all_branches and base_branch:
        try:
            git("checkout", base_branch)
        except subprocess.CalledProcessError:
            print(f"[ai-fixer] warning: could not return to {base_branch}",
                  file=sys.stderr)

    if all_branches:
        print("\n[ai-fixer] summary of created branches:")
        for label, branch, applied in all_branches:
            print(f"  {label:<12} {branch}  ({len(applied)} commit(s))")
        print(f"[ai-fixer] discard all:  git branch -D "
              f"{' '.join(b for _, b, _ in all_branches)}")

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
    ap.add_argument("--include-minor", action="store_true",
                    help="also process the 'Мелочи' section")
    ap.add_argument("--limit", type=int, default=DEFAULT_LIMIT,
                    help=f"max findings per project (default {DEFAULT_LIMIT}, "
                         f"0 = unlimited)")
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

