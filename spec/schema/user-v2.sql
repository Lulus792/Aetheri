-- Schema v2; requires common-v2.sql. One trusted writer per account.
PRAGMA application_id = 1096042581; -- ATHU
PRAGMA user_version = 2;

-- Authenticated projection of system-owned installation facts, never app-written.
CREATE TABLE install_bindings (
    install_id BLOB PRIMARY KEY NOT NULL CHECK (length(install_id) = 16),
    component_id TEXT NOT NULL,
    publisher_ref BLOB NOT NULL CHECK (length(publisher_ref) = 16),
    owner_scope BLOB NOT NULL CHECK (length(owner_scope) = 16),
    system_epoch BLOB NOT NULL CHECK (length(system_epoch) = 16),
    install_revision INTEGER NOT NULL CHECK (install_revision >= 1),
    state TEXT NOT NULL CHECK (state IN ('active','revoked')),
    UNIQUE (install_id,component_id,publisher_ref,owner_scope)
) STRICT;

CREATE TABLE data_epochs (
    data_epoch_id BLOB PRIMARY KEY NOT NULL CHECK (length(data_epoch_id) = 16),
    component_id TEXT NOT NULL,
    publisher_ref BLOB NOT NULL CHECK (length(publisher_ref) = 16),
    owner_scope BLOB NOT NULL CHECK (length(owner_scope) = 16),
    storage_ref BLOB NOT NULL UNIQUE CHECK (length(storage_ref) = 16),
    state TEXT NOT NULL CHECK (state IN ('active','retained','purging','purged')),
    data_schema_version INTEGER NOT NULL CHECK (data_schema_version >= 0),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    UNIQUE (data_epoch_id,component_id,publisher_ref,owner_scope)
) STRICT;

CREATE TABLE attachments (
    install_id BLOB PRIMARY KEY NOT NULL,
    data_epoch_id BLOB NOT NULL UNIQUE,
    component_id TEXT NOT NULL,
    publisher_ref BLOB NOT NULL,
    owner_scope BLOB NOT NULL,
    FOREIGN KEY (install_id,component_id,publisher_ref,owner_scope)
        REFERENCES install_bindings(install_id,component_id,publisher_ref,owner_scope),
    FOREIGN KEY (data_epoch_id,component_id,publisher_ref,owner_scope)
        REFERENCES data_epochs(data_epoch_id,component_id,publisher_ref,owner_scope)
) STRICT;

CREATE TABLE launches (
    launch_id BLOB PRIMARY KEY NOT NULL CHECK (length(launch_id) = 16),
    operation_id BLOB NOT NULL UNIQUE REFERENCES operations(operation_id),
    install_id BLOB NOT NULL REFERENCES install_bindings(install_id),
    data_epoch_id BLOB NOT NULL REFERENCES data_epochs(data_epoch_id),
    generation_ref BLOB NOT NULL CHECK (length(generation_ref) = 16),
    lease_ref BLOB NOT NULL CHECK (length(lease_ref) = 16),
    system_epoch BLOB NOT NULL CHECK (length(system_epoch) = 16),
    boot_id BLOB NOT NULL CHECK (length(boot_id) = 16),
    session_ref TEXT NOT NULL CHECK (length(CAST(session_ref AS BLOB)) BETWEEN 1 AND 128),
    seat_ref TEXT NOT NULL CHECK (length(CAST(seat_ref AS BLOB)) BETWEEN 1 AND 128),
    supervisor_unit TEXT NOT NULL CHECK (length(CAST(supervisor_unit AS BLOB)) BETWEEN 1 AND 256),
    invocation_ref BLOB CHECK (invocation_ref IS NULL OR length(invocation_ref) = 16),
    state TEXT NOT NULL CHECK (state IN
        ('requested','preparing','starting','running','stopping','exited','start_failed','recovery_required')),
    readiness TEXT NOT NULL CHECK (readiness IN ('unknown','ready','unavailable')),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1)
) STRICT;
CREATE INDEX live_launches_by_install ON launches(install_id,state);

