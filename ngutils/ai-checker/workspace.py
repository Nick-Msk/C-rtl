"""Target workspace for ai-checker (location-independent).

The reviewed repo is declared by the embedding project, in priority:
  1. Python config (AI_CHECKER_CONFIG) calling the API below,
  2. env AI_CHECKER_ROOT / AI_CHECKER_PROJECTS,
  3. legacy fallback: the dir containing ai-checker.

The config runs at IMPORT time, before common.py computes ROOT.
"""
from __future__ import annotations
import os
import runpy
from pathlib import Path

HERE = Path(__file__).resolve().parent
_CONFIG_DIR = HERE
_root: Path | None = None
_names: list[str] | None = None


def add_projects_root(path: str | Path) -> None:
    """Repo root holding the projects; relative paths resolve against
    the config file's directory."""
    global _root
    p = Path(path)
    _root = (p if p.is_absolute() else _CONFIG_DIR / p).resolve()


def add_projects(*names: str) -> None:
    """Restrict review/fix to these dir names; omit => auto-discover."""
    global _names
    _names = list(names)


def _load_env_config() -> None:
    global _CONFIG_DIR
    cfg = os.environ.get("AI_CHECKER_CONFIG")
    if cfg and Path(cfg).is_file():
        _CONFIG_DIR = Path(cfg).resolve().parent
        runpy.run_path(cfg, run_name="__ai_checker_config__")


_load_env_config()


def root() -> Path:
    if _root is not None:
        return _root
    env = os.environ.get("AI_CHECKER_ROOT")
    if env:
        return Path(env).resolve()
    return HERE.parent                      # legacy nested layout


def project_names() -> list[str] | None:
    if _names is not None:
        return _names
    env = os.environ.get("AI_CHECKER_PROJECTS")
    if env:
        return env.split()
    return None
