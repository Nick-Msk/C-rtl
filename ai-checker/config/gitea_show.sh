#!/usr/bin/env -S uv run --script
import sys
import urllib.error
from _gitea_api import REPO_ISSUES_URL, api_request

args = sys.argv[1:]
if not args:
    print("Usage: config/gitea_show.sh <num>", file=sys.stderr)
    sys.exit(1)

num = args[0].lstrip("#").strip()

try:
    issue, _ = api_request(f"{REPO_ISSUES_URL}/{num}")
    comments, _ = api_request(f"{REPO_ISSUES_URL}/{num}/comments")
except urllib.error.HTTPError as e:
    print(f"Тикет #{num} не найден (HTTP {e.code})", file=sys.stderr)
    sys.exit(1)

print("=" * 60)
print(f"#{issue['number']} [{issue['state'].upper()}] {issue['title']}")
print(f"Автор: {issue['user']['login']} | Создан: {issue['created_at'][:16].replace('T', ' ')}")
print(f"URL:   {issue['html_url']}")
print("=" * 60)

body = (issue.get("body") or "").strip()
print(body if body else "(описание пустое)")

if comments:
    print("\n" + "-" * 60)
    print(f"Comments ({len(comments)}):")
    for c in comments:
        created = c["created_at"][:16].replace("T", " ")
        author = c["user"]["login"]
        c_body = (c.get("body") or "").strip()
        print(f"\n[{created}] @{author}:")
        print(c_body)
else:
    print("\n(no comments)")


