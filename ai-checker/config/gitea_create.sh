#!/usr/bin/env -S uv run --script
import sys
from _gitea_api import REPO_ISSUES_URL, api_request

if len(sys.argv) < 2 or not sys.argv[1].strip():
    print("Usage: config/gitea_create.sh 'Header' ['Desc']", file=sys.stderr)
    sys.exit(1)

title = sys.argv[1]
body = sys.argv[2] if len(sys.argv) > 2 else ""

issue, _ = api_request(REPO_ISSUES_URL, method="POST", payload={"title": title, "body": body})
print(f"Created #{issue['number']}: {issue['html_url']}")

