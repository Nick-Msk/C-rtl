"""Shared helpers for check.py and fix.py.

Constants, regexes, the finding parser, and the flock-based lock.
Nothing here talks to LM Studio, runs git, or knows about LLMs.
"""
from __future__ import annotations

import os
import re
import sys
from functools import lru_cache
from pathlib import Path

import workspace as _ws

# ── Paths ────────────────────────────────────────────────────────────
AI_ROOT = Path(__file__).resolve().parent.parent      # .../ai-checker
_WS = _ws.resolve()
ROOT = _WS.root_path if _WS else Path.cwd()
OUT_ROOT = Path(os.environ.get("AI_CHECKER_OUT") or (AI_ROOT / "reports"))
LOCK_PATH = OUT_ROOT / ".lock"
FIX_DIR = OUT_ROOT / "fixes"

# ── Finding parsing ──────────────────────────────────────────────────
PART_HDR_RE = re.compile(r"Part[ \t]+(\d+)/\d+")
TABLE_LOCAL_BASE = 1000

# ── Extensions ───────────────────────────────────────────────────────
SRC_EXTS = {".h", ".hpp", ".c", ".cc", ".cpp", ".cxx"}
HEADER_EXTS = {".h", ".hpp"}
CPP_EXTS = {".hpp", ".cc", ".cpp", ".cxx"}

# ── Discovery / edit policy ──────────────────────────────────────────
# Dirs that never contain projects (discovery blacklist).
EXCLUDE_DIRS = {"build", "bkp", "docs", "scripts", "ai-checker",
                "skeleton", ".git", ".github", ".vscode"}

# Files at repo root that are never source.
FORBIDDEN_ROOT_NAMES = {"Makefile", "Makefile.common", "Doxyfile",
                        "Dockerfile.dev", "README.md", "CHANGELOG.md",
                        "DISCLAIMER.md", "LICENSE", ".gitignore"}

# ── Limits (defaults; live values come from db.params) ─────────────
MAX_CHARS = 80_000          # chunk budget for check.py - check.max_chars
MAX_SOURCE_CHARS = 100_000  # source budget for fix.py prompt - fix.max_source_chars
DEFAULT_LIMIT = 50          # findings per project in fix.py - fix.default_limit

# ── Finding-format regexes (shared by renumber and parse) ───────────
PART_RE = re.compile(r"^##[ \t]+Part[ \t]+(\d+)/\d+", re.MULTILINE)
HEADER_NUM_RE = re.compile(r"^###[ \t]+(\d+)\.[ \t]*", re.MULTILINE)
ITEM_SEP = re.compile(
    r"^(?:#{2,4}[ \t]+)?"
    r"(?:\d+(?:[./]\d+)*[.)]|[-*])"   # 1.  1.1.  1/10.  1/10.1.  -  *
    r"[ \t]+",
    re.MULTILINE,
)

FINDING_HEADER_SPLIT = re.compile(
    r"^###[ \t]+\d+(?:[./]\d+)*\.[ \t]+",
    re.MULTILINE,
)

# Markdown table (used in 'Мелочи' sections):
#   | 16 | log_preambule | lv + 1 ... |
TABLE_ROW_RE = re.compile(r"^[ \t]*\|(.+)\|[ \t]*$", re.MULTILINE)
TABLE_SEP_CELL_RE = re.compile(r"^:?-+:?$")

# A finding whose whole verdict is "the code is fine" is not a finding.
# Anchored at the END of the text and preceded by a sentence/cell
# boundary, so "Всё ок, но ..." or "проверить, что всё ок" survive.
_NO_OP_RE = re.compile(
    r"(?:^|[.!?;:]\s+|\n)\s*"
    r"(?:вс[ёе]\s+(?:ок|ok|в\s+порядке|корректно|хорошо)"
    r"|замечаний\s+нет|нет\s+замечаний"
    r"|проблем\s+не\s+(?:найдено|обнаружено)"
    r"|no\s+issues?(?:\s+found)?|looks\s+good|lgtm)"
    r"\s*[.!]?\s*$",
    re.IGNORECASE)

FILE_REF_RE = re.compile(
    r"\b([\w./-]+\.(?:c|h|hpp|cc|cpp|cxx))(?::\d+)?")

