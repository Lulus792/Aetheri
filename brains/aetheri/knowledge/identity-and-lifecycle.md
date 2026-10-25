# Identity and lifecycle contract

25 October 2026. Status: identity/lifecycle model accepted by the user.
B/wlroots/systemd/SQLite and the module/interface split remain approved.
The subsequent [schema/API draft](schema-and-api.md) proposes concrete encodings,
constraints and limits; acceptance of this model is not runtime implementation.

## Principles

Names, content, installations, mutable data, running programs and requests have
different lifetimes. Never substitute one identity for another merely because a
display name or filesystem path matches. A reference is not an authorization grant.
Knowing any ID below must not bypass caller authentication or scope checks.

Identifiers are protocol data; the normal UI presents readable names, versions,
owners and explanations, with internal IDs available in details. Illustrations
such as I1/G1/D1 are short schematic labels, not valid wire identifiers.

## Main identities

| Identity | Meaning | Reuse/change rule |
| --- | --- | --- |
| ComponentId | Stable logical product/component, e.g. aetheri.terminal | Retained across rename, releases, updates and reinstall of the same verified product |
| ArtifactId | Exact immutable manifest/payload/target composition | Same verified content may be reused; different content/target has a different digest |
| InstallId | One local installation/enrollment lifetime in a defined machine/account scope | Preserved through updates; removal ends it; reinstall receives a new ID |
| GenerationId | One activation of an artifact under that installation | Fresh on update, rollback activation or replacement; never reassigned |
| DataEpochId | One owned lifetime of mutable app/private data in a scope | Normally retained across compatible updates; new after purge; deliberate retained-data reuse requires an explicit attach |
| LaunchId | One managed running app invocation/process group | Fresh for each new run/restart; not a PID, window ID or permanent app identity |
| OperationId | One accepted install/update/remove/purge or other durable mutation intent | Stable through retry, worker/UI restart and recovery of that exact intent |

Supporting references include AccountId, PlanId, AuthorityEpoch and entity revisions.
Use only the identities needed at a given boundary; these are not separate services.

### Logical component and publisher

Recommend a bounded canonical ASCII ComponentId with a readable stable name, such
as aetheri.terminal. Display name, icon, install location and release label are
mutable metadata and not identity. A namespace string alone proves no ownership.

Signed/catalogue trust policy must bind the ID to the expected publisher and
approved signing-key history. Key rotation is an explicit trust transition, not
an automatic new product; another publisher cannot claim the same component and
inherit its data/grants. Replacing a product identity or transferring trust needs
a reviewed migration. IDs for removed products are not reassigned to unrelated ones.

Component kinds include apps, shared runtimes, tools, plugins and essential system
components. They share catalogue concepts but not identical permissions/removal
policies. Unknown required kinds/capabilities must be rejected, not treated as an app.

### Release label versus exact artifact

A label such as 0.3.0 is useful to humans but not enough to identify installed code.
Two builds can have the same label and different payloads or targets.

Recommend a versioned cryptographic content-digest ArtifactId over a defined
canonical manifest containing target/ABI, entry points and all immutable payload
digests/metadata required for installation. Exact canonicalization, algorithms,
archive rules and signed-envelope format remain to specify.

Hash verification proves agreement with expected content; trust in the publisher
also requires signature/source policy. Downloaded, parsed and verified are distinct
states. Only verified committed store objects may be launch targets. A signature
envelope can be renewed without silently changing the payload identity semantics.

### Installations, scope and catalogue ownership

InstallId records ComponentId, owner scope, install reason and active GenerationId.
Default first scope: one active enrollment per component and owner scope. Explicit
side-by-side profiles/channels, if wanted, are separate future product choices.

The system authority owns committed package/store/activation facts and references
for each scope. The user broker owns private user grants and data attachments.
A user-visible enrollment projection is not a second independently editable copy
of the system install/activation record.

Use a stable AccountId for persistent ownership, mapped to the current OS account.
A username or reused Unix UID must not make a new account inherit an old account's
data/grants. Machine scope is separate from personal scope; one user's removal
must not silently become all-user uninstall.

### Activation generations and running versions

An InstallId can activate multiple successive generations. Each GenerationId
references one immutable ArtifactId and the chosen runtime/entry-point composition.
It is not an incrementing human version number.

Updating I1 from artifact A to B makes a fresh generation G2. Rolling back to A
makes another fresh generation G3, not a resurrection of G1. This keeps old requests,
launch tickets and cleanup records from silently matching a new activation.

Recommendation for the first implementation: staging may happen while an app is
running; activation waits until affected launches and mutable-data writers are
quiescent. Compatible live activation is an explicit later capability, not the default.
Shared-runtime updates retain versions required by other consumers.
Do not silently discard unsaved user work to activate an update. If the app cannot
stop cleanly under the selected policy, leave activation waiting and explain why.

