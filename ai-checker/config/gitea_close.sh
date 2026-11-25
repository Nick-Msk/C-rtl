#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE}")" && pwd)"
TOKEN=$(tr -d '[:space:]' < "$DIR/GITEA_TOKEN")
REPO_URL="http://localhost:3000/api/v1/repos/skelet/test1/issues"

ISSUE_NUM="${1:?Укажи номер тикета первым аргументом (например: 1)}"
COMMENT="${2:-}"

# Если передан комментарий вторым аргументом — сначала постим его в тикет
if [ -n "$COMMENT" ]; then
  COMMENT_PAYLOAD=$(python3 -c 'import sys, json; print(json.dumps({"body": sys.argv}))' "$COMMENT")
  curl -sf -X POST "$REPO_URL/$ISSUE_NUM/comments" \
    -H "Content-Type: application/json" \
    -H "Authorization: token $TOKEN" \
    -d "$COMMENT_PAYLOAD" > /dev/null
fi

# Закрываем сам тикет
curl -sf -X PATCH "$REPO_URL/$ISSUE_NUM" \
  -H "Content-Type: application/json" \
  -H "Authorization: token $TOKEN" \
  -d '{"state": "closed"}' > /dev/null

echo "Closed #$ISSUE_NUM"

