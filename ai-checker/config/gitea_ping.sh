#!/usr/bin/env -S uv run --script
import sys
from _gitea_api import BASE_URL, api_request

try:
    api_request(f"{BASE_URL}/user")
    print("OK")
except Exception as e:
    print(f"FAIL: {e}", file=sys.stderr)
    sys.exit(1)

