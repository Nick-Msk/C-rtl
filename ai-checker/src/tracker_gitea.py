"""Gitea self-hosted backend. Reads ONLY track.gitea_* .

gitea_link    http://localhost:3000
gitea_owner   user or org
gitea_repo    repository name
gitea_token   access token (or env GITEA_TOKEN), scope write:issue
gitea_labels  comma-separated NUMERIC label IDs (Gitea API wants IDs, not names)
gitea_dry_run pin this backend to dry
"""
from __future__ import annotations

import os

import db
from tracker_common import PushResult, Ticket, Tracker, post_json


class GiteaTracker(Tracker):
    name = "gitea"

    def configure(self) -> str | None:
        self.link = db.get_param("track", "gitea_link", "").rstrip("/")
        self.owner = db.get_param("track", "gitea_owner", "")
        self.repo = db.get_param("track", "gitea_repo", "")
        self.token = (os.environ.get("GITEA_TOKEN")
                      or db.get_param("track", "gitea_token", ""))
        raw = db.get_param("track", "gitea_labels", "")
        self.labels = [int(x) for x in raw.split(",") if x.strip().isdigit()]
        missing = [k for k, v in (("gitea_link", self.link),
                                  ("gitea_owner", self.owner),
                                  ("gitea_repo", self.repo),
                                  ("gitea_token", self.token)) if not v]
        if missing:
            return "empty: " + ", ".join("track." + m for m in missing)
        return None

    def force_dry(self) -> bool:
        return db.get_param("track", "gitea_dry_run", False)

    def build(self, t: Ticket) -> dict:
        body = (f"ai-checker key: `{t.key}`\n"
                f"project: `{t.project}`  section: {t.section}\n"
                f"local status: `{t.status}`\n\n{t.text}\n")
        payload = {"title": f"[{t.project}] {t.title}"[:255], "body": body}
        if self.labels:
            payload["labels"] = self.labels
        return payload

    def create(self, t: Ticket, payload: dict) -> PushResult:
        url = f"{self.link}/api/v1/repos/{self.owner}/{self.repo}/issues"
        data, err = post_json(url, payload,
                              {"Authorization": f"token {self.token}"})
        if err:
            return PushResult(error=err, remote_status=t.remote_status)
        num = data.get("number")
        return PushResult(remote_id=str(num) if num is not None else "",
                          remote_url=data.get("html_url", ""),
                          remote_status=t.remote_status or "open")


def tracker() -> Tracker:
    return GiteaTracker()
