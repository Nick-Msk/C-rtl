#!/usr/bin/env -S uv run --script
import sys
import urllib.error
from _gitea_api import REPO_ISSUES_URL, api_request

if len(sys.argv) < 2:
    print("Usage: config/gitea_close.sh <num | 1-6 | 4,6,7-11 | '*'> ['Comment']", file=sys.stderr)
    sys.exit(1)

target = sys.argv[1].strip()
comment = sys.argv[2] if len(sys.argv) > 2 else ""


def parse_targets(spec: str) -> list[int]:
    if spec == "*":
        items, _ = api_request(f"{REPO_ISSUES_URL}?state=open&type=issues&limit=100")
        return sorted(i["number"] for i in items)

    nums: set[int] = set()
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            a, b = map(int, part.split("-", 1))
            nums.update(range(min(a, b), max(a, b) + 1))
        else:
            nums.add(int(part))
    return sorted(nums)


numbers = parse_targets(target)
if not numbers:
    print("No opened tickets.")
    sys.exit(0)

for num in numbers:
    try:
        if comment:
            api_request(f"{REPO_ISSUES_URL}/{num}/comments", method="POST", payload={"body": comment})
        api_request(f"{REPO_ISSUES_URL}/{num}", method="PATCH", payload={"state": "closed"})
        print(f"Closed #{num}")
    except urllib.error.HTTPError as e:
        print(f"Skip #{num} (HTTP {e.code})", file=sys.stderr)