@lru_cache(maxsize=1)
def allowed_top_dirs() -> frozenset[str]:
    """Top-level dirs the fixer may touch = discovered projects.

    Derived, not hardcoded: a project created by 'make cloneproj NAME=x'
    is fixable immediately. skeleton/build/bkp stay out via EXCLUDE_DIRS.
    Cached per process — runs are short-lived.
    """

    return frozenset(p.name for p in discover_projects())


if _WS is None:
    print(f"[ai-checker] warn: no *{_ws.SUFFIX} found from {Path.cwd()}; "
          f"project commands will fail (DB-only commands still work)",
          file=sys.stderr)


def discover_projects() -> list[Path]:
    if _WS is None:
        sys.exit(f"ERROR: no workspace descriptor. Create <name>{_ws.SUFFIX} "
                 f"in the repo you review, or set AI_CHECKER_WORKSPACE.")
    return _WS.project_paths()

def rel_display(p: Path) -> str:
    """Human-readable path relative to the workspace (AI_ROOT.parent),
    which holds BOTH the tool and the reviewed repo. Tool artifacts
    (reports, fix logs) live OUTSIDE ROOT, so relative_to(ROOT) raises."""
    p = Path(p).resolve()
    anchor = AI_ROOT.parent
    try:
        return str(p.relative_to(anchor))
    except ValueError:
        return str(p)

def rotate_reports(project_dir: Path) -> list[Path]:
    """Move all *.md except the newest into <project_dir>/hist/.

    Returns the list of moved paths (for logging). Only `*.md` is
    touched; `_types.h` and other files stay put. `_files/` handled
    by the same function if called with that directory.
    """
    reports = sorted(project_dir.glob("*.md"),
                     key=lambda p: p.stat().st_mtime)
    if len(reports) <= 1:
        return []
    hist = project_dir / "hist"
    hist.mkdir(exist_ok=True)
    moved: list[Path] = []
    for p in reports[:-1]:
        target = hist / p.name
        # If we already have a file with this name in hist (rare
        # but possible after clock skew), add a numeric suffix.
        if target.exists():
            stem, suffix = p.stem, p.suffix
            i = 1
            while (hist / f"{stem}.{i}{suffix}").exists():
                i += 1
            target = hist / f"{stem}.{i}{suffix}"
        p.rename(target)
        moved.append(target)
    return moved


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


def lang_of(f: Path) -> str:
    return "c" if f.suffix.lower() in (".c", ".h") else "cpp"


def _parse_table_rows(body: str) -> list[tuple[str, str]]:
    """Return (id, text) for each data row of the markdown table.

    `id` is the first cell if it looks numeric, else 1-based row index.
    `text` renders all cells as '<header>: <cell>' joined by '; '.
    """
    out: list[tuple[str, str]] = []
    header: list[str] | None = None
    row_no = 0
    for line in body.split("\n"):
        m = TABLE_ROW_RE.match(line)
        if not m:
            header = None
            row_no = 0
            continue
        cells = [c.strip() for c in m.group(1).split("|")]
        if not cells:
            continue
        if all(not c or TABLE_SEP_CELL_RE.match(c) for c in cells):
            continue
        if header is None:
            header = cells
            continue
        row_no += 1
        parts: list[str] = []
        for i, c in enumerate(cells):
            if not c:
                continue
            label = header[i] if i < len(header) else f"col{i + 1}"
            parts.append(f"{label}: {c}")
        if not parts:
            continue
        text = "; ".join(parts)
        
        # Always use the 1-based row position, ignore the model's '#'
        # column (it often continues numbering from earlier sections).
        fid = str(row_no)
        out.append((fid, text))
    return out


