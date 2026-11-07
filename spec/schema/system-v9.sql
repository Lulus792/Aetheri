-- Root-owned unit allocation intent, separate from execution REQUEST and data.
CREATE TABLE unit_allocations (
    allocation_id BLOB PRIMARY KEY CHECK (typeof(allocation_id)='blob' AND length(allocation_id)=16 AND substr(hex(allocation_id),13,1)='4' AND substr(hex(allocation_id),17,1) IN ('8','9','A','B')),
    operation_id BLOB NOT NULL UNIQUE REFERENCES operations(operation_id),
    lease_id BLOB NOT NULL UNIQUE REFERENCES launch_dispatches(lease_id),
    profile BLOB NOT NULL CHECK (typeof(profile)='blob' AND length(profile)=384 AND substr(profile,1,8)=X'4145544845584531'),
    profile_digest BLOB NOT NULL CHECK (typeof(profile_digest)='blob' AND length(profile_digest)=32),
    recipe_digest BLOB NOT NULL CHECK (typeof(recipe_digest)='blob' AND length(recipe_digest)=32),
    placement_digest BLOB NOT NULL CHECK (typeof(placement_digest)='blob' AND length(placement_digest)=32),
    selection_digest BLOB CHECK (selection_digest IS NULL OR (typeof(selection_digest)='blob' AND length(selection_digest)=32)),
    prepared_unix_ms INTEGER NOT NULL CHECK (prepared_unix_ms>=0),
    witnessed_unix_ms INTEGER CHECK (witnessed_unix_ms IS NULL OR witnessed_unix_ms>=prepared_unix_ms),
    CHECK ((selection_digest IS NULL)=(witnessed_unix_ms IS NULL))
) STRICT;
CREATE TRIGGER admit_unit_allocation BEFORE INSERT ON unit_allocations
WHEN NEW.selection_digest IS NOT NULL OR NOT EXISTS (
    SELECT 1 FROM authority_meta m JOIN launch_dispatches d ON d.lease_id=NEW.lease_id
    JOIN generation_leases l ON l.lease_id=d.lease_id JOIN operations o ON o.operation_id=d.root_operation_id
    JOIN installs i ON i.install_id=d.install_ref JOIN admission_gates g ON g.subject_kind='install' AND g.subject_ref=i.install_id
    WHERE m.kind='system' AND m.authority_epoch=substr(NEW.profile,9,16)
      AND m.runtime_incarnation=substr(NEW.profile,25,16) AND m.current_boot_id=substr(NEW.profile,73,16)
      AND NEW.operation_id=d.root_operation_id AND NEW.operation_id=substr(NEW.profile,201,16)
      AND NEW.lease_id=substr(NEW.profile,185,16) AND d.launch_id=substr(NEW.profile,169,16)
      AND d.state='reserved' AND d.invocation_ref IS NULL AND d.close_cause_epoch IS NULL
      AND l.state='reserved' AND o.state='running' AND i.state='active' AND i.active_generation_id=d.generation_ref
      AND g.state='open' AND g.closing_operation_id IS NULL AND g.revision=d.install_gate_revision
      AND NOT EXISTS (SELECT 1 FROM operation_steps s WHERE s.operation_id=o.operation_id AND s.step_no>0))
BEGIN SELECT RAISE(ABORT,'unit allocation not admitted'); END;
CREATE TRIGGER freeze_unit_allocation BEFORE UPDATE ON unit_allocations
WHEN NOT (OLD.selection_digest IS NULL AND NEW.selection_digest IS NOT NULL
    AND NEW.allocation_id IS OLD.allocation_id AND NEW.operation_id IS OLD.operation_id
    AND NEW.lease_id IS OLD.lease_id AND NEW.profile IS OLD.profile AND NEW.profile_digest IS OLD.profile_digest
    AND NEW.recipe_digest IS OLD.recipe_digest AND NEW.placement_digest IS OLD.placement_digest AND NEW.prepared_unix_ms IS OLD.prepared_unix_ms
    AND EXISTS (SELECT 1 FROM authority_meta m JOIN launch_dispatches d ON d.lease_id=OLD.lease_id
        JOIN generation_leases l ON l.lease_id=d.lease_id JOIN operations o ON o.operation_id=OLD.operation_id
        JOIN admission_gates g ON g.subject_kind='install' AND g.subject_ref=d.install_ref
        WHERE m.kind='system' AND m.authority_epoch=substr(OLD.profile,9,16)
          AND m.runtime_incarnation=substr(OLD.profile,25,16) AND m.current_boot_id=substr(OLD.profile,73,16)
          AND d.state='reserved' AND d.invocation_ref IS NULL AND d.close_cause_epoch IS NULL
          AND l.state='reserved' AND o.state='running' AND g.state='open' AND g.closing_operation_id IS NULL
          AND g.revision=d.install_gate_revision
          AND NOT EXISTS (SELECT 1 FROM operation_steps s WHERE s.operation_id=o.operation_id AND s.step_no>0)))
BEGIN SELECT RAISE(ABORT,'unit allocation immutable'); END;
CREATE TRIGGER retain_unit_allocation BEFORE DELETE ON unit_allocations
BEGIN SELECT RAISE(ABORT,'unit allocation retirement requires separate protocol'); END;
CREATE TRIGGER admit_unit_execution_stage BEFORE INSERT ON operation_steps
WHEN ((NEW.step_no=1 AND NEW.action='stage' AND NEW.subject_kind='generation') OR
      (NEW.step_no=3 AND NEW.action='activate' AND NEW.subject_kind='launch'))
  AND EXISTS (SELECT 1 FROM launch_dispatches d WHERE d.root_operation_id=NEW.operation_id AND d.action_id='run')
  AND NOT EXISTS (SELECT 1 FROM unit_allocations u JOIN authority_meta m
      ON m.kind='system' AND m.authority_epoch=substr(u.profile,9,16)
      AND m.runtime_incarnation=substr(u.profile,25,16) AND m.current_boot_id=substr(u.profile,73,16)
      WHERE u.operation_id=NEW.operation_id AND u.profile_digest=NEW.evidence_digest AND u.selection_digest IS NOT NULL)
BEGIN SELECT RAISE(ABORT,'execution unit selection missing'); END;
