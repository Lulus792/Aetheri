# Launch, document access, revocation and removal protocol

Drafted 25 October 2026; protocol accepted by the user on 26 October 2026.
The [filesystem/ownership/enforcement direction](filesystem-and-permissions.md),
including ext4, LUKS2 and SELinux plus namespaces, remains accepted.
The accepted [P0 scope](first-linux-prototype.md) defines minimal implementation scope.
No services, policies or new SQL tables exist through this document. The accepted [management core](schema-and-api.md) remains
the starting point, not a claim that it already implements document grants.

## Responsibilities and invariants

The user broker owns the account's app/data/grant coordination. The system authority
owns package/generation admission and privileged launch setup. systemd owns actual
supervision; a small trusted launcher applies setup and then execs the app, rather
than becoming another permanent supervisor. The document interface is a broker
responsibility, not one new daemon per file or logical feature.

Keep three durable admission boundaries: root installation/generation, user data
epoch and document grant. Closing one rejects relevant new admissions immediately.
Already reserved work remains accounted for until observed release/reconciliation.
No timeout or missing UI acknowledgement proves an open access has disappeared.

Rules common to all four flows:

- Authenticate current sender, label, registered run and scope; IDs are references,
  not bearer privileges. Never authorize using a supplied PID or app name alone.
- Store accepted intent and allocated correlations before external effects.
- Use short local transactions. Never wait for another authority, systemd, a file
  picker or unsaved-work decision while holding a SQLite transaction.
- Revalidate exact identities and fence revisions before dispatch or handing out
  access. An old request cannot retarget a new install, data root or path occupant.
- Durable operations survive UI disconnect; events are hints followed by snapshots.
- Ambiguous launch/save/delete outcome enters recovery. Do not blindly repeat.
- Confinement is verified before untrusted app instructions execute. Missing required
  policy blocks admission; no hidden unconfined fallback.

A fence is a recorded admission revision/state, not an expiring password.
An access lease describes an outstanding holder, not permission to delete after
its heartbeat stops. Most coordination is event-driven; no per-second app polling
or database transaction per read chunk is required.

## App start

PrepareLaunch and ApplyPlan are the accepted public core for a normal launch.
Root reservation/dispatch names below describe internal protocol roles, not methods
already present in management-v1.xml.

| Step | Owner / durable action | Why |
| --- | --- | --- |
| 1. Resolve | User broker authenticates action/session; resolves exact I/G/D and applicable policy | Display name or stale desktop entry cannot select arbitrary code |
| 2. Accept | Revalidate plan; allocate OperationId, LaunchId, proposed root LeaseId and deterministic unit reference; store requested run plus local data reservation | Correlation exists before a supervisor can create processes |
| 3. Reserve code | Root authority checks current epoch, eligible account, exact active install/generation and open admission gate; persists reservation | Removal/update and launch cannot race through a later unchecked start |
| 4. Prepare view | Trusted setup prepares exact read-only code/runtime views, D payload, approved endpoints/handles and policy context | The target cannot run before isolation is ready |
| 5. Ready check | User broker rechecks its data/grant fences and commits readiness for that exact reservation | A revoked/detached root cannot quietly pass a stale preparation |
| 6. Dispatch | Root authority serializes against installation closure, rechecks fence/reservation, journals dispatching, then makes the bounded systemd request | Supervisor acceptance has a durable correlation |
| 7. Observe | Verify BootId, unit/invocation, trusted-launcher target-exec evidence and actual process identity; update launch/lease observations | A job path or a started helper is not proof the target app started |
| 8. Return | Complete the launch operation with LaunchId when the declared start postcondition is known; track readiness separately | A functioning process and an interactive application are different states |

The user broker can allocate LeaseId before the root row exists. That stored
reference is not proof of a root reservation. Only an authenticated root receipt
for the complete tuple makes it usable; a rejected reservation ends or reconciles
the pending launch without exposing its data.

