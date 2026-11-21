-- ai-checker schema (v11)
-- Loaded by db._schema_sql() when creating a fresh check.db.
-- Version tracked in db.SCHEMA_VERSION and PRAGMA user_version.
--
-- To regenerate from scratch:
--   rm config/check.db*
--   make ai-check-<proj>

-- Single source of truth for issue statuses.
-- terminal = 1: final state — automation never overwrites it and
-- never picks such an issue up for reprocessing.
-- descr is refreshed from db.STATUSES_META; terminal is local policy.
CREATE TABLE IF NOT EXISTS statuses (
    name     TEXT PRIMARY KEY,
    descr    TEXT NOT NULL,
    terminal INTEGER NOT NULL DEFAULT 0 CHECK (terminal IN (0, 1))
);

CREATE TABLE IF NOT EXISTS base_commit (
    sha         TEXT PRIMARY KEY,
    short_sha   TEXT NOT NULL,
    branch      TEXT,
    first_dt    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS llmruns (
    runid       INTEGER PRIMARY KEY AUTOINCREMENT,
    kind        TEXT NOT NULL CHECK (kind IN ('check','fix')),
    url         TEXT NOT NULL,
    name        TEXT,
    start_time  TEXT NOT NULL,
    end_time    TEXT,
    status      TEXT NOT NULL DEFAULT 'running'
                CHECK (status IN ('running','ok','failed')),
    in_tokens   INTEGER NOT NULL DEFAULT 0,
    out_tokens  INTEGER NOT NULL DEFAULT 0,
    requests    INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_llmruns_start ON llmruns(start_time);

CREATE TABLE IF NOT EXISTS report (
    run_id      INTEGER REFERENCES llmruns(runid),
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project     TEXT NOT NULL,
    path        TEXT NOT NULL UNIQUE,
    base_sha    TEXT NOT NULL REFERENCES base_commit(sha),
    model       TEXT,
    kind        TEXT NOT NULL,
    started     TEXT NOT NULL,
    duration_s  REAL,
    dt          TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS issue (
    runid       INTEGER NOT NULL REFERENCES llmruns(runid),
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    report_id   INTEGER NOT NULL REFERENCES report(id) ON DELETE CASCADE,
    finding_id  TEXT NOT NULL,
    chunk_id    INTEGER NOT NULL,
    local_id    INTEGER NOT NULL,
    section     TEXT NOT NULL,
    title       TEXT NOT NULL,
    text        TEXT NOT NULL,
    status      TEXT NOT NULL DEFAULT 'added' REFERENCES statuses(name),
    reason      TEXT,
    direction   TEXT,
    fix_branch  TEXT,
    fix_commit  TEXT,
    merge_commit TEXT,
    fix_subject TEXT,
    dt          TEXT NOT NULL,
    UNIQUE (report_id, chunk_id, local_id)
);

CREATE INDEX IF NOT EXISTS idx_issue_runid ON issue(runid);
CREATE INDEX IF NOT EXISTS idx_issue_status ON issue(status);
CREATE INDEX IF NOT EXISTS idx_issue_report ON issue(report_id);

CREATE TABLE ticket (
    runid      INTEGER NOT NULL REFERENCES llmruns(runid), -- = issue.runid (check run)
    issue_id   INTEGER NOT NULL REFERENCES issue(id) ON DELETE CASCADE,
    tracker    TEXT NOT NULL,           -- 'jira' | 'github' | ...
    key        TEXT NOT NULL,           -- 'NGUTILS-123'
    url        TEXT,
    remote_state TEXT,                  -- last status in Jira
    intent     TEXT CHECK (intent IN ('merge','reject','rework','defer')),  -- NULL | 'merge' | 'reject' | 'rework'
    intent_dt  TEXT,
    synced_dt  TEXT NOT NULL,
    PRIMARY KEY (tracker, key)
);
CREATE UNIQUE INDEX idx_ticket_issue ON ticket(issue_id, tracker);

CREATE TABLE proposal_commit (
    runid           INTEGER NOT NULL REFERENCES llmruns(runid),
    issue_id        INTEGER NOT NULL REFERENCES issue(id) ON DELETE CASCADE,
    seq             INTEGER NOT NULL DEFAULT 1,
    fix_runid       INTEGER NOT NULL DEFAULT 0 REFERENCES llmruns(runid),
    file            TEXT NOT NULL,
    edits_json      TEXT,
    old_snippet     TEXT NOT NULL,
    new_snippet     TEXT NOT NULL,
    explanation     TEXT NOT NULL DEFAULT '',
    commit_msg      TEXT NOT NULL,
    commit_subject  TEXT NOT NULL DEFAULT '',
    reason          TEXT NOT NULL DEFAULT '',
    file_sha256     TEXT NOT NULL,
    dt              TEXT NOT NULL,
    PRIMARY KEY (runid, issue_id, seq)
);

CREATE INDEX IF NOT EXISTS idx_proposal_fix_run ON proposal_commit(fix_runid);
CREATE INDEX IF NOT EXISTS idx_proposal_commit ON proposal_commit(runid);
CREATE INDEX IF NOT EXISTS idx_proposal_commit_dt ON proposal_commit(dt);

CREATE TABLE IF NOT EXISTS modules (
    name  TEXT PRIMARY KEY,
    descr TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS params (
    module        TEXT NOT NULL REFERENCES modules(name),
    name          TEXT NOT NULL,
    value         TEXT NOT NULL,
    default_value TEXT NOT NULL,
    dt            TEXT NOT NULL,
    PRIMARY KEY (module, name)
);

-- Actors: specific operations (pipeline step / Python method)
-- that transition an issue between statuses. Excludes ai-check,
-- which creates 'added' directly (INSERT) rather than performing a transition.
CREATE TABLE actors (
    name  TEXT PRIMARY KEY,
    descr TEXT
);

-- Valid transitions. Step eligibility =
--   SELECT DISTINCT status WHERE actor = ? AND allowed = 'y'.
-- allowed is a DB-owned debug flag ('n' disables the transition);
-- seeding does NOT reset it.
CREATE TABLE transition_status (
    status     TEXT NOT NULL REFERENCES statuses(name),
    actor      TEXT NOT NULL REFERENCES actors(name),
    new_status TEXT NOT NULL REFERENCES statuses(name),
    allowed    TEXT NOT NULL DEFAULT 'y' CHECK (allowed IN ('y','n')),
    descr      TEXT,
    PRIMARY KEY (status, actor, new_status)
);
CREATE INDEX idx_transition_actor ON transition_status(actor);

