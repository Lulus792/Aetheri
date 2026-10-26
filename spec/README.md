# Versioned catalogue and API contracts

26 October 2026. P0 contract implementation. These files define SQLite structure,
migration behavior and D-Bus messages; they do not implement the Linux authorities,
SELinux policy, document transport or process supervision.

## Files and versions

| Files | Purpose |
| --- | --- |
| [common-v1](schema/common-v1.sql), [system-v1](schema/system-v1.sql), [user-v1](schema/user-v1.sql) | Frozen initial catalogue definitions |
| [common-v2](schema/common-v2.sql), [system-v2](schema/system-v2.sql), [user-v2](schema/user-v2.sql) | Canonical complete v2 bootstrap definitions |
| [management-v1.xml](api/management-v1.xml) | Unchanged 14-method management core |
| [documents-v1.xml](api/documents-v1.xml) | Five new caller-scoped document methods |
| [runtime-v1.xml](api/runtime-v1.xml) | Four new broker-only runtime methods |
| [catalogue_contracts.py](../tools/catalogue_contracts.py) | Development bootstrap, validation and atomic migration harness |
| [test_contracts.py](../tools/test_contracts.py) | File-backed migration/crash tests, SQL behavior and wire compatibility checks |
| [runtime.md](runtime.md), [runtime-v1.json](vectors/runtime-v1.json) | P0 fixture/owner/intent byte formats and independent frozen vectors |
| [runtime_contracts.py](../tools/runtime_contracts.py), [test_runtime_contracts.py](../tools/test_runtime_contracts.py) | Development encoding/payload reference and adversarial format checks |
| [vm-inputs.json](vm-inputs.json), [prepare_vm.py](../tools/prepare_vm.py) | Pinned development-carrier downloads and verification, without installation/execution |

Schema user_version=2 and protocol major/minor 1.1 are independent.
Old management argument lists are unchanged. New interfaces require explicit
documents.read-small.v1, access.revoke.v1 and runtime.dispatch.v1 capabilities.
XML annotations describe required lengths/limits/visibility; they do not enforce
authorization or validate incoming messages by themselves.

Canonical snapshots intentionally retain complete definitions for both versions.
The v2 bootstrap and migration consume the same target SQL. This avoids separate
handwritten migration copies of every CHECK/trigger. v1 hashes in the test suite
detect accidental changes to the reference.

## New records and constraints

V2 has 23 table designs: eight shared, eight system-specific, seven user-specific.
A system catalogue contains 16 tables; a user catalogue 15. Five designs are new:

- admission_gates: exact install/data target, state/revision and closure operation.
  Owner-specific insert guards reject the wrong target kind or missing object.
- launch_dispatches: lease/launch/generation agreement, authorized account/setup
  correlation, root launch operation and supervisor observations.
- resources: selected file, owner, current broker/boot binding, bounded relative
  locator and observations. P0 files are at most 65536 bytes.
- grants: immutable resource/recipient/install/data scope, native read permission
  and terminal revocation state.
- access_leases: per-run/native outstanding access with a unique retry binding.

A grant's own revision is its admission fence; there is no second grant gate row.
Grant/access transitions require increasing revisions. A revoked grant, released
access, terminal dispatch or released code lease cannot restart under its old ID.
Unknown dispatch cannot return to dispatching through a retry.

Composite foreign keys prevent mismatched launch/install/data and lease/broker/
generation bindings. Protected rows cannot be pruned while they represent
unresolved operations, live launches, dispatches or accesses.

Closing a data gate or ending a launch closes its active grants. Revocation cannot
complete while an access is unreleased; private purge also requires settled grants
and closed data admission, in addition to the existing detach/run rules.
Those database observations require truthful updates from future services.

A broker runtime change closes open admission, invalidates selected resources
and marks held native accesses uncertain. A system-authority runtime change
marks nonterminal dispatches uncertain. Neither is proof that app processes exited.
Only the future trusted service may settle actual holders/reopen safe gates.

## Bootstrap and startup

Use a dedicated empty connection with bootstrap(db, kind, version=2).
The harness creates structural SQL and application/version identifiers atomically.
Authority metadata is not invented: the caller must supply the real owner/epoch/
boot/runtime context. A structural check with metadata=False is not service readiness.

Before serving, a Linux authority must validate the correct catalogue type/version,
metadata, owner and runtime prerequisites, then reconcile obligations.
No untrusted client may execute these SQL files directly or open administrative DBs.
SQL cannot prove caller identity, a real descriptor closing, supervisor drain,
filesystem ownership or SELinux enforcement.

Minimum SQL feature floor: SQLite 3.37 for STRICT. Tests ran on the recorded newer
bundled runtime; the floor is not a tested/security-approved dependency release.

## Migration 1 -> 2

The Python harness has no command-line production-database mutator.
It models the procedure to port into the future C17 authority. Call
migrate_v1_to_v2(db, kind, backup=distinct_empty_connection) only with externally
quiesced services/writers and owned, idle connections.

1. Validate v1 application ID, kind, exact canonical schema, foreign keys and integrity.
   Unknown versions, drift, corruption and a nonempty/aliased backup are rejected.
2. Make and validate a SQLite-consistent backup with the backup API. Never overwrite
   an existing backup or automatically restore it over a source.
3. Disable foreign keys outside the rebuild transaction, take BEGIN EXCLUSIVE and
   detect a writer/schema change after the backup.
4. Drop old triggers, create new temporary tables with final reference names and
   copy all original columns/rows. New metadata fields remain unset.
5. Drop old tables, rename the replacement tables, install canonical indexes/triggers.
6. Backfill install/data gates as closed. Preserve all old IDs, receipts, steps,
   links, leases and run rows. Infer no new dispatch/grant from old incomplete work.
