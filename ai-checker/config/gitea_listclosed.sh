#!/usr/bin/env -S uv run --script
import sys
from _gitea_api import REPO_ISSUES_URL, api_request

limit = 7
items, headers = api_request(f"{REPO_ISSUES_URL}?state=closed&type=issues&sort=recentupdate&limit={limit}")

total_str = headers.get("X-Total-Count", headers.get("x-total-count", ""))
total = int(total_str) if total_str.isdigit() else len(items)

if not items:
    print("NO closed.")
    sys.exit(0)

for i in items:
    title = i["title"].replace("\n", " ")
    if len(title) > 55:
        title = title[:52] + "..."
    print(f"#{i['number']:<4} [{i['state']}]  {title}")

if total > len(items):
    print(f"... and {total - len(items)} more")

