# Minimal contract extension and first Linux prototype

26 October 2026. P0 scope accepted by the user after the
[launch/access/revocation/removal protocol](launch-access-and-removal.md).
The [versioned contracts](../../../spec/README.md) now implement the SQL/XML and
development migration/test portion below. No VM or Linux authority is implemented
through that contract work.

The subsequent [runtime-contract slice](../../../spec/runtime.md) adds bounded
fixture/owner/intent byte references, frozen vectors and host tests. The
[carrier input lock](../../../spec/vm-inputs.json) pins preparation downloads;
neither addition implements Linux enforcement or completes the guest setup.

## Recommendation

Build P0 as a headless, end-to-end lifecycle proof on disposable Linux storage:

```text
trusted fixture enrollment -> real confined launch -> small native read grant
                          -> revoke -> graceful drain -> exact private-data removal
```

Use a real system authority, user broker, systemd supervision and enforcing SELinux.
A small probe app is sufficient to exercise actual process and access boundaries.
Use one account initially, plus a second app/data identity to prove separation.

P0 deliberately exercises small read-only external documents. Native write/commit,
large streaming, directory/project grants, saved grants across reboot and direct
POSIX access follow after this proof. Their accepted protocol remains the product
direction; they are not claimed implemented or replaced by weaker fallbacks.

The SQL/XML are now implemented and selected database/migration behaviors tested;
Linux supervision, policy, document IO and service readiness remain target behavior.
No production data exists to migrate. Existing v1 artefacts remain frozen references.

## Minimal database change

Recommend schema version 2, independent of wire interface major 1.
Add five table designs: one shared, one system-owned and three user-owned.
Reuse plans, operations, operation_steps and operation_links instead of creating
another operation journal.

The current v2 implementation has 23 distinct table designs:
16 in the system catalogue and 15 in a user catalogue, counting eight common
definitions in each. The original v1 source retains its 18 designs.
Table count does not determine service count.

Use the accepted BLOB16 IDs, BLOB32 digests, STRICT typing, explicit foreign keys,
bounded text and non-wrapping signed-64-bit revision rules. No arbitrary JSON blobs,
payload content or persistent native descriptor numbers belong in these tables.

### admission_gates — common

Fields: subject_kind (install/data), subject_ref (ID), state (open/closing/closed),
revision and optional closing_operation_id referencing local operations.
Primary key: (subject_kind, subject_ref). The system variant permits install
targets only; the user variant permits its data epochs only. Local constraints/
triggers require the appropriate referenced object. Do not invent a foreign key
across catalogues.

Closing requires an accepted local operation. An open gate has no closing operation;
a terminal closed gate may shed its historical operation reference only after
receipt/recovery retention permits it. Every close/reopen increases revision.
Removed installs and purging/purged data cannot reopen. Creation and mutation
serialize with the owning authority's reservation admission.

A grant already has its own authorization state/revision, so do not add a duplicate
grant gate row. The grant revision serves as its admission fence.

### launch_dispatches — system

Minimum fields:

- launch_id (primary key), lease_id (unique), root_operation_id (unique FK).
- broker_epoch, broker_incarnation, account_scope_ref.
- install_ref, generation_ref, data_epoch_ref.
- boot_id, action_id, verified session_ref and seat_ref.
- install_gate_revision and ready_data_gate_revision.
- deterministic unit_name, optional observed invocation_ref.
- state: reserved/dispatching/observed/closing/terminal/uncertain; revision.
- Optional paired close_cause_epoch / close_cause_operation_ref.

Require a local launch-kind generation lease. Composite uniqueness/foreign-key
constraints must make its generation, broker epoch and LaunchId agree with the
dispatch record; likewise generation must belong to the stated install.
Account scope references the authenticated eligible account. Data/broker/close-cause
references are cross-authority correlations, not fake local SQL foreign keys.

Freeze identity/setup fields after allocation. An observed invocation cannot be
silently replaced by an unrelated restart. State transitions and complete drain
need real supervisor evidence. The close cause records the calling authority's
durable removal/purge intent without rewriting an already completed launch result.

This row closes the gap between reserving code and sending a supervisor request.
A process-local mutex disappearing does not erase dispatching/uncertain work.

### resources — user

