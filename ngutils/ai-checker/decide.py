"""Record a human decision on an issue (by full key runid/chunk/local)."""
from __future__ import annotations

import argparse
import sys

import db


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("keys", nargs="+")
    ap.add_argument("--status", required=True,
                    choices=["rejected", "no-action", "added"])
    ap.add_argument("--reason", default="")
    a = ap.parse_args()
    rc = 0
    for key in a.keys:
        parts = key.split("/")
        if len(parts) != 3 or not all(p.isdigit() for p in parts):
            print(f"bad key {key!r}: want runid/chunk/local", file=sys.stderr)
            rc = 1
            continue
        runid, chunk, local = map(int, parts)
        with db.connect() as con:
            cur = con.execute(
                "UPDATE issue SET status = ?, dt = ?, "
                "  reason = COALESCE(NULLIF(?, ''), reason) "
                "WHERE runid = ? AND chunk_id = ? AND local_id = ?",
                (a.status, db._now_iso(), a.reason, runid, chunk, local))
        if cur.rowcount:
            print(f"{key} -> {a.status}")
        else:
            print(f"{key}: not found", file=sys.stderr)
            rc = 1
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
