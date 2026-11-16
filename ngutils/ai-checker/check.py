"""AI code review for ngutils projects via LM Studio (OpenAI-compatible API)."""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path

import db
import gitutil
import llm
from common import (
    CPP_EXTS,
    HEADER_EXTS,
    MAX_CHARS,
    OUT_ROOT,
    ROOT,
    acquire_lock,
    discover_projects,
    gather_files,
    lang_of,
    parse_findings,
    release_lock,
    renumber_findings,
    rotate_reports,
)
from report import fmt_dur, format_header_md
from sysinfo import system_info

SYSTEM_PROMPT = """\
Ты — опытный C/C++ ревьюер системных библиотек. Анализируй строго и по делу.
Ищи:
1. Ошибки логики и потенциальные баги
2. UB, утечки памяти, double-free, неинициализированные данные
3. Обработку ошибок и граничные случаи
4. Нарушения thread-safety, если применимо
5. Недоработки API, неудобные интерфейсы
6. Переносимость (Linux/macOS x86/macOS Arm)
7. Несоответствия стилю (если видно)
8. Пробелы в тестах/документации

Формат — Markdown, разделы:
## Критично
## Важно
## Мелочи
## Что хорошо

Не выдумывай проблем. Если код в порядке — так и скажи.

The context below starts with a block titled "_types.h" containing
ALL struct/union/enum/typedef declarations from this project's
headers, extracted verbatim. Use it to look up any type you need.
Do not claim you cannot see a header — the relevant type
declarations are right there in that block.
"""

def chunk_files(files: list[Path], budget: int = MAX_CHARS, *,
                project_name: str = "",
                full_headers: bool = False
                ) -> list[list[tuple[Path | None, str]]]:
    """Pack source files into chunks.

    By default, header files are NOT sent verbatim. Only their type
    declarations (struct/union/enum/typedef) are extracted and
    prepended to EVERY chunk, so the model always sees the types
    without paying for a full 60 KB header per chunk. The digest is
    also dumped to build/ai-checker/<project>/_types.h for inspection.

    Set full_headers=True (--full-headers) to include headers verbatim.
    """

    headers = [f for f in files if f.suffix.lower() in HEADER_EXTS]
    sources = [f for f in files if f.suffix.lower() not in HEADER_EXTS]

    if not sources:
        content_files = headers
        types_block = ""
    elif full_headers:
        content_files = headers + sources
        types_block = ""
    else:
        content_files = sources
        digest = build_types_digest(headers)
        if digest and project_name:
            dump = OUT_ROOT / project_name / "_types.h"
            dump.parent.mkdir(parents=True, exist_ok=True)
            dump.write_text(digest + "\n", encoding="utf-8")
        if digest:
            any_cpp = any(h.suffix.lower() in CPP_EXTS
                                    for h in headers)
            fence = "cpp" if any_cpp else "c"
            types_block = (
                "### File: _types.h "
                "(extracted type declarations from all headers)\n"
                f"```{fence}\n{digest}\n```\n"
            )
        else:
            types_block = ""

    types_size = len(types_block)

    def read_block(f: Path) -> str:
        try:
            text = f.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = f.read_text(encoding="utf-8", errors="replace")
        return (f"### File: {f.relative_to(ROOT)}\n"
                f"```{lang_of(f)}\n{text}\n```\n")

    chunks: list[list[tuple[Path, str]]] = []
    cur: list[tuple[Path, str]] = []
    cur_size = types_size

    for f in content_files:
        block = read_block(f)
        if cur and cur_size + len(block) > budget:
            chunks.append(cur)
            cur, cur_size = [], types_size
        cur.append((f, block))
        cur_size += len(block)
    if cur or not chunks:
        chunks.append(cur)

    result: list[list[tuple[Path | None, str]]] = []
    for chunk in chunks:
        result.append([(None, types_block)] + chunk if types_block else chunk)
    return result