Minimum fields: resource_id (primary key), owner_scope, kind=file, fixture_root_ref,
bounded relative locator/display_name, observed_boot_id, broker_incarnation,
observed device/inode, observed_size, state and revision.

P0 states: bound/needs_reselect/retired. Size is 0..65536 bytes. Device/inode values
must be representable in the specified integer range or be rejected, never truncated.
They are observations, not sufficient persistent identity.

The trusted fixture controller selects a file under a pre-registered, protected
test root. The broker validates the regular-file type, read-only access and held
descriptor under the bounded resolver; the locator
is not authority to reopen any matching path later. Source fixture contents stay
immutable during the read test. Only selected resources get rows.

On broker incarnation change, old bindings become needs_reselect. P0 does not
automatically reconstruct a grant from pathname, inode or a restarted client.
Resource/volume registry generalization belongs to a later contract.

### grants — user

Minimum fields: grant_id (primary key), resource_id (FK), launch_id, install_id,
data_epoch_id, issuer_ref, rights, mode, lifetime, state and revision.

P0 permits rights=read, mode=native and lifetime=launch only.
State: pending/active/closing/revoked/expired. Issuer is an authenticated trusted
selection/controller principal; it is not an app-supplied publisher/name.
The target run's install/data tuple must agree through a composite FK to launches
and the necessary unique parent key. Resource/account ownership must agree too.

Activation requires an eligible running recipient, bound current-incarnation
resource and open install/data admission. Closing blocks new access. Closed states
are terminal; a renewed permission has a new GrantId.

Opening another access does not alter the authorization revision. A revoke preview
targets this exact grant and all its uses in the already-declared recipient run,
not a frozen count of handles. Changed authorization or recipient scope does
invalidate it. Delegation and multi-run saved grants are not part of P0.

### access_leases — user

Minimum fields: access_id (primary key), grant_id, launch_id, broker_incarnation,
request_key, mode=native, state and revision.
State: opening/open/closing/released/uncertain.
Use a composite FK so grant and recipient launch agree. Uniqueness for
(launch_id, broker_incarnation, request_key) binds a retry to one access/request.

Before return, persist the access and establish the broker-owned descriptor mapping.
A duplicate key with changed grant/context is RequestConflict. Keep its receipt
while the recipient run can retry; do not prune live correlations into new work.

Only the broker holds the document descriptor in P0. The app receives a logical
AccessId and bounded bytes. The database stores no descriptor integer as a durable
handle. A lost response is queried/retried under the same context, not interpreted
as proof no access was issued.

After verified old-broker exit, native-only descriptors can be settled, old handles
invalidated and grants expired/closed. App launches and their code/data leases
still require their own reconciliation; broker death does not prove app death.

### Small changes to existing tables

| Existing record | Required extension |
| --- | --- |
| authority_meta | runtime_incarnation, separate from durable authority_epoch; set before serving after each process start |
| plans | kind=revoke, grant_ref, strict target shape; existing request/plan idempotency retained |
| plan_reads | Resource/grant/gate subject kinds for relevant revision checks |
| operations | grant result kind; reuse existing state/phase/outcome, steps and subscriptions |
| launches / generation_leases | Composite unique parent keys needed for new agreement constraints |

runtime_incarnation may be unset during a stopped migration, but the service must
not admit requests until a fresh valid value and recovery checks exist.
Ordinary restart preserves the healthy durable authority epoch. Restore/clone
reconciliation retains the distinct epoch-change rules.

A revoke plan uses grant_ref, its recipient install/data context, retention=none
and no artifact/action. The target must belong to that authority/account.
Keep install/activate/remove/purge/launch meanings intact. Revoke is a real new
kind, not a purge request with an unrelated interpretation.

### Migration and compatibility

Keep v1 source contracts as a reference. Implement explicit 1-to-2 migrations and
fresh-v2 bootstrap; do not silently relabel existing files or recreate a catalogue
when schema recognition fails. Existing row IDs and accepted receipts must survive.

Gate writers/admission, settle or explicitly reconcile in-flight work and create
a verified consistent backup. Migrate common + owner changes as one local schema
transaction, including required table rebuilds, indexes, triggers and validation.
Only then commit user_version=2. Both authorities must pass their schema/recovery
gates before the combined service admits launches. There is no cross-database
atomic upgrade.

