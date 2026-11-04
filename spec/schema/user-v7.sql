CREATE TRIGGER admit_user_recovery_pair BEFORE INSERT ON run_recovery_pairs
WHEN NOT EXISTS (
    SELECT 1 FROM authority_meta m JOIN launches l ON l.launch_id = NEW.launch_id
    JOIN execution_processes p ON p.launch_id = l.launch_id AND p.role = 2
    JOIN operations o ON o.operation_id = l.operation_id
    WHERE m.singleton = 1 AND m.kind = 'user' AND m.runtime_incarnation = NEW.user_runtime
      AND p.profile = NEW.profile AND p.profile_digest = NEW.profile_digest
      AND l.boot_id = m.current_boot_id AND l.state IN ('recovery_required','stopping')
      AND o.state = 'completed' AND o.outcome = 'applied' AND o.result_kind = 'launch' AND o.result_ref = l.launch_id
      AND EXISTS (SELECT 1 FROM admission_gates g WHERE g.subject_kind = 'data' AND g.subject_ref = l.data_epoch_id AND g.state = 'closed')
      AND NOT EXISTS (SELECT 1 FROM grants g WHERE g.launch_id = l.launch_id)
      AND NOT EXISTS (SELECT 1 FROM access_leases a WHERE a.launch_id = l.launch_id))
BEGIN SELECT RAISE(ABORT, 'user recovery pair not admitted'); END;
CREATE TRIGGER retain_user_recovery_pair BEFORE DELETE ON run_recovery_pairs
WHEN NOT EXISTS (SELECT 1 FROM launches WHERE launch_id = OLD.launch_id AND state = 'exited')
BEGIN SELECT RAISE(ABORT, 'paired user run not settled'); END;
CREATE TRIGGER retain_user_pair_launch BEFORE DELETE ON launches
WHEN EXISTS (SELECT 1 FROM run_recovery_pairs WHERE launch_id = OLD.launch_id)
BEGIN SELECT RAISE(ABORT, 'prune recovery pairs first'); END;
DROP TRIGGER admit_user_closure;
CREATE TRIGGER admit_user_closure BEFORE INSERT ON run_closures
WHEN NEW.state <> 'closing' OR NEW.revision <> 1 OR NEW.quiet_receipt IS NULL OR NOT EXISTS (
    SELECT 1 FROM launches l JOIN operations o ON o.operation_id = l.operation_id
    WHERE l.launch_id = NEW.launch_id AND l.operation_id = NEW.start_operation_id
      AND l.invocation_ref = NEW.invocation_ref AND o.state = 'completed' AND o.outcome = 'applied'
      AND o.result_kind = 'launch' AND o.result_ref = l.launch_id AND (
        (l.state = 'running' AND l.readiness = 'ready') OR
        (l.state = 'recovery_required' AND EXISTS (
          SELECT 1 FROM run_recovery_pairs r JOIN authority_meta m ON m.singleton = 1
          WHERE r.launch_id = l.launch_id AND r.user_runtime = m.runtime_incarnation
            AND r.profile = NEW.profile AND r.profile_digest = NEW.profile_digest))))
BEGIN SELECT RAISE(ABORT, 'user closure not admitted'); END;
