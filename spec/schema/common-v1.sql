-- Discussion draft. Apply common-v1.sql, then exactly one owner-specific schema.
-- New empty catalogues only; NOT a migration or a complete service implementation.
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = DELETE;
PRAGMA synchronous = EXTRA;

CREATE TABLE authority_meta (
    singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
    kind TEXT NOT NULL CHECK (kind IN ('system','user')),
    authority_epoch BLOB NOT NULL CHECK (length(authority_epoch) = 16),
    current_boot_id BLOB NOT NULL CHECK (length(current_boot_id) = 16),
    owner_scope_ref BLOB,
    catalogue_revision INTEGER NOT NULL DEFAULT 1 CHECK (catalogue_revision >= 1),
    CHECK ((kind = 'system' AND owner_scope_ref IS NULL) OR
           (kind = 'user' AND length(owner_scope_ref) = 16 AND owner_scope_ref IS NOT NULL))
) STRICT;

CREATE TABLE plans (
    plan_id BLOB PRIMARY KEY NOT NULL CHECK (length(plan_id) = 16),
    authority_epoch BLOB NOT NULL CHECK (length(authority_epoch) = 16),
    actor_ref BLOB NOT NULL CHECK (length(actor_ref) = 16),
    scope_ref BLOB NOT NULL CHECK (length(scope_ref) = 16),
    kind TEXT NOT NULL CHECK (kind IN ('install','activate','remove','purge','launch')),
    component_id TEXT NOT NULL CHECK (
        length(component_id) BETWEEN 1 AND 128 AND
        component_id NOT GLOB '*[^a-z0-9._-]*' AND instr(component_id,char(0)) = 0),
    install_ref BLOB CHECK (install_ref IS NULL OR length(install_ref) = 16),
    generation_ref BLOB CHECK (generation_ref IS NULL OR length(generation_ref) = 16),
    data_epoch_ref BLOB CHECK (data_epoch_ref IS NULL OR length(data_epoch_ref) = 16),
    artifact_ref BLOB CHECK (artifact_ref IS NULL OR length(artifact_ref) = 32),
    retention TEXT NOT NULL CHECK (retention IN ('none','keep_data','purge_data')),
    action_id TEXT CHECK (action_id IS NULL OR (length(action_id) BETWEEN 1 AND 64
        AND action_id NOT GLOB '*[^a-z0-9._-]*' AND instr(action_id,char(0)) = 0)),
    intent_digest BLOB NOT NULL CHECK (length(intent_digest) = 32),
    boot_id BLOB NOT NULL CHECK (length(boot_id) = 16),
    expires_boottime_ns INTEGER NOT NULL CHECK (expires_boottime_ns >= 0),
    state TEXT NOT NULL DEFAULT 'open' CHECK (state IN ('open','accepted','invalidated','expired')),
    UNIQUE (plan_id, actor_ref, intent_digest),
    CHECK ((kind = 'launch' AND action_id IS NOT NULL) OR (kind <> 'launch' AND action_id IS NULL)),
    CHECK ((kind = 'install' AND install_ref IS NULL AND artifact_ref IS NOT NULL) OR
           (kind = 'activate' AND install_ref IS NOT NULL AND artifact_ref IS NOT NULL) OR
           (kind = 'remove' AND install_ref IS NOT NULL AND retention <> 'none') OR
           (kind = 'purge' AND data_epoch_ref IS NOT NULL AND retention = 'purge_data') OR
           (kind = 'launch' AND install_ref IS NOT NULL AND generation_ref IS NOT NULL
                            AND data_epoch_ref IS NOT NULL AND retention = 'none'))
) STRICT;

CREATE TABLE plan_reads (
    plan_id BLOB NOT NULL REFERENCES plans(plan_id),
    source_epoch BLOB NOT NULL CHECK (length(source_epoch) = 16),
    subject_kind TEXT NOT NULL CHECK (subject_kind IN
        ('scope','component','install','generation','data','policy')),
    subject_ref BLOB NOT NULL CHECK (length(subject_ref) BETWEEN 1 AND 128),
    expected_revision INTEGER NOT NULL CHECK (expected_revision >= 1),
    PRIMARY KEY (plan_id, source_epoch, subject_kind, subject_ref)
) STRICT;

CREATE TABLE plan_effects (
    plan_id BLOB NOT NULL REFERENCES plans(plan_id),
    effect_no INTEGER NOT NULL CHECK (effect_no >= 0),
    subject_kind TEXT NOT NULL CHECK (subject_kind IN
        ('install','generation','data','grant','integration','storage')),
    component_id TEXT NOT NULL,
    subject_ref BLOB NOT NULL CHECK (length(subject_ref) IN (16,32)),
    disposition TEXT NOT NULL CHECK (disposition IN
        ('create','activate','stop','remove','retain','revoke')),
    reason_code TEXT NOT NULL CHECK (length(reason_code) BETWEEN 1 AND 64),
    PRIMARY KEY (plan_id, effect_no)
) STRICT;

