-- Additive execution-lifetime journal. Apply after the complete frozen v2 schema.
CREATE TABLE run_closures (
    launch_id BLOB PRIMARY KEY NOT NULL CHECK (length(launch_id) = 16),
    start_operation_id BLOB NOT NULL UNIQUE REFERENCES operations(operation_id),
    profile BLOB NOT NULL CHECK (length(profile) = 384),
    profile_digest BLOB NOT NULL CHECK (length(profile_digest) = 32),
    invocation_ref BLOB NOT NULL CHECK (length(invocation_ref) = 16),
    context_digest BLOB NOT NULL CHECK (length(context_digest) = 32),
    start_result_digest BLOB NOT NULL CHECK (length(start_result_digest) = 32),
    state TEXT NOT NULL CHECK (state IN ('closing','released','uncertain')),
    begin_receipt BLOB NOT NULL CHECK (length(begin_receipt) = 136),
    quiet_receipt BLOB CHECK (quiet_receipt IS NULL OR length(quiet_receipt) = 136),
    end_digest BLOB CHECK (end_digest IS NULL OR length(end_digest) = 32),
    release_receipt BLOB CHECK (release_receipt IS NULL OR length(release_receipt) = 136),
    began_unix_ms INTEGER NOT NULL CHECK (began_unix_ms >= 0),
    ended_unix_ms INTEGER CHECK (ended_unix_ms IS NULL OR ended_unix_ms >= began_unix_ms),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    CHECK ((state = 'released') = (release_receipt IS NOT NULL)),
    CHECK ((state = 'released') = (end_digest IS NOT NULL)),
    CHECK ((state = 'released') = (ended_unix_ms IS NOT NULL)),
    CHECK (state <> 'released' OR quiet_receipt IS NOT NULL)
) STRICT;
CREATE TRIGGER immutable_closure BEFORE UPDATE OF launch_id,start_operation_id,profile,
    profile_digest,invocation_ref,context_digest,start_result_digest,begin_receipt,began_unix_ms ON run_closures
BEGIN SELECT RAISE(ABORT, 'immutable run closure'); END;
CREATE TRIGGER closure_transition BEFORE UPDATE ON run_closures
WHEN OLD.state = 'released' OR NEW.revision <= OLD.revision OR
    (OLD.state = 'uncertain' AND NEW.state = 'closing') OR
    (OLD.quiet_receipt IS NOT NULL AND NEW.quiet_receipt IS NOT OLD.quiet_receipt)
BEGIN SELECT RAISE(ABORT, 'invalid run closure transition'); END;
CREATE TRIGGER keep_unsettled_closure BEFORE DELETE ON run_closures
WHEN OLD.state <> 'released'
BEGIN SELECT RAISE(ABORT, 'run closure not settled'); END;
CREATE TRIGGER restart_closure_recovery AFTER UPDATE OF runtime_incarnation ON authority_meta
WHEN OLD.runtime_incarnation IS NOT NEW.runtime_incarnation
BEGIN
    UPDATE run_closures SET state = 'uncertain',revision = revision + 1 WHERE state = 'closing';
END;