CREATE TRIGGER data_owner BEFORE INSERT ON data_epochs
WHEN NEW.owner_scope IS NOT (SELECT owner_scope_ref FROM authority_meta WHERE singleton = 1 AND kind = 'user')
BEGIN SELECT RAISE(ABORT, 'wrong data owner'); END;
CREATE TRIGGER binding_owner BEFORE INSERT ON install_bindings
WHEN NEW.owner_scope IS NOT (SELECT owner_scope_ref FROM authority_meta WHERE singleton = 1 AND kind = 'user')
BEGIN SELECT RAISE(ABORT, 'wrong binding owner'); END;
CREATE TRIGGER immutable_data_identity BEFORE UPDATE OF
    data_epoch_id,component_id,publisher_ref,owner_scope,storage_ref ON data_epochs
BEGIN SELECT RAISE(ABORT, 'immutable data ownership'); END;
CREATE TRIGGER immutable_binding_identity BEFORE UPDATE OF
    install_id,component_id,publisher_ref,owner_scope ON install_bindings
BEGIN SELECT RAISE(ABORT, 'immutable binding ownership'); END;
CREATE TRIGGER attach_active BEFORE INSERT ON attachments
WHEN NOT EXISTS (SELECT 1 FROM install_bindings WHERE install_id = NEW.install_id AND state = 'active')
  OR NOT EXISTS (SELECT 1 FROM data_epochs WHERE data_epoch_id = NEW.data_epoch_id AND state = 'active')
BEGIN SELECT RAISE(ABORT, 'inactive attachment'); END;
CREATE TRIGGER replace_attachment BEFORE UPDATE ON attachments
BEGIN SELECT RAISE(ABORT, 'detach before attaching'); END;
CREATE TRIGGER detach_quiescent BEFORE DELETE ON attachments
WHEN EXISTS (SELECT 1 FROM launches WHERE install_id = OLD.install_id AND state NOT IN ('exited','start_failed'))
BEGIN SELECT RAISE(ABORT, 'launch not reconciled'); END;
CREATE TRIGGER purge_detached BEFORE UPDATE OF state ON data_epochs
WHEN NEW.state IN ('retained','purging','purged') AND EXISTS (
    SELECT 1 FROM attachments WHERE data_epoch_id = OLD.data_epoch_id)
BEGIN SELECT RAISE(ABORT, 'data still attached'); END;
CREATE TRIGGER no_purge_resurrection BEFORE UPDATE OF state ON data_epochs
WHEN (OLD.state = 'purging' AND NEW.state NOT IN ('purging','purged')) OR
     (OLD.state = 'purged' AND NEW.state <> 'purged')
BEGIN SELECT RAISE(ABORT, 'purge cannot resurrect data'); END;
CREATE TRIGGER admit_launch BEFORE INSERT ON launches
WHEN NEW.state <> 'requested' OR NOT EXISTS (
    SELECT 1 FROM attachments a JOIN install_bindings i USING (install_id)
    JOIN data_epochs d USING (data_epoch_id)
    JOIN operations o ON o.operation_id = NEW.operation_id
    JOIN plans p ON p.plan_id = o.plan_id
    WHERE a.install_id = NEW.install_id AND a.data_epoch_id = NEW.data_epoch_id
      AND i.state = 'active' AND d.state = 'active' AND i.system_epoch = NEW.system_epoch
      AND p.kind = 'launch' AND p.install_ref = NEW.install_id
      AND p.generation_ref = NEW.generation_ref AND p.data_epoch_ref = NEW.data_epoch_id)
BEGIN SELECT RAISE(ABORT, 'launch not admitted'); END;
CREATE TRIGGER immutable_launch_identity BEFORE UPDATE OF
    launch_id,operation_id,install_id,data_epoch_id,generation_ref,lease_ref,
    system_epoch,boot_id,session_ref,seat_ref,supervisor_unit ON launches
BEGIN SELECT RAISE(ABORT, 'immutable launch identity'); END;
CREATE TRIGGER terminal_launch BEFORE UPDATE OF state ON launches
WHEN OLD.state IN ('exited','start_failed') AND NEW.state <> OLD.state
BEGIN SELECT RAISE(ABORT, 'new run needs new identity'); END;