### Mutable-data lifetime

DataEpochId binds component/publisher identity, account/machine scope and a managed
storage root. Apps own file contents within that root; broker/authority metadata
governs its lifetime and grants. The ID is not just the directory's spelling.

Compatible updates reuse the data epoch. A schema migration is recorded separately;
an OS/artifact rollback does not prove the newer data schema can be read safely.

Remove-and-keep-data detaches a retained epoch. Reinstall creates a new InstallId;
it may offer an explicit reviewed attach to compatible retained data, or create a
fresh epoch. It must not find a similarly named folder and silently adopt it.
A purging/revoked epoch cannot be reattached.

Permissions do not automatically return on reinstall. Retained profiles may contain
sensitive app state, so retention and credential choices must be described clearly.
Any deliberate credential reuse needs verified identity and current authorization.

## ID generation and reference rules

Recommend UUIDv4-style opaque identifiers for local InstallId, GenerationId,
DataEpochId, LaunchId, OperationId, PlanId, AccountId and authority/event incarnations.
Use a cryptographically suitable OS random source, standard encoding/validation,
database uniqueness constraints and retry-on-collision. Do not write a random-number
generator, derive IDs from usernames/time/PIDs or claim absolute mathematical uniqueness.
Generator failure must stop allocation rather than use a weak fallback.

UUIDs identify objects; they are not bearer privileges. Ordering uses explicit
revisions/timestamps, not the numerical order of random IDs. Blob/text representation
and exact message types belong in the subsequent schema/API definitions.

Each authoritative catalogue has an AuthorityEpoch plus entity revisions.
Revisions increase within the epoch. A restored/replaced catalogue receives a new
epoch so old previews and event caches cannot match rewound revision numbers.
Ordinary process restart need not replace a healthy persistent catalogue epoch.
Event-stream service incarnation is separate from durable catalogue identity.

Database constraints should prevent dangling active-generation references, duplicate
active scope enrollments and incompatible state combinations. Removed object IDs
are not recycled. Bounded historical receipt retention is a separate policy.

## Component and data states

A logical ComponentId can exist in the catalogue without a local installation.
Availability is not installation.

Install lifecycle:

```text
preparing -> active -> removing -> removed
                 |
                 +-- update: prepare G2, then atomically change active reference
```

Generation lifecycle: staged -> active -> retired -> reclaimable -> reclaimed.
A retired generation may still be retained by an explicit pin, recovery policy
or verified runtime lease. Only an active compatible generation accepts normal new
launches. A removed install is not a launch target.

Data lifecycle: active -> retained/detached; active or retained -> purging -> purged.
An explicit reviewed attach can return retained data to active ownership.
Once purge has begun its irreversible effects, it is not an ordinary undo action.
These are principal lifecycle paths; operation failures and verified safe cancellation
must preserve truthful state rather than invent an active/removed result.

Reference checks include active installations, dependencies, explicit pins, other
users, recovery retention and verified in-flight/runtime leases. Filesystem existence
alone does not prove a live reference; a missing database row does not prove safe
deletion when recovery/integrity is unresolved.

## App-start contract

The authenticated caller requests a component/action in a scope. The user broker
resolves the actual installation and generation, validates current rights, reserves
a generation/data lease and records the start intent before invoking the supervisor.

A launch record contains:

- LaunchId, ComponentId, InstallId and exact GenerationId.
- AccountId, actual OS credentials, session/seat and current BootId.
- DataEpoch references and explicitly issued capabilities.
- Supervisor unit/invocation identity and pinned process references where available.
- Request/operation correlation, observed state, exit reason and related jobs/windows.

PID and unit name are observations, not sufficient authorization. Use authenticated
launch provenance and actual process incarnation; a process descriptor avoids the
same PID later referring to a different process. The pinned dependency/credential
mechanism must be validated for the selected kernel/systemd version.

Proposed states: requested -> preparing -> starting -> running -> stopping -> exited.
Branches include start_failed and recovery_required/unknown when observation is
insufficient. Supervisor acceptance is not app readiness; readiness is a separate
observed capability. An app crash ends a run; it does not delete the installation.

Multiple windows and browser subprocesses may belong to one LaunchId. A request
forwarded to an existing single-instance app returns that existing run plus an
action result, not a fictitious new process. Plugin/job identities can be linked
children while retaining explicit capabilities and stop ownership.

Recommendation: no automatic user-app restart by default. If later enabled, a new
actual run gets a new LaunchId linked to its predecessor. A stable restart/service
slot may be separate. Suspension does not change LaunchId if the same processes
survive; broker restart can reconcile and retain it if identity is verified.
After reboot, old runs are not current. Session restoration creates new launches;
never reuse old PIDs or claim old processes resumed.

