"""Jira backend. Reads ONLY track.jira_* .

jira_link      base URL, e.g. https://x.atlassian.net
jira_project   project key (rest api)
jira_token     API token / PAT (or env JIRA_TOKEN)
jira_auth      bearer | basic   (Cloud: basic + jira_user email)
jira_user      account email for basic auth
jira_issuetype issue type name
jira_api       rest | plain     plain = json-server mock, POST {link}/issues
jira_dry_run   pin this backend to dry even when track.mode=live
"""
from __future__ import annotations

import base64
import os

import db
from tracker_common import (
    PushResult,
    Ticket,
    Tracker,
    get_json,
    patch_json,
    post_json,
)


class JiraTracker(Tracker):
    name = "jira"

    def configure(self) -> str | None:
        self.link = db.get_param("track", "jira_link", "").rstrip("/")
        self.project = db.get_param("track", "jira_project", "")
        self.token = (os.environ.get("JIRA_TOKEN")
                      or db.get_param("track", "jira_token", ""))
        
        params = db.tracker_params("jira")
        self.issuetype = params.get("issuetype", "Bug")
        self.api = params.get("api", "rest")
        self.auth = params.get("auth", "bearer")
        self.user = db.get_param("track", "jira_user", "")
        
        if not self.link:
            return "track.jira_link is empty"
        if self.api == "rest":
            if not self.project:
                return "track.jira_project is empty"
            if not self.token:
                return "track.jira_token / JIRA_TOKEN is empty"
            if self.auth == "basic" and not self.user:
                return "track.jira_user is empty (basic auth)"
        return None

    def build(self, t: Ticket) -> dict:
        summary = f"[{t.project}] {t.title}"[:255]
        if self.api == "plain":
            return {"local_key": t.key, "project": t.project,
                    "section": t.section, "local_status": t.status,
                    "remote_status": t.remote_status or "To Do",
                    "summary": summary, "description": t.text}
        
        # Labels from tracker_labels table
        label_specs = db.tracker_labels("jira")
        label_names = {r["name"] for r in label_specs}
        labels = [n for n in ("ai-checker", t.section) if n in label_names]
        
        return {"fields": {
            "project": {"key": self.project},
            "summary": summary,
            "description": t.text,
            "issuetype": {"name": self.issuetype},
            "labels": labels,
        }}

    def force_dry(self) -> bool:
        return db.get_param("track", "jira_dry_run", False)

    def _headers(self) -> dict:
        if self.api != "rest" or not self.token:
            return {}
        if self.auth == "basic":
            raw = f"{self.user}:{self.token}".encode()
            return {"Authorization": "Basic " + base64.b64encode(raw).decode()}
        return {"Authorization": f"Bearer {self.token}"}

    def create(self, t: Ticket, payload: dict) -> PushResult:
        url = (f"{self.link}/issues" if self.api == "plain"
               else f"{self.link}/rest/api/2/issue")
        data, err = post_json(url, payload, self._headers())
        if err:
            return PushResult(error=err, remote_status=t.remote_status)
        if self.api == "plain":
            rid = str(data.get("id", ""))
            return PushResult(
                remote_id=rid,
                remote_url=f"{self.link}/issues/{rid}" if rid else "",
                remote_status=t.remote_status)
        key = data.get("key", "")
        return PushResult(remote_id=key,
                          remote_url=f"{self.link}/browse/{key}" if key else "",
                          remote_status=t.remote_status)

    closed_status = "Done"
    def close(self, link: dict, comment: str) -> tuple[str | None, bool]:
        rid = link["remote_id"]
        if self.api == "plain":
            _, err = patch_json(f"{self.link}/issues/{rid}",
                                {"state": "closed"}, self._headers())
            return err, False  # plain mock doesn't track state
        hdr = self._headers()
        # Check current status
        issue, err = get_json(f"{self.link}/rest/api/2/issue/{rid}", hdr)
        if err:
            return err, False
        is_done = issue.get("fields", {}).get("status", {}).get(
            "statusCategory", {}).get("key") == "done"
        if not is_done:
            data, err = get_json(
                f"{self.link}/rest/api/2/issue/{rid}/transitions", hdr)
            if err:
                return err, False
            transitions = data.get("transitions", []) if isinstance(data, dict) else []
            done = next((t for t in transitions
                         if (t.get("to", {}).get("statusCategory", {}).get("key")
                             == "done")), None)
            if done is None:
                return "no done-category transition available", False
            _, err = post_json(
                f"{self.link}/rest/api/2/issue/{rid}/transitions",
                {"transition": {"id": done["id"]}}, hdr)
            if err:
                return err, False
        _, err = post_json(f"{self.link}/rest/api/2/issue/{rid}/comment",
                           {"body": comment}, hdr)
        if err:
            return err, is_done
        return None, is_done

    def set_status(self, link: dict, remote_status: str) -> str | None:
        if self.api == "plain":
            return None  # mock doesn't support transitions
        rid = link["remote_id"]
        hdr = self._headers()
        # Get available transitions
        data, err = get_json(
            f"{self.link}/rest/api/2/issue/{rid}/transitions", hdr)
        if err:
            return err
        transitions = data.get("transitions", []) if isinstance(data, dict) else []
        # Find transition to target status
        target = next((t for t in transitions
                       if t.get("to", {}).get("name") == remote_status), None)
        if not target:
            return f"no transition to {remote_status!r}"
        _, err = post_json(
            f"{self.link}/rest/api/2/issue/{rid}/transitions",
            {"transition": {"id": target["id"]}}, hdr)
        return err

def tracker() -> Tracker:
    return JiraTracker()
