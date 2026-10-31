-- Root-only original observation-handoff journal, after complete frozen v4.
CREATE TABLE execution_handoffs (
    launch_id BLOB NOT NULL CHECK (length(launch_id) = 16),
    role INTEGER NOT NULL CHECK (role = 2),
    profile BLOB NOT NULL CHECK (length(profile) = 384),
    profile_digest BLOB NOT NULL CHECK (length(profile_digest) = 32),
    process_digest BLOB NOT NULL CHECK (length(process_digest) = 32),
    witness BLOB NOT NULL CHECK (length(witness) BETWEEN 374 AND 2560),
    witness_digest BLOB NOT NULL CHECK (length(witness_digest) = 32),
    state TEXT NOT NULL CHECK (state IN ('pending','observed','retiring','retired')),
    original_runtime BLOB NOT NULL CHECK (length(original_runtime) = 16),
    boot_id BLOB NOT NULL CHECK (length(boot_id) = 16),
    recorded_unix_ms INTEGER NOT NULL CHECK (recorded_unix_ms >= 0),
    observer_runtime BLOB CHECK (observer_runtime IS NULL OR length(observer_runtime) = 16),
    observed_unix_ms INTEGER CHECK (observed_unix_ms IS NULL OR observed_unix_ms >= 0),
    retirement_runtime BLOB CHECK (retirement_runtime IS NULL OR length(retirement_runtime) = 16),
    retiring_unix_ms INTEGER CHECK (retiring_unix_ms IS NULL OR retiring_unix_ms >= 0),
    retired_runtime BLOB CHECK (retired_runtime IS NULL OR length(retired_runtime) = 16),
    retired_unix_ms INTEGER CHECK (retired_unix_ms IS NULL OR retired_unix_ms >= 0),
    revision INTEGER NOT NULL DEFAULT 1 CHECK (revision >= 1),
    PRIMARY KEY (launch_id),
    FOREIGN KEY (launch_id,role) REFERENCES execution_processes(launch_id,role),
    CHECK ((observer_runtime IS NULL) = (observed_unix_ms IS NULL)),
    CHECK ((state = 'pending' AND revision = 1) OR (state IN ('observed','retiring') AND revision >= 2) OR (state = 'retired' AND revision >= 3)),
    CHECK (observer_runtime IS NULL OR observer_runtime <> original_runtime),
    CHECK ((state = 'pending' AND observer_runtime IS NULL AND retirement_runtime IS NULL AND retiring_unix_ms IS NULL AND retired_runtime IS NULL AND retired_unix_ms IS NULL)
        OR (state = 'observed' AND observer_runtime IS NOT NULL AND retirement_runtime IS NULL AND retiring_unix_ms IS NULL AND retired_runtime IS NULL AND retired_unix_ms IS NULL)
        OR (state = 'retiring' AND retirement_runtime IS NOT NULL AND retiring_unix_ms IS NOT NULL AND retired_runtime IS NULL AND retired_unix_ms IS NULL)
        OR (state = 'retired' AND retirement_runtime IS NOT NULL AND retiring_unix_ms IS NOT NULL AND retired_runtime IS NOT NULL AND retired_unix_ms IS NOT NULL))
) STRICT;
CREATE TRIGGER admit_execution_handoff BEFORE INSERT ON execution_handoffs
WHEN NEW.state <> 'pending' OR NEW.revision <> 1 OR NOT EXISTS (
    SELECT 1 FROM execution_processes p JOIN launch_dispatches d ON d.launch_id = p.launch_id
    JOIN generation_leases l ON l.lease_id = d.lease_id JOIN authority_meta m ON m.singleton = 1
    WHERE p.launch_id = NEW.launch_id AND p.role = 2 AND p.profile = NEW.profile
      AND p.profile_digest = NEW.profile_digest AND p.witness_digest = NEW.process_digest
      AND d.state IN ('dispatching','observed') AND l.state = 'running'
      AND m.kind = 'system' AND m.runtime_incarnation = NEW.original_runtime AND m.current_boot_id = NEW.boot_id
      AND NOT EXISTS (SELECT 1 FROM run_closures c WHERE c.launch_id = NEW.launch_id))
BEGIN SELECT RAISE(ABORT, 'handoff not admitted'); END;
CREATE TRIGGER immutable_execution_handoff BEFORE UPDATE ON execution_handoffs
WHEN NEW.launch_id IS NOT OLD.launch_id OR NEW.role IS NOT OLD.role OR NEW.profile IS NOT OLD.profile
  OR NEW.profile_digest IS NOT OLD.profile_digest OR NEW.process_digest IS NOT OLD.process_digest
  OR NEW.witness IS NOT OLD.witness OR NEW.witness_digest IS NOT OLD.witness_digest
  OR NEW.original_runtime IS NOT OLD.original_runtime OR NEW.boot_id IS NOT OLD.boot_id
  OR NEW.recorded_unix_ms IS NOT OLD.recorded_unix_ms OR OLD.revision = 9223372036854775807 OR NEW.revision <> OLD.revision + 1
BEGIN SELECT RAISE(ABORT, 'immutable handoff origin'); END;
CREATE TRIGGER advance_execution_handoff BEFORE UPDATE ON execution_handoffs
WHEN NOT (
    (OLD.state IN ('pending','observed') AND NEW.state = 'observed'
      AND NEW.observer_runtime IS NOT OLD.observer_runtime AND NEW.observer_runtime <> OLD.original_runtime
      AND NEW.observer_runtime = (SELECT runtime_incarnation FROM authority_meta WHERE singleton = 1)
      AND NEW.retirement_runtime IS NULL AND NEW.retiring_unix_ms IS NULL AND NEW.retired_runtime IS NULL AND NEW.retired_unix_ms IS NULL)
 OR (OLD.state IN ('pending','observed') AND NEW.state = 'retiring'
      AND NEW.observer_runtime IS OLD.observer_runtime AND NEW.observed_unix_ms IS OLD.observed_unix_ms
      AND NEW.retirement_runtime = (SELECT runtime_incarnation FROM authority_meta WHERE singleton = 1)
      AND NEW.retired_runtime IS NULL AND NEW.retired_unix_ms IS NULL)
 OR (OLD.state = 'retiring' AND NEW.state = 'retired'
      AND NEW.observer_runtime IS OLD.observer_runtime AND NEW.observed_unix_ms IS OLD.observed_unix_ms
      AND NEW.retirement_runtime IS OLD.retirement_runtime AND NEW.retiring_unix_ms IS OLD.retiring_unix_ms
      AND NEW.retired_runtime = (SELECT runtime_incarnation FROM authority_meta WHERE singleton = 1)))
 OR NEW.boot_id <> (SELECT current_boot_id FROM authority_meta WHERE singleton = 1)
 OR (NEW.state IN ('retiring','retired') AND NOT EXISTS (
    SELECT 1 FROM launch_dispatches d JOIN generation_leases l ON l.lease_id = d.lease_id
    JOIN run_closures c ON c.launch_id = d.launch_id
    WHERE d.launch_id = NEW.launch_id AND d.state = 'terminal' AND l.state = 'released' AND c.state = 'released' AND c.profile = NEW.profile))
BEGIN SELECT RAISE(ABORT, 'handoff transition not admitted'); END;
CREATE TRIGGER retain_execution_handoff BEFORE DELETE ON execution_handoffs
WHEN OLD.state <> 'retired'
BEGIN SELECT RAISE(ABORT, 'handoff not retired'); END;