Persist request correlation before dispatch. Derive supervisor references from the
recorded intent so reconnect can query the same intended run. There is still a
cross-authority gap: a very short-lived app might have started and exited before
recovery can establish evidence. Absence is not proof of never-started.
If outcome remains ambiguous, report unknown/recovery-required; do not blindly
re-execute an app with potentially destructive startup effects.

A removal gate serializes launch admission with removal: either a trusted start
reservation precedes the gate and is accounted for, or the new launch is denied.
Checking a flag and starting later without a reservation is insufficient.

## Preview, acceptance and idempotency

A PlanId is a server-owned immutable preview containing:

- Operation kind and exact component/install/generation/data targets.
- Actor and machine/account scope, requested retention policy and dependency effects.
- Relevant entity/read-set revisions, permissions and install/publisher trust constraints.
- Expected effects/retained items and a bounded validity period.

A plan is not execution authorization by possession of its ID. Authenticate and
authorize the current caller, then revalidate the scoped inputs before accepting
the operation. A different affected scope or destructive effect needs a new preview.

Avoid invalidating every plan for unrelated catalogue changes. Track the relevant
read set, including shared-consumer/retention conditions, rather than only a single
global counter. A reboot/epoch change invalidates unaccepted previews; accepted
durable operations are reconciled from their recorded intent.

The client supplies one request key for one deliberate action. The authority binds
it to an authenticated durable actor/scope and canonical request/plan identity.
Same key plus same request returns the same OperationId/result; a changed request
using that key is rejected. A UI reconnect must not invent a new key for an
outcome it has not yet established.

Authorization of each API caller is still current; possession of an old key grants
no rights. Deliberate repeat actions, such as a fresh reinstall, use new requests.
Accepted server-owned work can survive closing the initiating UI.

Idempotency has a retention/expiry contract. Do not promise keys are remembered
forever. An expired/pruned receipt or unaccepted preview must not silently turn an
old retry into fresh destructive work. Use bounded server-issued validity/incarnation
in the request protocol, preserve active correlations and require a fresh reviewed
intent after expiry. Exact wire deadlines/ticket formats remain to specify.
A timeout means outcome unknown until queried, not proof that nothing happened.

This is effectively one recorded logical intent, not a claim that SQLite, the
filesystem and systemd together provide universal exactly-once side effects.

## Operation state and outcome

Separate three fields so a blocked operation is not mistaken for a failed phase:

| Field | Recommended values / purpose |
| --- | --- |
| state | accepted, running, blocked, recovering, completed |
| phase | validate, stage, quiesce, publish, cleanup, reconcile as applicable |
| outcome | Only for completed: applied, applied_with_retention, cancelled, failed |

Results also contain effects, deliberately retained objects, unresolved residue
and the next permitted action. A phase percentage is not used as proof of completion.
Missing acknowledgement, partial filesystem effects or uncertain backend state
must enter reconciliation/recovery, not an invented success/failure.

Completed/applied means the declared selected scope and postconditions were verified.
Applied-with-retention names the requested/shared/recovery objects that remain.
Failed may report known residual effects and a concrete repair requirement.
Recovery-required work is not marked completed merely to make the UI quiet.

### Install and update

1. Validate actor, scope, trust, target and current plan/read-set revisions.
2. Record the accepted OperationId, allocated identities and expected effects.
3. Download/unpack/verify with bounded unprivileged work and controlled staging.
4. Quiesce affected runs/writers as needed; revalidate before activation.
5. Durably publish the prepared store object and commit the new activation metadata
   according to the recovery protocol. These are not one magic cross-system transaction.
6. Reconcile registrations/receipts, report installed state and release temporary leases.

A failed pre-publication install does not expose a runnable half-install. Durable
store objects left by interruption remain staging/recovery objects with ownership,
not unexplained garbage. A committed object/activation can be recognized on restart;
do not rerun arbitrary installer scripts to guess whether the step already happened.

An update normally keeps InstallId and compatible DataEpochId but creates GenerationId.
An incompatible schema must block launch/migrate through an explicit tested contract.
System-base rollback and app/database rollback are separate operations.

### Remove and purge

1. Validate exact scope and generations; persist authorized removal intent.
2. Close new-launch/reattach admission and account for already reserved starts.
3. Stop/reconcile the selected app workers and revoke owned integrations/grants.
4. Remove selected code/data by verified ownership identity, not a mutable name.
5. Release dependency references, respecting other users, leases, pins and recovery.
6. Report actual removed, retained, blocked and failed effects.

A data purge targets an exact DataEpochId and held/revalidated storage identity.
Never recursively delete whatever now occupies a reused friendly path.
Symlinks, changed roots, mount crossings and a new reinstall are rejection/recovery
cases until validated. An old operation never retargets the latest active generation.

