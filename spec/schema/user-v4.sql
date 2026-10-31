CREATE TRIGGER admit_user_execution_process BEFORE INSERT ON execution_processes
WHEN NEW.role <> 2 OR NOT EXISTS (
    SELECT 1 FROM launches l JOIN operations o ON o.operation_id = l.operation_id
    JOIN operation_steps p ON p.operation_id = o.operation_id AND p.step_no = 2
    WHERE l.launch_id = NEW.launch_id AND o.operation_id = NEW.start_operation_id
      AND l.invocation_ref = NEW.invocation_ref AND l.state = 'starting' AND l.readiness = 'unknown'
      AND o.state = 'running' AND p.action = 'verify' AND p.subject_kind = 'integration'
      AND p.subject_ref = NEW.invocation_ref AND p.evidence_digest = NEW.context_digest AND p.state = 'applied'
      AND NOT EXISTS (SELECT 1 FROM operation_steps WHERE operation_id = o.operation_id AND step_no >= 3))
BEGIN SELECT RAISE(ABORT, 'user process not admitted'); END;
CREATE TRIGGER keep_user_execution_process BEFORE DELETE ON launches
WHEN EXISTS (SELECT 1 FROM execution_processes WHERE launch_id = OLD.launch_id)
BEGIN SELECT RAISE(ABORT, 'prune process evidence first'); END;
CREATE TRIGGER retain_user_execution_process BEFORE DELETE ON execution_processes
WHEN NOT EXISTS (SELECT 1 FROM launches WHERE launch_id = OLD.launch_id AND state = 'exited' AND readiness = 'unavailable')
BEGIN SELECT RAISE(ABORT, 'execution process not settled'); END;