CREATE UNIQUE INDEX launch_grant_binding ON launches(launch_id,install_id,data_epoch_id);

CREATE TABLE resources (
    resource_id BLOB PRIMARY KEY NOT NULL CHECK (length(resource_id) = 16),
    owner_scope BLOB NOT NULL CHECK (length(owner_scope) = 16),
    kind TEXT NOT NULL CHECK (kind = 'file'),
    fixture_root_ref BLOB NOT NULL CHECK (length(fixture_root_ref) = 16),
    locator TEXT NOT NULL CHECK (length(CAST(locator AS BLOB)) BETWEEN 1 AND 4096 AND
        instr(locator,char(0)) = 0 AND substr(locator,1,1) <> '/' AND substr(locator,-1) <> '/'
        AND instr('/' || locator || '/','/../') = 0 AND instr('/' || locator || '/','/./') = 0
        AND instr(locator,'//') = 0),
    display_name TEXT NOT NULL CHECK (length(CAST(display_name AS BLOB)) BETWEEN 1 AND 256 AND instr(display_name,char(0)) = 0),
    observed_boot_id BLOB NOT NULL CHECK (length(observed_boot_id) = 16),
    broker_incarnation BLOB NOT NULL CHECK (length(broker_incarnation) = 16),
    observed_device INTEGER NOT NULL CHECK (observed_device >= 0),
    observed_inode INTEGER NOT NULL CHECK (observed_inode >= 1),
    observed_size INTEGER NOT NULL CHECK (observed_size BETWEEN 0 AND 65536),
    state TEXT NOT NULL CHECK (state IN ('bound','needs_reselect','retired')),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1)
) STRICT;

CREATE TABLE grants (
    grant_id BLOB PRIMARY KEY NOT NULL CHECK (length(grant_id) = 16),
    resource_id BLOB NOT NULL REFERENCES resources(resource_id),
    launch_id BLOB NOT NULL,
    install_id BLOB NOT NULL,
    data_epoch_id BLOB NOT NULL,
    issuer_ref BLOB NOT NULL CHECK (length(issuer_ref) = 16),
    rights TEXT NOT NULL CHECK (rights = 'read'),
    mode TEXT NOT NULL CHECK (mode = 'native'),
    lifetime TEXT NOT NULL CHECK (lifetime = 'launch'),
    state TEXT NOT NULL CHECK (state IN ('pending','active','closing','revoked','expired')),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    UNIQUE (grant_id,launch_id),
    FOREIGN KEY (launch_id,install_id,data_epoch_id) REFERENCES launches(launch_id,install_id,data_epoch_id)
) STRICT;
CREATE INDEX grants_by_data ON grants(data_epoch_id,state);

CREATE TABLE access_leases (
    access_id BLOB PRIMARY KEY NOT NULL CHECK (length(access_id) = 16),
    grant_id BLOB NOT NULL,
    launch_id BLOB NOT NULL,
    broker_incarnation BLOB NOT NULL CHECK (length(broker_incarnation) = 16),
    request_key BLOB NOT NULL CHECK (length(request_key) = 16),
    mode TEXT NOT NULL CHECK (mode = 'native'),
    state TEXT NOT NULL CHECK (state IN ('opening','open','closing','released','uncertain')),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    UNIQUE (launch_id,broker_incarnation,request_key),
    FOREIGN KEY (grant_id,launch_id) REFERENCES grants(grant_id,launch_id)
) STRICT;
CREATE INDEX accesses_by_grant ON access_leases(grant_id,state);

CREATE TRIGGER user_gate_target BEFORE INSERT ON admission_gates
WHEN NEW.subject_kind <> 'data' OR NOT EXISTS (
    SELECT 1 FROM data_epochs WHERE data_epoch_id = NEW.subject_ref)
BEGIN SELECT RAISE(ABORT, 'invalid user gate'); END;
CREATE TRIGGER reopen_user_gate BEFORE UPDATE ON admission_gates
WHEN NEW.state = 'open' AND NOT EXISTS (
    SELECT 1 FROM data_epochs WHERE data_epoch_id = NEW.subject_ref AND state = 'active')
