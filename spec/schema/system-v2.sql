-- Schema v2; requires common-v2.sql. No filesystem publication is done here.
PRAGMA application_id = 1096042579; -- ATHS
PRAGMA user_version = 2;

CREATE TABLE scopes (
    scope_id BLOB PRIMARY KEY NOT NULL CHECK (length(scope_id) = 16),
    kind TEXT NOT NULL CHECK (kind IN ('machine','account')),
    os_uid INTEGER,
    state TEXT NOT NULL CHECK (state IN ('active','retired')),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    CHECK ((kind = 'machine' AND os_uid IS NULL) OR
           (kind = 'account' AND os_uid BETWEEN 0 AND 4294967294 AND os_uid IS NOT NULL))
) STRICT;
CREATE UNIQUE INDEX one_machine_scope ON scopes(kind) WHERE kind = 'machine';
CREATE UNIQUE INDEX active_uid ON scopes(os_uid) WHERE kind = 'account' AND state = 'active';

CREATE TABLE components (
    component_id TEXT PRIMARY KEY NOT NULL CHECK (
        length(component_id) BETWEEN 1 AND 128 AND
        component_id NOT GLOB '*[^a-z0-9._-]*' AND instr(component_id,char(0)) = 0),
    publisher_ref BLOB NOT NULL CHECK (length(publisher_ref) = 16),
    kind TEXT NOT NULL CHECK (kind IN ('app','runtime','tool','plugin','base')),
    display_name TEXT NOT NULL CHECK (
        length(CAST(display_name AS BLOB)) BETWEEN 1 AND 256 AND instr(display_name,char(0)) = 0),
    protected INTEGER NOT NULL CHECK (protected IN (0,1)),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1)
) STRICT;

CREATE TABLE artifacts (
    artifact_id BLOB PRIMARY KEY NOT NULL CHECK (length(artifact_id) = 32),
    component_id TEXT NOT NULL REFERENCES components(component_id),
    version_label TEXT NOT NULL CHECK (
        length(CAST(version_label AS BLOB)) BETWEEN 1 AND 128 AND instr(version_label,char(0)) = 0),
    target TEXT NOT NULL CHECK (length(target) BETWEEN 1 AND 128),
    store_state TEXT NOT NULL CHECK (store_state IN ('staged','verified','reclaimed')),
    UNIQUE (artifact_id, component_id)
) STRICT;

CREATE TABLE installs (
    install_id BLOB PRIMARY KEY NOT NULL CHECK (length(install_id) = 16),
    component_id TEXT NOT NULL REFERENCES components(component_id),
    scope_id BLOB NOT NULL REFERENCES scopes(scope_id),
    state TEXT NOT NULL CHECK (state IN ('preparing','active','removing','removed')),
    active_generation_id BLOB,
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    UNIQUE (install_id, component_id),
    FOREIGN KEY (active_generation_id, install_id)
        REFERENCES generations(generation_id, install_id) DEFERRABLE INITIALLY DEFERRED,
    CHECK ((state IN ('preparing','removed') AND active_generation_id IS NULL) OR
           (state IN ('active','removing') AND active_generation_id IS NOT NULL))
) STRICT;
CREATE UNIQUE INDEX one_enrollment ON installs(component_id,scope_id) WHERE state <> 'removed';

CREATE TABLE generations (
    generation_id BLOB PRIMARY KEY NOT NULL CHECK (length(generation_id) = 16),
    install_id BLOB NOT NULL,
    component_id TEXT NOT NULL,
    artifact_id BLOB NOT NULL,
    publication_state TEXT NOT NULL CHECK (publication_state IN ('staged','committed','reclaiming','reclaimed')),
    activated_once INTEGER NOT NULL DEFAULT 0 CHECK (activated_once IN (0,1)),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    UNIQUE (generation_id, install_id),
    FOREIGN KEY (install_id, component_id) REFERENCES installs(install_id, component_id),
    FOREIGN KEY (artifact_id, component_id) REFERENCES artifacts(artifact_id, component_id)
) STRICT;
CREATE INDEX generations_by_artifact ON generations(artifact_id);

