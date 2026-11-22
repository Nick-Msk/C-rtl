"""Workspace descriptor for ngutils — read by ai-checker.

Lives in the REVIEWED repo, not in the tool: ai-checker moved out to
kr_book/ai-checker, so ROOT can no longer be derived from __file__.
Paths here are relative to THIS FILE, never to cwd.
"""
workspace.root(".")
workspace.projects("bool", "checker", "common", "error", "fs2", "log", "path")