Changing CHECK sets requires SQLite's supported schema-change procedure, not
editing sqlite_schema text. Enforce the documented foreign-key sequencing for
table rebuilds and verify foreign_key_check/integrity_check. Opening with old
writers must fail rather than interpret v2 through v1 assumptions.
No automatic 2-to-1 downgrade is promised.

Before accepting the migration implementation, test populated v1 fixtures,
preserved IDs/operations, intentional migration failure/rollback and successful
reopen. A restored old backup requires the existing new-epoch reconciliation.
These migration cases now run in tools/test_contracts.py; details and limits are
recorded in the [contract guide](../../../spec/README.md).

## Minimal API extension

Keep the 14 accepted management methods and their argument meanings.
Add five public document methods and four root-only runtime methods.
Their argument types/order/limits are now recorded in the additive XML files
linked from the [contract guide](../../../spec/README.md). No RPC service exists yet.

Negotiate interface major 1 with additive minor capabilities. P0 advertises only
implemented semantics, for example documents.read-small.v1, access.revoke.v1 and
runtime.dispatch.v1. Existing management methods outside the fixture implementation
return UnsupportedCapability without side effects. Absence of a capability must
not be disguised as successful install/save/force-stop.

### Public org.aetheri.Documents1

| Method | Proposed inputs | Result / behavior |
| --- | --- | --- |
| GetGrant | GrantId | ResourceId, recipient LaunchId, rights, state, revision, current runtime incarnation |
| OpenRead | GrantId, expected grant revision, expected authority epoch/runtime incarnation, request key | Idempotent AccessId, size and access state |
| ReadRange | AccessId, expected runtime incarnation, offset, maximum bytes | Bounded bytes and EOF |
| CloseAccess | AccessId, expected runtime incarnation | Idempotent closure state and revision |
| PrepareRevokeGrant | GrantId, expected revision | Standard PlanId/epoch/boot/deadline/digest/effect-count reply |

IDs/incarnations are ay of exactly 16 bytes, digests 32 bytes, revisions/offsets are
t capped at 2^63-1, counts u, states strings and EOF b. Rights can be as but P0
accepts only read. Return no private host paths. Resolve the sender's actual run
from authenticated credentials/label and supervisor identity; supplied IDs alone
cannot select another app/account.

ReadRange accepts 1..16384 bytes, offsets within the supported object and total
fixture size at most 65536 bytes. It uses an explicit offset without a shared
implicit stream position. Oversized inputs/resources fail with LimitExceeded.
This deliberately bounded small-read test is not a production bulk-stream API.
Large files/stream cancellation are deferred rather than silently truncated.

Every OpenRead/ReadRange admits against current grant/resource/incarnation state.
A recorded read already admitted before closure may finish its bounded step;
revocation waits for its settlement. Already returned/queued bytes count as delivered.
CloseAccess settles that access, not an unrelated app run.

PrepareRevokeGrant uses the existing ApplyPlan, GetOperation, step reporting and
subscriptions. A closing grant denies new reads; completion waits for all native
accesses/in-flight reads to settle. AccessDenied/NotFound/StaleRevision and existing
request/plan errors remain explicit; add a StaleContext error for invalid runtime
handles. Unknown IDs never reveal other scopes.

The P0 fixture controller supplies trusted selection via a protected harness
channel for a fixed test root. Do not add a production GrantAnyPath API.
A real trusted picker/portal and launch-with-documents interface remain later work.
No graphical authorization dialog is claimed tested by the harness.

### Private org.aetheri.Runtime1

| Method | Role |
| --- | --- |
| ReserveLaunch | Persist the checked exact launch/lease/account/context tuple and return root operation/fence receipt |
| DispatchLaunch | Accept that lease's user-ready data-fence/context and expected root gate revision; dispatch at most the recorded intent |
| CloseLaunch | Record the authenticated close-cause epoch/operation, cancel unsent work or request graceful drain |
| GetDispatch | Return authoritative dispatch state/revision and bounded supervisor observations for reconciliation |

ReserveLaunch's tuple includes lease/launch IDs, broker epoch/incarnation, account,
install/generation/data IDs, BootId, expected system epoch/install revision and
declared action/session/seat. The root derives the unit and permitted setup.
It accepts no arbitrary executable string, environment, mount path or SELinux label.