Install removal ends its enrollment even if deliberately retained data remains.
Machine-wide removal and personal removal have different effects. Unavailable
encrypted other-user storage, delegated access or external copies must be shown
explicitly rather than claiming universal erasure.

Purge/revocation tombstones are minimal system-owned administrative records, not
copies of app data or secrets. Keep them while pending work, references or request
validity requires them. Bound terminal history and expose its purpose/retention.
A tombstone can be pruned only when it can no longer authorize/resurrect old work;
its identity is never reissued to new data. Exact retention budgets remain open.

### Cancellation

Before externally applied effects, cancel and clean owned staging where safe.
During application/publication/deletion, cancellation is a phase-aware stop request,
not a time machine. Finish a necessary bounded consistency step or pause at a safe
boundary, then report effects and recovery. Do not claim that deleted bytes or
already-issued effects were automatically undone.

An explicit compensating/reinstall operation can be planned if needed; it gets a
new OperationId and cannot silently expand scope. Cancelling the UI subscription
is not cancelling the underlying durable operation.

## Recovery, restore and garbage collection

The system authority owns machine operation recovery; the user broker owns private
data/grant receipts. No claim of one atomic transaction across these catalogues.
Each step has recorded preconditions, expected effects and reconcilable evidence.
If a prerequisite is lost, block destructive work instead of treating absence as
permission to act on a newly matching path/name.

Base-image rollback keeps current mutable lifecycle intent, so it cannot resurrect
a purged app merely by booting an older OS. Database/data schema compatibility
must still be checked. Restoring an older full backup is different and may
deliberately restore old data; label that consequence. A restore/import procedure
needs epoch/identity remapping and reconciliation before old operations can run.
Cloning a system is not permission to replay another machine's pending operations.

Reclaim immutable objects only after authoritative reference checks and coordinated
leases; directory scans alone are insufficient. Show active bytes, retained versions,
shared objects and staging/recovery residue without counting all shared bytes as
immediately reclaimable. Logs/receipts have bounded ownership and contain no secret
values or unnecessary copies of private payloads.

## Examples and future acceptance cases

| Scenario | Expected identities / behavior |
| --- | --- |
| Rename Terminal | Same ComponentId; display label changes |
| Update I1 from A to B | Same I1, fresh G2; compatible D1 retained |
| Roll back B to A | Fresh G3 references A; no revival of G1 launch tickets |
| Close/reopen app | Same install/generation; new LaunchId |
| Second window in same process | Same LaunchId, separate window/action identity |
| Suspend/resume | Same LaunchId if the actual run survives |
| Reboot and restore windows | New launches; old PID/BootId never becomes current |
| UI loses install response | Same retry key returns original OperationId or explicit expired/unknown status |
| Remove and keep data | I1 ends, D1 explicitly retained/detached, grants not automatically restored |
| Purge while reinstall starts | Old operation remains scoped to I1/D1; new I2/D2 protected |
| Reinstall with kept profile | New I2; explicit compatible D1 attach, fresh current permission checks |
| Shared runtime used elsewhere | Keep referenced object; show retaining consumer/reason |
| Crash between file publish and DB update | Reconcile recorded evidence; no runnable guessed half-state |
| Rapid app start/exit before acknowledgement | Reconcile; never assume no run and blindly duplicate it |
| Account/UID recreated | Fresh AccountId; no inherited old authority by matching UID/name |
| Registry restore or incompatible schema | New epoch/reconciliation; stale previews denied |
| Cancellation after partial purge | Truthful partial/recovery result; no promise of undo |
| Damaged metadata or replaced data root | Stop destructive work; do not garbage-collect by guessing |

These remain specification examples, not evidence of end-to-end lifecycle execution.
The subsequent [schema/API draft](schema-and-api.md) supplies SQL/XML and executes
selected constraint cases in disposable SQLite catalogues. Wire interoperability,
authentication, filesystem/supervisor recovery and fault injection remain untested.

## Primary references

- [RFC 9562](https://www.rfc-editor.org/rfc/rfc9562.html): UUID format and generation considerations.
- [pidfd_open](https://man7.org/linux/man-pages/man2/pidfd_open.2.html): live task descriptors.
- [systemd service API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.systemd1.html): unit/job/invocation observations.
- [SQLite atomic commit](https://www.sqlite.org/atomiccommit.html): database transaction behavior and assumptions.
- [Module plan](modules-and-interfaces.md) and [app lifecycle](app-lifecycle.md): accepted boundaries and required removal behavior.

References inspected on 25 October 2026. Lifecycle rules and proposed identity
names are original Aetheri design, not guarantees supplied by these libraries.
