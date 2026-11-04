CREATE TRIGGER admit_system_recovery_pair BEFORE INSERT ON run_recovery_pairs
WHEN NOT EXISTS (
    SELECT 1 FROM authority_meta m JOIN launch_dispatches d ON d.launch_id = NEW.launch_id
    JOIN generation_leases l ON l.lease_id = d.lease_id
    JOIN execution_handoffs h ON h.launch_id = d.launch_id
    JOIN operations o ON o.operation_id = d.root_operation_id
    WHERE m.singleton = 1 AND m.kind = 'system' AND m.runtime_incarnation = NEW.root_runtime
      AND h.profile = NEW.profile AND h.profile_digest = NEW.profile_digest
      AND h.boot_id = m.current_boot_id AND NEW.user_runtime <> d.broker_incarnation
      AND d.close_cause_epoch IS NULL
      AND ((d.state IN ('observed','closing','uncertain') AND l.state = 'running') OR
           (d.state = 'terminal' AND l.state = 'released' AND EXISTS (
             SELECT 1 FROM run_closures c WHERE c.launch_id = d.launch_id
               AND c.profile = NEW.profile AND c.state = 'released')))
      AND o.state = 'completed' AND o.outcome = 'applied'
      AND o.result_kind = 'launch' AND o.result_ref = d.launch_id
      AND ((h.original_runtime = NEW.root_runtime AND h.observer_runtime IS NULL
              AND (h.state = 'pending' OR d.state = 'terminal')) OR
           (h.observer_runtime = NEW.root_runtime AND (h.state = 'observed' OR d.state = 'terminal'))))
BEGIN SELECT RAISE(ABORT, 'system recovery pair not admitted'); END;
CREATE TRIGGER retain_system_recovery_pair BEFORE DELETE ON run_recovery_pairs
WHEN NOT EXISTS (
    SELECT 1 FROM launch_dispatches d JOIN generation_leases l ON l.lease_id = d.lease_id
    WHERE d.launch_id = OLD.launch_id AND d.state = 'terminal' AND l.state = 'released')
BEGIN SELECT RAISE(ABORT, 'paired system run not settled'); END;
CREATE TRIGGER retain_system_pair_dispatch BEFORE DELETE ON launch_dispatches
WHEN EXISTS (SELECT 1 FROM run_recovery_pairs WHERE launch_id = OLD.launch_id)
BEGIN SELECT RAISE(ABORT, 'prune recovery pairs first'); END;
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
          SELECT 1 FROM execution_handoffs h JOIN authority_meta m ON m.singleton = 1
          WHERE h.launch_id = d.launch_id AND h.profile = NEW.profile AND h.profile_digest = NEW.profile_digest
            AND h.state = 'observed' AND h.observer_runtime = m.runtime_incarnation AND h.boot_id = m.current_boot_id
            AND (EXISTS (SELECT 1 FROM run_recovery_owners r WHERE r.launch_id = d.launch_id
                  AND r.runtime = m.runtime_incarnation AND r.profile_digest = h.profile_digest
                  AND r.witness_digest = h.witness_digest AND r.handoff_revision = h.revision)
              OR EXISTS (SELECT 1 FROM run_recovery_pairs r WHERE r.launch_id = d.launch_id
                  AND r.root_runtime = m.runtime_incarnation AND r.profile = NEW.profile))))))
BEGIN SELECT RAISE(ABORT, 'system closure not admitted'); END;