CREATE TABLE operations (
    operation_id BLOB PRIMARY KEY NOT NULL CHECK (length(operation_id) = 16),
    plan_id BLOB NOT NULL UNIQUE,
    actor_ref BLOB NOT NULL CHECK (length(actor_ref) = 16),
    request_key BLOB NOT NULL CHECK (length(request_key) = 16),
    intent_digest BLOB NOT NULL CHECK (length(intent_digest) = 32),
    state TEXT NOT NULL CHECK (state IN ('accepted','running','blocked','recovering','completed')),
    phase TEXT NOT NULL CHECK (phase IN ('validate','stage','quiesce','publish','cleanup','reconcile')),
    reason_code TEXT NOT NULL DEFAULT '' CHECK (length(reason_code) <= 64 AND instr(reason_code,char(0)) = 0),
    outcome TEXT CHECK (outcome IN ('applied','applied_with_retention','cancelled','failed')),
    accepted_unix_ms INTEGER NOT NULL CHECK (accepted_unix_ms >= 0),
    completed_unix_ms INTEGER CHECK (completed_unix_ms IS NULL OR completed_unix_ms >= 0),
    result_kind TEXT CHECK (result_kind IN ('install','generation','data','launch')),
    result_ref BLOB CHECK (result_ref IS NULL OR length(result_ref) = 16),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    UNIQUE (actor_ref, request_key),
    FOREIGN KEY (plan_id, actor_ref, intent_digest)
        REFERENCES plans(plan_id, actor_ref, intent_digest),
    CHECK ((state = 'completed' AND outcome IS NOT NULL) OR
           (state <> 'completed' AND outcome IS NULL)),
    CHECK ((state = 'completed') = (completed_unix_ms IS NOT NULL)),
    CHECK ((result_kind IS NULL) = (result_ref IS NULL))
) STRICT;

CREATE TABLE operation_steps (
    operation_id BLOB NOT NULL REFERENCES operations(operation_id),
    step_no INTEGER NOT NULL CHECK (step_no >= 0),
    action TEXT NOT NULL CHECK (action IN
        ('verify','stage','quiesce','publish','activate','revoke','delete','release','reconcile')),
    subject_kind TEXT NOT NULL CHECK (subject_kind IN
        ('install','generation','data','grant','integration','storage','launch')),
    subject_ref BLOB NOT NULL CHECK (length(subject_ref) IN (16,32)),
    state TEXT NOT NULL CHECK (state IN ('planned','executing','applied','uncertain')),
    evidence_digest BLOB CHECK (evidence_digest IS NULL OR length(evidence_digest) = 32),
    PRIMARY KEY (operation_id, step_no)
) STRICT;

-- Durable correlation across authorities, not a cross-database foreign key.
CREATE TABLE operation_links (
    operation_id BLOB NOT NULL REFERENCES operations(operation_id),
    link_no INTEGER NOT NULL CHECK (link_no >= 0),
    remote_epoch BLOB NOT NULL CHECK (length(remote_epoch) = 16),
    remote_plan_ref BLOB NOT NULL CHECK (length(remote_plan_ref) = 16),
    remote_intent_digest BLOB NOT NULL CHECK (length(remote_intent_digest) = 32),
    remote_request_key BLOB NOT NULL CHECK (length(remote_request_key) = 16),
    remote_operation_ref BLOB CHECK (remote_operation_ref IS NULL OR length(remote_operation_ref) = 16),
    state TEXT NOT NULL CHECK (state IN ('prepared','accepted','completed','recovery_required')),
    PRIMARY KEY (operation_id,link_no),
    UNIQUE (remote_epoch,remote_request_key),
    CHECK (state NOT IN ('accepted','completed') OR remote_operation_ref IS NOT NULL)
) STRICT;

CREATE TRIGGER immutable_plan BEFORE UPDATE OF
    plan_id, authority_epoch, actor_ref, scope_ref, kind, component_id, install_ref,
    generation_ref, data_epoch_ref, artifact_ref, retention, action_id, intent_digest,
    boot_id, expires_boottime_ns ON plans
BEGIN SELECT RAISE(ABORT, 'immutable plan'); END;

CREATE TRIGGER accept_current_plan BEFORE INSERT ON operations
WHEN NOT EXISTS (
    SELECT 1 FROM plans p, authority_meta m
    WHERE p.plan_id = NEW.plan_id AND p.state = 'open'
      AND p.authority_epoch = m.authority_epoch AND p.boot_id = m.current_boot_id
)
BEGIN SELECT RAISE(ABORT, 'stale or consumed plan'); END;

CREATE TRIGGER consume_plan AFTER INSERT ON operations
BEGIN UPDATE plans SET state = 'accepted' WHERE plan_id = NEW.plan_id; END;

CREATE TRIGGER immutable_step_target BEFORE UPDATE OF
    operation_id, step_no, action, subject_kind, subject_ref ON operation_steps
BEGIN SELECT RAISE(ABORT, 'immutable step target'); END;

CREATE TRIGGER immutable_link_intent BEFORE UPDATE OF operation_id,link_no,
    remote_epoch,remote_plan_ref,remote_intent_digest,remote_request_key ON operation_links
BEGIN SELECT RAISE(ABORT, 'immutable delegated intent'); END;

CREATE TRIGGER completed_operation_terminal BEFORE UPDATE OF state,outcome ON operations
WHEN OLD.state = 'completed' AND (NEW.state <> OLD.state OR NEW.outcome IS NOT OLD.outcome)
BEGIN SELECT RAISE(ABORT, 'completed operation is terminal'); END;

CREATE TRIGGER immutable_operation_intent BEFORE UPDATE OF operation_id,plan_id,
    actor_ref,request_key,intent_digest,accepted_unix_ms ON operations
BEGIN SELECT RAISE(ABORT, 'immutable operation intent'); END;

CREATE TRIGGER immutable_remote_receipt BEFORE UPDATE OF remote_operation_ref ON operation_links
WHEN OLD.remote_operation_ref IS NOT NULL AND NEW.remote_operation_ref IS NOT OLD.remote_operation_ref
BEGIN SELECT RAISE(ABORT, 'remote receipt already bound'); END;
