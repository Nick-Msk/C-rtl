"""AI code review for ngutils projects via LM Studio (OpenAI-compatible API)."""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path

import requests

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OUT_ROOT = ROOT / "build" / "ai-checker"

EXCLUDE_DIRS = {"build", "bkp", "docs", "scripts", "ai-checker",
                ".git", ".github", ".vscode"}
SRC_EXTS = {".h", ".hpp", ".c", ".cc", ".cpp", ".cxx"}
MAX_CHARS = 40_000

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
"""


def discover_projects() -> list[Path]:
    out: list[Path] = []
    for p in sorted(ROOT.iterdir()):
        if not p.is_dir() or p.name in EXCLUDE_DIRS or p.name.startswith("."):
            continue
        if (p / "include").is_dir() or (p / "src").is_dir():
            out.append(p)
    return out


def gather_files(project: Path, include_tests: bool) -> list[Path]:
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


def lang_of(f: Path) -> str:
    return "c" if f.suffix.lower() in (".c", ".h") else "cpp"


def chunk_files(files: list[Path], budget: int = MAX_CHARS):
    chunks: list[list[tuple[Path, str]]] = []
    cur: list[tuple[Path, str]] = []
    size = 0
    for f in files:
        try:
            text = f.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = f.read_text(encoding="utf-8", errors="replace")
        block = f"### File: {f.relative_to(ROOT)}\n```{lang_of(f)}\n{text}\n```\n"
        if cur and size + len(block) > budget:
            chunks.append(cur)
            cur, size = [], 0
        cur.append((f, block))
        size += len(block)
    if cur:
        chunks.append(cur)
    return chunks


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

def list_models(base_url: str) -> list[str]:
    r = requests.get(f"{base_url.rstrip('/')}/v1/models", timeout=10)
    r.raise_for_status()
    return [m["id"] for m in r.json().get("data", [])]


def pick_model(models: list[str]) -> str:
    for m in models:
        if "qwen" in m.lower():
            return m
    return models[0]


def ask(base_url: str, model: str, messages: list[dict],
        stream: bool, usage_out: dict | None = None):
    url = f"{base_url.rstrip('/')}/v1/chat/completions"
    payload = {"model": model, "messages": messages,
               "temperature": 0.2, "stream": stream}
    if stream:
        # OpenAI-compatible; LM Studio >= 0.3 honours this and puts
        # the usage block into the last SSE event before [DONE].
        payload["stream_options"] = {"include_usage": True}
    r = requests.post(url, json=payload, stream=stream, timeout=900)
    r.raise_for_status()
    if not stream:
        data = r.json()
        if usage_out is not None:
            usage_out.update(data.get("usage") or {})
        yield data["choices"][0]["message"]["content"]
        return
    r.encoding = "utf-8"
    for line in r.iter_lines(decode_unicode=True):
        if not line or not line.startswith("data:"):
            continue
        chunk = line[5:].strip()
        if chunk == "[DONE]":
            break
        try:
            ev = json.loads(chunk)
        except json.JSONDecodeError:
            continue
        u = ev.get("usage")
        if u and usage_out is not None:
            usage_out.update(u)
        choices = ev.get("choices") or []
        if not choices:
            continue
        piece = choices[0].get("delta", {}).get("content")
        if piece:
            yield piece


def main() -> int:
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--all", action="store_true",
                   help="review every discovered project (default)")
    g.add_argument("--project", help="review a single project by name")
    g.add_argument("--file", help="review a single source file")
    ap.add_argument("--model", default=os.environ.get("QWEN_MODEL", ""))
    ap.add_argument("--base-url",
                    default=os.environ.get("LMSTUDIO_URL",
                                           "http://127.0.0.1:1234"))
    ap.add_argument("--no-tests", action="store_true",
                    help="exclude test/ directories from the review")
    ap.add_argument("--no-stream", action="store_true")
    ap.add_argument("--models", action="store_true",
                    help="list models loaded in LM Studio")
    args = ap.parse_args()

    try:
        models = list_models(args.base_url)
    except requests.RequestException as e:
        sys.exit(f"ERROR: cannot reach LM Studio at {args.base_url}: {e}\n"
                 f"       Start LM Studio, load a Qwen model, "
                 f"or set LMSTUDIO_URL=...")

    if args.models:
        for m in models:
            print(m)
        return 0

    if not models:
        sys.exit("ERROR: LM Studio reports no loaded models")

    model = args.model or pick_model(models)
    print(f"[ai-checker] endpoint={args.base_url} model={model}", file=sys.stderr)

    if args.file:
        f = Path(args.file)
        if not f.is_absolute():
            f = (Path.cwd() / f).resolve()
        if not f.is_file():
            sys.exit(f"ERROR: file not found: {f}")
        jobs = [(f.stem, [f])]
    else:
        projects = discover_projects()
        if args.project:
            projects = [p for p in projects if p.name == args.project]
            if not projects:
                sys.exit(f"ERROR: unknown project '{args.project}'")
        jobs = [(p.name, gather_files(p, include_tests=not args.no_tests))
                for p in projects]

    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    ts = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    stream = not args.no_stream

    for name, files in jobs:
        if not files:
            print(f"[ai-checker] {name}: no sources, skipped", file=sys.stderr)
            continue
        chunks = chunk_files(files)

        # '--no-tests' is only meaningful for project mode; in file mode
        # the single file is whatever the user pointed at, tag it as-is.
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

        buf: list[str] = [f"# AI review: {name}\n\n_model: `{model}`_\n\n@USAGE@\n\n"]
        usage = {"prompt_tokens": 0, "completion_tokens": 0,
                 "total_tokens": 0, "requests": 0}

        for i, chunk in enumerate(chunks, 1):
            if len(chunks) > 1:
                hdr = f"## Part {i}/{len(chunks)}\n\n"
                buf.append(hdr)
                sys.stdout.write(hdr)
            user = build_user_prompt(name, chunk)
            chunk_usage: dict = {}
            try:
                for piece in ask(args.base_url, model,
                                 [{"role": "system", "content": SYSTEM_PROMPT},
                                  {"role": "user",   "content": user}],
                                 stream=stream, usage_out=chunk_usage):
                    sys.stdout.write(piece)
                    sys.stdout.flush()
                    buf.append(piece)
                for k in ("prompt_tokens", "completion_tokens", "total_tokens"):
                    usage[k] += chunk_usage.get(k, 0)
                usage["requests"] += 1
                if chunk_usage:
                    print(f"\n[ai-checker]   chunk {i}/{len(chunks)}: "
                          f"prompt={chunk_usage.get('prompt_tokens', 0):,} "
                          f"completion={chunk_usage.get('completion_tokens', 0):,}",
                          file=sys.stderr)
            except Exception as e:  # noqa: BLE001
                msg = f"\n\n[error: {e}]\n"
                print(msg)
                buf.append(msg)
            buf.append("\n\n")
            print()

        content = "".join(buf).replace("@USAGE@", format_usage_md(usage), 1)
        report.write_text(content, encoding="utf-8")
        print(f"[ai-checker] done: {name} -> {report_rel}  "
              f"[{usage['prompt_tokens']:,} in / "
              f"{usage['completion_tokens']:,} out]", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
