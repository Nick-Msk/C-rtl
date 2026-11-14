"""Shared report header formatting for check.py and fix.py."""
from __future__ import annotations

from datetime import datetime


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

