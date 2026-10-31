-- Additive immutable original-process evidence; apply after complete frozen v3.
CREATE TABLE execution_processes (
    launch_id BLOB NOT NULL CHECK (length(launch_id) = 16),
    role INTEGER NOT NULL CHECK (role IN (1,2)),
    start_operation_id BLOB NOT NULL REFERENCES operations(operation_id),
    profile BLOB NOT NULL CHECK (length(profile) = 384),
    profile_digest BLOB NOT NULL CHECK (length(profile_digest) = 32),
    invocation_ref BLOB NOT NULL CHECK (length(invocation_ref) = 16),
    context_digest BLOB NOT NULL CHECK (length(context_digest) = 32),
    process_identity BLOB NOT NULL CHECK (length(process_identity) = 32),
    witness BLOB NOT NULL CHECK (length(witness) BETWEEN 149 AND 2048),
    witness_digest BLOB NOT NULL CHECK (length(witness_digest) = 32),
    recorded_unix_ms INTEGER NOT NULL CHECK (recorded_unix_ms >= 0),
    PRIMARY KEY (launch_id,role)
) STRICT;
CREATE TRIGGER immutable_execution_process BEFORE UPDATE ON execution_processes
BEGIN SELECT RAISE(ABORT, 'immutable execution process'); END;
