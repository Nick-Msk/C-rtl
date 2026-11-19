"""List and delete ai-checker fix branches.

Safety model:
  - By default, only branches already merged into HEAD are deleted
    (git branch -d). Unmerged branches are listed but preserved.
  - --force / FORCE=1 flips to git branch -D (delete regardless).
  - --dry-run / DRY_RUN=1 prints the plan without touching git.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from common import ROOT


def git(*args: str, input: str | None = None,
        check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *args], cwd=ROOT, text=True,
        input=input, capture_output=True, check=check)

def delete_branch(name: str, force: bool = False) -> bool:
    """Delete a local branch. Returns True on success, False on
    failure (branch missing, current branch, unmerged + not force).
    Never raises — this is best-effort cleanup."""
    flag = "-D" if force else "-d"
    r = git("branch", flag, name, check=False)
    return r.returncode == 0

def list(prefix: str, check_run: int | None = None
          ) -> tuple[list[str], list[str]]:
    """Return (merged, unmerged) branches matching '<prefix>*'.

    check_run filters by the '-c<N>-' marker in the branch name
    (new format `ai-checker-<proj>-c<check>-f<fix>-<seq>/<chunk>-<local>`).
    Old branches (`ai-checker-<proj>-fix<N>-<seq>/...`) have no
    `-c<N>-` and are only listed when check_run is None.
    """
    current_r = git("rev-parse", "--abbrev-ref", "HEAD", check=False)
    current = current_r.stdout.strip() if current_r.returncode == 0 else ""
    if current == "HEAD":
        print("warn: detached HEAD; all branches treated as unmerged",
              file=sys.stderr)
        current = ""

    r = git("for-each-ref", "--format=%(refname:short)",
             "refs/heads/", check=False)
    if r.returncode != 0:
        print(f"warn: for-each-ref failed: {r.stderr.strip()}",
              file=sys.stderr)
        return [], []

    branches = [b.strip() for b in r.stdout.splitlines() if b.strip()]
    branches = [b for b in branches
                if b != current and b.startswith(prefix)]
    if check_run is not None:
        marker = f"-c{check_run}-"
        branches = [b for b in branches if marker in b]

    merged: list[str] = []
    unmerged: list[str] = []
    for b in branches:
        mb = git("merge-base", "--is-ancestor", b, "HEAD", check=False)
        if mb.returncode == 0:
            merged.append(b)
        else:
            unmerged.append(b)
    return merged, unmerged


def _print_list(label: str, branches: list[str]) -> None:
    if not branches:
        return
    print(f"\n{label} ({len(branches)}):")
    for b in branches:
        print(f"  {b}")

# ── clean-tree check ─────────────────────────────────────────────────
def git_clean_check() -> tuple[str, str]:
    """Verify we're in a git repo with a clean tree. Return (branch, HEAD sha)."""
    r = git("rev-parse", "--verify", "HEAD", check=False)
    if r.returncode != 0:
        sys.exit("ERROR: not a git repository (or no commits yet)")
    branch = git("rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    if branch == "HEAD":
        sys.exit("ERROR: detached HEAD, checkout a branch first")
    status = git("status", "--porcelain").stdout.strip()
    if status:
        sys.exit("ERROR: working tree is dirty. Commit or stash before "
                 "running with --apply.\n"
                 f"       Uncommitted changes:\n{status}")
    sha = git("rev-parse", "HEAD").stdout.strip()
    return branch, sha


def make_fix_branch_name(project: str, fix_runid: int, seq: int,
                         chunk_id: int, local_id: int,
                         check_runid: int = 0) -> str:
    """ai-checker-<proj>-c<check>-f<fix>-<seq>/<chunk>-<local>

    - c<check>        = check-run (llmruns.runid, kind='check'),
                        which report this fix belongs to
    - f<fix>          = fix-run (llmruns.runid, kind='fix'),
                        which ai-fix invocation created it
    - <seq>           = per-fix-run sequence (1, 2, ...)
    - <chunk>-<local> = the finding key

    Filtering by check-run: `git branch --list 'ai-checker-<p>-c<N>-*'`.
    Filtering by fix-run:   `git branch --list 'ai-checker-<p>-*-f<N>-*'`.

    `check_runid=0` falls back to omitting the `c<N>` part
    (legacy reports with no llmruns entry).
    """
    parts = [f"ai-checker-{project}"]
    if check_runid:
        parts.append(f"c{check_runid}")
    parts.append(f"f{fix_runid}-{seq}/{chunk_id}-{local_id}")
    return "-".join(parts)


# ── apply + commit ───────────────────────────────────────────────────
def _apply_edit(path: Path, old: str, new: str) -> bool:
    """Replace ALL occurrences of old with new (uniform-fix support).

    Callers (fix.validate_edits) already verified the count matches
    the LLM's expectation, so replace-all is safe here.
    """
    try:
        src = path.read_text(encoding="utf-8")
        if old not in src:
            return False
        path.write_text(src.replace(old, new), encoding="utf-8")
        return True
    except OSError:
        return False


def commit_one_fix(base_sha: str, base_branch: str, branch_name: str,
                   edits: list[tuple[Path, str, str]], msg: str) -> str:
    """Create branch from base_sha, apply all edits, commit, return to base.

    Clean-tree invariant is enforced by git_clean_check before this
    runs. On any failure (checkout, edit, add, commit) the partial
    branch is deleted and the tree is reset to base_branch.
    """
    r = git("checkout", "-b", branch_name, base_sha, check=False)
    if r.returncode != 0:
        raise RuntimeError(f"git checkout -b failed: {r.stderr.strip()}")

    def _cleanup(reason: str) -> None:
        git("reset", "--hard", check=False)
        git("checkout", base_branch, check=False)
        git("branch", "-D", branch_name, check=False)
        raise RuntimeError(reason)

    for path, old, new in edits:
        if not _apply_edit(path, old, new):
            _cleanup(f"apply_edit failed on {path.relative_to(ROOT)}")

    for path, _, _ in edits:
        rel = str(path.relative_to(ROOT))
        r = git("add", "--", rel, check=False)
        if r.returncode != 0:
            _cleanup(f"git add failed for {rel}: {r.stderr.strip()}")

    r = git("commit", "-F", "-", input=msg, check=False)
    if r.returncode != 0:
        _cleanup(f"git commit failed: {r.stderr.strip()}")

    sha = git("rev-parse", "--short", "HEAD").stdout.strip()

    r = git("checkout", base_branch, check=False)
    if r.returncode != 0:
        # The commit exists; only the return-to-base failed.
        raise RuntimeError(
            f"commit {sha} on {branch_name} succeeded, "
            f"but 'git checkout {base_branch}' failed: {r.stderr.strip()}")
    return sha

def branches_exist(names: list[str]) -> set[str]:
    """Return the subset of `names` that exist as local branches.

    Fail-safe: if `git for-each-ref` fails, returns `set(names)` so
    we don't hide information due to a transient git error.
    """
    if not names:
        return set()
    r = git("for-each-ref", "--format=%(refname:short)",
            "refs/heads/", check=False)
    if r.returncode != 0:
        return set(names)
    existing = {b.strip() for b in r.stdout.splitlines() if b.strip()}
    return set(names) & existing

def merged_branches(prefix: str = "ai-checker-") -> list[str]:
    """Local branches matching '<prefix>*' that are ancestors of HEAD.

    Uses `git merge-base --is-ancestor` — portable across git versions
    (unlike `git branch --merged --format`, which returns empty on
    some macOS builds).
    """
    r = git("for-each-ref", "--format=%(refname:short)",
            "refs/heads/", check=False)
    if r.returncode != 0:
        return []
    branches = [b.strip() for b in r.stdout.splitlines()
                if b.strip() and b.strip().startswith(prefix)]
    merged: list[str] = []
    for b in branches:
        mb = git("merge-base", "--is-ancestor", b, "HEAD", check=False)
        if mb.returncode == 0:
            merged.append(b)
    return merged

def main() -> int:
    ap = argparse.ArgumentParser(
        description="Delete ai-checker fix branches")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--project",
                   help="only branches for this project")
    g.add_argument("--all", action="store_true",
                   help="all ai-checker-* branches (default)")
    ap.add_argument("--check-run", type=int, default=None,
                    help="only branches from this check-run "
                         "(name contains '-c<N>-')")
    ap.add_argument("--force", action="store_true",
                    help="also delete unmerged branches (-D instead of -d)")
    ap.add_argument("--dry-run", action="store_true",
                    help="print what would be deleted, do nothing")
    args = ap.parse_args()

    prefix = f"ai-checker-{args.project}-" if args.project else "ai-checker-"

    merged, unmerged = list(prefix)
    if not merged and not unmerged:
        msg = f"no branches match '{prefix}*'"
        if args.check_run is not None:
            msg += f" with check-run c{args.check_run}"
        print(msg)
        return 0

    scope = f"'{prefix}*'"
    if args.check_run is not None:
        scope += f" filtered to check-run c{args.check_run}"
    print(f"pattern: {scope}")
    _print_list("merged (safe to delete)", merged)
    _print_list("unmerged (preserved unless FORCE=1)", unmerged)

    to_delete = merged + (unmerged if args.force else [])
    if not to_delete:
        print("\nnothing to delete (use FORCE=1 to also delete unmerged)")
        return 0

    if args.dry_run:
        print(f"\n[dry-run] would delete {len(to_delete)} branch(es)")
        return 0

    flag = "-D" if args.force else "-d"
    deleted = 0
    failed = 0
    print()
    for b in to_delete:
        r = git("branch", flag, b, check=False)
        if r.returncode != 0:
            print(f"  FAILED:  {b}: {r.stderr.strip()}", file=sys.stderr)
            failed += 1
        else:
            print(f"  deleted: {b}")
            deleted += 1

    print(f"\n{deleted} deleted, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())

