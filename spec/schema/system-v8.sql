-- system interruption admission is separate from successful same-boot closure.
CREATE TRIGGER admit_system_interruption_binding BEFORE INSERT ON run_interruption_bindings
WHEN NOT EXISTS (SELECT 1 FROM authority_meta m JOIN launch_dispatches d ON d.launch_id=NEW.launch_id
    JOIN generation_leases l ON l.lease_id=d.lease_id JOIN operations o ON o.operation_id=d.root_operation_id
    JOIN execution_processes p ON p.launch_id=d.launch_id AND p.role=2
    WHERE m.kind='system' AND m.runtime_incarnation=NEW.root_runtime AND m.current_boot_id=NEW.current_boot
      AND d.boot_id=NEW.original_boot AND p.profile=NEW.profile AND p.profile_digest=NEW.profile_digest
      AND d.close_cause_epoch IS NULL AND o.state='completed' AND o.outcome='applied'
      AND o.result_kind='launch' AND o.result_ref=d.launch_id
      AND ((d.state IN ('observed','closing','uncertain') AND l.state='running') OR
           (d.state='terminal' AND l.state='released' AND EXISTS (
             SELECT 1 FROM run_interruption_steps s WHERE s.launch_id=d.launch_id AND s.kind=3))))
BEGIN SELECT RAISE(ABORT,'interruption binding not admitted'); END;
CREATE TRIGGER admit_system_interruption_step BEFORE INSERT ON run_interruption_steps
WHEN NEW.kind NOT IN (1,3) OR NOT EXISTS (
    SELECT 1 FROM launch_dispatches d JOIN generation_leases l ON l.lease_id=d.lease_id
    JOIN operations o ON o.operation_id=d.root_operation_id
    JOIN run_interruption_bindings b ON b.launch_id=d.launch_id AND b.binding_digest=NEW.binding_digest
    JOIN authority_meta m ON m.kind='system' AND m.runtime_incarnation=b.root_runtime AND m.current_boot_id=b.current_boot
    WHERE d.launch_id=NEW.launch_id AND d.root_operation_id=NEW.start_operation_id
      AND d.invocation_ref=NEW.invocation_ref AND b.profile=NEW.profile AND b.profile_digest=NEW.profile_digest
      AND d.boot_id=b.original_boot AND d.state IN ('observed','closing','uncertain') AND l.state='running'
      AND o.state='completed' AND o.outcome='applied' AND o.result_kind='launch' AND o.result_ref=d.launch_id
      AND (NEW.kind=1 OR EXISTS (SELECT 1 FROM run_interruption_steps s
        WHERE s.launch_id=d.launch_id AND s.kind=1 AND s.profile=NEW.profile)))
BEGIN SELECT RAISE(ABORT,'interruption stage not admitted'); END;
CREATE TRIGGER retain_system_interruption_binding BEFORE DELETE ON run_interruption_bindings
WHEN NOT EXISTS (SELECT 1 FROM launch_dispatches d JOIN generation_leases l ON l.lease_id=d.lease_id WHERE d.launch_id=OLD.launch_id AND d.state='terminal' AND l.state='released')
BEGIN SELECT RAISE(ABORT,'interrupted run not settled'); END;
CREATE TRIGGER retain_system_interruption_step BEFORE DELETE ON run_interruption_steps
WHEN NOT EXISTS (SELECT 1 FROM launch_dispatches d JOIN generation_leases l ON l.lease_id=d.lease_id WHERE d.launch_id=OLD.launch_id AND d.state='terminal' AND l.state='released')
BEGIN SELECT RAISE(ABORT,'interrupted run not settled'); END;
CREATE TRIGGER retain_system_interruption_launch BEFORE DELETE ON launch_dispatches
WHEN EXISTS (SELECT 1 FROM run_interruption_bindings WHERE launch_id=OLD.launch_id)
BEGIN SELECT RAISE(ABORT,'prune interruption history first'); END;
