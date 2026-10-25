-- Discussion draft; requires common-v1.sql. No filesystem publication is done here.
PRAGMA application_id = 1096042579; -- ATHS
PRAGMA user_version = 1;

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
