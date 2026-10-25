-- Discussion draft; requires common-v1.sql. One trusted writer per account.
PRAGMA application_id = 1096042581; -- ATHU
PRAGMA user_version = 1;

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