CREATE TABLE generation_edges (
    consumer_id BLOB NOT NULL REFERENCES generations(generation_id),
    provider_id BLOB NOT NULL REFERENCES generations(generation_id),
    PRIMARY KEY (consumer_id,provider_id),
    CHECK (consumer_id <> provider_id)
) STRICT;
CREATE INDEX edges_by_provider ON generation_edges(provider_id);

CREATE TABLE generation_leases (
    lease_id BLOB PRIMARY KEY NOT NULL CHECK (length(lease_id) = 16),
    generation_id BLOB NOT NULL REFERENCES generations(generation_id),
    holder_authority BLOB NOT NULL CHECK (length(holder_authority) = 16),
    holder_ref BLOB NOT NULL CHECK (length(holder_ref) = 16),
    kind TEXT NOT NULL CHECK (kind IN ('launch','recovery','pin')),
    boot_id BLOB NOT NULL CHECK (length(boot_id) = 16),
    state TEXT NOT NULL CHECK (state IN ('reserved','running','reconcile','released')),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    UNIQUE (holder_authority,holder_ref,kind)
) STRICT;
CREATE INDEX leases_by_generation ON generation_leases(generation_id,state);

CREATE TRIGGER admit_edge BEFORE INSERT ON generation_edges
WHEN NOT EXISTS (SELECT 1 FROM generations WHERE generation_id = NEW.provider_id AND publication_state = 'committed')
BEGIN SELECT RAISE(ABORT, 'provider unavailable'); END;
CREATE TRIGGER admit_lease BEFORE INSERT ON generation_leases
WHEN NOT EXISTS (SELECT 1 FROM generations WHERE generation_id = NEW.generation_id AND publication_state = 'committed')
BEGIN SELECT RAISE(ABORT, 'generation unavailable'); END;

CREATE TRIGGER immutable_component BEFORE UPDATE OF component_id,publisher_ref,kind ON components
BEGIN SELECT RAISE(ABORT, 'immutable component binding'); END;
CREATE TRIGGER immutable_artifact BEFORE UPDATE OF artifact_id,component_id,version_label,target ON artifacts
BEGIN SELECT RAISE(ABORT, 'immutable artifact'); END;
CREATE TRIGGER immutable_install BEFORE UPDATE OF install_id,component_id,scope_id ON installs
BEGIN SELECT RAISE(ABORT, 'immutable install'); END;
CREATE TRIGGER removed_install_terminal BEFORE UPDATE OF state ON installs
WHEN OLD.state = 'removed' AND NEW.state <> 'removed'
BEGIN SELECT RAISE(ABORT, 'removed install is terminal'); END;
CREATE TRIGGER immutable_generation BEFORE UPDATE OF generation_id,install_id,component_id,artifact_id ON generations
BEGIN SELECT RAISE(ABORT, 'immutable generation'); END;
CREATE TRIGGER no_generation_rearm BEFORE UPDATE OF activated_once ON generations
WHEN OLD.activated_once = 1 AND NEW.activated_once <> 1
BEGIN SELECT RAISE(ABORT, 'generation already activated'); END;

CREATE TRIGGER prepare_install BEFORE INSERT ON installs
WHEN NEW.state <> 'preparing' OR NEW.active_generation_id IS NOT NULL
BEGIN SELECT RAISE(ABORT, 'install must start preparing'); END;
CREATE TRIGGER verify_activation BEFORE UPDATE OF active_generation_id ON installs
WHEN NEW.active_generation_id IS NOT NULL AND NEW.active_generation_id IS NOT OLD.active_generation_id
 AND (NEW.state <> 'active' OR NOT EXISTS (
    SELECT 1 FROM generations g JOIN artifacts a ON a.artifact_id = g.artifact_id
    WHERE g.generation_id = NEW.active_generation_id AND g.install_id = NEW.install_id
      AND g.publication_state = 'committed' AND g.activated_once = 0 AND a.store_state = 'verified'
 ))
BEGIN SELECT RAISE(ABORT, 'invalid activation'); END;
CREATE TRIGGER record_activation AFTER UPDATE OF active_generation_id ON installs
WHEN NEW.active_generation_id IS NOT NULL AND NEW.active_generation_id IS NOT OLD.active_generation_id
BEGIN
    UPDATE generations SET activated_once = 1, revision = revision + 1
    WHERE generation_id = NEW.active_generation_id;
