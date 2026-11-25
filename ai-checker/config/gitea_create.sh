#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOKEN=$(tr -d '[:space:]' < "$DIR/GITEA_TOKEN")
REPO_URL="http://localhost:3000/api/v1/repos/skelet/test1/issues"

TITLE="${1:?Укажи заголовок тикета первым аргументом}"
BODY="${2:-}"

PAYLOAD=$(python3 -c 'import sys, json; print(json.dumps({"title": sys.argv[1], "body": sys.argv[2]}))' "$TITLE" "$BODY")

RESPONSE=$(curl -sf -X POST "$REPO_URL" \
  -H "Content-Type: application/json" \
  -H "Authorization: token $TOKEN" \
  -d "$PAYLOAD")

echo "$RESPONSE" | python3 -c 'import sys, json; d = json.load(sys.stdin); print("Created #" + str(d["number"]) + ": " + d["html_url"])'

