"""Push ai-checker issues to external tracker (Jira / GitHub Issues).

Dry-run mode (default): export JSON payload to reports/<proj>/tracker-payload.json.
Live mode: POST to Jira REST API (requires jira_link + jira_project + jira_token).
"""
from __future__ import annotations

import json
import os
import sys

import db
from common import OUT_ROOT, rel_display


def _jira_payload(issue: dict, project: str, remote_status: str) -> dict:
    """Jira issue create payload (fields format)."""
    return {
        "fields": {
            "project": {"key": db.get_param("track", "jira_project", "")},
            "summary": f"[{project}] {issue['title'][:200]}",
            "description": issue["text"],
            "issuetype": {"name": "Bug"},
            "labels": ["ai-checker", project, issue["section"]],
            "local_key": issue["key"],
            # Custom field for local key (if configured in Jira)
            # "customfield_10001": f"{issue['runid']}/{issue['chunk_id']}/{issue['local_id']}",
        }
    }


def push_issues(project: str, issues: list[dict], *, dry_run: bool = True) -> None:
    """Export or POST issues to tracker. dry_run=True writes JSON, False POSTs."""
    if not issues:
        return
    
    link = db.get_param("track", "jira_link", "")
    if not link and not dry_run:
        print("[tracker] warn: jira_link not set, skipping push", file=sys.stderr)
        return
    
    # Load status mapping
    with db.connect() as con:
        rows = con.execute(
            "SELECT status, remote_status FROM tracker_mapping").fetchall()
    mapping = {r["status"]: r["remote_status"] for r in rows}
    
    payloads = []
    for issue in issues:
        remote_status = mapping.get(issue["status"], "To Do")
        payloads.append({
            "local_key": issue["key"],
            "local_status": issue["status"],
            "remote_status": remote_status,
            "payload": _jira_payload(issue, project, remote_status),
        })
    
    if dry_run:
        out_dir = OUT_ROOT / project
        out_dir.mkdir(parents=True, exist_ok=True)
        out_path = out_dir / "tracker-payload.json"
        out_path.write_text(json.dumps(payloads, indent=2, ensure_ascii=False),
                            encoding="utf-8")
        print(f"[tracker] dry-run: {len(payloads)} issue(s) -> {rel_display(out_path)}")
        
        # Если json-server запущен, отправь туда же
        import urllib.error
        import urllib.request
        server_url = "http://localhost:3001/issues"
        try:
            urllib.request.urlopen(f"{server_url.rsplit('/', 1)[0]}/", timeout=1)
            for p in payloads:
                data = json.dumps(p).encode("utf-8")
                req = urllib.request.Request(server_url, data=data,
                                            headers={"Content-Type": "application/json"},
                                            method="POST")
                urllib.request.urlopen(req, timeout=5)
            print(f"[tracker] also posted to {server_url}")
        except urllib.error.URLError:
            pass  # json-server не запущен — тихо пропустить
        return
    
    # Live mode: POST to Jira
    import urllib.error
    import urllib.request
    
    token = os.environ.get("JIRA_TOKEN") or db.get_param("check", "jira_token", "")
    if not token:
        print("[tracker] ERROR: jira_token not set (params or JIRA_TOKEN env)",
              file=sys.stderr)
        return
    
    url = f"{link.rstrip('/')}/rest/api/2/issue"
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {token}",
    }
    
    created = failed = 0
    for p in payloads:
        data = json.dumps(p["payload"]).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                result = json.loads(resp.read())
                print(f"[tracker] {p['local_key']} -> {result['key']}")
                created += 1
        except urllib.error.HTTPError as exc:
            print(f"[tracker] {p['local_key']} FAILED: {exc.code} {exc.reason}",
                  file=sys.stderr)
            failed += 1
    
    print(f"[tracker] {created} created, {failed} failed")
