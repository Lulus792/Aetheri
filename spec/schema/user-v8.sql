-- user interruption admission is separate from successful same-boot closure.
CREATE TRIGGER admit_user_interruption_binding BEFORE INSERT ON run_interruption_bindings
WHEN NOT EXISTS (SELECT 1 FROM authority_meta m JOIN launches l ON l.launch_id=NEW.launch_id
    JOIN operations o ON o.operation_id=l.operation_id
    JOIN execution_processes p ON p.launch_id=l.launch_id AND p.role=2
    WHERE m.kind='user' AND m.runtime_incarnation=NEW.user_runtime AND m.current_boot_id=NEW.current_boot
      AND l.boot_id=NEW.original_boot AND p.profile=NEW.profile AND p.profile_digest=NEW.profile_digest
      AND o.state='completed' AND o.outcome='applied' AND o.result_kind='launch' AND o.result_ref=l.launch_id
      AND (l.state IN ('recovery_required','stopping') OR (l.state='exited' AND EXISTS (
        SELECT 1 FROM run_interruption_steps s WHERE s.launch_id=l.launch_id AND s.kind=4))))
BEGIN SELECT RAISE(ABORT,'interruption binding not admitted'); END;
CREATE TRIGGER admit_user_interruption_step BEFORE INSERT ON run_interruption_steps
WHEN NEW.kind NOT IN (2,4) OR NOT EXISTS (
    SELECT 1 FROM launches l JOIN operations o ON o.operation_id=l.operation_id
    JOIN run_interruption_bindings b ON b.launch_id=l.launch_id AND b.binding_digest=NEW.binding_digest
    JOIN authority_meta m ON m.kind='user' AND m.runtime_incarnation=b.user_runtime AND m.current_boot_id=b.current_boot
    WHERE l.launch_id=NEW.launch_id AND l.operation_id=NEW.start_operation_id
      AND l.invocation_ref=NEW.invocation_ref AND b.profile=NEW.profile AND b.profile_digest=NEW.profile_digest
      AND l.boot_id=b.original_boot AND l.state IN ('recovery_required','stopping')
      AND o.state='completed' AND o.outcome='applied' AND o.result_kind='launch' AND o.result_ref=l.launch_id
      AND (NEW.kind=2 OR EXISTS (SELECT 1 FROM run_interruption_steps s
        WHERE s.launch_id=l.launch_id AND s.kind=2 AND s.profile=NEW.profile)))
BEGIN SELECT RAISE(ABORT,'interruption stage not admitted'); END;
CREATE TRIGGER retain_user_interruption_binding BEFORE DELETE ON run_interruption_bindings
WHEN NOT EXISTS (SELECT 1 FROM launches l WHERE l.launch_id=OLD.launch_id AND l.state='exited')
BEGIN SELECT RAISE(ABORT,'interrupted run not settled'); END;
CREATE TRIGGER retain_user_interruption_step BEFORE DELETE ON run_interruption_steps
WHEN NOT EXISTS (SELECT 1 FROM launches l WHERE l.launch_id=OLD.launch_id AND l.state='exited')
BEGIN SELECT RAISE(ABORT,'interrupted run not settled'); END;
CREATE TRIGGER retain_user_interruption_launch BEFORE DELETE ON launches
WHEN EXISTS (SELECT 1 FROM run_interruption_bindings WHERE launch_id=OLD.launch_id)
BEGIN SELECT RAISE(ABORT,'prune interruption history first'); END;
