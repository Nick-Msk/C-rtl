"""Shared helpers for check.py and fix.py.

Constants, regexes, the finding parser, and the flock-based lock.
Nothing here talks to LM Studio, runs git, or knows about LLMs.
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

# ── Paths ────────────────────────────────────────────────────────────
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OUT_ROOT = ROOT / "build" / "ai-checker"
FIX_DIR = OUT_ROOT / "fixes"
LOCK_PATH = OUT_ROOT / ".lock"

# ── Extensions ───────────────────────────────────────────────────────
SRC_EXTS = {".h", ".hpp", ".c", ".cc", ".cpp", ".cxx"}
HEADER_EXTS = {".h", ".hpp"}
CPP_EXTS = {".hpp", ".cc", ".cpp", ".cxx"}

# ── Discovery / edit policy ──────────────────────────────────────────
# Dirs that never contain projects (discovery blacklist).
EXCLUDE_DIRS = {"build", "bkp", "docs", "scripts", "ai-checker",
                ".git", ".github", ".vscode"}

# Top-level dirs the fixer may touch. Skeleton is excluded on purpose.
ALLOWED_TOP_DIRS = {"bool", "checker", "common", "error", "fs2", "log",
                    "path"}

# Files at repo root that are never source.
FORBIDDEN_ROOT_NAMES = {"Makefile", "Makefile.common", "Doxyfile",
                        "Dockerfile.dev", "README.md", "CHANGELOG.md",
                        "DISCLAIMER.md", "LICENSE", ".gitignore"}

# ── Limits ───────────────────────────────────────────────────────────
MAX_CHARS = 80_000          # chunk budget for check.py
MAX_SOURCE_CHARS = 60_000   # source budget for fix.py prompt
DEFAULT_LIMIT = 50          # findings per project in fix.py

# ── Finding-format regexes (shared by renumber and parse) ───────────
PART_RE = re.compile(r"^##[ \t]+Part[ \t]+(\d+)/\d+", re.MULTILINE)
HEADER_NUM_RE = re.compile(r"^###[ \t]+(\d+)\.[ \t]*", re.MULTILINE)
ITEM_SEP = re.compile(
    r"^(?:#{2,4}[ \t]+)?"
    r"(?:\d+(?:\.\d+)*[.)]|[-*])"
    r"[ \t]+",
    re.MULTILINE,
)
FILE_REF_RE = re.compile(
    r"\b([\w./-]+\.(?:c|h|hpp|cc|cpp|cxx))(?::\d+)?")


# ── Lock (flock-based; kernel releases on exit incl. SIGKILL) ───────
def acquire_lock():
    """Exclusive non-blocking flock(2). Returns open fd or None."""
    try:
        import fcntl
    except ImportError:
        print("[ai-checker] warn: no fcntl module, "
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
        sys.exit(
            f"ERROR: another ai-checker/ai-fixer is already running "
            f"(pid {other}).\n"
            f"       Lock: {LOCK_PATH}\n"
            f"       Wait for it to finish, or if truly stuck: kill {other}"
        )
    fd.seek(0)
    fd.truncate()
    fd.write(f"{os.getpid()}\n")
    fd.flush()
    return fd


def release_lock(fd) -> None:
    if fd is None:
        return
    try:
        import fcntl
        fcntl.flock(fd, fcntl.LOCK_UN)
    except (ImportError, OSError):
        pass
    finally:
        fd.close()


# ── Discovery ────────────────────────────────────────────────────────
def discover_projects() -> list[Path]:
    out: list[Path] = []
    for p in sorted(ROOT.iterdir()):
        if not p.is_dir() or p.name in EXCLUDE_DIRS or p.name.startswith("."):
            continue
        if (p / "include").is_dir() or (p / "src").is_dir():
            out.append(p)
    return out


def lang_of(f: Path) -> str:
    return "c" if f.suffix.lower() in (".c", ".h") else "cpp"


# ── Finding parsing ──────────────────────────────────────────────────
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
        items = ITEM_SEP.split(body)
        for item in items[1:]:
            text = item.strip()
            if len(text) >= 20:
                findings.append({"section": m.group(1), "text": text})
    return findings


def iter_findings(report_path: Path, include_minor: bool,
                  limit: int) -> list[dict]:
    text = report_path.read_text(encoding="utf-8")
    findings = parse_findings(text, include_minor)
    if limit and len(findings) > limit:
        print(f"warning: {len(findings)} findings, capped to {limit} "
              f"(use --limit 0 for all)", file=sys.stderr)
        findings = findings[:limit]
    return findings


# ── Renumbering (used by check.py; coupled to parse_findings format) ─
def renumber_findings(body: str) -> str:
    """Prefix each finding number with its chunk number.

    Chunk 1, id 1 -> '### 11.'   chunk 2, id 1 -> '### 21.'
    Id is per-chunk, so identification is unambiguous as long as a
    project does not need 10+ chunks.
    """
    if not body:
        return body
    lines = body.split("\n")
    chunk = 1
    gid = 0
    for i, line in enumerate(lines):
        m = PART_RE.match(line)
        if m:
            chunk = int(m.group(1))
            gid = 0
            continue
        m = HEADER_NUM_RE.match(line)
        if not m:
            continue
        gid += 1
        lines[i] = HEADER_NUM_RE.sub(f"### {chunk}{gid}. ", line, count=1)
    return "\n".join(lines)


# ── Path resolution for findings ─────────────────────────────────────
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


def gather_files(project: Path, include_tests: bool) -> list[Path]:
    """All source files of a project (check.py use)."""
    subs = ["include", "src"]
    if include_tests:
        subs.append("test")
    files: list[Path] = []
    for sub in subs:
        d = project / sub
        if not d.is_dir():
            continue
        for f in sorted(d.rglob("*")):
            if f.is_file() and f.suffix.lower() in SRC_EXTS:
                files.append(f)
    return files


def gather_context_files(finding: dict, project: Path) -> list[Path]:
    """Files mentioned in a finding, or whole project as fallback.

    Prefers .c/.cpp over .h/.hpp when falling back, because the type
    digest already covers declarations and is prepended separately.
    """
    hits: list[Path] = []
    seen: set[Path] = set()
    for token in FILE_REF_RE.findall(finding["text"]):
        p = resolve_path(token, project)
        if p and p not in seen:
            seen.add(p)
            hits.append(p)
    if hits:
        return hits
    files_c: list[Path] = []
    files_h: list[Path] = []
    for sub in ("src", "test", "include"):
        d = project / sub
        if not d.is_dir():
            continue
        for f in sorted(d.rglob("*")):
            if not f.is_file() or f.suffix.lower() not in SRC_EXTS:
                continue
            if f.suffix.lower() in HEADER_EXTS:
                files_h.append(f)
            else:
                files_c.append(f)
    return files_c + files_h


# ── Type-digest location (used by both sides) ───────────────────────
def types_digest_path(project_name: str) -> Path:
    return OUT_ROOT / project_name / "_types.h"

