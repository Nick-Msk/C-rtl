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
import sys

import db
from tracker_common import (
    PushResult,
    Ticket,
    Tracker,
    get_json,
    patch_json,
    post_json,
)


def _no_api(err: str) -> bool:
    return any(c in err for c in ("HTTP 404", "HTTP 405", "HTTP 501"))

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
        self.board_template = db.get_param(
            "track", "gitea_board_template", "basic_kanban")
        self._proj_id = None
        self._col_id = None

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

    def _headers(self):
        return {"Authorization": f"token {self.token}"}

    def _base(self):
        return f"{self.link}/api/v1/repos/{self.owner}/{self.repo}"

    def ensure_project(self, project: str) -> str | None:
        """One Gitea project board per ai-checker project, idempotent.
        Degrades to 'no board' if this Gitea lacks the projects API."""
        base, hdr = self._base(), self._headers()
        rows, err = get_json(f"{base}/projects?limit=100", hdr)
        if err:
            if _no_api(err):
                print("[tracker] warn: Gitea projects API unavailable; "
                      "issues won't be grouped into boards", file=sys.stderr)
                return None
            return err
        pid = next((p["id"] for p in rows if p.get("title") == project), None)
        if pid is None:
            data, err = post_json(f"{base}/projects", {
                "title": project,
                "description": f"ai-checker findings for {project}",
                "template_type": self.board_template,
            }, hdr)
            if err:
                if _no_api(err):
                    return None
                return err
            pid = data.get("id")
        self._proj_id = pid
        cols, err = get_json(f"{base}/projects/{pid}/columns", hdr)
        self._col_id = cols[0]["id"] if (not err and cols) else None
        return None

    def create(self, t: Ticket, payload: dict) -> PushResult:
        base, hdr = self._base(), self._headers()
        data, err = post_json(f"{base}/issues", payload, hdr)
        if err:
            return PushResult(error=err, remote_status=t.remote_status)
        num = data.get("number")
        gid = data.get("id")          # GLOBAL id — нужен для привязки к доске
        if self._proj_id and self._col_id and gid:
            _, lerr = post_json(
                f"{base}/projects/{self._proj_id}/columns/"
                f"{self._col_id}/issues/{gid}", {}, hdr)
            if lerr:
                print(f"[tracker] warn: board link {t.key}: {lerr}",
                      file=sys.stderr)   # тикет создан; привязка best-effort
        return PushResult(remote_id=str(num) if num is not None else "",
                          remote_url=data.get("html_url", ""),
                          remote_status=t.remote_status or "open")

    closed_status = "closed"

    def close(self, link: dict, comment: str) -> str | None:
        base, hdr = self._base(), self._headers()
        num = link["remote_id"]
        _, err = patch_json(f"{base}/issues/{num}", {"state": "closed"}, hdr)
        if err:
            return err
        _, err = post_json(f"{base}/issues/{num}/comments",
                           {"body": comment}, hdr)
        return err

def tracker() -> Tracker:
    return GiteaTracker()