7. Check foreign keys/integrity, set user_version=2 and commit. Restore connection
   foreign-key enforcement and validate the canonical v2 structure.

No executescript call can implicitly commit the middle of this transaction.
Errors before commit roll back. Loss of acknowledgement after commit can leave a
valid v2 catalogue; reopening/version validation resolves that case. A valid v2
repeat returns False without replaying migration or touching a new backup.

The runtime incarnation remains NULL after migration, with gates closed.
Reconciliation must precede opening service admission. Existing legacy runs/leases
are preserved as obligations, not promoted into fresh starts.
The two catalogues upgrade separately; both service gates must accept their version/
recovery state before combined launches. No distributed atomic upgrade or automatic
v2-to-v1 downgrade is claimed. Older writers reject v2.

The caller's external quiescence requirement remains essential; the backup/data-
version check is not a replacement for the final Linux service coordination.
Backups contain private administrative metadata and need protected storage.
Backup restoration remains an explicit new-authority-epoch recovery procedure.

## API semantics

Public object: /org/aetheri/Documents1 on org.aetheri.User1.
Private object: /org/aetheri/Runtime1 on org.aetheri.System1.

The XML is authoritative for argument order/directions/types. Local IDs and
incarnations are 16-byte ay, intent digests 32-byte ay; optional invocation ID is
empty or 16 bytes. Integer fields stored in SQLite cannot exceed 2^63-1.

GetGrant returns ResourceId, recipient LaunchId, rights, state, revision and current
runtime incarnation. rights is an array containing read in P0; SQL stores its
single normalized read value. OpenRead binds the exact grant revision, catalogue
epoch, runtime incarnation and request key to a run derived from authenticated
sender identity. Same binding retry returns/query-resolves the same AccessId;
changed content with the same key is RequestConflict.

ReadRange returns bytes plus EOF, takes an explicit offset and maximum 1..16384
bytes, and rechecks current admission. Already admitted bounded reads may finish
before closing settles. It never silently truncates an unsupported large resource.
CloseAccess is idempotent for that caller's access. Both reject old runtime handles.

PrepareRevokeGrant returns the standard preview fields used with the unchanged
ApplyPlan/GetOperation/subscription core. The revoke kind is new; plans carry
grant_ref plus matching install/data/component/account context. Acceptance requires
the recorded grant read-set revision still matching active authorization.

Private ReserveLaunch binds the complete checked tuple; DispatchLaunch accepts
the corresponding ready/fence values; CloseLaunch records an exact durable cause;
GetDispatch returns bounded actual observations. These accept no arbitrary
executable, mount, environment, UID or policy label. Unit names are derived as
aetheri-app-<lowercase 32 hex LaunchId>.service. Root methods require the verified
registered broker, not merely a same-UID sender or an asserted broker UUID.

StaleContext is added under org.aetheri.Error1 for an invalid runtime handle.
Existing InvalidArgument, LimitExceeded, NotAuthorized/NotFound, StaleRevision,
StalePlan/PlanExpired, RequestConflict, NeedsRecovery, UnsupportedCapability and
IncompatibleVersion meanings remain. Error text contains no raw SQL/private paths.
P0 exposes native per-run small reads only; direct/saved/project grants, bulk
streams and document edits remain outside these added interfaces.

The descriptions are producer/consumer obligations. XML/SQL tests do not execute
a D-Bus server, validate kernel credentials or test content transfer.

## Running checks

Run from the repository root with Python 3.11+ and SQLite 3.37+:

```text
python tools/test_contracts.py
python tools/test_runtime_contracts.py
python tools/test_vm_inputs.py
```

The suite runs the unchanged v1 constraints as a regression, validates extension
signatures against an independent frozen inventory, and exercises v2 with both
catalogue kinds. Its temporary databases/backups live under ignored out/ and are
removed after path-checked cleanup.

Named tests include populated migration/fresh-v2 equivalence, preservation of
every original table's rows, old/new version guards, source drift/corruption,
backup aliasing, intervening writer, read-only source, an engine page-limit
SQLITE_FULL failure, and both partial catalogue upgrades.

Ten injected precommit checkpoints per owner test rollback/retry. Separate child
processes terminate with os._exit at old-table deletion, version update and after
commit, then reopen the real file-backed database. This tests SQLite recovery on
the development host, not physical power loss or Linux service recovery.

SQL cases cover scoped grants, stale revisions/incarnations, retry-key conflict,
closure/drain obligations, root dispatch races, immutable observed identity,
unresolved-record pruning and old purge versus fresh data. They do not supply
descriptor/process evidence by merely updating rows.

The implementation notes and latest executed result are recorded in the
[project state](../brains/aetheri/STATE.md) and
[session journal](../brains/aetheri/journal/2026-10-26-first-prototype.md).

## Sources and limits

- [SQLite ALTER TABLE](https://sqlite.org/lang_altertable.html): rebuild sequencing.
- [SQLite backup API](https://sqlite.org/backup.html): consistent database backup.
- [Python sqlite3](https://docs.python.org/3.12/library/sqlite3.html): transaction,
  backup and executescript behavior.
- [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html):
  introspection/message types.

Inspected 26 October 2026. The first www.sqlite.org ALTER TABLE fetch timed out;
the sqlite.org copy was read successfully. No claims rely on the timed-out response.
Linux service implementation, actual policy/peer mapping, real document/root
identity, general signed package formats and hardware/power-loss acceptance remain
future work. P0 bounded byte formats now have a reference and tests in runtime.md;
they are not a general package importer. Development-carrier downloads are handled
separately from these tests; current verification/install status is in the
[test environment](../brains/aetheri/knowledge/testumgebung.md).