END;

CREATE TRIGGER guard_generation_reclaim BEFORE UPDATE OF publication_state ON generations
WHEN NEW.publication_state IN ('reclaiming','reclaimed') AND (
    EXISTS (SELECT 1 FROM installs WHERE active_generation_id = OLD.generation_id) OR
    EXISTS (SELECT 1 FROM generation_edges WHERE provider_id = OLD.generation_id) OR
    EXISTS (SELECT 1 FROM generation_leases WHERE generation_id = OLD.generation_id AND state <> 'released'))
BEGIN SELECT RAISE(ABORT, 'generation still referenced'); END;
CREATE TRIGGER guard_artifact_reclaim BEFORE UPDATE OF store_state ON artifacts
WHEN NEW.store_state = 'reclaimed' AND EXISTS (
    SELECT 1 FROM generations WHERE artifact_id = OLD.artifact_id AND publication_state <> 'reclaimed')
BEGIN SELECT RAISE(ABORT, 'artifact still referenced'); END;

CREATE UNIQUE INDEX lease_dispatch_binding ON generation_leases
    (lease_id,generation_id,holder_authority,holder_ref);

CREATE TABLE launch_dispatches (
    launch_id BLOB PRIMARY KEY NOT NULL CHECK (length(launch_id) = 16),
    lease_id BLOB NOT NULL UNIQUE,
    root_operation_id BLOB NOT NULL UNIQUE REFERENCES operations(operation_id),
    broker_epoch BLOB NOT NULL CHECK (length(broker_epoch) = 16),
    broker_incarnation BLOB NOT NULL CHECK (length(broker_incarnation) = 16),
    account_scope_ref BLOB NOT NULL REFERENCES scopes(scope_id),
    install_ref BLOB NOT NULL,
    generation_ref BLOB NOT NULL,
    data_epoch_ref BLOB NOT NULL CHECK (length(data_epoch_ref) = 16),
    boot_id BLOB NOT NULL CHECK (length(boot_id) = 16),
    action_id TEXT NOT NULL CHECK (length(action_id) BETWEEN 1 AND 64 AND
        action_id NOT GLOB '*[^a-z0-9._-]*' AND instr(action_id,char(0)) = 0),
    session_ref TEXT NOT NULL CHECK (length(CAST(session_ref AS BLOB)) BETWEEN 1 AND 128 AND instr(session_ref,char(0)) = 0),
    seat_ref TEXT NOT NULL CHECK (length(CAST(seat_ref AS BLOB)) BETWEEN 1 AND 128 AND instr(seat_ref,char(0)) = 0),
    install_gate_revision INTEGER NOT NULL CHECK (install_gate_revision >= 1),
    ready_data_gate_revision INTEGER CHECK (ready_data_gate_revision >= 1),
    unit_name TEXT NOT NULL CHECK (unit_name = 'aetheri-app-' || lower(hex(launch_id)) || '.service'),
    invocation_ref BLOB CHECK (invocation_ref IS NULL OR length(invocation_ref) = 16),
    state TEXT NOT NULL CHECK (state IN ('reserved','dispatching','observed','closing','terminal','uncertain')),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    close_cause_epoch BLOB CHECK (close_cause_epoch IS NULL OR length(close_cause_epoch) = 16),
    close_cause_operation_ref BLOB CHECK (close_cause_operation_ref IS NULL OR length(close_cause_operation_ref) = 16),
    FOREIGN KEY (generation_ref,install_ref) REFERENCES generations(generation_id,install_id),
    FOREIGN KEY (lease_id,generation_ref,broker_epoch,launch_id)
        REFERENCES generation_leases(lease_id,generation_id,holder_authority,holder_ref),
    CHECK ((close_cause_epoch IS NULL) = (close_cause_operation_ref IS NULL)),
    CHECK (state NOT IN ('dispatching','observed') OR ready_data_gate_revision IS NOT NULL),
    CHECK (state <> 'observed' OR invocation_ref IS NOT NULL)
) STRICT;
CREATE INDEX dispatches_by_install ON launch_dispatches(install_ref,state);

