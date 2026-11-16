"""LM Studio client. ALL HTTP calls to the LLM go through here.

No sqlite3, no git, no file I/O — only HTTP + config lookup.
Callers pass messages, get strings back. Failure modes are
requests.RequestException (re-exported) and LLMError.
"""
from __future__ import annotations

import json
import os
import sys

import requests

import db

DEFAULT_URL = "http://127.0.0.1:1234"

# Re-export so callers can `except llm.RequestException:` without
# importing requests themselves.
RequestException = requests.RequestException


class LLMError(Exception):
    """Reserved for semantic errors (no models, invalid payload)."""


# ── Config resolution ────────────────────────────────────────────────
# Priority: explicit CLI > DB param > env var > default.
# Env vars are kept for backward compatibility with pre-DB setups.
def resolve_url(explicit: str = "") -> str:
    if explicit:
        return explicit
    v = db.get_param("llm", "url", "")
    if v:
        return str(v)
    return os.environ.get("LMSTUDIO_URL", DEFAULT_URL)


def resolve_model(base_url: str, explicit: str = "") -> str:
    if explicit:
        return explicit
    v = db.get_param("llm", "model_name", "")
    if v:
        return str(v)
    env = os.environ.get("QWEN_MODEL", "")
    if env:
        return env
    models = list_models(base_url)
    if not models:
        raise LLMError(f"no models loaded at {base_url}")
    return pick_model(models)


def pick_model(models: list[str], preferred: str = "") -> str:
    if preferred:
        return preferred
    for m in models:
        if "qwen" in m.lower():
            return m
    return models[0]


# ── HTTP primitives ─────────────────────────────────────────────────
def list_models(base_url: str) -> list[str]:
    r = requests.get(f"{base_url.rstrip('/')}/v1/models", timeout=10)
    r.raise_for_status()
    return [m["id"] for m in r.json().get("data", [])]


def check_health(base_url: str) -> list[str]:
    """Caller-friendly wrapper: list_models or sys.exit with a clear msg."""
    try:
        models = list_models(base_url)
    except RequestException as exc:
        sys.exit(f"ERROR: cannot reach LM Studio at {base_url}: {exc}")
    if not models:
        sys.exit("ERROR: LM Studio reports no loaded models")
    return models


def chat(base_url: str, model: str, messages: list[dict],
         *, temperature: float = 0.2, stream: bool = False,
         usage_out: dict | None = None):
    """POST /v1/chat/completions.

    Streaming: yields str pieces; final SSE event may carry `usage`.
    Non-stream: yields exactly one str (the full response).
    """
    url = f"{base_url.rstrip('/')}/v1/chat/completions"
    payload = {
        "model": model,
        "messages": messages,
        "temperature": temperature,
        "stream": stream,
    }
    if stream:
        payload["stream_options"] = {"include_usage": True}
    r = requests.post(url, json=payload, stream=stream, timeout=600)
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


def chat_once(base_url: str, model: str, messages: list[dict],
              *, temperature: float = 0.2,
              usage_out: dict | None = None) -> str:
    """Non-streaming convenience: return the full response string."""
    out = ""
    for piece in chat(base_url, model, messages,
                      temperature=temperature, stream=False,
                      usage_out=usage_out):
        out = piece
    return out
