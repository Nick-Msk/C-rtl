#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOKEN=$(tr -d '[:space:]' < "$DIR/GITEA_TOKEN")

if curl -sf -H "Authorization: token $TOKEN" "http://localhost:3000/api/v1/user" > /dev/null; then
  echo "OK"
else
  echo "FAIL: Gitea недоступна или неверный токен" >&2
  exit 1
fi

