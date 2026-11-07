-- Original Root issuer observations; migration never backfills old identities.
CREATE TABLE unit_issuers (
    allocation_id BLOB PRIMARY KEY REFERENCES unit_allocations(allocation_id),
    origin BLOB NOT NULL CHECK (typeof(origin)='blob' AND length(origin)=1064 AND substr(origin,1,8)=X'4145544855425331'),
    issuer BLOB NOT NULL CHECK (typeof(issuer)='blob' AND length(issuer) BETWEEN 192 AND 2048 AND substr(issuer,1,8)=X'4145544855495331'),
    enrolled_unix_ms INTEGER NOT NULL CHECK (enrolled_unix_ms>=0),
    CHECK (substr(origin,9,16)=substr(issuer,9,16))
) STRICT;
CREATE TRIGGER admit_unit_issuer BEFORE INSERT ON unit_issuers
WHEN NOT EXISTS (
    SELECT 1 FROM unit_allocations a JOIN authority_meta m JOIN launch_dispatches d ON d.lease_id=a.lease_id
    JOIN generation_leases l ON l.lease_id=d.lease_id JOIN operations o ON o.operation_id=a.operation_id
    JOIN installs i ON i.install_id=d.install_ref JOIN admission_gates g ON g.subject_kind='install' AND g.subject_ref=i.install_id
    WHERE a.allocation_id=NEW.allocation_id AND a.selection_digest IS NOT NULL
      AND m.kind='system' AND m.authority_epoch=substr(a.profile,9,16)
      AND m.runtime_incarnation=substr(a.profile,25,16) AND m.current_boot_id=substr(a.profile,73,16)
      AND substr(NEW.origin,9,16)=m.current_boot_id AND NEW.enrolled_unix_ms>=a.witnessed_unix_ms
      AND d.state='reserved' AND d.invocation_ref IS NULL AND d.close_cause_epoch IS NULL
      AND l.state='reserved' AND o.state='running' AND i.state='active' AND i.active_generation_id=d.generation_ref
      AND g.state='open' AND g.closing_operation_id IS NULL AND g.revision=d.install_gate_revision
      AND NOT EXISTS (SELECT 1 FROM operation_steps s WHERE s.operation_id=o.operation_id AND s.step_no>0)
      AND NOT EXISTS (SELECT 1 FROM unit_custodies c WHERE c.allocation_id=a.allocation_id))
BEGIN SELECT RAISE(ABORT,'unit issuer not admitted'); END;
CREATE TRIGGER freeze_unit_issuer BEFORE UPDATE ON unit_issuers
BEGIN SELECT RAISE(ABORT,'original unit issuer immutable'); END;
CREATE TRIGGER retain_unit_issuer BEFORE DELETE ON unit_issuers
BEGIN SELECT RAISE(ABORT,'unit issuer retirement requires separate protocol'); END;
CREATE TRIGGER require_custody_issuer BEFORE INSERT ON unit_custodies
WHEN NOT EXISTS (SELECT 1 FROM unit_issuers i WHERE i.allocation_id=NEW.allocation_id AND i.origin=NEW.origin)
BEGIN SELECT RAISE(ABORT,'original unit issuer missing'); END;
CREATE TRIGGER require_custody_step_issuer BEFORE INSERT ON unit_custody_steps
WHEN NOT EXISTS (SELECT 1 FROM unit_issuers i JOIN unit_custodies c USING(allocation_id)
    WHERE i.allocation_id=NEW.allocation_id AND i.origin=c.origin)
BEGIN SELECT RAISE(ABORT,'original unit issuer missing'); END;
