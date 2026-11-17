-- ai-checker schema (v11)
-- Loaded by db._schema_sql() when creating a fresh check.db.
-- Version tracked in db.SCHEMA_VERSION and PRAGMA user_version.
--
-- To regenerate from scratch:
--   rm config/check.db*
--   make ai-check-<proj>

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
    fix_subject TEXT,
    dt          TEXT NOT NULL,
    UNIQUE (report_id, chunk_id, local_id)
);

CREATE INDEX IF NOT EXISTS idx_issue_runid ON issue(runid);
CREATE INDEX IF NOT EXISTS idx_issue_status ON issue(status);
CREATE INDEX IF NOT EXISTS idx_issue_report ON issue(report_id);

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

