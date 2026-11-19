"""Shared report header formatting for check.py and fix.py."""

from __future__ import annotations

from datetime import datetime

USAGE_KEYS = ("prompt_tokens", "completion_tokens", "total_tokens", "requests")


def new_usage() -> dict:
    """Empty LLM usage accumulator. Identical shape in check.py and fix.py."""
    return {k: 0 for k in USAGE_KEYS}


def add_chunk_usage(dst: dict, chunk: dict) -> dict:
    """Fold ONE server response into dst and count the request.

    The server reports token counts only; 'requests' is ours, so it is
    always incremented by exactly one here (never read from chunk).
    """
    for k in ("prompt_tokens", "completion_tokens", "total_tokens"):
        dst[k] = dst.get(k, 0) + chunk.get(k, 0)
    dst["requests"] = dst.get("requests", 0) + 1
    return dst


def merge_usage(dst: dict, src: dict) -> dict:
    """Run-level rollup: dst += src, in place. Missing keys count as 0."""
    for k in USAGE_KEYS:
        dst[k] = dst.get(k, 0) + src.get(k, 0)
    return dst


def usage_line(prefix: str, u: dict) -> str:
    """'[ai-checker] tokens: 12,179 in / 6,973 out (1 request(s))'.

    Returns a string instead of printing: check.py logs to stderr,
    fix.py to stdout — the caller keeps its own stream.
    """
    return (f"[{prefix}] tokens: "
            f"{u.get('prompt_tokens', 0):,} in / "
            f"{u.get('completion_tokens', 0):,} out "
            f"({u.get('requests', 0)} request(s))")

def fmt_dur(sec: float) -> str:
    if sec < 60:
        return f"{sec:.1f}s"
    m, s = divmod(round(sec), 60)
    if m < 60:
        return f"{m}m {s:02d}s"
    h, m = divmod(m, 60)
    return f"{h}h {m:02d}m {s:02d}s"


def format_header_md(title: str, model: str, base_url: str,
                     started: datetime, duration: float,
                     step_durs: list[float], usage: dict,
                     sysinfo: dict) -> str:
    comp = usage.get("completion_tokens", 0)
    total = usage.get("total_tokens", 0) or (
        usage.get("prompt_tokens", 0) + comp)
    tps = (comp / duration) if duration > 0 and comp else None
    step_str = ("  (per step: "
                + ", ".join(fmt_dur(d) for d in step_durs) + ")"
                if len(step_durs) > 1 else "")

    rows = [
        ("started",  started.strftime("%Y-%m-%d %H:%M:%S")),
        ("duration", fmt_dur(duration) + step_str),
        ("model",    f"`{model}`"),
        ("endpoint", f"`{base_url}`"),
        ("tokens",   (f"{usage.get('prompt_tokens', 0):,} prompt / "
                      f"{comp:,} completion / {total:,} total "
                      f"({usage.get('requests', 0)} req)")),
        ("speed",    f"{tps:.1f} completion tok/s" if tps else "n/a"),
        ("host",     sysinfo["host"]),
        ("os",       sysinfo["os"]),
        ("cpu",      f"{sysinfo['cpu']} ({sysinfo['cores']})"),
        ("ram",      sysinfo["ram"]),
        ("python",   sysinfo["python"]),
    ]
    body = "\n".join(f"| {k:<8} | {v} |" for k, v in rows)
    return (f"# {title}\n\n"
            f"| field    | value |\n|----------|-------|\n{body}\n\n")

