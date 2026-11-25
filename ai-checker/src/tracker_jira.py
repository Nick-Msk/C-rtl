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
from tracker_common import PushResult, Ticket, Tracker, post_json


class JiraTracker(Tracker):
    name = "jira"

    def configure(self) -> str | None:
        self.link = db.get_param("track", "jira_link", "").rstrip("/")
        self.project = db.get_param("track", "jira_project", "")
        self.token = (os.environ.get("JIRA_TOKEN")
                      or db.get_param("track", "jira_token", ""))
        self.auth = db.get_param("track", "jira_auth", "bearer")
        self.user = db.get_param("track", "jira_user", "")
        self.issuetype = db.get_param("track", "jira_issuetype", "Bug")
        self.api = db.get_param("track", "jira_api", "rest")
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

    def force_dry(self) -> bool:
        return db.get_param("track", "jira_dry_run", False)

    def _headers(self) -> dict:
        if self.api != "rest" or not self.token:
            return {}
        if self.auth == "basic":
            raw = f"{self.user}:{self.token}".encode()
            return {"Authorization": "Basic " + base64.b64encode(raw).decode()}
        return {"Authorization": f"Bearer {self.token}"}

    def build(self, t: Ticket) -> dict:
        summary = f"[{t.project}] {t.title}"[:255]
        if self.api == "plain":
            return {"local_key": t.key, "project": t.project,
                    "section": t.section, "local_status": t.status,
                    "remote_status": t.remote_status or "To Do",
                    "summary": summary, "description": t.text}
        return {"fields": {
            "project": {"key": self.project},
            "summary": summary,
            "description": t.text,
            "issuetype": {"name": self.issuetype},
            "labels": ["ai-checker", t.project,
                       "key-" + t.key.replace("/", "-")],
        }}

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


def tracker() -> Tracker:
    return JiraTracker()
