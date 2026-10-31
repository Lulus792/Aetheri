# Versioned catalogue and API contracts

31 October 2026. P0 contract implementation. These files define SQLite structure,
migration behavior and D-Bus messages; they do not implement the Linux authorities,
SELinux policy, document transport or process supervision.

The separate [C17 runtime prototype](runtime.md#confined-protocol-services) now
implements confined system/user discovery endpoints. This does not enable the
lifecycle methods defined by these contracts.
The separate explicit live fixture now also proves User-mediated actual process/
executable registration and explicit readiness. Its
[native journal and observation bytes](runtime.md#user-mediated-application-registration-and-readiness)
reuse v2 without changing frozen SQL/XML. No normal app/session registration API
is installed. Separate authenticated start-result receipts now complete both
owner operations while Root's generation lease remains running; see
[launch-result completion](runtime.md#launch-result-completion-without-run-or-lease-release).
Root now also records an [actual execution observation](runtime.md#root-execution-observation-journal)
after its original process evidence matches authenticated User confirmation.
Historical start retries preserve later states and never release the lease.
The separate [v3 run-closure path](runtime.md#separate-durable-run-closure) now
settles the successful fixed test run after authenticated User quiescence and
independent original-process/group end evidence. It preserves those completed
start results. General owner-restart recovery, holders and removal remain work.

## Files and versions

| Files | Purpose |
| --- | --- |
| [common-v1](schema/common-v1.sql), [system-v1](schema/system-v1.sql), [user-v1](schema/user-v1.sql) | Frozen initial catalogue definitions |
| [common-v2](schema/common-v2.sql), [system-v2](schema/system-v2.sql), [user-v2](schema/user-v2.sql) | Canonical complete v2 bootstrap definitions |
| [common-v3](schema/common-v3.sql), [system-v3](schema/system-v3.sql), [user-v3](schema/user-v3.sql) | Explicit additive run-closure journal after frozen v2; no existing table rewrite |
| [common-v4](schema/common-v4.sql), [system-v4](schema/system-v4.sql), [user-v4](schema/user-v4.sql) | Immutable original-process records after frozen v3; role-specific enrollment and retention guards |
| [system-v6](schema/system-v6.sql) | Explicit Root recovery-owner records and guarded closure admission after frozen v5 |
| [lifetime.h](../src/catalogue/lifetime.h), [receipt codecs](../src/catalogue/lifetime.c) | Separate durable run closure and authenticated cross-owner receipt chain |
| [management-v1.xml](api/management-v1.xml) | Unchanged 14-method management core |
| [documents-v1.xml](api/documents-v1.xml) | Five new caller-scoped document methods |
| [runtime-v1.xml](api/runtime-v1.xml) | Four new broker-only runtime methods |
| [broker-v1.xml](api/broker-v1.xml) | Private P0 broker identity bootstrap 1.0, independent of public wire 1.1; no launch capability |
| [catalogue-v1.xml](api/catalogue-v1.xml), [development-v1.xml](api/development-v1.xml) | Private installation/reservation receipts 1.1 and protected fixture tests 1.4; no public launch capability |
| [setup-v1.xml](api/setup-v1.xml), [setup.h](../src/catalogue/setup.h) | Private no-exec setup 1.0, protected profile and recoverable Root journal; separate from app dispatch |
| [projection.h](../src/catalogue/projection.h), [wire codec](../src/authority/projection.h) | Separate system read/user write, bounded typed snapshot and exact retry |
| [data.h](../src/catalogue/data.h), [data tests](../tests/test_data.c) | Explicit first P0 data enrollment, independent allocation witness and atomic closed-gate attachment |
| [reservation.h](../src/catalogue/reservation.h), [reservation tests](../tests/test_reservation.c), [wire tests](../tests/test_reservation_wire.c) | Atomic private code reservation, scoped status and withdrawal before dispatch; no app execution |
| [launch.h](../src/catalogue/launch.h), [launch tests](../tests/test_launch.c) | Durable user acceptance before RPC, actual root receipt binding and pre-dispatch withdrawal after broker replacement |
| [view.h](../src/linux/view.h), [view tests](../tests/test_view.c) | Native private mount views from exact held code/data roots; connected to setup-only supervision, without target execution |
| [catalogue_contracts.py](../tools/catalogue_contracts.py) | Development bootstrap, validation and atomic migration harness |
| [store.h](../src/catalogue/store.h), [store.c](../src/catalogue/store.c) | Native v2 creation, exact v2/v3/v4 (Root also v5) opening and explicit additive upgrades under the retained owner lock |
| [embed_schema.py](../tools/embed_schema.py) | Build-time embedding of frozen v2 and versioned additions; no runtime SQL-file loading |
| [native catalogue tests](../tests/test_catalogue.c), [migration interop](../tools/test_catalogue_native.py) | Native lifecycle/failure checks and opening populated development migrations |
| [test_contracts.py](../tools/test_contracts.py) | File-backed migration/crash tests, SQL behavior and wire compatibility checks |
| [runtime.md](runtime.md), [runtime-v1.json](vectors/runtime-v1.json) | P0 fixture/owner/intent byte formats and independent frozen vectors |
| [image.h](../src/linux/image.h), [image-v1.json](vectors/image-v1.json) | Bounded 112-byte AETHIMG1 executable observation codec; authenticated source and current-process matching are separate obligations |
| [execution.h](../src/catalogue/execution.h), [execution-completion-v1.json](vectors/execution-completion-v1.json) | Canonical 200-byte start-result receipt and separate Root/User completion; no run/lease release or bearer trust |
| [runtime_contracts.py](../tools/runtime_contracts.py), [test_runtime_contracts.py](../tools/test_runtime_contracts.py) | Development encoding/payload reference and adversarial format checks |
| [fixture.h](../src/core/fixture.h), [fixture.c](../src/core/fixture.c), [native fixture tests](../tools/test_fixture_native.py) | Canonical C17 metadata decoding and field/bounds checks; no payload hashing, trust or importer |
| [root.h](../src/core/root.h), [storage.h](../src/core/storage.h), [storage reference tests](../tests/test_storage_ref.c) | Runtime held/named mount identity and expected private-root binding; no durable volume enrollment or public grant transport |
| [allocation.h](../src/core/allocation.h), [allocation-v1.json](vectors/allocation-v1.json), [native tests](../tools/test_allocation_native.py) | Immutable 168-byte allocation object witness and create-only publication/reopening; no volume authentication or complete operation journal |
| [publish.h](../src/core/publish.h), [publication tests](../tools/test_publish_native.py) | Same-mount no-replace fixture publication and repeat flushes; installed journal/MAC integration and internal AETHSTO1 are described in runtime.md |
| [vm-inputs.json](vm-inputs.json), [prepare_vm.py](../tools/prepare_vm.py) | Pinned development-carrier downloads and verification, without installation/execution |
| [provision_windows.py](../tools/provision_windows.py), [run_vm.py](../tools/run_vm.py) | Local archive extraction and guarded disposable-guest installation/control |
| [p0-carrier.ks.in](p0-carrier.ks.in), [inspect_carrier.py](../tools/inspect_carrier.py) | Scoped virtual-disk installation and observed Linux carrier prerequisites |
| [sync_guest.py](../tools/sync_guest.py), [test_vm_tools.py](../tools/test_vm_tools.py) | Explicit source copying and host orchestration regressions |
| [CMakeLists.txt](../CMakeLists.txt), [core](../src/core/storage.h), [native tests](../tests/test_core.c) | C17 ownership/storage primitives; build and scope in runtime.md |
| [peer capture](../src/linux/peer.h), [peer test](../tests/test_peer.c) | Required bus-supplied credentials/process pin; app/run authorization remains separate |
| [process observer](../src/linux/process.h), [process test](../tests/test_process.c) | Current process/service/invocation/cgroup evidence; trusted registration and admission remain separate |
| [replacement-observer test](../tests/test_process_identity.c), [process identity vector](vectors/process-identity-v1.json) | Boot-bound pidfs identity through a fresh authenticated connection; no journal/owner-restart adoption |
| [handoff.h](../src/catalogue/handoff.h), [Root v5](schema/system-v5.sql), [handoff vector](vectors/execution-handoff-v1.json) | Canonical original-object expectations and separate Root handoff intent/observation/retirement journal; no normal installed recovery endpoint |
| [native handoff bridge](../src/authority/handoff.h) | Original export/PREPARE and SQL-bound restore/OBSERVE; opt-in four-slot PID1 transport and confined restart after a completed run; active-run owner recovery remains separate |
| [retained-handle test](../tests/test_lifetime_handoff.c) | Actual bounded PID1 handoff; separate process-free target variant retains original units without controller pins, then verifies retirement/GC; not Root/User journal recovery |
| [execution process vector](vectors/execution-process-v1.json) | Canonical AETHPRC1 durable context/profile binding; its native APIs are in execution.h |
| [p0-fixture.cil](../policy/p0-fixture.cil), [policy controller](../tools/test_linux_policy.py) | Explicit disposable-carrier MAC probe, outside default tests and production policy |

The default schema remains user_version=2; an explicit quiesced-owner upgrade
adds v3's run_closures table and guards. Open never migrates and create still
creates v2. A second explicit v3-to-v4 upgrade adds immutable execution-process
evidence. V1/v2/v3 files are frozen; each later version composes the previous
complete schema with its additive files. Schema and protocol versions are independent.
Catalogue1 is 1.1; Development1 is 1.4 with the additive RefreshExecFixtureInstall
projection beside the original fixture reservation, launch and setup tests.
Setup1 1.0 is a private no-exec prototype. Their implementation and enforcing
checks are described in [runtime.md](runtime.md#authenticated-installation-projection).
Execution1 1.0 now defines the selected executing-starter exchange, with a native
authenticated client and an explicit confined reply fixture; it has no installed
authority producer or public capability. It is generated separately from the
default service tables. See [runtime.md](runtime.md#authenticated-executing-starter-client).
An explicit isolated enforcing-carrier test now joins the native producer, actual
starter/private mounts and target ELF observation. Its private test-control
methods are not installed contracts or public launch capabilities. Normal owner
inputs remain closed; admission/readiness/recovery are still implementation work.
See the [live test boundary](runtime.md#isolated-live-rootstarterexecution-test).
That fixture now connects an actual registered User process, its own catalogue/
private-data checks and durable preparation to Root request/arm. The reusable
native admission adapter retains and rechecks the original peer. These private
test envelopes do not install normal Runtime1 dispatch or public capabilities.
See [connected owners](runtime.md#connected-user-and-root-execution-admission).
The frozen management/documents/runtime/broker XML and v1/v2 SQL are unchanged;
the Development1 minor extension adds no public capability.
The existing private Runtime1 ReserveLaunch/GetDispatch methods now run, with
CloseLaunch restricted to withdrawal of a still-reserved, never-dispatched tuple.
DispatchLaunch remains unsupported, and runtime.dispatch.v1 is not advertised.
Old management argument lists are unchanged. New interfaces require explicit
documents.read-small.v1, access.revoke.v1 and runtime.dispatch.v1 capabilities.
XML annotations describe required lengths/limits/visibility; they do not enforce
authorization or validate incoming messages by themselves.

The initial v1/v2 snapshots retain complete definitions; v3/v4 and Root v5/v6 compose their additions.
The v2 bootstrap and migration consume the same target SQL. This avoids separate
handwritten migration copies of every CHECK/trigger. Frozen v1/v2/v3 hashes in the
test suite detect accidental changes to those sources.

## New records and constraints

The separate [active Root integration](runtime.md#interrupted-active-root-test)
now has actual live-original transfer/OBSERVE evidence, but its recovery closure
failed and its original User later expired. Preserve its held lease, four PID1
handles and exact fixture. New-User recovery is the next implementation step;
the new optional test is not a passing active-run acceptance result.

Root-only v6 adds immutable per-runtime `run_recovery_owners` observations and
narrowly extends closure insertion after a same-boot Root replacement. The native
recovery functions retain original start/profile/receipts and require the still-
original User plus a current SQL-bound handoff OBSERVE. They can finish closure,
never reopen gates or dispatch. Actual installed active-run recovery is pending;
see [recovery transactions](runtime.md#root-replacement-closure-transactions).
The explicit v5-to-v6 migration changes no existing row; frozen v1–v5 sources,
normal v2 creation and User v4 stay fixed.

Root-only v5 adds execution_handoffs: an immutable original AETHHND1 expectation
linked to the existing app-process witness, with separate observation/retirement
state. The explicit v4-to-v5 migration changes no old row, runtime, revision or
gate; User stays v4 and User v5 is rejected. Normal create remains v2 and open
never migrates. Existing-intent retries return no new transfer/removal effect
flags, including after a lost successful commit. Actual handle-set validation
and PID1 effects stay outside SQLite. See the
[handoff journal](runtime.md#durable-original-observation-handoff).

V4 adds execution_processes per owner, with immutable original profile, context,
process identity, witness bytes/hash and enrollment time. In v4, fresh Root ARM,
Root execution observation and User READY require their corresponding validated
record. Root's target must retain the original starter's kernel identity; User
enrolls after registration and before readiness. Historical reads can cross an
owner-runtime replacement on the same boot, without adopting an old run or
reopening admission. Enrollment requires the original runtime. See the
[durable process contract](runtime.md#durable-original-process-enrollment).

The explicit v3-to-v4 migration preserves existing rows and gates and never
backfills missing process evidence. It does not authorize legacy pending starts
or implement their recovery. Separate temporary tests select v4 or Root v5; normal
installed catalogues remain v2. Native v2 requires the lifetime upgrade first.
Each DDL/version change is atomic under the retained writer lock; unknown commit
acknowledgements query the actual canonical target instead of restoring a backup.

V3 adds one run_closures table per owner. It preserves the complete original
execution profile, original start-result digest, invocation/context and separate
closing/quiescence/end receipts. A completed start operation is never reopened or
extended with unfinished shutdown work. Root release atomically settles its run
and generation lease; User then acknowledges the actual authenticated Root result.
Runtime replacement marks unfinished closure uncertain rather than adopting it.
The [runtime guide](runtime.md#separate-durable-run-closure) defines exact bytes,
owner transactions, actual evidence and the fixed-profile limits.

Both native and development v2-to-v3 migration use an exclusive transaction,
validate the complete source/target and preserve all existing rows, identities,
revisions and gates. They require independently quiesced old owners/writers.
DDL/version changes roll back together before commit. After an unknown commit,
inspect the actual canonical version and retry the same upgrade; never copy an
old backup over a possibly committed catalogue. V1-to-v2 native migration remains
unsupported. Only the separate live test currently upgrades its temporary stores;
normal installed catalogues remain v2. No public lifecycle capability follows.

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

## Native catalogue boundary

The C17 store implements v2 creation, exact v2/v3/v4 (Root also v5) validation and startup-context writes.
The v1-to-v2 migration itself remains the Python development implementation above;
native opening rejects v1/unknown versions rather than recreating or migrating them.
The later explicit v3/v4 upgrades add independent run/process evidence; published
v1/v2/v3 SQL and existing protocol versions remain unchanged.

| Internal C API | Behavior |
| --- | --- |
| ae_catalogue_create | Explicit O_EXCL creation, canonical schema and supplied authority metadata in one transaction; flush parent directory afterward |
| ae_catalogue_open | Existing database only; validate application/version, exact schema, foreign keys, integrity and expected owner/epoch |
| ae_catalogue_upgrade_lifetimes | Explicit additive v2-to-v3 transaction under the existing owner lock; no normal automatic migration |
| ae_catalogue_upgrade_processes | Explicit additive v3-to-v4 process-evidence transaction; no backfill, admission or existing-row mutation |
| ae_catalogue_upgrade_handoffs | Explicit Root-only v4-to-v5 original-handoff journal; User/default creation unchanged |
| ae_catalogue_handoff, ae_catalogue_get_handoff | Root original intent/observation/retirement and historical maintenance read; no installed effect/admission or run recovery |
| ae_catalogue_begin_runtime | Atomically set a fresh runtime/boot, advance catalogue revision, close existing open gates and trigger recovery marking |
| ae_catalogue_get_info | Return checked identity/current-context fields; never a readiness or holder-drain assertion |
| ae_catalogue_enroll_account | System owner only: explicitly persist a trusted account/UID and advance catalogue revision atomically; exact active retry is unchanged; never rebind/reactivate a scope |
| ae_catalogue_check_account | Require the exact active system-account/UID row and return its revision; not a process proof or future admission fence |
| ae_catalogue_prepare_user_execution | Persist exact User execution preparation before external submission; fresh-only submit flag, immutable full-profile binding and retained recovery obligation; no ARM/readiness/drain proof |
| ae_catalogue_observe_execution | Record actual execution evidence as Root step5/observed while retaining the running lease/operation; historical retries never rearm or restore later state |
| ae_catalogue_complete_execution / ae_catalogue_finish_user_execution | Separate Root start-result receipt and User acknowledgement; complete only the start operations while retaining dispatch/run/lease obligations |
| ae_catalogue_begin_run_close / ae_catalogue_quiesce_user_run | Separate v3 Root dispatch fence and original User quiescence acknowledgement |
| ae_catalogue_release_run / ae_catalogue_end_user_run | After independent actual end/holder proof, atomically release Root's lease and acknowledge User exited without changing start results |
| ae_catalogue_get_run_receipt | Read original historical closure evidence; no new process effect or fresh kernel proof |
| ae_catalogue_enroll_execution_process | Persist original authenticated process evidence at its exact stage; immutable bytes/time and no replayed effects |
| ae_catalogue_get_execution_process | Same-authority/account/boot historical maintenance read, including after owner-runtime replacement; no run adoption or current process proof |
| ae_catalogue_close | Close SQLite and release process-owned writer resources; reject inherited fork handles |

The caller supplies an already protected, stable absolute directory and identity
from its trusted registry. The directory must be 0700 and belong to the effective
UID; regular catalogue/lock/journal files must be 0600, same owner and singly linked.
openat2/no-symlink/no-mount-crossing checks protect relative files, with no weaker
fallback. The live root/database/lock identities and SQLite's moved-file observation
are checked on use. This does not establish registered-volume identity or MAC
protection from same-UID apps; those remain prerequisites for a real authority.

Owned files are catalogue.sqlite3 and a stable writer.lock. The latter serializes
cooperating native writers and stays in place after its CLOEXEC handle closes.
SQLite manages catalogue.sqlite3-journal during rollback transactions/recovery;
the store never removes a hot journal by hand. WAL/SHM sidecars and other journal
modes are rejected. No attached database, extension loading, trusted-schema
functions or double-quoted string fallback is enabled. SQL/value limits are 1 MiB,
cache target 512 KiB and mmap disabled for this P0 connection.

The opaque handle is single-threaded and process-bound. Do not open/close another
descriptor to the database inode while SQLite owns POSIX locks, or carry its
connection across fork. The preflight descriptor closes before SQLite opens.
The caller still owes protection of the whole path, root/volume registration,
actual kernel boot identity, fresh runtime generation and service admission.

Creation errors preserve owned files for inspection. A failed directory flush can
follow a successful metadata commit; repeat creation then fails with EEXIST, and
explicit existing-file validation resolves the visible database. This is not a
physical durability proof. Startup transaction failures/lost replies similarly
require observation/reconciliation, never a claim that nothing happened.
Closing gates retain their durable intent; native startup does not settle holders,
reopen gates or establish global service readiness.

At build time, embed_schema.py generates an ignored generated/schema.c in each
build directory from the three canonical v2 SQL files. The native library contains
those statements and builds its validation reference in memory using the selected
SQLite library. Normalization preserves text literals and escaped quotes while
handling external whitespace/comments and ordinary quoted identifiers. The
development validator now follows the same rule; literal-space drift is rejected.
Python 3.11+ is a build/test dependency, not a runtime interpreter in the authority.

Eight Linux CTest cases pass in Debug ASan/UBSan and Release, including four new
catalogue cases. They cover both owners, native creation/reopen, wrong owner/epoch,
duplicate/fork writers, schema/version/constraint/corruption failures, path and
sidecar rejection, replacement detection, revision overflow, actual SQLite
page-limit SQLITE_FULL rollback, injected commit/directory-flush errors and four
child-process exits around commit across both owner kinds. The cross-language
case opens populated Python migrations without changing any row or file byte.
No production catalogue, deployed migrator, authority RPC or app grant is created
by these tests. Build commands and the other Linux prerequisites are in runtime.md.

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
python tools/test_vm_tools.py
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
Full Linux lifecycle service implementation, app/run policy/peer mapping, real document/root
identity, general signed package formats and hardware/power-loss acceptance remain
future work. P0 bounded byte formats now have a reference and tests in runtime.md;
they are not a general package importer. Development-carrier downloads are handled
separately from these tests; current verification/install status is in the
[test environment](../brains/aetheri/knowledge/testumgebung.md).
