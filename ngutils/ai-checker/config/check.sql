CREATE TABLE IF NOT EXISTS base_commit (
    sha         TEXT PRIMARY KEY,          -- full 40-char sha
    short_sha   TEXT NOT NULL,             -- 7-char for display
    branch      TEXT,                      -- nullable; unknown -> NULL
    first_dt    TEXT NOT NULL              -- ISO-8601, when first seen
);

CREATE TABLE IF NOT EXISTS report (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project     TEXT NOT NULL,             -- 'log', 'fs2', ...
    path        TEXT NOT NULL UNIQUE,      -- ROOT-relative
    base_sha    TEXT NOT NULL REFERENCES base_commit(sha),
    model       TEXT,                      -- e.g. 'qwen3.8-27b-splash'
    kind        TEXT NOT NULL,             -- 'full', 'notest', 'file'
    started     TEXT NOT NULL,             -- ISO-8601
    duration_s  REAL,
    dt          TEXT NOT NULL              -- when ingested
);

CREATE TABLE IF NOT EXISTS issue (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    report_id   INTEGER NOT NULL REFERENCES report(id) ON DELETE CASCADE,
    finding_id  TEXT NOT NULL,             -- '1/10', '16' (as in report)
    chunk_id    INTEGER NOT NULL,          -- '1/10' -> 1
    local_id    INTEGER NOT NULL,          -- '1/10' -> 10
    section     TEXT NOT NULL,             -- 'Критично', 'Важно', 'Мелочи'
    title       TEXT NOT NULL,             -- first line, for scan
    text        TEXT NOT NULL,             -- full finding
    status      TEXT NOT NULL DEFAULT 'proposed-to-fix'
                CHECK (status IN ('proposed-to-fix','analyze-by-dev',
                                  'fixed','rejected','failed')),
    reason      TEXT,                      -- classifier reason
    direction   TEXT,                      -- for needs_review
    fix_branch  TEXT,                      -- ai-checker-log-fix1-1
    fix_commit  TEXT,                      -- short sha
    dt          TEXT NOT NULL,             -- last status change
    UNIQUE (report_id, chunk_id, local_id)
);

CREATE INDEX IF NOT EXISTS idx_issue_status ON issue(status);
CREATE INDEX IF NOT EXISTS idx_issue_report ON issue(report_id);