BEGIN SELECT RAISE(ABORT, 'data cannot reopen'); END;
CREATE TRIGGER create_data_gate AFTER INSERT ON data_epochs
BEGIN
    INSERT INTO admission_gates(subject_kind,subject_ref,state)
    VALUES ('data',NEW.data_epoch_id,CASE WHEN NEW.state = 'active' THEN 'open' ELSE 'closed' END);
END;
CREATE TRIGGER protect_data_gate BEFORE DELETE ON data_epochs
WHEN EXISTS (SELECT 1 FROM admission_gates WHERE subject_kind = 'data' AND subject_ref = OLD.data_epoch_id)
BEGIN SELECT RAISE(ABORT, 'remove gate before pruning data'); END;

CREATE TRIGGER launch_data_admission BEFORE INSERT ON launches
WHEN NOT EXISTS (
    SELECT 1 FROM admission_gates f JOIN authority_meta m ON m.singleton = 1
    JOIN operations o ON o.operation_id = NEW.operation_id JOIN plans p ON p.plan_id = o.plan_id
    JOIN install_bindings i ON i.install_id = NEW.install_id
    WHERE f.subject_kind = 'data' AND f.subject_ref = NEW.data_epoch_id AND f.state = 'open'
      AND m.runtime_incarnation IS NOT NULL AND NEW.boot_id = m.current_boot_id
      AND p.scope_ref = m.owner_scope_ref AND p.component_id = i.component_id)
 OR NEW.supervisor_unit <> 'aetheri-app-' || lower(hex(NEW.launch_id)) || '.service'
BEGIN SELECT RAISE(ABORT, 'local launch gate closed'); END;

CREATE TRIGGER resource_current BEFORE INSERT ON resources
WHEN NEW.state <> 'bound' OR NOT EXISTS (
    SELECT 1 FROM authority_meta WHERE singleton = 1 AND kind = 'user'
      AND owner_scope_ref = NEW.owner_scope AND current_boot_id = NEW.observed_boot_id
      AND runtime_incarnation = NEW.broker_incarnation)
BEGIN SELECT RAISE(ABORT, 'resource owner/context mismatch'); END;
CREATE TRIGGER immutable_resource BEFORE UPDATE OF resource_id,owner_scope,kind,fixture_root_ref,
    locator,display_name,observed_boot_id,broker_incarnation,observed_device,observed_inode,observed_size ON resources
BEGIN SELECT RAISE(ABORT, 'immutable selected resource'); END;
CREATE TRIGGER resource_transition BEFORE UPDATE ON resources
WHEN NEW.revision <= OLD.revision OR NOT (
    NEW.state = OLD.state OR
    (OLD.state = 'bound' AND NEW.state IN ('needs_reselect','retired')) OR
    (OLD.state = 'needs_reselect' AND NEW.state = 'retired'))
BEGIN SELECT RAISE(ABORT, 'resource cannot rebind'); END;

CREATE TRIGGER pending_grant BEFORE INSERT ON grants WHEN NEW.state <> 'pending'
BEGIN SELECT RAISE(ABORT, 'grant starts pending'); END;
CREATE TRIGGER immutable_grant BEFORE UPDATE OF grant_id,resource_id,launch_id,install_id,
    data_epoch_id,issuer_ref,rights,mode,lifetime ON grants
BEGIN SELECT RAISE(ABORT, 'immutable grant scope'); END;
CREATE TRIGGER grant_transition BEFORE UPDATE ON grants
WHEN NEW.revision <= OLD.revision OR NOT (
    NEW.state = OLD.state OR
    (OLD.state = 'pending' AND NEW.state IN ('active','closing')) OR
    (OLD.state = 'active' AND NEW.state = 'closing') OR
    (OLD.state = 'closing' AND NEW.state IN ('revoked','expired')))
