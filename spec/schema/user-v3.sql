-- User-owner v3 additions; v2 tables and triggers remain unchanged.
CREATE TRIGGER admit_user_closure BEFORE INSERT ON run_closures
WHEN NEW.state <> 'closing' OR NEW.revision <> 1 OR NEW.quiet_receipt IS NULL OR NOT EXISTS (
    SELECT 1 FROM launches l JOIN operations o ON o.operation_id = l.operation_id
    WHERE l.launch_id = NEW.launch_id AND l.operation_id = NEW.start_operation_id
      AND l.invocation_ref = NEW.invocation_ref AND l.state = 'running' AND l.readiness = 'ready'
      AND o.state = 'completed' AND o.outcome = 'applied' AND o.result_kind = 'launch'
      AND o.result_ref = l.launch_id)
BEGIN SELECT RAISE(ABORT, 'user closure not admitted'); END;
CREATE TRIGGER keep_user_closure_launch BEFORE DELETE ON launches
WHEN EXISTS (SELECT 1 FROM run_closures WHERE launch_id = OLD.launch_id)
BEGIN SELECT RAISE(ABORT, 'prune run closure first'); END;