CREATE TRIGGER system_gate_target BEFORE INSERT ON admission_gates
WHEN NEW.subject_kind <> 'install' OR NOT EXISTS (
    SELECT 1 FROM installs WHERE install_id = NEW.subject_ref)
BEGIN SELECT RAISE(ABORT, 'invalid system gate'); END;
CREATE TRIGGER reopen_system_gate BEFORE UPDATE ON admission_gates
WHEN NEW.state = 'open' AND NOT EXISTS (
    SELECT 1 FROM installs WHERE install_id = NEW.subject_ref AND state IN ('preparing','active'))
BEGIN SELECT RAISE(ABORT, 'install cannot reopen'); END;
CREATE TRIGGER create_install_gate AFTER INSERT ON installs
BEGIN
    INSERT INTO admission_gates(subject_kind,subject_ref,state) VALUES ('install',NEW.install_id,'open');
END;
CREATE TRIGGER protect_install_gate BEFORE DELETE ON installs
WHEN EXISTS (SELECT 1 FROM admission_gates WHERE subject_kind = 'install' AND subject_ref = OLD.install_id)
BEGIN SELECT RAISE(ABORT, 'remove gate before pruning install'); END;
CREATE TRIGGER system_no_revoke BEFORE INSERT ON plans WHEN NEW.kind = 'revoke'
BEGIN SELECT RAISE(ABORT, 'private revocation belongs to user authority'); END;

CREATE TRIGGER admit_launch_lease BEFORE INSERT ON generation_leases
WHEN NEW.kind = 'launch' AND NOT EXISTS (
    SELECT 1 FROM generations g JOIN installs i ON i.install_id = g.install_id
    JOIN artifacts a ON a.artifact_id = g.artifact_id
    JOIN admission_gates f ON f.subject_kind = 'install' AND f.subject_ref = i.install_id
    JOIN authority_meta m ON m.singleton = 1
    WHERE g.generation_id = NEW.generation_id AND i.state = 'active'
      AND i.active_generation_id = g.generation_id AND a.store_state = 'verified'
      AND f.state = 'open' AND m.runtime_incarnation IS NOT NULL AND NEW.boot_id = m.current_boot_id)
BEGIN SELECT RAISE(ABORT, 'launch admission closed'); END;
CREATE TRIGGER immutable_lease BEFORE UPDATE OF
    lease_id,generation_id,holder_authority,holder_ref,kind,boot_id ON generation_leases
BEGIN SELECT RAISE(ABORT, 'immutable lease'); END;
CREATE TRIGGER release_drained_lease BEFORE UPDATE OF state ON generation_leases
WHEN NEW.state = 'released' AND EXISTS (
    SELECT 1 FROM launch_dispatches WHERE lease_id = OLD.lease_id AND state <> 'terminal')
BEGIN SELECT RAISE(ABORT, 'dispatch not drained'); END;
CREATE TRIGGER released_lease_terminal BEFORE UPDATE OF state ON generation_leases
WHEN OLD.state = 'released' AND NEW.state <> 'released'
BEGIN SELECT RAISE(ABORT, 'released lease cannot revive'); END;

CREATE TRIGGER admit_dispatch BEFORE INSERT ON launch_dispatches
WHEN NEW.state <> 'reserved' OR NOT EXISTS (
    SELECT 1 FROM generation_leases l
    JOIN installs i ON i.install_id = NEW.install_ref
    JOIN admission_gates f ON f.subject_kind = 'install' AND f.subject_ref = i.install_id
    JOIN scopes s ON s.scope_id = NEW.account_scope_ref
    JOIN operations o ON o.operation_id = NEW.root_operation_id
    JOIN plans p ON p.plan_id = o.plan_id
    JOIN authority_meta m ON m.singleton = 1
    WHERE l.lease_id = NEW.lease_id AND l.kind = 'launch' AND l.state = 'reserved'
      AND i.state = 'active' AND i.active_generation_id = NEW.generation_ref
      AND f.state = 'open' AND f.revision = NEW.install_gate_revision
      AND s.kind = 'account' AND s.state = 'active' AND m.runtime_incarnation IS NOT NULL
      AND NEW.boot_id = m.current_boot_id AND l.boot_id = NEW.boot_id
      AND o.state <> 'completed' AND p.kind = 'launch'
      AND p.install_ref = NEW.install_ref AND p.generation_ref = NEW.generation_ref
      AND p.data_epoch_ref = NEW.data_epoch_ref AND p.component_id = i.component_id
      AND p.scope_ref = NEW.account_scope_ref AND p.action_id = NEW.action_id)
