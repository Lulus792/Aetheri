-- System-owner v3 additions; v2 tables and triggers remain unchanged.
CREATE TRIGGER admit_system_closure BEFORE INSERT ON run_closures
WHEN NEW.state <> 'closing' OR NEW.revision <> 1 OR NEW.quiet_receipt IS NOT NULL OR NOT EXISTS (
    SELECT 1 FROM launch_dispatches d JOIN operations o ON o.operation_id = d.root_operation_id
    JOIN generation_leases l ON l.lease_id = d.lease_id
    WHERE d.launch_id = NEW.launch_id AND d.root_operation_id = NEW.start_operation_id
      AND d.invocation_ref = NEW.invocation_ref AND d.state = 'observed' AND l.state = 'running'
      AND o.state = 'completed' AND o.outcome = 'applied' AND o.result_kind = 'launch'
      AND o.result_ref = d.launch_id)
BEGIN SELECT RAISE(ABORT, 'system closure not admitted'); END;
CREATE TRIGGER keep_system_closure_launch BEFORE DELETE ON launch_dispatches
WHEN EXISTS (SELECT 1 FROM run_closures WHERE launch_id = OLD.launch_id)
BEGIN SELECT RAISE(ABORT, 'prune run closure first'); END;
