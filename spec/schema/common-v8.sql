-- Explicit cross-boot maintenance; immutable admission and stage history only.
CREATE TABLE run_interruption_bindings (
    launch_id BLOB NOT NULL CHECK (length(launch_id)=16),
    root_runtime BLOB NOT NULL CHECK (length(root_runtime)=16),
    user_runtime BLOB NOT NULL CHECK (length(user_runtime)=16),
    original_boot BLOB NOT NULL CHECK (length(original_boot)=16),
    current_boot BLOB NOT NULL CHECK (length(current_boot)=16 AND current_boot<>original_boot),
    profile BLOB NOT NULL CHECK (length(profile)=384),
    profile_digest BLOB NOT NULL CHECK (length(profile_digest)=32),
    binding BLOB NOT NULL CHECK (length(binding)=248),
    binding_digest BLOB NOT NULL CHECK (length(binding_digest)=32),
    recorded_unix_ms INTEGER NOT NULL CHECK (recorded_unix_ms>=0),
    PRIMARY KEY (launch_id,root_runtime,user_runtime),
    UNIQUE (launch_id,binding_digest)
) STRICT;
CREATE TABLE run_interruption_steps (
    launch_id BLOB NOT NULL CHECK (length(launch_id)=16),
    kind INTEGER NOT NULL CHECK (kind BETWEEN 1 AND 4),
    start_operation_id BLOB NOT NULL REFERENCES operations(operation_id),
    profile BLOB NOT NULL CHECK (length(profile)=384),
    profile_digest BLOB NOT NULL CHECK (length(profile_digest)=32),
    invocation_ref BLOB NOT NULL CHECK (length(invocation_ref)=16),
    context_digest BLOB NOT NULL CHECK (length(context_digest)=32),
    start_result_digest BLOB NOT NULL CHECK (length(start_result_digest)=32),
    binding_digest BLOB NOT NULL CHECK (length(binding_digest)=32),
    prior_receipt BLOB CHECK (prior_receipt IS NULL OR length(prior_receipt)=352),
    receipt BLOB NOT NULL CHECK (length(receipt)=352),
    revision INTEGER NOT NULL CHECK (revision>=1),
    recorded_unix_ms INTEGER NOT NULL CHECK (recorded_unix_ms>=0),
    PRIMARY KEY (launch_id,kind),
    FOREIGN KEY (launch_id,binding_digest) REFERENCES run_interruption_bindings(launch_id,binding_digest),
    CHECK ((kind=1)=(prior_receipt IS NULL))
) STRICT;
CREATE TRIGGER immutable_interruption_binding BEFORE UPDATE ON run_interruption_bindings
BEGIN SELECT RAISE(ABORT,'immutable interruption binding'); END;
CREATE TRIGGER immutable_interruption_step BEFORE UPDATE ON run_interruption_steps
BEGIN SELECT RAISE(ABORT,'immutable interruption step'); END;
