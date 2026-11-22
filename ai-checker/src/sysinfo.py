"""Best-effort system snapshot for ai-checker report headers.

All lookups are best-effort: if a sysctl, /proc entry, or platform
call fails, the corresponding field becomes 'unknown'. The tool keeps
working; only the header table gets less informative.

`system_info()` is guaranteed never to raise — the whole body is
wrapped in a try/except that falls back to all-'unknown' fields.
"""
from __future__ import annotations

import os
import platform
import subprocess
from pathlib import Path

UNKNOWN = "unknown"


def _sysctl(name: str) -> str | None:
    try:
        return subprocess.check_output(
            ["sysctl", "-n", name],
            stderr=subprocess.DEVNULL, text=True, timeout=2,
        ).strip() or None
    except (OSError, subprocess.SubprocessError):
        return None


def _cpu_brand() -> str:
    sysname = platform.system()
    if sysname == "Darwin":
        v = _sysctl("machdep.cpu.brand_string")
        if v:
            return v
    elif sysname == "Linux":
        try:
            with open("/proc/cpuinfo", encoding="utf-8") as f:
                for line in f:
                    if line.startswith("model name"):
                        return line.split(":", 1)[1].strip()
        except OSError:
            pass
    return platform.processor() or UNKNOWN


def _ram_bytes() -> int | None:
    sysname = platform.system()
    if sysname == "Darwin":
        v = _sysctl("hw.memsize")
        if v and v.isdigit():
            return int(v)
    elif sysname == "Linux":
        try:
            with open("/proc/meminfo", encoding="utf-8") as f:
                for line in f:
                    if line.startswith("MemTotal:"):
                        return int(line.split()[1]) * 1024
        except (OSError, ValueError, IndexError):
            pass
    return None


def _cores() -> tuple[int | None, int | None]:
    logical = os.cpu_count()
    physical: int | None = None
    sysname = platform.system()
    if sysname == "Darwin":
        v = _sysctl("hw.physicalcpu")
        if v and v.isdigit():
            physical = int(v)
    elif sysname == "Linux":
        try:
            seen: set[tuple[str, str]] = set()
            pid = ""
            with open("/proc/cpuinfo", encoding="utf-8") as f:
                for line in f:
                    if line.startswith("physical id"):
                        pid = line.split(":", 1)[1].strip()
                    elif line.startswith("core id"):
                        seen.add((pid, line.split(":", 1)[1].strip()))
            if seen:
                physical = len(seen)
        except OSError:
            pass
    return physical, logical


def _unknown() -> dict:
    return {
        "host": UNKNOWN,
        "os": UNKNOWN,
        "cpu": UNKNOWN,
        "cores": UNKNOWN,
        "ram": UNKNOWN,
        "python": UNKNOWN,
    }


def system_info() -> dict:
    """Best-effort system snapshot. Never raises.

    On any unexpected failure the whole dict collapses to 'unknown'
    fields, so callers can write headers and JSON unconditionally.
    """
    try:
        return _system_info_impl()
    except Exception:  # noqa: BLE001 — last-resort guard, never crash a review
        return _unknown()


def _system_info_impl() -> dict:
    sysname = platform.system()
    host = platform.node() or UNKNOWN

    if sysname == "Darwin":
        hw = _sysctl("hw.model") or host
        os_str = f"macOS {platform.mac_ver()[0]}"
    elif sysname == "Linux":
        hw = host
        for p in ("/sys/class/dmi/id/product_name",
                  "/sys/devices/virtual/dmi/id/product_name"):
            try:
                hw = Path(p).read_text(encoding="utf-8").strip() or host
                break
            except OSError:
                continue
        try:
            pretty = None
            with open("/etc/os-release", encoding="utf-8") as f:
                for line in f:
                    if line.startswith("PRETTY_NAME="):
                        pretty = line.split("=", 1)[1].strip().strip('"')
                        break
            os_str = pretty or f"Linux {platform.release()}"
        except OSError:
            os_str = f"Linux {platform.release()}"
    else:
        hw = host
        os_str = f"{sysname} {platform.release()}"

    physical, logical = _cores()
    ram = _ram_bytes()
    return {
        "host": hw,
        "os": f"{os_str} ({platform.machine()})",
        "cpu": _cpu_brand(),
        "cores": (f"{physical}p/{logical}l" if physical
                  else f"{logical} logical" if logical else UNKNOWN),
        "ram": f"{ram / (1024 ** 3):.1f} GB" if ram else UNKNOWN,
        "python": platform.python_version(),
    }

