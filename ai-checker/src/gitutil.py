"""List and delete ai-checker fix branches.

Safety model:
  - By default, only branches already merged into HEAD are deleted
    (git branch -d). Unmerged branches are listed but preserved.
  - --force / FORCE=1 flips to git branch -D (delete regardless).
  - --dry-run / DRY_RUN=1 prints the plan without touching git.
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

from common import OUT_ROOT, ROOT, rel_display

MERGE_AREA = Path(os.environ.get("AI_CHECKER_MERGEAREA")
                  or (OUT_ROOT.parent / "mergearea"))

FIX_LOG_SUBDIR = Path("logs") / "result"

def baseline_branch_name(project: str, runid: int) -> str:
    return f"ai-baseline/{project}-c{runid}"      # НЕ 'ai-checker-...'

def mergearea_path(project: str, runid: int) -> Path:
    return MERGE_AREA / f"{project}-{runid}"

def git_in(cwd: Path, *args: str, input: str | None = None,
           check: bool = True) -> subprocess.CompletedProcess:
    """git в конкретном воркдереве (не в ROOT)."""
    return subprocess.run(["git", *args], cwd=cwd, text=True, input=input,
                          capture_output=True, check=check)

def repo_root() -> Path:
    r = git("rev-parse", "--show-toplevel", check=False)
    if r.returncode != 0:
        raise RuntimeError("not a git repository")
    return Path(r.stdout.strip()).resolve()


def work_prefix() -> Path:
    """ROOT relative to the git toplevel. Worktrees mirror the TOPLEVEL,
    not ROOT, so every ROOT-relative path needs this prefix inside a
    worktree."""
    return ROOT.resolve().relative_to(repo_root())

def head_sha() -> str:
    r = git("rev-parse", "HEAD", check=False)
    if r.returncode != 0:
        raise RuntimeError("not a git repository (or no commits yet)")
    return r.stdout.strip()




def fix_logs_dir(project: str) -> Path:
    """Surviving logs live on the tool side, next to the reports."""
    return OUT_ROOT / project / "fix-logs"


def parse_fix_worktree(wt: Path) -> tuple[str, int, int, int] | None:
    """Inverse of fix_worktree_path: '<proj>-<runid>_<chunk>_<local>'.
    None for anything else (baseline container, stray dirs)."""
    m = re.fullmatch(r"(.+)-(\d+)_(\d+)_(\d+)", wt.name)
    if not m:
        return None
    return m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))


def harvest_fix_logs(wt: Path) -> list[Path]:
    """Copy test/analyze logs out of a fix worktree before it is deleted.
    Name: <runid>_<chunk>_<local>_<timestamp>_<original-name>.
    Best-effort: never raises, a lost log must not break a merge."""
    ident = parse_fix_worktree(wt)
    if ident is None:
        return []
    project, runid, chunk, local = ident
    try:
        src_dir = wt / work_prefix() / project / FIX_LOG_SUBDIR
    except (RuntimeError, ValueError):
        return []
    if not src_dir.is_dir():
        return []
    dst_dir = fix_logs_dir(project)
    ts = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    saved: list[Path] = []
    for src in sorted(src_dir.rglob("*.log")):
        name = "_".join(src.relative_to(src_dir).parts)
        dst = dst_dir / f"{runid}_{chunk}_{local}_{ts}_{name}"
        try:
            dst_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            saved.append(dst)
        except OSError as exc:
            print(f"  warn: cannot save log {src.name}: {exc}", file=sys.stderr)
    return saved

def fix_worktree_path(project: str, runid: int,
                      chunk_id: int, local_id: int) -> Path:
    """mergearea/<project>-<runid>/<project>-<runid>_<chunk>_<local>"""
    return mergearea_path(project, runid) / f"{project}-{runid}_{chunk_id}_{local_id}"


def ensure_baseline_branch(project: str, runid: int, sha: str,
                           *, reset: bool = False) -> str:
    """Pin baseline branch at `sha` without checking out a worktree."""
    branch = baseline_branch_name(project, runid)
    if reset or not branch_exists(branch):
        r = git("branch", "-f", branch, sha, check=False)
        if r.returncode != 0:
            raise RuntimeError(f"cannot pin baseline: {r.stderr.strip()}")
    return branch


def ensure_fix_worktree(project: str, runid: int,
                        chunk_id: int, local_id: int,
                        branch_name: str, base_sha: str | None = None) -> Path:
    """Mount an isolated worktree for a single fix branch."""
    wt = fix_worktree_path(project, runid, chunk_id, local_id)
    if (wt / ".git").exists():
        return wt
    wt.parent.mkdir(parents=True, exist_ok=True)
    if wt.exists():
        shutil.rmtree(wt, ignore_errors=True)
    git("worktree", "prune", check=False)

    if branch_exists(branch_name):
        r = git("worktree", "add", str(wt), branch_name, check=False)
    else:
        if not base_sha:
            raise RuntimeError(f"branch {branch_name} missing and no base_sha given")
        r = git("worktree", "add", "-b", branch_name, str(wt), base_sha, check=False)
    if r.returncode != 0:
        raise RuntimeError(f"fix worktree add failed: {r.stderr.strip()}")
    return wt


def remove_fix_worktree(wt: Path) -> list[Path]:
    """Harvest logs, then unmount and delete one fix worktree. Returns
    saved log paths. THE single teardown route — merge, reject and clean
    all get log preservation for free."""

    saved: list[Path] = []
    if wt.exists():
        saved = harvest_fix_logs(wt)
        for p in saved:
            print(f"  LOG      {rel_display(p)}", file=sys.stderr)
        git("worktree", "remove", "--force", str(wt), check=False)
        if wt.exists():
            shutil.rmtree(wt, ignore_errors=True)
        git("worktree", "prune", check=False)
    parent = wt.parent
    if parent.is_dir() and parent.parent == MERGE_AREA:
        try:
            next(parent.iterdir())
        except StopIteration:
            parent.rmdir()
    return saved


def remove_baseline(project: str, runid: int) -> None:
    """Remove all child fix worktrees, container dir, and baseline branch."""
    container = mergearea_path(project, runid)
    branch = baseline_branch_name(project, runid)
    if container.is_dir():
        for child in tuple(container.iterdir()):
            if child.is_dir():
                remove_fix_worktree(child)
        shutil.rmtree(container, ignore_errors=True)
        git("worktree", "prune", check=False)
    git("branch", "-D", branch, check=False)


def ensure_baseline(project: str, runid: int, sha: str,
                    reset: bool = False) -> tuple[str, Path]:
    """Pin the reviewed state: branch at `sha` + worktree. Idempotent and
    self-healing: reuses the branch, re-adds a missing worktree.
    Called by ai-check (sha=HEAD) and lazily by ai-fix for adopted reports
    (sha=report.base_sha)."""
    branch = baseline_branch_name(project, runid)
    path = mergearea_path(project, runid)
    wt_exists = (path / ".git").exists()

    if wt_exists and reset:
        r = git_in(path, "checkout", "-f", "-B", branch, sha, check=False)
        if r.returncode != 0:
            raise RuntimeError(f"cannot reset baseline worktree: {r.stderr.strip()}")
        return branch, path

    if reset or not branch_exists(branch):
        r = git("branch", "-f", branch, sha, check=False)
        if r.returncode != 0:
            raise RuntimeError(f"cannot pin baseline: {r.stderr.strip()}")
    if not (path / ".git").exists():
        MERGE_AREA.mkdir(parents=True, exist_ok=True)
        if path.exists():
            shutil.rmtree(path, ignore_errors=True)
        git("worktree", "prune", check=False)
        r = git("worktree", "add", str(path), branch, check=False)
        if r.returncode != 0:
            raise RuntimeError(f"worktree add failed: {r.stderr.strip()}")
    elif reset:
        git_in(path, "checkout", "-f", branch, check=False)
    return branch, path


def dirty_files() -> set[str]:
    r = git("status", "--porcelain", check=False)
    out: set[str] = set()
    for line in r.stdout.splitlines():
        if len(line) <= 3:
            continue
        p = line[3:].strip()
        if " -> " in p:
            p = p.split(" -> ", 1)[1]
        out.add(p.strip('"'))
    return out


def merge_touched_files(branch: str) -> set[str]:
    r = git("diff", "--name-only", f"HEAD...{branch}", check=False)
    return {ln.strip() for ln in r.stdout.splitlines() if ln.strip()}

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

def list_branches(prefix: str, check_run: int | None = None
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
                         check_runid: int = 0,
                         ticket_id: int | None = None) -> str:
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
    if ticket_id:
        parts.append(f"t{ticket_id}")
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

def commit_one_fix(project: str, runid: int, chunk_id: int, local_id: int,
                   base_sha: str, branch_name: str,
                   edits: list[tuple[Path, str, str]], msg: str,
                   *, verify=None) -> tuple[str, Path]:
    """Create an isolated worktree at `base_sha`, commit edits, then run
    `verify(wt)` INSIDE that worktree. A patch that fails verification
    never survives: the finally-block removes the worktree and deletes
    the branch, so only green fix branches exist."""

    wt = ensure_fix_worktree(project, runid, chunk_id, local_id,
                             branch_name, base_sha=base_sha)
    ok = False
    prefix = work_prefix()
    try:
        targets: list[Path] = []
        for path, old, new in edits:
            rel = str(path.relative_to(ROOT))
            target = wt / prefix / rel
            targets.append(target)
            if not target.is_file():
                raise RuntimeError(f"file missing in worktree: {prefix / rel}")
            if not _apply_edit(target, old, new):
                raise RuntimeError(
                    f"apply_edit failed on {prefix / rel} "
                    f"(snippet not found at {base_sha[:7]})")
        r = git_in(wt, "add", "--", *[str(t) for t in targets], check=False)
        if r.returncode != 0:
            raise RuntimeError(f"git add failed: {r.stderr.strip()}")
        r = git_in(wt, "commit", "-F", "-", input=msg, check=False)
        if r.returncode != 0:
            raise RuntimeError(f"git commit failed: {r.stderr.strip()}")
        sha = git_in(wt, "rev-parse", "--short", "HEAD").stdout.strip()
        if not sha:
            raise RuntimeError("commit made but cannot resolve sha")
        if verify is not None:
            verify(wt)          #Raises -> ok stays False -> rollback
        ok = True
        return sha, wt
    finally:
        if not ok:
            remove_fix_worktree(wt)
            git("branch", "-D", branch_name, check=False)


def branch_exists(name: str) -> bool:
    r = git("rev-parse", "--verify", "--quiet", f"refs/heads/{name}",
            check=False)
    return r.returncode == 0


def is_merged(name: str, into: str = "HEAD") -> bool:
    """True if branch is an ancestor of `into` (already merged)."""
    if not branch_exists(name):
        return False
    return git("merge-base", "--is-ancestor", name, into,
               check=False).returncode == 0

def commit_exists(sha: str) -> bool:
    """Object is in the object DB. Says nothing about reachability."""
    return git("cat-file", "-e", f"{sha}^{{commit}}",
               check=False).returncode == 0


def is_ancestor(sha: str, into: str = "HEAD") -> bool:
    """True if sha is already in `into` history — the merge FACT.

    Unlike is_merged(), does NOT require a branch label: works after
    _cleanup_old_branches deleted the merged branch. Call
    commit_exists() first — merge-base returns >1 for a bad object.
    """
    return git("merge-base", "--is-ancestor", sha, into,
               check=False).returncode == 0


def merge_fix(branch: str, *, wt: Path, delete: bool = False,
              verify=None) -> tuple[str, bool]:
    """Verify the merge result in an isolated worktree (tests run THERE,
    never in the user's tree), then land it on base in ROOT via ff-only.

    A dirty tree is fine unless it overlaps the files the merge touches.
    """

    if not branch_exists(branch):
        raise RuntimeError(f"no such branch: {branch}")
    if is_merged(branch):
        remove_fix_worktree(wt)
        if delete:
            git("branch", "-d", branch, check=False)
        return git("rev-parse", "--short", "HEAD").stdout.strip(), False

    overlap = merge_touched_files(branch) & dirty_files()
    if overlap:
        raise RuntimeError("local changes overlap the merge — stash them: "
                           + ", ".join(sorted(overlap)))

    # 1. Гоняем тесты в полной изоляции прямо на ветке фикса (поверх baseline)
    if verify is not None:
        verify(wt)

    # 2. Вливаем проверенную ветку фикса в текущую ветку основного репозитория
    r = git("merge", "--no-ff", "-m",
            f"Merge {branch}\n\nAI-Branch: {branch}", branch, check=False)
    if r.returncode != 0:
        git("merge", "--abort", check=False)
        raise RuntimeError(f"merge conflict: {(r.stderr or r.stdout).strip()}")

    sha = git("rev-parse", "--short", "HEAD").stdout.strip()
    remove_fix_worktree(wt)
    if delete:
        git("branch", "-d", branch, check=False)

    return sha, True

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

def make_verifier(project: str, targets: list[str],
                  *, log_dir: Path | None = None):
    """Return verify(wt) running `make <target>` inside the worktree."""

    sub = work_prefix()

    def verify(wt: Path) -> None:
        for tpl in targets:
            target = tpl.format(project=project)
            print(f"  VERIFY   make {target}  [{wt.name}]", file=sys.stderr)
            r = subprocess.run(["make", target], check=False, cwd=wt / sub,
                               text=True, capture_output=True)
            out = (r.stdout or "") + (r.stderr or "")
            if log_dir is not None:
                try:
                    log_dir.mkdir(parents=True, exist_ok=True)
                    (log_dir / f"{wt.name}__{target}.log").write_text(
                        out, encoding="utf-8")
                except OSError:
                    pass
            if r.returncode != 0:
                tail = "\n".join(out.splitlines()[-25:])
                raise RuntimeError(f"{target} failed:\n{tail}")
    return verify


def discard_fix(project: str, runid: int, chunk_id: int, local_id: int,
                branch: str, *, force: bool = False) -> bool:
    """Unmount the fix worktree FIRST — git refuses branch deletion
    while any worktree holds the branch — then delete the branch.
    Logs are harvested by remove_fix_worktree on the way out."""

    wt = fix_worktree_path(project, runid, chunk_id, local_id)
    remove_fix_worktree(wt)
    return delete_branch(branch, force=force)


def cleanup_project_baselines(project: str, keep_runid: int) -> None:
    """Remove all baseline worktrees and branches for `project` except `keep_runid`."""

    prefix = f"ai-baseline/{project}-c"
    r = git("for-each-ref", "--format=%(refname:short)",
            f"refs/heads/{prefix}*", check=False)
    if r.returncode == 0:
        for br in r.stdout.splitlines():
            br = br.strip()
            if not br.startswith(prefix):
                continue
            tail = br[len(prefix):]
            if tail.isdigit() and int(tail) == keep_runid:
                continue
            rid = int(tail) if tail.isdigit() else None
            if rid is not None:
                remove_baseline(project, rid)
            else:
                git("branch", "-D", br, check=False)

    # На случай осиротевших папок в mergearea/<project>-<N> без ветки
    if MERGE_AREA.is_dir():
        dir_prefix = f"{project}-"
        for p in MERGE_AREA.iterdir():
            if not p.is_dir() or not p.name.startswith(dir_prefix):
                continue
            tail = p.name[len(dir_prefix):]
            if tail.isdigit() and int(tail) != keep_runid:
                git("worktree", "remove", "--force", str(p), check=False)
                if p.exists():
                    shutil.rmtree(p, ignore_errors=True)
        git("worktree", "prune", check=False)

def project_worktree_dirs(project: str) -> list[Path]:
    """mergearea/<project>-<runid> containers of this project."""
    if not MERGE_AREA.is_dir():
        return []
    prefix = f"{project}-"
    return [p for p in sorted(MERGE_AREA.iterdir())
            if p.is_dir() and p.name.startswith(prefix)
            and p.name[len(prefix):].isdigit()]


def project_branches(project: str) -> tuple[list[str], list[str]]:
    """(fix branches, baseline branches) of one project."""
    r = git("for-each-ref", "--format=%(refname:short)",
            "refs/heads/", check=False)
    if r.returncode != 0:
        return [], []
    names = [b.strip() for b in r.stdout.splitlines() if b.strip()]
    return ([b for b in names if b.startswith(f"ai-checker-{project}-")],
            [b for b in names if b.startswith(f"ai-baseline/{project}-c")])


def clean_project(project: str, *, dry_run: bool = False) -> dict:
    """Nuke EVERY git artifact of one project. Logs are harvested first;
    reports/ and the DB are NOT touched. Branches go with -D: 'clean'
    means clean, an unmerged fix is exactly what is being thrown away."""
    stats = {"logs": 0, "worktrees": 0, "branches": 0, "dirs": 0}
    fix_br, base_br = project_branches(project)
    containers = project_worktree_dirs(project)
    print(f"[ai-clean] {project}: {len(containers)} mergearea dir(s), "
          f"{len(fix_br)} fix branch(es), {len(base_br)} baseline branch(es)")

    # Children FIRST: a fix worktree lives inside the container, and the
    # parent must not be torn down while a child is still mounted.
    for container in containers:
        for child in sorted(container.iterdir()):
            if not child.is_dir():
                continue
            if dry_run:
                print(f"  WOULD    harvest + remove {child.name}")
            else:
                stats["logs"] += len(remove_fix_worktree(child))
            stats["worktrees"] += 1
        if dry_run:
            print(f"  WOULD    remove {container.name}/")
        else:
            # Legacy: the container itself may be a baseline worktree.
            git("worktree", "remove", "--force", str(container), check=False)
            if container.exists():
                shutil.rmtree(container, ignore_errors=True)
        stats["dirs"] += 1

    if not dry_run:
        git("worktree", "prune", check=False)

    for b in fix_br + base_br:
        if dry_run:
            print(f"  WOULD    git branch -D {b}")
            stats["branches"] += 1
        elif delete_branch(b, force=True):
            print(f"  deleted  {b}")
            stats["branches"] += 1
        else:
            print(f"  FAILED   {b}", file=sys.stderr)
    return stats


def _warn_orphan_issues(project: str) -> None:
    """Branches are gone, but DB rows may still say 'fixed'. merge.py
    recreates a missing branch from fix_commit, so until gc collects the
    object ai-merge would RESURRECT what clean just deleted."""
    import db
    rows = db.fixed_issues(project)
    if not rows:
        return
    keys = " ".join(f"{r['runid']}/{r['chunk_id']}/{r['local_id']}" for r in rows)
    print(f"[ai-clean] warn: {len(rows)} issue(s) still 'fixed' in DB — "
          f"ai-merge can resurrect them from sha until gc runs.\n"
          f"           close them:  make ai-reject-{project} FIXLIST=\"{keys}\"",
          file=sys.stderr)

def run_branch_cleanup(args) -> int:
    """Delete ai-checker-* branches by prefix: merged ones, plus
    unmerged with --force. Worktrees are NOT touched here."""
    prefix = f"ai-checker-{args.project}-" if args.project else "ai-checker-"
    merged, unmerged = list_branches(prefix, args.check_run)
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

    deleted = failed = 0
    print()
    for b in to_delete:
        if delete_branch(b, force=args.force):
            print(f"  deleted: {b}")
            deleted += 1
        else:
            print(f"  FAILED:  {b}", file=sys.stderr)
            failed += 1
    print(f"\n{deleted} deleted, {failed} failed")
    return 0 if failed == 0 else 1


def run_clean(args) -> int:
    """Nuke mergearea dirs + ALL branches (fix and baseline) per project.
    Logs are harvested first; reports/ and the DB are untouched."""
    from common import acquire_lock, discover_projects, release_lock
    projects = ([args.project] if args.project
                else [p.name for p in discover_projects()])
    rc = 0
    lock = acquire_lock()
    try:
        for name in projects:
            st = clean_project(name, dry_run=args.dry_run)
            print(f"[ai-clean] {name}: {st['logs']} log(s) saved, "
                  f"{st['worktrees']} worktree(s), "
                  f"{st['branches']} branch(es), {st['dirs']} dir(s)")
            if not args.dry_run:
                _warn_orphan_issues(name)
    finally:
        release_lock(lock)
    return rc


def run_clean_baselines(args) -> int:
    """Remove baselines whose issues no actor can act on anymore.
    Eligibility comes from transition_status via db.open_baselines()."""
    import db
    from common import acquire_lock, release_lock
    live = {(b["project"], b["run_id"]) for b in db.open_baselines()}
    lock = acquire_lock()
    try:
        removed = 0
        for b in db.all_baselines():
            if (b["project"], b["run_id"]) in live:
                continue
            if args.dry_run:
                print(f"  WOULD    {b['baseline_branch']} + mergearea dir")
            else:
                remove_baseline(b["project"], b["run_id"])
                print(f"  removed  {b['baseline_branch']}")
            removed += 1
        print(f"[ai-clean-baselines] {removed} baseline(s)"
              f"{' would be removed' if args.dry_run else ' removed'}")
    finally:
        release_lock(lock)
    return 0


def remote_web_url(remote: str = "origin") -> str:
    """https://github.com/owner/repo from the origin URL, or ''."""
    r = git("remote", "get-url", remote, check=False)
    url = r.stdout.strip()
    if not url:
        return ""
    m = re.match(r"git@([^:]+):(.+?)(?:\.git)?$", url)
    if m:
        return f"https://{m.group(1)}/{m.group(2)}"
    return re.sub(r"\.git$", "", url)

def commit_patch(sha: str) -> str:
    """The actual committed diff, without commit metadata."""
    r = git("show", "--format=", "--no-color", sha, check=False)
    return r.stdout.strip() if r.returncode == 0 else ""

def blob_text(sha: str, rel: str) -> str | None:
    r = git("show", f"{sha}:{rel}", check=False)
    return r.stdout if r.returncode == 0 else None


def main() -> int:
    ap = argparse.ArgumentParser(
        description="ai-checker git artifacts: branch cleanup and worktree teardown")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--project", help="only this project")
    g.add_argument("--all", action="store_true",
                   help="every project (default)")
    ap.add_argument("--check-run", type=int, default=None,
                    help="only branches from this check-run ('-c<N>-')")
    ap.add_argument("--force", action="store_true",
                    help="also delete unmerged branches (-D instead of -d)")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan, change nothing")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--clean", action="store_true",
                      help="nuke mergearea + ALL branches (logs harvested first)")
    mode.add_argument("--clean-baselines", action="store_true",
                      help="remove baselines with no pending work")
    args = ap.parse_args()

    if args.clean:
        return run_clean(args)
    if args.clean_baselines:
        return run_clean_baselines(args)
    return run_branch_cleanup(args)


if __name__ == "__main__":
    raise SystemExit(main())