BEGIN SELECT RAISE(ABORT, 'invalid grant transition/revision'); END;
CREATE TRIGGER activate_current_grant BEFORE UPDATE ON grants
WHEN NEW.state = 'active' AND NOT EXISTS (
    SELECT 1 FROM launches l JOIN resources r ON r.resource_id = NEW.resource_id
    JOIN authority_meta m ON m.singleton = 1
    JOIN admission_gates f ON f.subject_kind = 'data' AND f.subject_ref = NEW.data_epoch_id
    JOIN attachments a ON a.install_id = NEW.install_id AND a.data_epoch_id = NEW.data_epoch_id
    JOIN install_bindings i ON i.install_id = a.install_id
    WHERE l.launch_id = NEW.launch_id AND l.state = 'running' AND l.boot_id = m.current_boot_id
      AND r.owner_scope = m.owner_scope_ref AND r.state = 'bound'
      AND r.broker_incarnation = m.runtime_incarnation AND r.observed_boot_id = m.current_boot_id
      AND f.state = 'open' AND i.state = 'active')
BEGIN SELECT RAISE(ABORT, 'grant activation not admitted'); END;
CREATE TRIGGER grant_settled BEFORE UPDATE ON grants
WHEN NEW.state IN ('revoked','expired') AND EXISTS (
    SELECT 1 FROM access_leases WHERE grant_id = OLD.grant_id AND state <> 'released')
BEGIN SELECT RAISE(ABORT, 'grant has outstanding access'); END;

CREATE TRIGGER admit_access BEFORE INSERT ON access_leases
WHEN NEW.state <> 'opening' OR NOT EXISTS (
    SELECT 1 FROM grants g JOIN resources r ON r.resource_id = g.resource_id
    JOIN launches l ON l.launch_id = g.launch_id JOIN authority_meta m ON m.singleton = 1
    JOIN admission_gates f ON f.subject_kind = 'data' AND f.subject_ref = g.data_epoch_id
    WHERE g.grant_id = NEW.grant_id AND g.launch_id = NEW.launch_id AND g.state = 'active'
      AND r.state = 'bound' AND r.broker_incarnation = NEW.broker_incarnation
      AND m.runtime_incarnation = NEW.broker_incarnation AND r.observed_boot_id = m.current_boot_id
      AND l.state = 'running' AND l.boot_id = m.current_boot_id AND f.state = 'open')
BEGIN SELECT RAISE(ABORT, 'access admission closed'); END;
CREATE TRIGGER immutable_access BEFORE UPDATE OF access_id,grant_id,launch_id,
    broker_incarnation,request_key,mode ON access_leases
BEGIN SELECT RAISE(ABORT, 'immutable access binding'); END;
CREATE TRIGGER access_transition BEFORE UPDATE ON access_leases
WHEN NEW.revision <= OLD.revision OR NOT (
    NEW.state = OLD.state OR
    (OLD.state = 'opening' AND NEW.state IN ('open','closing','released','uncertain')) OR
    (OLD.state = 'open' AND NEW.state IN ('closing','released','uncertain')) OR
    (OLD.state = 'closing' AND NEW.state IN ('released','uncertain')) OR
    (OLD.state = 'uncertain' AND NEW.state IN ('closing','released')))
BEGIN SELECT RAISE(ABORT, 'invalid access transition/revision'); END;
CREATE TRIGGER open_current_access BEFORE UPDATE ON access_leases
WHEN NEW.state = 'open' AND NOT EXISTS (
    SELECT 1 FROM grants g JOIN resources r ON r.resource_id = g.resource_id
    JOIN authority_meta m ON m.singleton = 1
    JOIN admission_gates f ON f.subject_kind = 'data' AND f.subject_ref = g.data_epoch_id
    JOIN launches l ON l.launch_id = g.launch_id
    WHERE g.grant_id = NEW.grant_id AND g.state = 'active' AND r.state = 'bound'
      AND m.runtime_incarnation = NEW.broker_incarnation AND f.state = 'open'
      AND r.observed_boot_id = m.current_boot_id AND l.boot_id = m.current_boot_id AND l.state = 'running')
BEGIN SELECT RAISE(ABORT, 'access became stale'); END;

CREATE TRIGGER close_resource_grants AFTER UPDATE OF state ON resources WHEN NEW.state <> 'bound'
BEGIN
    UPDATE grants SET state = 'closing',revision = revision + 1
    WHERE resource_id = NEW.resource_id AND state IN ('pending','active');
