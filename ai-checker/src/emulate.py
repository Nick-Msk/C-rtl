"""Deterministic LLM stand-in for scenario tests.

Activated by AI_CHECKER_EMULATE_LLM=1. Fixtures live in
tests/fixtures/<project>/{check,classifier}.json (override dir with
AI_CHECKER_FIXTURES). No network, no db import: pure stdlib.
"""
from __future__ import annotations

import json
import os
from pathlib import Path

AI_ROOT = Path(__file__).resolve().parent.parent
FIXTURES = Path(os.environ.get("AI_CHECKER_FIXTURES")
                or (AI_ROOT / "tests" / "fixtures"))


class MissingFixture(RuntimeError):
    pass


def enabled() -> bool:
    return os.environ.get("AI_CHECKER_EMULATE_LLM", "0") == "1"


def _load(project: str, phase: str) -> dict:
    p = FIXTURES / project / f"{phase}.json"
    if not p.is_file():
        raise MissingFixture(f"emulate: missing fixture {p}")
    return json.loads(p.read_text(encoding="utf-8"))


def check_response(project: str, chunk_idx: int) -> str:
    data = _load(project, "check")
    key = str(chunk_idx)
    if key not in data:
        raise MissingFixture(f"emulate: check.json has no chunk {key}")
    return data[key]["report"]


def classifier_verdict(project: str, finding_id: str) -> dict:
    data = _load(project, "classifier")
    if finding_id not in data:
        raise MissingFixture(
            f"emulate: classifier.json has no {finding_id!r}")
    return data[finding_id]