# ── Type digest: extract struct/union/enum/typedef from headers ─────
def _strip_preprocessor(text: str) -> str:
    """Drop preprocessor directives, including multi-line continuations."""
    out = []
    cont = False
    for line in text.split("\n"):
        if cont:
            cont = line.rstrip().endswith("\\")
            continue
        if line.lstrip().startswith("#"):
            cont = line.rstrip().endswith("\\")
            continue
        out.append(line)
    return "\n".join(out)


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def _strip_extern_c(text: str) -> str:
    """Remove `extern "C" {` wrappers so the splitter sees flat content."""
    return re.sub(r'\bextern\s+"C"\s*\{', "", text)


def _split_top_level(text: str) -> list[str]:
    """Split text into top-level declarations.

    A declaration ends at a `;` at brace depth 0. Additionally, a
    function body `{ ... }` at depth 0 ends the current declaration
    (functions have no trailing `;`), unless the declaration started
    with `typedef`, `struct`, `union`, or `enum` — in that case the
    trailing `} name;` is part of the type definition and must be
    kept together.

    Limitation: a function definition whose first word is
    `struct`/`union`/`enum` (e.g. `struct X *foo() { ... }`) will be
    incorrectly merged with the following declaration. Extremely
    rare in practice; add a typedef for the return type to avoid.
    """
    decls: list[str] = []
    buf_start = 0
    depth = 0
    is_type = False
    n = len(text)
    i = 0
    while i < n:
        if i == buf_start and depth == 0:
            # Peek the first word of this top-level declaration.
            j = i
            while j < n and text[j].isspace():
                j += 1
            k = j
            while k < n and (text[k].isalnum() or text[k] == "_"):
                k += 1
            is_type = text[j:k] in ("typedef", "struct", "union", "enum")
            if j > i:
                buf_start = j
                i = j
                continue

        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth = max(0, depth - 1)
            if depth == 0 and not is_type:
                # End of a top-level function body (or similar).
                decl = text[buf_start:i + 1].strip()
                if decl:
                    decls.append(decl)
                buf_start = i + 1
                is_type = False
        elif c == ";" and depth == 0:
            decl = text[buf_start:i + 1].strip()
            if decl:
                decls.append(decl)
            buf_start = i + 1
            is_type = False
        i += 1

    tail = text[buf_start:].strip()
    if tail:
        decls.append(tail)
    return decls

_FORWARD_DECL_RE = re.compile(r"^\s*(?:struct|union|enum)\s+\w+\s*;\s*$")

def _is_type_decl(decl: str) -> bool:
    s = decl.strip()
    if not s:
        return False
    if s.startswith("typedef "):
        return True
    if not s.startswith(("struct ", "union ", "enum ")):
        return False
    # `struct X v = { ... };` is a variable declaration, not a type.
    eq = s.find("=")
    brace = s.find("{")
    if eq != -1 and (brace == -1 or eq < brace):
        return False
    return "{" in s or bool(_FORWARD_DECL_RE.match(s))

def extract_types(text: str) -> str:
    """Extract top-level struct/union/enum/typedef declarations.

    Function prototypes, variable declarations, macros, and everything
    inside function bodies are dropped. Comments are stripped.
    """
    text = _strip_preprocessor(text)
    text = _strip_comments(text)
    text = _strip_extern_c(text)
    kept = [d for d in _split_top_level(text) if _is_type_decl(d)]
    return "\n\n".join(kept)

def build_types_digest(headers: list[Path]) -> str:
    """Combine type declarations from all headers, with provenance."""
    parts: list[str] = []
    for h in headers:
        try:
            text = h.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        digest = extract_types(text)
        if digest:
            parts.append(f"/* === from {h.relative_to(ROOT)} === */\n{digest}")
    return "\n\n".join(parts)

def build_user_prompt(project: str, chunk: list[tuple[Path, str]]) -> str:
    return f"Проект: `{project}`. Проведи ревью исходников ниже.\n" + \
           "\n".join(b for _, b in chunk)



def format_usage_md(u: dict) -> str:
    if not u.get("requests"):
        return "_Token usage: not reported by server._"
    p = u.get("prompt_tokens", 0)
    c = u.get("completion_tokens", 0)
    t = u.get("total_tokens", 0) or (p + c)
    return (f"**Token usage:** {p:,} prompt / {c:,} completion / "
            f"{t:,} total  ({u['requests']} request(s))")