DispatchLaunch binds the exact lease and expected install/data fence values.
A repeated request returns/query-reconciles the recorded operation; uncertain
dispatch is not sent again just because a unit lookup is empty.
CloseLaunch is idempotent against the exact lease and durable cause.
It has no force-kill flag in P0. GetDispatch exposes no unauthorized account state.

Only the correctly registered trusted user broker may call this interface.
The authority derives account eligibility and validates the sender; being UID-equal
to the desktop or knowing an ID is insufficient. Broker incarnation and ownership
registration must survive/resolve replies without retaining stale admission.

XML signatures are now frozen for this P0 contract. Canonical fixture/intent bytes
and policy/credential mapping still need implementation before live exposure.
These remaining gates do not invalidate the finite acceptance criteria.

## P0 test environment

Recommend a dedicated QEMU x86-64/UEFI VM on Windows, initially 2 vCPUs, 8 GiB RAM
and a 48 GiB sparse disk under the ignored vm directory. Downloads/build outputs
use the existing ignored project locations. Sparse capacity is not a promise of
actual disk use; monitor growth. User ceiling remains 16 GiB VM RAM.

Use a maintained glibc/systemd/SELinux Linux installation as the test carrier.
Candidate: Fedora Server 44 x86_64, currently offered on the official download page,
with a minimal headless installation and explicitly chosen ext4 storage.
Pin the exact verified image/checksum/signing source and installed tool/library
versions when creating the VM. This selects a test carrier, not Aetheri's final
distribution/root filesystem. Fedora's complete desktop is unnecessary for P0.

Prefer WHPX after verifying host/firmware support. Record TCG explicitly if needed
for functional tests; its timing is not native performance evidence.
Existing shared WSL remains unchanged and is not assumed to supply the required
systemd/SELinux boot/enforcement environment.

Read-only host observation on 26 October 2026: QEMU system/image tools not found
on PATH, WSL lists Alpine-Dev, F: approximately 232.4 GiB free. This does not prove
QEMU is absent elsewhere or WHPX enabled/disabled. No tool, distro or VM installed.

Use only synthetic guest data. No writable host-folder pass-through, personal
documents, real credentials or hardware disks in tests. Networking is for verified
setup/dependency acquisition as needed; functional app scenarios run offline.
Full-disk encryption is optional for this disposable test VM, as already discussed;
LUKS2 personal-system acceptance remains required later.

## Source/runtime scope

Plan a small C17 CMake/Ninja implementation using glibc, libsystemd/sd-bus, SQLite
and libselinux with the maintained policy build/load tooling needed by the carrier.
Record exact versions/licenses before installation/build. Reuse existing primitive
implementations; no new runtime language/compiler or bespoke crypto/security engine.

Expected responsibilities:

- System authority: catalogue, code lease, dispatch/gates and exact fixture removal.
- User broker: data/grant coordination and bounded native reads.
- Trusted launcher: setup then exec; no extra permanent supervisor.
- Probe app: normal read/exit plus controlled dirty/child/error cases.
- Test controller: fixture selection, negative probes and reproducible reports.

A second confined probe runs with the same Unix UID and a different app/data
identity. This is the same-UID isolation case; UID inequality must not accidentally
make the test pass. The administrative test harness is explicitly outside the app
trust boundary and is not a substitute for a production consent UI.

Do not create empty future source directories now. Before actual implementation,
use a few coherent modules corresponding to these responsibilities and their tests.

## Scope boundaries

| P0 demonstrates | Deferred |
| --- | --- |
| Real supervised confined run and child ownership | Desktop/compositor/NexaUI visual design |
| Fixed trusted test enrollment and payload/data roots | General installer, network downloads, signing/update distribution |
| Selected small immutable file read | PDF/browser/editor integration, large streaming and native saving |
| Per-run native grant closure | Remembered grants, direct descriptors, directory/project profiles |
| Graceful drain/block and exact private purge | Interactive force-stop UI/API and real vault/device adapters |
| Catalogue/recovery fixtures and selected failure injection | Bootable original Aetheri image, encryption/secure-boot/hardware acceptance |

Removal in P0 reports only supported fixture effects. If a manifest asks for
unsupported credentials/plugins/integrations, refuse or report unsupported scope;
do not issue a universal full-removal success. Shared fixture runtime references
can be represented to prove reference-aware retention.

