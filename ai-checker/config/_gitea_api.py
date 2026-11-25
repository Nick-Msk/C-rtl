import json
import urllib.error
import urllib.request
from pathlib import Path

DIR = Path(__file__).resolve().parent
BASE_URL = "http://localhost:3000/api/v1"
REPO_ISSUES_URL = f"{BASE_URL}/repos/skelet/test1/issues"


def get_token() -> str:
    return (DIR / "GITEA_TOKEN").read_text().strip()


def api_request(url: str, method: str = "GET", payload: dict | None = None) -> tuple[dict | list | None, dict]:
    headers = {"Authorization": f"token {get_token()}"}
    data = None

    if payload is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(payload).encode("utf-8")

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=10) as resp:
        body = resp.read().decode("utf-8")
        return (json.loads(body) if body else None), dict(resp.headers)

