-- Role 1 is the original starter; role 2 is its actual post-exec process.
CREATE TRIGGER admit_system_execution_process BEFORE INSERT ON execution_processes
WHEN NOT EXISTS (
    SELECT 1 FROM launch_dispatches d JOIN operations o ON o.operation_id = d.root_operation_id
    JOIN generation_leases l ON l.lease_id = d.lease_id
    JOIN operation_steps p ON p.operation_id = o.operation_id AND p.step_no = 2
    WHERE d.launch_id = NEW.launch_id AND o.operation_id = NEW.start_operation_id
      AND p.subject_ref = NEW.invocation_ref
      AND p.evidence_digest = NEW.context_digest AND o.state = 'running'
      AND p.action = 'verify' AND p.subject_kind = 'integration'
      AND ((NEW.role = 1 AND d.state = 'reserved' AND d.invocation_ref IS NULL AND l.state = 'reserved' AND p.state IN ('executing','applied')
            AND NOT EXISTS (SELECT 1 FROM operation_steps WHERE operation_id = o.operation_id AND step_no >= 3))
        OR (NEW.role = 2 AND d.state = 'dispatching' AND d.invocation_ref = NEW.invocation_ref AND l.state = 'running' AND p.state = 'applied'
            AND EXISTS (SELECT 1 FROM execution_processes s WHERE s.launch_id = NEW.launch_id AND s.role = 1
              AND s.profile = NEW.profile AND s.process_identity = NEW.process_identity
              AND s.invocation_ref = NEW.invocation_ref))))
BEGIN SELECT RAISE(ABORT, 'system process not admitted'); END;
CREATE TRIGGER keep_system_execution_process BEFORE DELETE ON launch_dispatches
WHEN EXISTS (SELECT 1 FROM execution_processes WHERE launch_id = OLD.launch_id)
BEGIN SELECT RAISE(ABORT, 'prune process evidence first'); END;
CREATE TRIGGER retain_system_execution_process BEFORE DELETE ON execution_processes
WHEN NOT EXISTS (SELECT 1 FROM launch_dispatches d JOIN generation_leases l ON l.lease_id = d.lease_id
    WHERE d.launch_id = OLD.launch_id AND d.state = 'terminal' AND l.state = 'released')
BEGIN SELECT RAISE(ABORT, 'execution process not settled'); END;
