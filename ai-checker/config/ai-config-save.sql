-- ai-checker configuration dump.
-- saved 2026-11-24T12:58:45   schema v13   from /Users/skelet/code/vss_proj/kr_book/ai-checker/config/check.db
--
-- RESTORED : params.value, statuses.terminal/resolved,
--            transition_status.allowed
-- CONTEXT  : descr, default_value — code-owned, refreshed by seed,
--            editing them here has NO effect.
--
-- Apply: make ai-config-restore FILENAME=<this file>
--    or: sqlite3 config/check.db < <this file>

-- ── statuses (10) ──────────────────────────────
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('added','found by ai-review, not triaged yet',0,0)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('proposed-to-fix','validated local patch cached, not committed',0,0)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('analyze-by-dev','non-local change, needs a human',0,1)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('fixed','committed to a fix branch, awaiting merge',0,1)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('rejected','rejected by a human',1,1)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('failed','LLM/validation/commit failed',1,1)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('stale','superseded by a newer project report',1,1)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('no-action','classifier verified: not a defect',1,1)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('suppressed','matches an earlier rejected/no-action finding',1,1)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;
INSERT INTO statuses (name,descr,terminal,resolved) VALUES ('merged','patch merged into the base branch',1,0)
  ON CONFLICT(name) DO UPDATE SET terminal=excluded.terminal, resolved=excluded.resolved;

-- ── params (15, 3 changed) ────────────────────
INSERT INTO params (module,name,value,default_value,dt) VALUES ('check','carry_similarity','0.8','0.8','2026-11-18T21:44:58')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('check','carry_statuses','rejected,no-action','rejected,no-action','2026-11-18T21:44:58')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('check','max_chars','360000','80000','2026-11-17T16:38:52')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;   -- changed (default 80000)
INSERT INTO params (module,name,value,default_value,dt) VALUES ('check','suppress_hint','1','1','2026-11-18T22:59:49')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('check','tag_context_chars','240000','20000','2026-11-18T14:36:32')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;   -- changed (default 20000)
INSERT INTO params (module,name,value,default_value,dt) VALUES ('check','temperature','0.2','0.2','2026-11-17T16:38:52')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('fix','default_limit','50','50','2026-11-17T16:38:52')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('fix','max_diff_lines','20','20','2026-11-17T18:43:39')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('fix','max_source_chars','340000','100000','2026-11-17T16:38:52')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;   -- changed (default 100000)
INSERT INTO params (module,name,value,default_value,dt) VALUES ('fix','temperature','0.0','0.0','2026-11-17T16:38:52')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('fix','verify_on_commit','1','1','2026-11-24T10:30:23')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('fix','verify_on_merge','0','0','2026-11-24T10:30:23')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('fix','verify_targets','test-{project},analyze-{project}','test-{project},analyze-{project}','2026-11-22T14:12:43')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('llm','model_name','','','2026-11-17T16:38:52')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;
INSERT INTO params (module,name,value,default_value,dt) VALUES ('llm','url','http://127.0.0.1:1234','http://127.0.0.1:1234','2026-11-17T16:38:52')
  ON CONFLICT(module,name) DO UPDATE SET value=excluded.value, dt=excluded.dt;

-- ── transition_status (34, 0 disabled) ────────────
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','carry','suppressed','y','matches an earlier rejected/no-action')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','classify','analyze-by-dev','y','real defect, non-local fix')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','classify','failed','y','LLM/validation error')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','classify','no-action','y','verified not a defect')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','classify','proposed-to-fix','y','validated local patch cached')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','classify','analyze-by-dev','y','refetch -> non-local')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','classify','failed','y','cached proposal went stale')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','classify','no-action','y','refetch -> not a defect')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','classify','proposed-to-fix','y','re-validated cached proposal')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','commit','failed','y','commit failed')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','commit','fixed','y','successful commit of a proposal')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','decide','no-action','y','human: not a defect')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('analyze-by-dev','decide','no-action','y','human: not a defect')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('failed','decide','added','y','human reopen')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('failed','decide','no-action','y','human: not a defect')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('no-action','decide','added','y','human reopen')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','decide','no-action','y','human: not a defect')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('rejected','decide','added','y','human reopen')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('stale','decide','added','y','human reopen')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('suppressed','decide','added','y','human reopen')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('fixed','merge','merged','y','patch merged into base')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('analyze-by-dev','propose','failed','y','forced proposal failed validation')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('analyze-by-dev','propose','proposed-to-fix','y','forced patch for a non-local finding')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('failed','propose','proposed-to-fix','y','forced failed to reanalyze')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('no-action','propose','failed','y','forced proposal failed validation')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('no-action','propose','proposed-to-fix','y','forced patch for a not-a-defect finding')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','reject','rejected','y','human rejected')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('analyze-by-dev','reject','rejected','y','human rejected')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('failed','reject','rejected','y','human closed a failed finding')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('fixed','reject','rejected','y','human rejected the committed patch')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('no-action','reject','rejected','y','human rejected')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','reject','rejected','y','human rejected the proposal')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('added','stale','stale','y','superseded by newer report')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
INSERT INTO transition_status (status,actor,new_status,allowed,descr) VALUES ('proposed-to-fix','stale','stale','y','superseded by newer report')
  ON CONFLICT(status,actor,new_status) DO UPDATE SET allowed=excluded.allowed;