The root tuple includes machine/root epoch, broker epoch/account, installation,
generation, launch, lease, boot and authorized session/setup context. Same tuple
retry finds the same reservation. Reuse with changed context is RequestConflict.
Account eligibility is checked even for a machine-wide installation.

```mermaid
sequenceDiagram
    participant UI as Desktop / app
    participant B as User broker
    participant S as System authority
    participant M as systemd + trusted launcher
    UI->>B: PrepareLaunch / ApplyPlan
    B->>B: Commit intent + local reservation
    B->>S: Reserve exact launch tuple
    S->>S: Check gate; commit code lease
    S-->>B: Reservation receipt
    B->>B: Prepare data/grants; verify local fences
    B->>S: Ready for this reservation
    S->>S: Check gate; commit dispatch intent
    S->>M: Start recorded unit with trusted setup
    M-->>S: Target execution / invocation evidence
    S-->>B: Observed launch result
    B-->>UI: Operation result + LaunchId
```

Serialize root dispatch and gate closure using a per-install admission lock/state
machine, not a long database transaction. The bounded supervisor request may be
in flight when a response is lost. Durable dispatching state then blocks successful
drain/reclamation until reconciled. An in-memory mutex disappearing on daemon crash
cannot reopen a durable gate or erase that obligation.

If private-data closure starts after the ready check, the broker holds its local
reservation and asks root to cancel/drain the corresponding launch. Root serializes
that cancellation with dispatch; the broker cannot release/delete D until the root
confirms no remaining possible dispatch or running holder. Closure requests must
also handle reservations whose replies are still in flight.

The supervisor unit reference is derived from LaunchId, not freely supplied by an
app. Setup accepts a checked executable/action and descriptor allowlist, not shell
text or arbitrary mount/SELinux settings. The launcher needs a bounded failure path
if coordination disappears. It execs the app only after verified setup; a generic
service-active notification for the launcher alone is insufficient.

Running, ready and exited remain separate. An optional authenticated native readiness
channel can mark ready; foreign apps may have readiness unavailable. A startup timeout
causes observation/recovery or a declared start failure, not an automatic second run.
A fast start/exit can have a known result if evidence survives; otherwise report
recovery_required instead of asserting it never ran.

For single-instance behavior, the broker may route the action to a verified existing
compatible run in the permitted session. The new operation returns that LaunchId
and records action delivery; it does not invent a new launch. Use the operation's
correlation to deduplicate delivery where supported. An unknown action result is
not universally safe to replay.

## File selection and grant identity

A trusted picker identifies the intended recipient and exact requested access.
A window's title/icon is not authenticated identity. A native app does not gain
the whole Downloads or Documents directory merely by opening one selected PDF.

New supporting references:

| Reference | Meaning |
| --- | --- |
| ResourceId | Account-local reference to a selected file, destination or directory root |
| GrantId | Recorded authorization for that resource, recipient and bounded rights/lifetime |
| AccessId | One tracked delivery/use under a grant, tied to a run/holder and transport mode |

Allocate references when needed, not one permanent SQL row for every file in /home.
They use the core opaque-ID rules and do not grant access by possession alone.

A grant records account, InstallId, DataEpochId, verified component/publisher,
ResourceId, rights, mode, lifetime, policy revision and state. A compatible update
may reuse a saved grant only after current manifest/policy checks; it cannot expand
rights. Reinstall does not inherit old grants. Retained app data is not retained
authorization.

Recommend these lifetimes:

| Lifetime | Use |
| --- | --- |
| One operation | A selected save/export or bounded task |
| This launch | Default open-document access while the selected run exists |
| This installation, explicitly saved | A recurring selected project/folder; each new run revalidates it |

A direct access remains tracked until holders are settled even if its authorization
expires. Saved authorizations survive compatible runs; live AccessIds and old handles
do not automatically become valid after reboot.