Seed trusted fixture manifests/payloads through controlled setup before serving.
While services run, enrollment/re-enrollment goes through the owning authority's
supported fixed-fixture install path, never direct harness SQL writes or arbitrary
package paths. Probe writes to its own config/state/cache remain permitted so
private removal can be verified; external selected documents remain read-only.

## Acceptance and implementation order

| Gate | Required evidence |
| --- | --- |
| A0: carrier | Actual VM/kernel/userspace versions; systemd and SELinux enforcing; required isolation/setup available |
| A1: contracts | Fresh v2, populated-v1 migration, failure rollback/reopen, FK/integrity and malformed/conflicting ID/revision cases |
| A2: launch | Probe + child in recorded invocation/group; correct UID/label; no target execution before verified setup |
| A3: confinement | Same-UID probe denied broker DB/journal, other app data, sibling document and unauthorized runtime API |
| A4: access | Selected bytes match fixture; wrong grant/run/incarnation, oversized read and replaced locator denied |
| A5: revoke | New reads denied at closure; admitted bounded read settled; completed revocation leaves no native access obligation |
| A6: races | Remove/revoke at reservation/ready/dispatch barriers; no possible launch after successful drain |
| A7: recovery | Lost replies and broker/root restart reconcile one intent; old native context cannot reopen by path; no blind duplicate start |
| A8: removal | App/child drain, dirty/unresponsive block without silent SIGKILL, exact old D removal; new I2/D2 and user document survive |
| A9: retention/failure | Shared fixture provider retained for its consumer; IO/full-disk/flush faults produce blocked/recovery state and honest receipts |
| A10: measurements | Reproducible launch/read/close/remove timings, resident memory, idle wakeups and storage overhead with VM/accelerator recorded |

The contract portion of A1 now has executed evidence; Linux gates remain unexecuted.
A SQL-only green run is not completion. A0/A3 failure blocks Linux acceptance; permissive/unconfined runs
cannot replace them. Inspect filesystem inventory, actual process identities and
both catalogues, not just CLI exit codes or status strings. Preserve failure
evidence and distinguish injected IO faults from physical power-loss testing.

Implement in this order:

1. Freeze the bounded schema/API/fixture formats and implement the versioned
   migrations/XML plus meaningful positive/negative contract tests.
2. Create the verified disposable carrier, compile the minimal services/probe and
   establish actual policy/credential isolation.
3. Connect launch + native read/revoke + graceful removal using real supervision.
4. Exercise race/recovery/failure gates and publish the bounded evidence.

P1 can then add document_edits, controlled saves and bulk streaming; direct tool/
project access follows with its stronger holder/drain tests. UI prototypes can
build on the proven backend without being mistaken for security evidence.
Performance budgets should follow measurements rather than invented latency claims.
Record the carrier baseline separately from Aetheri service overhead; a complete
Fedora VM's footprint is not a measurement of the future Aetheri system image.

## Evidence for this planning turn

The subsequent implementation ran 23 named tests on Python 3.12.14 / SQLite 3.53.1,
exit 0, including populated migration, rollback/crash recovery and XML conformance.
V1 SQL/XML remain unchanged and their regression is included.
No D-Bus service, VM, SELinux policy, real app launch, file access or purge has run.
See the [contract guide](../../../spec/README.md) for the exact executed boundary.

Primary sources inspected 26 October 2026:

- [SQLite schema changes](https://www.sqlite.org/lang_altertable.html) and
  [PRAGMAs](https://www.sqlite.org/pragma.html): supported migration procedure and application-managed user_version.
- [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html):
  typed interfaces and additive feature/identity handling.
- [QEMU introduction](https://www.qemu.org/docs/master/system/introduction.html):
  Windows WHPX and TCG are distinct execution modes.
- [Fedora Server download](https://fedoraproject.org/server/download/):
  current Fedora Server 44 x86_64 media and verified-checksum workflow.
- [Accepted protocol](launch-access-and-removal.md) links the systemd, kernel,
  descriptor and flush references supporting the underlying lifecycle design.

The Fedora Quick Docs SELinux getting-started URL returned an access-denied
challenge; it is not evidence that a particular guest policy is installed/enforcing.
Carrier suitability is a recommendation and must be verified inside the actual VM.
