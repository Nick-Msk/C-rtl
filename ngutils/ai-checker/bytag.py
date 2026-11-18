"""Doxygen-tag-driven context extraction for AI review.

Standalone: stdlib only, no imports from check/common/db. Runnable and
testable on its own, reusable by the future tree engine.

Doc-anchored, not declaration-anchored: every function/macro in ngutils
carries a doxygen block, so we walk the blocks and grab what follows.
No top-level C splitter is needed -> nothing duplicated from check.py.

Tags (doxygen only, backticked): `[[constructor]]`, `[[core]]`, and
`[[core-header]]` in the @file block (= the whole header is core).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

KNOWN_TAGS = frozenset({"core", "constructor", "core-header"})
# Selection order; on overflow the last bucket is dropped first.
BUCKETS = ("constructor", "core", "trivial")

C_KEYWORDS = frozenset({
    "sizeof", "typeof", "_Generic", "_Alignof", "_Alignas",
    "_Static_assert", "_Noreturn", "_Atomic", "_Thread_local",
})

DOC_RE = re.compile(r"/\*\*(.*?)\*/", re.DOTALL)
TAG_RE = re.compile(r"\[\[([a-z][a-z-]*)\]\]")
KEEP = ("@brief", "@pre", "@post", "@raise", "@warning", "@note", "@return")
NAME_RE = re.compile(r"([A-Za-z_]\w*)\s*\(")
CONST_RE = re.compile(r"([A-Za-z_]\w*)\s*(?:=|;)")
DEFINE_RE = re.compile(r"[ \t]*#[ \t]*define[ \t]+(\w+)")
BLOCK_CMT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_CMT_RE = re.compile(r"//[^\n]*")


@dataclass
class Symbol:
    name: str
    kind: str                    # macro | inline | proto | const
    text: str                    # declaration, comments stripped
    doc: str                     # compact contract
    tags: frozenset[str]
    line: int

    def bucket(self, file_tags: frozenset[str]) -> str:
        if "constructor" in self.tags:
            return "constructor"
        if "core" in self.tags or "core-header" in file_tags:
            return "core"
        return "trivial"

    def render(self) -> str:
        return (f"/* {self.doc} */\n" if self.doc else "") + self.text


@dataclass
class Header:
    path: str
    text: str 
    tags: frozenset[str]
    symbols: list[Symbol]


def _contract(block: str) -> str:
    """@brief/@pre/@raise/@warning/@note/@return, with continuations."""
    keep: list[str] = []
    active = False
    for raw in block.split("\n"):
        ln = re.sub(r"^\s*\*?\s?", "", raw).strip()
        tag = next((t for t in KEEP if ln.startswith(t)), None)
        if tag:
            keep.append(f"{tag} {ln[len(tag):].strip()}")
            active = True
        elif ln.startswith("@"):
            active = False        # @param, @code, ... dropped
        elif active and ln and keep:
            keep[-1] += " " + ln
    return " | ".join(keep)


def _grab(tail: str) -> tuple[str, str] | None:
    """(declaration, kind) for whatever follows a doc block."""
    s = tail.lstrip()
    # A second doc block owns the declaration, not this one.
    while s.startswith("/*") and not s.startswith("/**"):
        end = s.find("*/")
        if end == -1:
            return None
        s = s[end + 2:].lstrip()
    if not s or s.startswith("/**"):
        return None

    m = DEFINE_RE.match(s)
    if m:
        lines = s.split("\n")
        buf, i = [lines[0]], 0
        while lines[i].rstrip().endswith("\\") and i + 1 < len(lines):
            i += 1
            buf.append(lines[i])
        return "\n".join(buf), "macro"
    if s.startswith("#"):
        return None

    brace, semi = s.find("{"), s.find(";")
    if brace != -1 and (semi == -1 or brace < semi):
        depth, j = 0, brace
        while j < len(s):
            if s[j] == "{":
                depth += 1
            elif s[j] == "}":
                depth -= 1
                if depth == 0:
                    return s[:j + 1], "inline"
            j += 1
        return None
    if semi != -1:
        decl = s[:semi + 1]
        return decl, ("proto" if NAME_RE.search(decl) else "const")
    return None


def scan(text: str, path: str = "") -> Header:
    file_tags: set[str] = set()
    symbols: list[Symbol] = []
    seen: set[str] = set()
    for m in DOC_RE.finditer(text):
        block = m.group(1)
        tags = frozenset(t for t in TAG_RE.findall(block) if t in KNOWN_TAGS)
        if "@file" in block:
            file_tags |= tags
            continue
        got = _grab(text[m.end():])
        if got is None:
            continue
        raw, kind = got
        if kind == "macro":
            name = DEFINE_RE.match(raw).group(1)
        else:
            head = raw.split("{", 1)[0]
            # A struct FIELD decl (e.g. `size_t size;` after a field doc block)
            # has no '(' and no '=' — not an API symbol, skip it.
            if kind in ("proto", "const") and "(" not in head and "=" not in head:
                continue
            nm = NAME_RE.search(head) or CONST_RE.search(head)
            if not nm:
                continue
            name = nm.group(1)
        if name in seen:
            continue
        seen.add(name)
        body = LINE_CMT_RE.sub("", BLOCK_CMT_RE.sub("", raw)).strip()
        symbols.append(Symbol(name, kind, body, _contract(block), tags,
                              text.count("\n", 0, m.start()) + 1))
        
    return Header(path, text, frozenset(file_tags), symbols)


def build_block(headers: list[Path], budget: int,
                *, root: Path | None = None) -> tuple[str, dict]:
    """(context block, stats). Deterministic: same headers + budget ->
    identical bytes, so the server can reuse the KV cache across chunks.
    Never make this depend on the chunk contents.
    """
    scanned: list[Header] = []
    for h in headers:
        try:
            text = h.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        try:
            disp = str(h.relative_to(root)) if root else str(h)
        except ValueError:
            disp = str(h)
        scanned.append(scan(text, disp))

    items = [(s.bucket(hd.tags), (hi, s.name), s.render(), hi)
             for hi, hd in enumerate(scanned) for s in hd.symbols]
    hdr_cost = [len(f"/* === from {hd.path} === */") + 2
                for hd in scanned]
    paid: set[int] = set()
    chosen: set[tuple[int, str]] = set()
    used, dropped = 0, []
    for bucket in BUCKETS:
        for b, key, body, hi in items:
            if b != bucket or key in chosen:
                continue
            cost = len(body) + 2 + (0 if hi in paid else hdr_cost[hi])
            if used + cost > budget:
                dropped.append(key[1])
                continue
            chosen.add(key)
            paid.add(hi)
            used += cost

    parts: list[str] = []
    counts = dict.fromkeys(BUCKETS, 0)
    for hi, hd in enumerate(scanned):
        kept = [s for s in hd.symbols if (hi, s.name) in chosen]
        if not kept:
            continue
        parts.append(f"/* === from {hd.path} === */")
        for s in kept:
            counts[s.bucket(hd.tags)] += 1
            parts.append(s.render())
    block = "\n\n".join(parts) + ("\n" if parts else "")
    return block, {"bytes": len(block), "budget": budget,
                   "kept": len(chosen), "counts": counts,
                   "dropped": dropped,
                   "file_tags": {hd.path: sorted(hd.tags)
                                 for hd in scanned if hd.tags}}

def _project_prefixes(known: set[str]) -> list[str]:
    """Derive name prefixes (fs2, fs2arr, FS2ARR, ...) from captured symbols."""

    out: set[str] = set()
    for n in known:
        m = re.match(r"[A-Za-z][A-Za-z0-9]*", n)   # stops at first '_'
        if m and len(m.group(0)) >= 3:
            out.add(m.group(0))
    return sorted(out, key=len, reverse=True)

def load_headers(paths: list[Path], *, root: Path | None = None) -> list[Header]:
    out: list[Header] = []
    for h in paths:
        try:
            text = h.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            print(f"bytag: skip {h}: {exc}", file=sys.stderr)
            continue
        try:
            disp = str(h.relative_to(root)) if root else str(h)
        except ValueError:
            disp = str(h)
        out.append(scan(text, disp))
    return out

def _include_guard(text: str) -> str | None:
    """The `#define` mirroring the leading `#ifndef` — never needs docs."""

    m = re.search(r"^[ \t]*#[ \t]*ifndef[ \t]+(\w+)", text, re.MULTILINE)
    return m.group(1) if m else None

# Drop string/char literals so words inside them ("...borrowed (%u...")
# are not taken for symbols. Do it BEFORE block/line comment stripping.
STR_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.DOTALL)

def missing_docs(text: str, known: set[str]) -> list[str]:
    """Project-prefixed identifiers present in code but never captured
    by a doc block. Prefix-based heuristic — a lint, not ground truth.
    """

    prefixes = _project_prefixes(known)
    if not prefixes:
        return []
    stripped = LINE_CMT_RE.sub("", BLOCK_CMT_RE.sub("", text))

    stripped = STR_RE.sub("", text)
    stripped = LINE_CMT_RE.sub("", BLOCK_CMT_RE.sub("", stripped))

    guard = _include_guard(stripped)
    # \w* MUST apply to every alternative, otherwise `#define fs2arr_attach`
    # captures the bare prefix `fs2arr` instead of the full name.

    alts = "(?:" + "|".join(map(re.escape, prefixes)) + r")\w*"
    
    # '(' only (not '{'): skip struct/enum defs and compound literals,
    # so `typedef struct fs2array {` is not reported as an undocumented symbol.

    used    = re.compile(r"\b(" + alts + r")\s*\(")
    defined = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(" + alts + ")", re.MULTILINE)
    found = set(used.findall(stripped)) | set(defined.findall(stripped))
    found.discard(guard)
    return sorted(found - known - C_KEYWORDS)

def cmd_stats(headers: list[Header], as_json: bool) -> int:
    rows = []
    for hd in headers:
        for s in hd.symbols:
            rows.append({"file": hd.path, "name": s.name, "kind": s.kind,
                         "bucket": s.bucket(hd.tags), "line": s.line,
                         "tags": sorted(s.tags), "bytes": len(s.render())})
    if as_json:
        print(json.dumps(rows, indent=2, ensure_ascii=False))
        return 0
    for r in rows:
        print(f"{r['bucket']:<12}{r['kind']:<7}{r['bytes']:>6}  "
              f"{r['name']:<32}{','.join(r['tags'])}")
    return 0


def cmd_missing(headers: list[Header]) -> int:
    known: set[str] = set()
    for hd in headers:
        known |= {s.name for s in hd.symbols}
    total = 0
    for hd in headers:
        for name in missing_docs(hd.text, known):
            print(f"{hd.path}: {name}")
            total += 1
    if total:
        print(f"/* {total} undocumented symbol(s) */", file=sys.stderr)
        return 1
    print("/* all project-prefixed symbols have a doc block */",
          file=sys.stderr)
    return 0


def cmd_block(headers: list[Header], budget: int, as_json: bool) -> int:
    block, st = build_block(headers, budget)
    if as_json:
        print(json.dumps({"stats": st, "block": block}, indent=2,
                         ensure_ascii=False))
        return 0
    sys.stdout.write(block)
    print(f"\n/* {st['kept']} symbols, {st['bytes']}/{st['budget']} B, "
          f"{st['counts']} */", file=sys.stderr)
    if st["dropped"]:
        print(f"/* OVER BUDGET, dropped {len(st['dropped'])}: "
              f"{', '.join(st['dropped'])} */", file=sys.stderr)
        return 1
    return 0


def _parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("headers", nargs="+", type=Path)
    ap.add_argument("-b", "--budget", type=int, default=20_000)
    ap.add_argument("-r", "--root", type=Path, default=None,
                    help="display paths relative to this root")
    ap.add_argument("--stats", action="store_true", help="inventory only")
    ap.add_argument("--missing", action="store_true",
                    help="list project symbols without a doc block")
    ap.add_argument("--json", action="store_true")
    return ap.parse_args()


def main() -> int:
    a = _parse_args()
    headers = load_headers(a.headers, root=a.root)
    if a.missing:
        return cmd_missing(headers)
    if a.stats:
        return cmd_stats(headers, a.json)
    return cmd_block(headers, a.budget, a.json)


if __name__ == "__main__":
    raise SystemExit(main())