Grant rights distinguish read content, write content, list directory, create child,
rename and remove. Writing one selected file does not grant sibling deletion.
A directory grant authorizes bounded traversal under its held root, including the
agreed child operations. Symlink/mount escape is not implied permission.

The initial native file grant pins the selected object. A replaced path occupant
is not automatically the same resource. Broker-coordinated save replacement can
deliberately advance that resource's identity/revision. Directory grants refer to
the selected root and its allowed contents. A saved locator after reboot must be
revalidated against registered storage and identity evidence; uncertainty requires
selection/authorization again, not silent path-based adoption.

Picker cancellation grants nothing. A pending prompt is revalidated when answered:
if the target installation or data epoch was removed meanwhile, no grant is issued.
If Files opens a document in an app not yet running, consent is bound to the selected
installation/data and attached only to the run that passes admission. This requires
a typed launch-with-documents extension; the existing PrepareLaunch has no hidden
arbitrary argv/document path parameter.

The XDG FileChooser contract returns URIs. An adapter must resolve selection into
our checked resource/grant model; URI text by itself is not privileged file authority.

## Two access modes

| Mode | Delivery | Revocation boundary |
| --- | --- | --- |
| Native brokered document access | Opaque SDK handle; broker checks operations and owns file/stream state | Deny new operations, cancel streams/commits and settle in-flight work without necessarily closing the app |
| Direct compatibility access | Real file descriptor or bounded mounted project view | Stop further issuance immediately; existing holders may require process-group shutdown before full closure |

Recommend native brokered access for original Aetheri document workflows. It allows
controlled save/replace and useful revocation without restarting a whole editor.
Control uses the typed local API; bulk content uses a bounded authenticated stream
rather than large D-Bus messages. The broker checks grant/fence state at operation
and stream boundaries. Use in-memory state backed by durable transitions; do not
write SQLite for every byte/chunk.

This external-document interface does not route rendering, simulation or every
private app-state read through RPC. Apps use their confined private payload normally.
NexaUI should expose a platform document/picker adapter; the toolkit is not the
authorization authority. Physim/NexaCloud document integration is separate future
work, not a change to those projects here.

Opening/handing out an access must serialize with grant closure. A native broker
retains the actual descriptor and hands the app a logical handle. A direct handoff
is journalled before sending the descriptor or exposing the mount. A lost handoff
acknowledgement is uncertain until the recipient is reconciled; never assume
"not acknowledged" means "not received."

Closing a direct descriptor number in the broker does not close a receiver's copy,
memory mapping or descendants' references. Apps cannot pass these capabilities to
untracked peers through unrestricted IPC. Authorized delegation needs a separate
recorded holder; otherwise it is denied by the confinement design. An app's claim
that it closed everything is not sufficient proof for a hostile holder.

Kernel policy bounds app/broker roles and cross-app access. Fine-grained native
document revocation does not require recompiling global SELinux policy for every
file selection. Direct access has explicitly coarser closure semantics. Existing
browser/rendering sandbox requirements remain additional constraints.

Data already delivered can have been read, copied or remembered. Revocation cannot
make a process forget it or delete independent exports/remote copies. Report the
actual access boundary rather than claiming retroactive secrecy.

## Native edit and save

Recommend broker-coordinated local saves:

1. BeginEdit checks active grant, write rights, exact resource revision and current
   target. Allocate an edit operation and owned temporary storage.
2. Stream changes into that temporary object. The original remains intact.
3. CommitEdit acquires the resource coordinator, rechecks grant/fence, expected
   revision and destination identity. Refuse unresolved overlapping direct writers.
4. Record commit intent, flush required content, perform the authorized same-filesystem
   publication, flush the required directory entries, then persist the final resource
   identity/revision and result receipt.
5. A retry queries the same edit/operation. A replaced identity, version conflict or
   uncertain publication produces conflict/recovery, not an invented fresh save.