def parse_findings(report_text: str) -> list[dict]:
    """Split report into findings with guaranteed-unique ids.

    ids are assigned here, not taken from the text: '<chunk>/<n>'
    for headers and lists, '<chunk>/<1000+n>' for tables.
    An id is reserved BEFORE the no-op filter, so dropping a row
    never renumbers the rows after it (ids are DB keys).
    """
    targets = {"Критично", "Важно", "Мелочи"}
    findings: list[dict] = []
    current_chunk = 1
    counters: dict[tuple[int, str], int] = {}

    sections = re.split(r"^## ", report_text, flags=re.MULTILINE)
    for sec in sections:
        m = re.match(r"^(\S+)", sec)
        if not m:
            continue
        name = m.group(1)
        if name == "Part":
            pm = PART_HDR_RE.match(sec)
            if pm:
                current_chunk = int(pm.group(1))
            continue
        if name not in targets:
            continue
        body = sec[m.end():]

        def next_id(kind: str, _chunk: int = current_chunk) -> str:
            k = (_chunk, kind)
            counters[k] = counters.get(k, 0) + 1
            n = counters[k]
            if kind in ("table", "list"):
                return f"{_chunk}/{TABLE_LOCAL_BASE + n}"
            return f"{_chunk}/{n}"

        def add(kind: str, text: str,
                _name: str = name, _chunk: int = current_chunk) -> None:
            if len(text) < 20:
                return
            fid = next_id(kind)          # reserve the id even if we drop it
            if _NO_OP_RE.search(text):
                return
            findings.append({"section": _name,
                             "id": fid,
                             "text": text,
                             "chunk": _chunk,
                             "kind": kind})

        matches = list(FINDING_HEADER_SPLIT.finditer(body))
        if matches:
            for i, match in enumerate(matches):
                end = (matches[i + 1].start()
                       if i + 1 < len(matches) else len(body))
                add("header", body[match.end():end].strip())
            continue

        table = _parse_table_rows(body)
        if table:
            for _fid, text in table:
                add("table", text)
            continue

        for item in ITEM_SEP.split(body)[1:]:
            add("list", item.strip())

    return findings

def iter_findings(report_path: Path,
                  limit: int) -> list[dict]:
    text = report_path.read_text(encoding="utf-8")
    findings = parse_findings(text)
    if limit and len(findings) > limit:
        print(f"warning: {len(findings)} findings, capped to {limit} "
              f"(use --limit 0 for all)", file=sys.stderr)
        findings = findings[:limit]
    return findings


# ── Renumbering (used by check.py; coupled to parse_findings format) ─
def renumber_findings(body: str) -> str:
    """Prefix each finding number with '<chunk>/' as a separator.

    Chunk 1, id 10 -> '### 1/10.'   chunk 2, id 3 -> '### 2/3.'
    Id is per-chunk (restarts at 1 in each chunk), so a single-chunk
    report gets '1/1.', '1/2.', ..., '1/14.' and a two-chunk one
    gets '1/1.'..'1/9.', then '2/1.'..'2/7.'.
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
        lines[i] = HEADER_NUM_RE.sub(f"### {chunk}/{gid}. ", line, count=1)
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


def parse_fixlist(raw: str):
    """Matcher over (runid, chunk, local). Shared by fix.py and decide.py.

      93/1/2        exact full key — the form ai-status prints
      93/1/2-5      locals 2..5 of run 93, chunk 1
      93/1/1-*      locals 1..∞
      93/1/*        every local of run 93 chunk 1
      1/2           chunk/local, ANY run (fix.py works inside one report)
      1/*           every local of chunk 1, any run
      *             everything
    """
    if not raw or not raw.strip():
        return None
    exact: set[tuple[int, int, int]] = set()
    pairs: set[tuple[int, int]] = set()          # (chunk, local), any run
    ranges: list[tuple[int | None, int, int, int]] = []
    wild: set[tuple[int | None, int]] = set()    # (run|None, chunk)
    match_all = False

    for tok in (t for t in re.split(r"[,\s]+", raw.strip()) if t):
        if tok == "*":
            match_all = True
            continue
        parts = tok.split("/")
        if len(parts) == 3:
            run, c, l = int(parts[0]), parts[1], parts[2]
        elif len(parts) == 2:
            run, c, l = None, parts[0], parts[1]
        else:
            continue
        try:
            chunk = int(c)
        except ValueError:
            continue
        if l == "*":
            wild.add((run, chunk))
            continue
        m = re.match(r"^(\d+)(?:-(\d+|\*))?$", l)
        if not m:
            continue
        lo = int(m.group(1))
        if m.group(2) is None:
            if run is None:
                pairs.add((chunk, lo))
            else:
                exact.add((run, chunk, lo))
        else:
            hi = 10**9 if m.group(2) == "*" else int(m.group(2))
            ranges.append((run, chunk, lo, hi))

    def matcher(key: tuple[int, int, int]) -> bool:
        run, chunk, local = key
        if match_all or key in exact or (chunk, local) in pairs:
            return True
        if (run, chunk) in wild or (None, chunk) in wild:
            return True
        return any(r in (None, run) and c == chunk and lo <= local <= hi
                   for r, c, lo, hi in ranges)
    return matcher