def main() -> int:
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--all", action="store_true",
                   help="review every discovered project (default)")
    g.add_argument("--project", help="review a single project by name")
    g.add_argument("--file", help="review a single source file")

    ap.add_argument("--model", default="")
    ap.add_argument("--base-url", default="")

    ap.add_argument("--no-tests", action="store_true",
                    help="exclude test/ directories from the review")
    ap.add_argument("--no-stream", action="store_true")
    ap.add_argument("--models", action="store_true",
                    help="list models loaded in LM Studio")

    ap.add_argument("--full-headers", action="store_true",
                    help="include full header text instead of extracting "
                         "only struct/union/enum/typedef declarations")
    args = ap.parse_args()

    args.base_url = llm.resolve_url(args.base_url)

    if args.models:
        models = llm.check_health(args.base_url)
        for m in models:
            print(m)
        return 0

    llm.check_health(args.base_url)  # exits on error
    try:
        model = llm.resolve_model(args.base_url, args.model)
    except llm.LLMError as e:
        sys.exit(f"ERROR: {e}")
    print(f"[ai-checker] endpoint={args.base_url} model={model}",
          file=sys.stderr)

    lock_fd = acquire_lock()
    try:
        runid = db.llmrun_start(kind="check", url=args.base_url,
                                name=model)
        print(f"[ai-checker] runid={runid}", file=sys.stderr)

        try:
            rc, usage = _run_checks(args, model, runid)
        except BaseException:
            db.llmrun_finish(runid, status="failed")
            raise

        db.llmrun_finish(runid, status="ok",
                         in_tokens=usage["prompt_tokens"],
                         out_tokens=usage["completion_tokens"],
                         requests=usage["requests"])

        if rc == 0:
            try:
                n_stale, branches = db.close_period(runid)
                if n_stale:
                    print(f"[ai-checker] closed {n_stale} stale issue(s) "
                          f"from previous run(s)", file=sys.stderr)
                for b in branches:
                    if gitutil.delete_branch(b, force=True):
                        print(f"[ai-checker] deleted branch {b}",
                              file=sys.stderr)
            except Exception as exc:  # noqa: BLE001
                print(f"[ai-checker] warn: close_period failed: {exc}",
                      file=sys.stderr)

        return rc
    finally:
        release_lock(lock_fd)

