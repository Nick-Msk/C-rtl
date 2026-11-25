#!/usr/bin/env -S uv run --script
import sys
from _gitea_api import REPO_ISSUES_URL, api_request, find_open_issue

args = sys.argv[1:]
if not args or not args[0].strip():
    print("Usage: config/gitea_create.sh 'Header' ['Desc']", file=sys.stderr)
    sys.exit(1)

title = args[0].strip()
body = args[1] if len(args) > 1 else ""

existing = find_open_issue(title)
if existing:
    print(f"Exists #{existing['number']}: {existing['html_url']}")
    sys.exit(0)

issue, _ = api_request(REPO_ISSUES_URL, method="POST", payload={"title": title, "body": body})
print(f"Created #{issue['number']}: {issue['html_url']}")

