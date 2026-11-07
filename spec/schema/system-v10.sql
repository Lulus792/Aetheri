-- Connection custody is separate from app/run/lease and allocation retirement.
CREATE TABLE unit_custodies (
    allocation_id BLOB PRIMARY KEY REFERENCES unit_allocations(allocation_id),
    origin BLOB NOT NULL CHECK (typeof(origin)='blob' AND length(origin)=1064 AND substr(origin,1,8)=X'4145544855425331'),
    node_count INTEGER NOT NULL CHECK (node_count BETWEEN 1 AND 8),
    prepared_unix_ms INTEGER NOT NULL CHECK (prepared_unix_ms>=0)
) STRICT;
CREATE TRIGGER admit_unit_custody BEFORE INSERT ON unit_custodies
WHEN NOT EXISTS (
    SELECT 1 FROM unit_allocations a JOIN authority_meta m JOIN launch_dispatches d ON d.lease_id=a.lease_id
    JOIN operations o ON o.operation_id=a.operation_id JOIN generation_leases l ON l.lease_id=a.lease_id
    JOIN operation_steps s ON s.operation_id=a.operation_id AND s.step_no=1 AND s.action='stage' AND s.subject_kind='generation'
    WHERE a.allocation_id=NEW.allocation_id AND a.selection_digest IS NOT NULL
      AND m.kind='system' AND m.authority_epoch=substr(a.profile,9,16)
      AND m.runtime_incarnation=substr(a.profile,25,16) AND m.current_boot_id=substr(a.profile,73,16)
      AND substr(NEW.origin,9,16)=m.current_boot_id AND NEW.prepared_unix_ms>=a.witnessed_unix_ms
      AND d.state='reserved' AND l.state='reserved' AND o.state='running'
      AND NOT EXISTS (SELECT 1 FROM operation_steps t WHERE t.operation_id=a.operation_id AND t.step_no>1))
BEGIN SELECT RAISE(ABORT,'unit custody not admitted'); END;
CREATE TRIGGER freeze_unit_custody BEFORE UPDATE ON unit_custodies
BEGIN SELECT RAISE(ABORT,'unit custody origin immutable'); END;
CREATE TRIGGER retain_unit_custody BEFORE DELETE ON unit_custodies
BEGIN SELECT RAISE(ABORT,'unit custody retirement requires separate protocol'); END;
CREATE TABLE unit_custody_steps (
    allocation_id BLOB NOT NULL REFERENCES unit_custodies(allocation_id),
    ordinal INTEGER NOT NULL CHECK (ordinal BETWEEN 1 AND 19),
    effect INTEGER NOT NULL CHECK (effect BETWEEN 1 AND 5),
    node INTEGER NOT NULL CHECK (node BETWEEN 0 AND 7 AND (effect IN (2,4) OR node=0)),
    intended_unix_ms INTEGER NOT NULL CHECK (intended_unix_ms>=0),
    confirmed_unix_ms INTEGER CHECK (confirmed_unix_ms IS NULL OR confirmed_unix_ms>=intended_unix_ms),
    PRIMARY KEY (allocation_id,effect,node), UNIQUE (allocation_id,ordinal)
) STRICT;
CREATE TRIGGER admit_unit_custody_step BEFORE INSERT ON unit_custody_steps
WHEN NEW.confirmed_unix_ms IS NOT NULL OR NOT EXISTS (
    SELECT 1 FROM unit_custodies c JOIN unit_allocations a USING(allocation_id) JOIN authority_meta m
    WHERE c.allocation_id=NEW.allocation_id AND NEW.node<c.node_count
      AND m.kind='system' AND m.authority_epoch=substr(a.profile,9,16)
      AND m.runtime_incarnation=substr(a.profile,25,16) AND m.current_boot_id=substr(a.profile,73,16)
      AND NEW.intended_unix_ms>=c.prepared_unix_ms
      AND NEW.ordinal=1+(SELECT count(*) FROM unit_custody_steps p WHERE p.allocation_id=c.allocation_id)
      AND NOT EXISTS (SELECT 1 FROM unit_custody_steps p WHERE p.allocation_id=c.allocation_id AND (p.confirmed_unix_ms IS NULL OR p.confirmed_unix_ms>NEW.intended_unix_ms))
      AND (NEW.effect>3 OR EXISTS (SELECT 1 FROM launch_dispatches d
          JOIN generation_leases l ON l.lease_id=d.lease_id JOIN operations o ON o.operation_id=d.root_operation_id
          JOIN installs i ON i.install_id=d.install_ref JOIN admission_gates g ON g.subject_kind='install' AND g.subject_ref=i.install_id
          JOIN operation_steps s ON s.operation_id=o.operation_id AND s.step_no=1 AND s.action='stage' AND s.subject_kind='generation'
          WHERE d.lease_id=a.lease_id AND d.state='reserved' AND d.invocation_ref IS NULL AND d.close_cause_epoch IS NULL
            AND l.state='reserved' AND o.state='running' AND s.evidence_digest=a.profile_digest
            AND i.state='active' AND i.active_generation_id=d.generation_ref AND g.state='open'
            AND g.closing_operation_id IS NULL AND g.revision=d.install_gate_revision))
      AND ((NEW.effect=1 AND NEW.ordinal=1) OR EXISTS (
          SELECT 1 FROM unit_custody_steps p WHERE p.allocation_id=c.allocation_id AND p.ordinal=NEW.ordinal-1
            AND ((NEW.effect=2 AND ((NEW.node=0 AND p.effect=1) OR (p.effect=2 AND p.node=NEW.node-1)))
              OR (NEW.effect=3 AND p.effect=2 AND p.node=c.node_count-1)
              OR (NEW.effect=4 AND ((NEW.node=c.node_count-1 AND (p.effect=3 OR (p.effect=2 AND p.node=c.node_count-1))) OR (p.effect=4 AND p.node=NEW.node+1)))
              OR (NEW.effect=5 AND p.effect=4 AND p.node=0)))))
BEGIN SELECT RAISE(ABORT,'unit custody step not admitted'); END;
CREATE TRIGGER confirm_unit_custody_step BEFORE UPDATE ON unit_custody_steps
WHEN NOT (OLD.confirmed_unix_ms IS NULL AND NEW.confirmed_unix_ms IS NOT NULL
    AND NEW.allocation_id IS OLD.allocation_id AND NEW.ordinal IS OLD.ordinal
    AND NEW.effect IS OLD.effect AND NEW.node IS OLD.node AND NEW.intended_unix_ms IS OLD.intended_unix_ms
    AND EXISTS (SELECT 1 FROM unit_allocations a JOIN authority_meta m
        WHERE a.allocation_id=OLD.allocation_id AND m.kind='system' AND m.authority_epoch=substr(a.profile,9,16)
          AND m.runtime_incarnation=substr(a.profile,25,16) AND m.current_boot_id=substr(a.profile,73,16)))
BEGIN SELECT RAISE(ABORT,'unit custody acknowledgement immutable'); END;
CREATE TRIGGER retain_unit_custody_step BEFORE DELETE ON unit_custody_steps
BEGIN SELECT RAISE(ABORT,'unit custody history retained'); END;