This serialization protects cooperating broker-mediated writers. Advisory locks or
metadata comparison cannot supply a universal atomic compare-and-replace against
uncoordinated POSIX/remote writers. Strong v1 save semantics are restricted to
supported local storage and managed writer admission. Direct-write compatibility
scopes must be quiescent for the strong commit; otherwise block or use a separately
disclosed weaker workflow rather than silently promising the same guarantee.

Default developer grants should be as narrow as practical: source read access,
selected build/cache output writes, and native editor saves. Tools such as Git or
formatters can request explicit direct-write project access. Expose the overlap:
a direct writer may need to finish before a strong coordinated save. Exact tool
profiles and the usability/performance tradeoff still need prototype measurement.

Revoking a grant rejects pending commits under it. Preserve the editor's unsaved
buffer/recovery in its own private state; offer a new Save As selection. Do not
silently restore the old permission to complete a save. A save acknowledgement
requires its durability postconditions; rename visibility alone is insufficient.

## Revocation

Grant lifecycle:

```text
pending -> active -> closing -> revoked
                   |
                   +-- closing remains while holders or outcomes are unresolved
active -> closing -> expired     (same holder settlement requirements)
```

A revoked/expired grant never becomes active again; renewed permission gets a
fresh GrantId. The durable operation may be blocked/recovering while the grant
stays closing. Admission is already denied during that interval.

| Step | Effect |
| --- | --- |
| Accept exact revocation | Authenticate actor/scope; freeze GrantId, expected revision and selected extent |
| Close admission | Commit closing + fence increment; reject new access, save, delivery and reacquisition |
| Cancel native work | Cancel streams and pending edits; settle already admitted work at bounded boundaries |
| Drain direct holders | Request closure; reconcile affected runs/descendants/delegations; stop where required |
| Finish | Mark revoked/expired only when the defined access postconditions are verified; report retained copies separately |

A read/save already inside an admitted critical step orders before or after closure;
the protocol must record that order. Already delivered bytes are not withdrawn.
An admitted publication may finish its bounded consistency work while revocation
waits; an edit that has not entered commit cannot acquire that admission afterward.
Use a shared admission coordinator with durable in-flight records, not a check
followed by an unrelated later rename. Do not keep a metadata write transaction
open while waiting for external document publication/flush; SQLite's own commit
flush remains part of its transaction semantics.
A durable completed-revocation receipt promises no remaining usable managed access
under that grant, not absence of remembered content.

For interactive apps, request a graceful close and respect unsaved work. A timeout
does not silently authorize force-killing it. Recommend whole-cgroup stop ownership
with automatic final SIGKILL disabled for this interactive path; a separate explicit
force-stop decision authorizes killing the selected run group. Proposed systemd
settings include KillMode=control-group and SendSIGKILL=no, subject to profile tests.
A failed stop may leave processes; unit inactive/failed alone is not a drain receipt.

Use observed invocation/boot identity and recursive cgroup membership/empty state,
plus settlement of authorized external holders. Prevent cgroup escape and arbitrary
user-supervisor activation. Cgroup emptiness alone is insufficient if handles were
allowed to leave to another recorded group. Keep closure blocked until all holders
and in-flight dispatches are resolved.

The UI distinguishes "new access blocked" from "access fully ended." Show the
holding app/task and permitted next actions. Do not mark complete because an
administrative row was updated or an app returned a friendly acknowledgement.

## App removal

PrepareChange(remove) remains the entry for the accepted install-removal contract.
The preview covers selected install, data, grants, vault entries, integrations,
background activation, shared code and deliberately retained objects.

| Phase | Combined work |
| --- | --- |
| Validate | Check exact I/G/D/storage references, scope, current read set and selected keep/purge effects |
| Fence | Persist root installation closure and account data/grant closure; account for reservations still in flight |
| Quiesce | Cancel undelivered launches; stop/reconcile runs and access holders without silently discarding work |
| Revoke | Settle native streams/edits/direct holders, vault and integration capabilities |
| Remove | Detach exact data, enter irreversible purge only when ready; quarantine/delete verified roots; release unneeded code |
| Verify/report | Check selected postconditions and durable delegations; report removed, requested retention and unresolved work |

