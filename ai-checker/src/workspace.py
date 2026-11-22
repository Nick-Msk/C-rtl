"""Workspace resolution: which repo we review, which projects it has.

The tool lives OUTSIDE the reviewed repo, so the repo declares itself
in <name>.workspace.py. This module loads it. Tool settings stay in
config/check.db — the descriptor describes the REPO only.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

SUFFIX = ".workspace.py"


class Workspace:
    """Builder handed to the descriptor as the name `workspace`."""

    def __init__(self, file: Path):
        self.file = file
        self._root: Path = file.parent
        self._projects: list[str] = []
        self._exclude: set[str] = set()

    def root(self, rel: str = ".") -> None:
        self._root = (self.file.parent / rel).resolve()

    def projects(self, *names: str) -> None:
        self._projects.extend(names)

    def exclude(self, *names: str) -> None:
        self._exclude.update(names)

    @property
    def root_path(self) -> Path:
        return self._root

    def project_paths(self) -> list[Path]:
        out: list[Path] = []
        for n in self._projects:
            if n in self._exclude:
                continue
            p = self._root / n
            if not p.is_dir():
                print(f"[workspace] warn: '{n}' declared but missing: {p}",
                      file=sys.stderr)
                continue
            out.append(p)
        return out


def _load(file: Path) -> Workspace:
    ws = Workspace(file)
    # exec, not import: the filename carries dots and a hyphen, so it
    # is not a module name. `workspace` is pre-bound to THIS instance —
    # no module-level global state, two descriptors cannot interfere.
    code = compile(file.read_text(encoding="utf-8"), str(file), "exec")
    exec(code, {"workspace": ws, "__file__": str(file)})  # noqa: S102
    return ws


def _candidates(start: Path) -> list[Path]:
    """Nearest descriptor: this dir, then its immediate subdirs, then up."""
    for d in (start, *start.parents):
        here = sorted(d.glob(f"*{SUFFIX}"))
        if here:
            return here
        try:
            near = sorted(p for sub in d.iterdir() if sub.is_dir()
                          for p in sub.glob(f"*{SUFFIX}"))
        except OSError:
            near = []
        if near:
            return near
    return []


def resolve(explicit: str | None = None) -> Workspace | None:
    """explicit > AI_CHECKER_WORKSPACE > nearest descriptor > None.

    None (not found) is NOT fatal: status.py / params.py only read the
    DB and must work without a repo. Ambiguity IS fatal — never guess.
    """
    for src in (explicit, os.environ.get("AI_CHECKER_WORKSPACE")):
        if src:
            f = Path(src).expanduser().resolve()
            if not f.is_file():
                sys.exit(f"ERROR: workspace file not found: {f}")
            return _load(f)
    cands = _candidates(Path.cwd())
    if len(cands) > 1:
        sys.exit("ERROR: ambiguous workspace — set AI_CHECKER_WORKSPACE "
                 "to one of:\n  " + "\n  ".join(str(c) for c in cands))
    return _load(cands[0]) if cands else None