END;
CREATE TRIGGER close_data_grants AFTER UPDATE OF state ON admission_gates
WHEN NEW.subject_kind = 'data' AND NEW.state <> 'open'
BEGIN
    UPDATE grants SET state = 'closing',revision = revision + 1
    WHERE data_epoch_id = NEW.subject_ref AND state IN ('pending','active');
END;
CREATE TRIGGER close_ended_launch_grants AFTER UPDATE OF state ON launches
WHEN NEW.state IN ('exited','start_failed')
BEGIN
    UPDATE grants SET state = 'closing',revision = revision + 1
    WHERE launch_id = NEW.launch_id AND state IN ('pending','active');
END;
CREATE TRIGGER restart_user_recovery AFTER UPDATE OF runtime_incarnation ON authority_meta
WHEN OLD.runtime_incarnation IS NOT NEW.runtime_incarnation
BEGIN
    UPDATE resources SET state = 'needs_reselect',revision = revision + 1
    WHERE state = 'bound' AND broker_incarnation IS NOT NEW.runtime_incarnation;
    UPDATE access_leases SET state = 'uncertain',revision = revision + 1
    WHERE state <> 'released' AND broker_incarnation IS NOT NEW.runtime_incarnation;
END;
CREATE TRIGGER purge_settled_access BEFORE UPDATE OF state ON data_epochs
WHEN NEW.state IN ('purging','purged') AND (
    NOT EXISTS (SELECT 1 FROM admission_gates WHERE subject_kind = 'data'
                AND subject_ref = NEW.data_epoch_id AND state = 'closed') OR
    EXISTS (SELECT 1 FROM grants WHERE data_epoch_id = NEW.data_epoch_id
            AND state NOT IN ('revoked','expired')))
BEGIN SELECT RAISE(ABORT, 'private access not settled'); END;

CREATE TRIGGER revoke_plan_scope BEFORE INSERT ON plans
WHEN NEW.kind = 'revoke' AND NOT EXISTS (
    SELECT 1 FROM grants g JOIN install_bindings i ON i.install_id = g.install_id
    JOIN authority_meta m ON m.singleton = 1
    WHERE g.grant_id = NEW.grant_ref AND g.install_id = NEW.install_ref
      AND g.data_epoch_id = NEW.data_epoch_ref AND g.state = 'active'
      AND NEW.scope_ref = m.owner_scope_ref AND NEW.component_id = i.component_id)
BEGIN SELECT RAISE(ABORT, 'revoke scope mismatch'); END;
CREATE TRIGGER accept_revoke_revision BEFORE INSERT ON operations
WHEN EXISTS (SELECT 1 FROM plans WHERE plan_id = NEW.plan_id AND kind = 'revoke')
 AND NOT EXISTS (
    SELECT 1 FROM plans p JOIN grants g ON g.grant_id = p.grant_ref
    JOIN plan_reads r ON r.plan_id = p.plan_id AND r.subject_kind = 'grant'
      AND r.subject_ref = g.grant_id JOIN authority_meta m ON m.singleton = 1
    WHERE p.plan_id = NEW.plan_id AND g.state = 'active'
      AND r.source_epoch = m.authority_epoch AND r.expected_revision = g.revision)
BEGIN SELECT RAISE(ABORT, 'stale revoke authorization'); END;

CREATE TRIGGER keep_live_access BEFORE DELETE ON access_leases WHEN OLD.state <> 'released'
BEGIN SELECT RAISE(ABORT, 'unresolved access cannot be pruned'); END;
CREATE TRIGGER keep_open_grant BEFORE DELETE ON grants WHEN OLD.state NOT IN ('revoked','expired')
BEGIN SELECT RAISE(ABORT, 'open grant cannot be pruned'); END;
CREATE TRIGGER keep_live_launch BEFORE DELETE ON launches WHEN OLD.state NOT IN ('exited','start_failed')
BEGIN SELECT RAISE(ABORT, 'live launch cannot be pruned'); END;