Closure is coordinated without claiming one transaction across two catalogues.
Root's installation fence blocks new code dispatch. The user's data fence blocks
new preparation/attach/access. Each authority records its own accepted child work;
the existing operation_links mechanism supplies durable correlation. Any missing
acknowledgement remains a tracked obligation.

The same drain machinery handles ordinary grant revocation and uninstall, with
different targets. An external document granted to an app survives removal; its
app access ends. Private credentials/config/cache/state/plugins and their scoped
registrations are included according to the preview. Device backend state belongs
to the backend component, not automatically to the Settings UI being removed.

Logical code enrollment may finish as removed while its old private purge remains
pending. A new I2/D2 is independent. A late old callback, stop result or purge step
must still match the original epoch, I1/D1/storage and operation; it cannot affect
I2/D2 merely because ComponentId matches.

Keep-data detaches D and revokes permissions; later adoption is explicit and checked.
Multi-user/machine-wide removal names each affected scope. Other users' private
data, locked/unavailable storage, backup/remote copies and dependencies cannot be
silently counted as successfully purged. If selected purge is blocked, the operation
stays blocked/recovering. Applied_with_retention describes declared retention,
not a way to conceal unexpected cleanup failures.

Before irreversible purge, cancellation may unwind only verified reversible
reservations/gates and reconcile actual effects. Already revoked grants are not
resurrected; restoring access needs a fresh authorized grant. Once D is purging/
purged, never rearm it. After partial deletion, stopping remaining work is not undo.
No automatic user-app restart or reinstall as compensation.

## Crash, reboot and suspend

| Interruption | Required response |
| --- | --- |
| UI closes | Accepted operation continues; later query same OperationId |
| Reserve reply lost | Retry/query exact root tuple; no second launch |
| Supervisor reply lost | Reconcile deterministic unit + boot/invocation/exec evidence; unknown is not permission to rerun |
| Broker crashes | Native sessions close/become invalid; direct recipients may still run, so keep access/code obligations |
| Authority restarts | Rebuild gates/dispatch obligations before admitting work; event incarnation changes independently of durable epoch |
| Native commit interrupted | Reconcile recorded temporary/final identity and pending flushes; no blind duplicate publication |
| Data purge interrupted | Resume exact frozen target after validation; preserve mismatch/IO failure as recovery |
| Suspend/resume | Same actual launches can survive; no lease expires into deletion; unaccepted plan deadlines still count suspend |
| Reboot | Reconcile old-boot references; restored windows create fresh runs; saved grants require fresh use/identity checks |
| Catalogue/backup restore | New authority epoch and explicit cross-authority reconciliation before replay |

Transient unit disappearance, a missing PID, an elapsed timeout or a lost broker
connection alone must not become a universal never-started/no-holder proof.
Secure deletion/remote erasure remain outside live managed-removal semantics.

## Required schema and API additions

This protocol exposed a gap in the initial v1 nucleus: its SQL/XML tracked
installation/data/launch/operation identity but has no complete grant/edit or
dispatch-gate model. Do not claim the existing 18 table designs or 14 methods
already implement these flows.

Logical records needed by the full protocol:

| Record | Minimum purpose |
| --- | --- |
| resources | Selected resource, owner, supported locator/identity evidence and revision |
| grants | Recipient I/D/account/publisher, rights, lifetime, native/direct mode, policy revision and closure state |
| access_leases | Grant + actual holder LaunchId/role, boot, delivery mode and open/closing/released/uncertain state |
| admission_gates | Exact install/data scope, owning operation, open/closing/closed state and monotonic fence revision |
| launch_dispatches | Root reservation + recipient account/authority, unit/boot/context, dispatch intent and observed outcome |
| document_edits | Grant/resource, owned temporary storage, expected revision, commit/flush/recovery state and result correlation |