BEGIN SELECT RAISE(ABORT, 'dispatch not admitted'); END;
CREATE TRIGGER immutable_dispatch BEFORE UPDATE OF launch_id,lease_id,root_operation_id,
    broker_epoch,broker_incarnation,account_scope_ref,install_ref,generation_ref,
    data_epoch_ref,boot_id,action_id,session_ref,seat_ref,install_gate_revision,unit_name ON launch_dispatches
BEGIN SELECT RAISE(ABORT, 'immutable dispatch tuple'); END;
CREATE TRIGGER frozen_dispatch_observation BEFORE UPDATE ON launch_dispatches
WHEN (OLD.invocation_ref IS NOT NULL AND NEW.invocation_ref IS NOT OLD.invocation_ref) OR
     (OLD.ready_data_gate_revision IS NOT NULL AND NEW.ready_data_gate_revision IS NOT OLD.ready_data_gate_revision) OR
     (OLD.close_cause_epoch IS NOT NULL AND (NEW.close_cause_epoch IS NOT OLD.close_cause_epoch OR
       NEW.close_cause_operation_ref IS NOT OLD.close_cause_operation_ref))
BEGIN SELECT RAISE(ABORT, 'dispatch evidence already bound'); END;
CREATE TRIGGER dispatch_transition BEFORE UPDATE ON launch_dispatches
WHEN NEW.revision <= OLD.revision OR NOT (
    NEW.state = OLD.state OR
    (OLD.state = 'reserved' AND NEW.state IN ('dispatching','closing','terminal','uncertain')) OR
    (OLD.state = 'dispatching' AND NEW.state IN ('observed','closing','terminal','uncertain')) OR
    (OLD.state = 'observed' AND NEW.state IN ('closing','terminal','uncertain')) OR
    (OLD.state = 'closing' AND NEW.state IN ('terminal','uncertain')) OR
    (OLD.state = 'uncertain' AND NEW.state IN ('observed','closing','terminal')))
BEGIN SELECT RAISE(ABORT, 'invalid dispatch transition/revision'); END;
CREATE TRIGGER dispatch_current_gate BEFORE UPDATE ON launch_dispatches
WHEN NEW.state = 'dispatching' AND OLD.state <> 'dispatching' AND NOT EXISTS (
    SELECT 1 FROM admission_gates f JOIN installs i ON i.install_id = f.subject_ref
    JOIN authority_meta m ON m.singleton = 1
    WHERE f.subject_kind = 'install' AND f.subject_ref = NEW.install_ref
      AND f.state = 'open' AND f.revision = NEW.install_gate_revision
      AND i.state = 'active' AND i.active_generation_id = NEW.generation_ref
      AND m.runtime_incarnation IS NOT NULL AND m.current_boot_id = NEW.boot_id)
BEGIN SELECT RAISE(ABORT, 'dispatch fence changed'); END;
CREATE TRIGGER restart_dispatch_recovery AFTER UPDATE OF runtime_incarnation ON authority_meta
WHEN OLD.runtime_incarnation IS NOT NEW.runtime_incarnation
BEGIN
    UPDATE launch_dispatches SET state = 'uncertain',revision = revision + 1 WHERE state <> 'terminal';
END;

CREATE TRIGGER keep_live_dispatch BEFORE DELETE ON launch_dispatches WHEN OLD.state <> 'terminal'
BEGIN SELECT RAISE(ABORT, 'unresolved dispatch cannot be pruned'); END;
CREATE TRIGGER keep_held_lease BEFORE DELETE ON generation_leases WHEN OLD.state <> 'released'
BEGIN SELECT RAISE(ABORT, 'held lease cannot be pruned'); END;
