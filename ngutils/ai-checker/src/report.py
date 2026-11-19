"""Shared report header formatting for check.py and fix.py."""

from __future__ import annotations

from datetime import datetime

USAGE_KEYS = ("prompt_tokens", "completion_tokens", "total_tokens", "requests")
VOLUME_KEYS = ("chars", "lines")


def new_usage() -> dict:
    """Empty accumulator: server token counters + our prompt volume."""
    return dict.fromkeys((*USAGE_KEYS, *VOLUME_KEYS), 0)


def measure(*texts: str) -> tuple[int, int]:
    """(chars, lines) of a prompt assembled from these message parts.

    Newlines are summed across parts, +1 for the last line, so a
    system+user prompt is measured the same way as a single string.
    """
    return (sum(len(t) for t in texts),
            sum(t.count("\n") for t in texts) + 1)


def merge_usage(dst: dict, src: dict) -> dict:
    """Run-level rollup: dst += src (tokens AND volume), in place."""
    for k in (*USAGE_KEYS, *VOLUME_KEYS):
        dst[k] = dst.get(k, 0) + src.get(k, 0)
    return dst


def volume_str(u: dict) -> str:
    """'39,842 chars / 1,050 lines', or '' when nothing was measured."""
    if not u.get("chars"):
        return ""
    s = f"{u['chars']:,} chars"
    if u.get("lines"):
        s += f" / {u['lines']:,} lines"
    return s


def usage_line(prefix: str, u: dict, *, label: str = "total") -> str:
    """One-line summary: prompt volume + tokens + request count.

    '[ai-checker] total: 39,842 chars / 1,050 lines · 12,443 in / 10,954 out (1 request(s))'
    Returns a string: check.py logs to stderr, fix.py to stdout.
    """
    vol = volume_str(u)
    return (f"[{prefix}] {label}: "
            + (f"{vol} · " if vol else "")
            + f"{u.get('prompt_tokens', 0):,} in / "
              f"{u.get('completion_tokens', 0):,} out "
              f"({u.get('requests', 0)} request(s))")

def add_chunk_usage(dst: dict, chunk: dict) -> dict:
    """Fold ONE server response into dst and count the request.

    The server reports token counts only; 'requests' is ours, so it is
    always incremented by exactly one here (never read from chunk).
    """
    for k in ("prompt_tokens", "completion_tokens", "total_tokens"):
        dst[k] = dst.get(k, 0) + chunk.get(k, 0)
    dst["requests"] = dst.get("requests", 0) + 1
    return dst


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

