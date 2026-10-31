-- Explicit Root recovery of a completed-start run; apply after frozen v5.
-- Current observation is separate from the immutable original profile/runtime.
CREATE TABLE run_recovery_owners (
    launch_id BLOB NOT NULL REFERENCES execution_handoffs(launch_id),
    runtime BLOB NOT NULL CHECK (length(runtime) = 16),
    profile_digest BLOB NOT NULL CHECK (length(profile_digest) = 32),
    witness_digest BLOB NOT NULL CHECK (length(witness_digest) = 32),
    handoff_revision INTEGER NOT NULL CHECK (handoff_revision >= 2),
    recorded_unix_ms INTEGER NOT NULL CHECK (recorded_unix_ms >= 0),
    PRIMARY KEY (launch_id,runtime)
) STRICT;
CREATE TRIGGER admit_run_recovery_owner BEFORE INSERT ON run_recovery_owners
WHEN NOT EXISTS (
    SELECT 1 FROM execution_handoffs h JOIN authority_meta m ON m.singleton = 1
    JOIN launch_dispatches d ON d.launch_id = h.launch_id
    JOIN generation_leases l ON l.lease_id = d.lease_id
    JOIN operations o ON o.operation_id = d.root_operation_id
    WHERE h.launch_id = NEW.launch_id AND h.profile_digest = NEW.profile_digest
      AND h.witness_digest = NEW.witness_digest AND h.revision = NEW.handoff_revision
      AND h.state = 'observed' AND h.observer_runtime = NEW.runtime
      AND m.kind = 'system' AND m.runtime_incarnation = NEW.runtime
      AND m.current_boot_id = h.boot_id AND NEW.runtime <> h.original_runtime
      AND d.state = 'uncertain' AND l.state = 'running' AND d.close_cause_epoch IS NULL
      AND o.state = 'completed' AND o.outcome = 'applied' AND o.result_kind = 'launch'
      AND o.result_ref = d.launch_id)
BEGIN SELECT RAISE(ABORT, 'recovery owner not admitted'); END;
CREATE TRIGGER immutable_run_recovery_owner BEFORE UPDATE ON run_recovery_owners
BEGIN SELECT RAISE(ABORT, 'immutable recovery observation'); END;
CREATE TRIGGER retain_run_recovery_owner BEFORE DELETE ON run_recovery_owners
WHEN NOT EXISTS (
    SELECT 1 FROM launch_dispatches d JOIN generation_leases l ON l.lease_id = d.lease_id
    WHERE d.launch_id = OLD.launch_id AND d.state = 'terminal' AND l.state = 'released')
BEGIN SELECT RAISE(ABORT, 'recovery run not settled'); END;
-- Preserve original admission, adding only the independently admitted recovered
-- owner. Completed-start joins and receipt integrity are also checked natively.
DROP TRIGGER admit_system_closure;
CREATE TRIGGER admit_system_closure BEFORE INSERT ON run_closures
WHEN NEW.state <> 'closing' OR NEW.revision <> 1 OR NEW.quiet_receipt IS NOT NULL OR NOT EXISTS (
    SELECT 1 FROM launch_dispatches d JOIN operations o ON o.operation_id = d.root_operation_id
    JOIN generation_leases l ON l.lease_id = d.lease_id
    WHERE d.launch_id = NEW.launch_id AND d.root_operation_id = NEW.start_operation_id
      AND d.invocation_ref = NEW.invocation_ref AND l.state = 'running'
      AND o.state = 'completed' AND o.outcome = 'applied' AND o.result_kind = 'launch'
      AND o.result_ref = d.launch_id AND (d.state = 'observed' OR (
        d.state = 'uncertain' AND EXISTS (
          SELECT 1 FROM run_recovery_owners r JOIN execution_handoffs h ON h.launch_id = r.launch_id
          JOIN authority_meta m ON m.singleton = 1
          WHERE r.launch_id = d.launch_id AND r.runtime = m.runtime_incarnation
            AND r.profile_digest = NEW.profile_digest AND h.profile = NEW.profile
            AND h.state = 'observed' AND h.observer_runtime = r.runtime
            AND h.witness_digest = r.witness_digest AND h.revision = r.handoff_revision
            AND h.boot_id = m.current_boot_id))))
BEGIN SELECT RAISE(ABORT, 'system closure not admitted'); END;