def _run_checks(args, model, runid: int) -> tuple[int, dict]:
    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    stream = not args.no_stream

    sysinfo = system_info()
    db.seed_defaults()
    temperature = db.get_param("check", "temperature", 0.2)
    max_chars = db.get_param("check", "max_chars", MAX_CHARS)

    try:
        (OUT_ROOT / "_machine.json").write_text(
            json.dumps(sysinfo, indent=2, ensure_ascii=False),
            encoding="utf-8")
    except OSError as e:
        print(f"[ai-checker] warn: cannot write _machine.json: {e}",
              file=sys.stderr)
    print(f"[ai-checker] host={sysinfo['host']}  "
          f"cpu={sysinfo['cpu']} ({sysinfo['cores']})  "
          f"ram={sysinfo['ram']}", file=sys.stderr)

    projects = discover_projects()
    if args.project:
        projects = [p for p in projects if p.name == args.project]
        if not projects:
            sys.exit(f"ERROR: unknown project '{args.project}'")
    if args.file:
        f = Path(args.file)
        if not f.is_absolute():
            f = (Path.cwd() / f).resolve()
        if not f.is_file():
            sys.exit(f"ERROR: file not found: {f}")
        jobs = [(f.stem, [f])]
    else:
        jobs = [(p.name, gather_files(p, include_tests=not args.no_tests))
                for p in projects]
    if not jobs:
        sys.exit(f"ERROR: no projects found under {ROOT}")

    all_usage = {"prompt_tokens": 0, "completion_tokens": 0,
                 "total_tokens": 0, "requests": 0}

    for name, files in jobs:
        if not files:
            print(f"[ai-checker] {name}: no sources, skipped", file=sys.stderr)
            continue
        chunks = chunk_files(files, budget=max_chars,
                             project_name=name,
                             full_headers=args.full_headers)

        tag = "_notest" if (args.no_tests and not args.file) else ""
        if args.file:
            sub = OUT_ROOT / "_files"
            report = sub / f"{ts}_{name}{tag}.md"
        else:
            sub = OUT_ROOT / name
            report = sub / f"{ts}{tag}.md"
        sub.mkdir(parents=True, exist_ok=True)
        report_rel = report.relative_to(ROOT)

        if args.no_tests and not args.file:
            print(f"[ai-checker] {name}: --no-tests, test/ excluded",
                  file=sys.stderr)
        print(f"[ai-checker] {name}: {len(files)} files, "
              f"{len(chunks)} chunk(s) -> {report_rel}", file=sys.stderr)

        t_start = time.perf_counter()
        started = datetime.now()
        chunk_durs: list[float] = []
        usage = {"prompt_tokens": 0, "completion_tokens": 0,
                 "total_tokens": 0, "requests": 0}
        body: list[str] = []

        for i, chunk in enumerate(chunks, 1):
            if len(chunks) > 1:
                hdr = f"## Part {i}/{len(chunks)}\n\n"
                body.append(hdr)
                sys.stdout.write(hdr)
            user = build_user_prompt(name, chunk)
            chunk_usage: dict = {}
            t0 = time.perf_counter()
            try:
                for piece in llm.chat(args.base_url, model,
                                      [{"role": "system", "content": SYSTEM_PROMPT},
                                       {"role": "user",   "content": user}],
                                      temperature=temperature,
                                      stream=stream, usage_out=chunk_usage):
                    sys.stdout.write(piece)
                    sys.stdout.flush()
                    body.append(piece)
                dt = time.perf_counter() - t0
                chunk_durs.append(dt)
                for k in ("prompt_tokens", "completion_tokens", "total_tokens"):
                    usage[k] += chunk_usage.get(k, 0)
                usage["requests"] += 1
                print(f"\n[ai-checker]   chunk {i}/{len(chunks)}: "
                      f"{fmt_dur(dt)}  prompt={chunk_usage.get('prompt_tokens', 0):,}  "
                      f"completion={chunk_usage.get('completion_tokens', 0):,}",
                      file=sys.stderr)
            except Exception as e:  # noqa: BLE001
                dt = time.perf_counter() - t0
                chunk_durs.append(dt)
                msg = f"\n\n[error: {e}]\n"
                print(msg)
                body.append(msg)
            body.append("\n\n")
            print()

        total_dur = time.perf_counter() - t_start
        header = format_header_md(name, model, args.base_url, started,
                                  total_dur, chunk_durs, usage, sysinfo)
        body_text = renumber_findings("".join(body))

        kind = "file" if args.file else ("notest" if args.no_tests else "full")
        try:
            ingest_findings = parse_findings(body_text)
            db.ingest_report(
                project=name,
                report_path=report,
                model=model,
                kind=kind,
                started=started,
                duration_s=total_dur,
                findings=ingest_findings,
                runid=runid,
            )
            print(f"[ai-checker] db: ingested {len(ingest_findings)} "
                  f"issue(s)", file=sys.stderr)
        except Exception as e:  # noqa: BLE001
            print(f"[ai-checker] warn: db ingest failed: {e}",
                  file=sys.stderr)

        report.write_text(header + body_text, encoding="utf-8")

        moved = rotate_reports(sub)
        if moved:
            print(f"[ai-checker] rotated {len(moved)} older report(s) "
                  f"-> {sub.relative_to(ROOT)}/hist/", file=sys.stderr)

        print(f"[ai-checker] done: {name} -> {report_rel}  "
              f"[{fmt_dur(total_dur)} · "
              f"{usage['prompt_tokens']:,} in / "
              f"{usage['completion_tokens']:,} out]", file=sys.stderr)

        for k in all_usage:
            all_usage[k] += usage.get(k, 0)

    return 0, all_usage

if __name__ == "__main__":
    raise SystemExit(main())
