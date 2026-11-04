-- Explicit paired-owner recovery. No old row/backfill or automatic migration.
CREATE TABLE run_recovery_pairs (
    launch_id BLOB NOT NULL CHECK (length(launch_id) = 16),
    root_runtime BLOB NOT NULL CHECK (length(root_runtime) = 16),
    user_runtime BLOB NOT NULL CHECK (length(user_runtime) = 16),
    profile BLOB NOT NULL CHECK (length(profile) = 384),
    profile_digest BLOB NOT NULL CHECK (length(profile_digest) = 32),
    binding BLOB NOT NULL CHECK (length(binding) = 232),
    binding_digest BLOB NOT NULL CHECK (length(binding_digest) = 32),
    recorded_unix_ms INTEGER NOT NULL CHECK (recorded_unix_ms >= 0),
    PRIMARY KEY (launch_id,root_runtime,user_runtime)
) STRICT;
CREATE TRIGGER immutable_recovery_pair BEFORE UPDATE ON run_recovery_pairs
BEGIN SELECT RAISE(ABORT, 'immutable recovery pair'); END;