These are responsibilities to persist, not six new daemons.
The accepted [P0 scope](first-linux-prototype.md) starts with five table designs;
document_edits and actual write/commit handling are deferred until the native-read
lifecycle and isolation proof passes. Some may share
existing journal structures if their full identity/state can be represented
without hidden volatile obligations. The [v2 contract implementation](../../../spec/README.md)
now supplies P0 DDL, enum changes, migration tests and additive XML. Native editing,
bulk transport and real runtime enforcement still require their implementation.

Native document request/stream/edit and access-revocation interfaces must be added
under explicit capabilities. Launch-with-documents and root-only reservation/
ready/close/query roles also need typed contracts. Use additive interfaces/minor
capabilities where compatible; a breaking existing meaning needs version review.
Grant revocation/edit commit require explicit operation-kind handling, not
mislabeling them as an existing purge/install request.

Working SDK verbs such as BeginEdit/CommitEdit in this document explain behavior;
they are not released command names or methods present in management-v1.xml.
Canonical message bytes, limits, stream cancellation and policy/principal mapping
remain implementation gates.

## Acceptance scenarios for the first Linux prototype

These are proposed tests, not executed results:

1. Start succeeds with matching identities; target code cannot run before verified
   setup. Single-instance delivery returns the existing run.
2. Remove/revoke at every start boundary, including lost reserve/dispatch replies;
   no launch can appear after its drain was declared complete.
3. Select one PDF: read works, sibling/parent access fails; cancelled picker and
   stale-recipient prompt create no grant.
4. A replaced path/root or unresolved saved locator cannot inherit permission;
   directory grants reject escapes and unsupported mounts.
5. Native revoke cancels new reads/commits and settles streams; app memory already
   received is not falsely reported erased.
6. Direct descriptors, mmap, forked children and authorized delegation keep closure
   pending; untracked descriptor transfer/cgroup escape is denied.
7. Native save is durable/idempotent with cooperating writers, rejects conflict/
   overlapping direct writers, and recovers after rename/fsync interruption.
8. Dirty/unresponsive app blocks graceful removal; only explicit force-stop grants
   the selected kill, and actual descendants/dispatches are verified drained.
9. Old I1/D1 purge never targets new I2/D2; keep-data never restores old grants.
10. Locked other-user storage/shared recovery versions produce truthful partial
    state or declared retention; no fabricated complete purge.
11. Broker/root restart, reboot/suspend, full disk and restored catalogue retain
    the correct obligations without busy polling or duplicate effects.
12. Measure native streaming, direct tool profiles, launch/admission overhead and
    memory/wakeups before promising a performance result.

## Primary references

- [systemd service API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.systemd1.html):
  jobs, transient units and observations; not an Aetheri launch transaction.
- [systemd kill policy](https://www.freedesktop.org/software/systemd/man/latest/systemd.kill.html):
  process-group stop and timeout/SIGKILL behavior.
- [cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html):
  recursive process ownership and populated notifications.
- [D-Bus credentials](https://dbus.freedesktop.org/doc/dbus-specification.html):
  authenticated observations; required label/identity fields must actually exist.
- [open](https://man7.org/linux/man-pages/man2/open.2.html) and
  [unlink](https://man7.org/linux/man-pages/man2/unlink.2.html):
  descriptor and open-file lifetime.
- [openat2](https://man7.org/linux/man-pages/man2/openat2.2.html):
  checked resolution primitives, not authorization by path prefix.
- [fsync](https://man7.org/linux/man-pages/man2/fsync.2.html):
  file versus directory durability.
- [FileChooser](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html):
  URI selection requiring an Aetheri adapter.

Inspected 25 October 2026. Aetheri's fences, grant model, dispatch handshake,
native-save restrictions and UI rules are original protocol decisions accepted
on 26 October 2026. Research/document checks are not live launch, grant,
revocation, removal or Linux security tests.
