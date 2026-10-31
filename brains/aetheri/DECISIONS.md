# Decisions

## Initial direction — 25 October 2026

Aetheri is a Linux-based personal daily-use OS, developed through Codex. Priorities
are original code, polished UI, speed, efficiency and understandable system structure.
Use SecondBrain-style memory and English project documentation. UI/design choices
remain with the user; technical detail work can proceed inside agreed boundaries.

x86-64 and UEFI are the target now and later. Free windows, dock/taskbar and global
search; calm compact dark/glass style, accents and animation. German/English UI,
DE/US layouts, scaling and extensive keyboard use. Later prepare visual alternatives.

The system must work offline without cloud accounts. No general Linux/Windows
application compatibility requirement. Temporary components, firmware, userspace
drivers and standards are accepted.

## Confirmed foundations — subsequent answers on 25 October 2026

- C17 primary. Targeted Rust is allowed where justified; small C++ adapters as needed.
- Existing open compiler, linker, build, debugger and VM tools accepted.
- glibc selected.
- Discussed graphics/input, crypto, maintained-browser-engine and document-library
  boundary accepted. Exact releases and transitive integration are not selected.
- Maintained audio/Bluetooth/WLAN backends accepted; no mandatory replacement.
- Original Aetheri code and documentation use MIT. LICENSE added. External licenses remain.
- User-triggered signed updates with notification and a recoverable previous version.
- Separate users, app isolation/permissions, encryption, recovery and backups remain
  goals. Secure Boot is allowed if useful; its former exclusion is superseded.

## Apps and lifecycle — 25 October 2026

- Own shell and own shell language from the first shell work, rather than deferring
  the language behind a permanent POSIX-shell-first plan.
- Original terminal/editor design and plugins, with tmux-style integration.
  Neovim/tmux are inspirations; no automatic dependency or compatibility promise.
- Original lightweight browser UI, ordinary web use with a retained engine,
  no default news/promotional clutter and user-controlled ad/content filtering.
- Physim should migrate to NexaUI when ready. Existing SDL/OpenGL requirements
  remain real until that separate migration is implemented and tested.
- NexaCloud Desktop confirmed as the requested data-management app.
- Component/process/file visibility and a component overview are requirements.
- Complete-removal option must include app-managed data, plugins and integrations;
  shared dependencies and user-created documents need explicit, visible treatment.
- One coherent device-management UI is wanted, rather than many separate utilities.

## Hardware and storage — 25 October 2026

Dedicated laptop first; later main PC: four CPU cores/eight threads and RTX 2080 Ti,
user-reported. Up to three monitors, 1080p/1440p/4K. Wired/Bluetooth keyboard and
mouse, 3.5 mm/Bluetooth headphones, WLAN and Bluetooth required.
Monitor modes/connections and exact devices remain open. The user subsequently
confirmed suspend/resume as required. The concrete sleep state and driver behavior
must be tested separately on the laptop and main PC; hibernation is not selected.

VM RAM allowance: up to 16 GB. Initial approximately 32 GB total storage discussion
was followed by allocation of F:. The user now states F: is ready for Aetheri while
Physim/NexaCloud outputs migrate away over time. Their directories, processes and
migrations must not be touched. Use F:/Aetheri, never whole-volume cleanup/formatting.
No laptop installation disk has been selected.

## Publication and memory

A new public GitHub repository is already requested. No further generic permission
is needed. Publication must accurately describe the planning stage and omit secrets
and generated images/build trees. Verify actual outcome before recording success.

Memory format: schema 1, template version 1, project aetheri, root `../..`.
The initial German question list remains archived. Historical proposals are not
current decisions; use this file for the latest user choices.

## Architecture discussion — 25 October 2026

Suspend/resume is confirmed by the user. The three overall approaches and component
comparisons are documented in [architecture options](knowledge/architecture-options.md).
These were initially proposals. The user subsequently approved the overall
recommendations and explicitly selected B, wlroots, systemd and SQLite.

## Approved architecture — 25 October 2026

- Controlled modular system (B): original desktop/apps and lifecycle policy on
  selected maintained system foundations.
- Original Wayland compositor based on wlroots. libweston is retained only as
  comparison history, not an unresolved default choice.
- Selected systemd components for service supervision, sessions and sleep integration.
  Do not equate this with enabling every optional service or adopting another desktop.
- SQLite for durable component/installation metadata, used in-process without
  introducing a separate database-server service.
- Versioned read-only system base, separately managed apps/tools/shared runtimes,
  controlled app-private state and one authoritative component inventory.

Reason: retain original product control and complete-removal semantics without
reimplementing all display/session/storage infrastructure first. Consequences:
we own integration, policy, lifecycle/recovery and release acceptance; must audit
versions, transitive dependencies and installed/activated components; must still
implement and test filesystem/service/database recovery across transaction boundaries.

Exact releases, sandbox/update/boot implementations, browser engine, filesystem,
plugin protocol and shell-language syntax are not implicitly fixed. Approval
selects architecture, not evidence of working software or measured performance.

## Module/interface planning — 25 October 2026

The user requested a detailed explanation and recommendation for modules and
interfaces. The [plan](knowledge/modules-and-interfaces.md) is prepared within the
approved architecture. Proposed finer decisions: privileged system authority plus
per-user broker, compositor/UI separation, typed local D-Bus/sd-bus control,
owner-scoped SQLite writers and recoverable operation contracts. Initially these
were proposals; the user has now accepted the module/interface direction.
No exact wire signature, schema, authentication/sandbox backend or implementation
is implied by that acceptance.

## Module approval and identity planning — 25 October 2026

Accepted by the user: the explained module/interface split, typed local control
(D-Bus/sd-bus direction), per-owner metadata writers and recoverable operations.
Core architectural choices remain settled. Working names are not installed APIs.

The user requests the next concrete identity/lifetime contract. The recommended
ComponentId/ArtifactId/InstallId/GenerationId/DataEpochId/LaunchId/OperationId model,
request semantics, expiry and cancellation rules are documented for discussion;
these fine-grained rules are not marked approved without the user's response.

Storage clarification: the user reports F: completely clear. Continue using
F:/Aetheri; this does not authorize whole-volume formatting or changes outside
our workspace. Read-only capacity observation: approximately 232.4 GiB free.

## Identity approval and concrete schema/API draft — 25 October 2026

The user accepted the explained identity/lifecycle recommendations and asked for
concrete SQLite and API contracts. This supersedes the earlier pending status of
that model. Distinct component/artifact/install/generation/data/run/operation IDs,
scope epochs/revisions, exact-target removal, retry/expiry, cancellation and recovery
are accepted in direction.

The [schema/API draft](knowledge/schema-and-api.md) proposes byte encodings,
18 distinct table designs (14 system / 11 user including common definitions),
14 D-Bus methods, explicit errors and initial resource/deadline/receipt budgets.
These new details remain recommendations; no approval is inferred in advance.
Executable SQL/XML fixtures validate selected constraints, not production services.

## Schema/API approval and storage discussion — 25 October 2026

The user accepted the initial concrete SQLite/API recommendations and requested
filesystem structure, storage ownership and permission boundaries next.
The initial SQL/XML, table relationships, operation semantics and starting budgets
are accepted. Canonical artifact/intent byte encodings, runtime authentication
and integration gates remain unresolved as already recorded in that contract.
Earlier pending schema/API status is superseded; no runtime acceptance is implied.

The [storage/permissions proposal](knowledge/filesystem-and-permissions.md)
recommends a conventional Linux technical hierarchy, managed code/data roots,
independent documents, explicit grants and mandatory same-UID separation.
Ext4/LUKS2 and SELinux plus namespaces are new recommendations for discussion,
not inferred dependency approval. No on-disk installation or host policy change.

## Storage approval and launch/access protocol — 25 October 2026

The user accepted the filesystem, storage ownership and permission recommendations.
This includes the proposed Linux-compatible layout, exact private roots versus
independent documents, ext4/LUKS2 and SELinux plus namespaces as foundations.
This supersedes their earlier pending status, not the remaining release/policy/
boot/integration and runtime acceptance gates. No install or host policy change.

The new [protocol proposal](knowledge/launch-access-and-removal.md) describes
root/data/grant admission and drain, native brokered document access, explicit direct
compatibility grants, controlled save/revocation and graceful removal.
These new detailed protocol choices remain recommendations.
The original 18 table designs and 14 methods do not yet implement the identified
grant/edit/gate/dispatch extensions; no silent schema/API expansion is recorded.

## Lifecycle protocol approval and P0 proposal — 26 October 2026

The user accepted the combined launch/file-grant/revocation/removal recommendations.
Durable fences/reservations, native brokered versus direct access, scoped grants,
controlled saves and honest closure/recovery are accepted in direction.
Interactive timeout does not automatically authorize forced termination.
This supersedes pending protocol status, not the unimplemented SQL/API extensions.

The [minimal extension/P0 proposal](knowledge/first-linux-prototype.md) recommends
five new table designs, schema v2 and nine additive public/private methods.
It limits the first implementation to confined launch, small native reads,
revocation and exact fixture removal; edits/bulk/direct/persistent grants follow.
A QEMU/Fedora Server 44 headless development carrier and initial VM resources are
new proposals. They do not select the final Aetheri base or install dependencies.

## P0 approval and contract implementation — 26 October 2026

The user accepted the minimal schema/API/P0 recommendations and explicitly requested
versioned contracts plus migration tests. The five-table first slice, schema v2,
nine additive methods and headless carrier scope are accepted.

Implemented v2 canonical snapshots, XML and a development bootstrap/migration
harness with named SQL/wire/migration tests. Kept v1 SQL/XML byte-for-byte frozen.
The migrator rebuilds under one transaction after a verified backup and preserves
old identities; migration backfills closed gates rather than silently admitting work.

No VM/service implementation or runtime security acceptance follows. Python here
is contract/test tooling; C17 remains the production language direction.

## Next implementation slice and hardware clarification — 26 October 2026

The user asked to begin the recommended next work. Added bounded P0 fixture,
private-root owner and plan-intent encoding references with independent fixed byte
vectors and negative host tests. They do not change frozen v1 SQL/XML or implement
general signed packages. Runtime principal, storage, target-exec and recovery
obligations are recorded in [P0 runtime contracts](../../spec/runtime.md);
their Linux backends remain unimplemented and require guest evidence.

Pinned QEMU 11.1.0 Windows build 2026-08-11 and Fedora Server 44-1.7 network media
for development-carrier preparation. QEMU was linked by the upstream download
page; the provider's SHA-512 matches, but Windows Authenticode reports an expired
certificate. Do not call it a valid Windows signature or silently execute the
installer. A suitable local tool-provisioning path remains to be reviewed.
The Fedora checksum signature was verified against the official full Fedora 44
fingerprint. Downloads/pinning do not select the final Aetheri base or prove a VM.

The user confirmed that the present development workstation is also the main-PC
target. Read-only Windows queries identify Xeon W-2123, nominal 32 GB RAM,
RTX 2080 Ti and three active monitor records. User further requires Aetheri to
work after a future AM5 upgrade. Preserve generic x86-64/UEFI and CPU feature
detection; no Intel-only baseline or unconditional -march=native. Exact future
CPU/mainboard/GPU and Linux physical acceptance remain open.

## Active implementation and installed carrier — 26 October 2026

The user set the continuing goal to implement Aetheri. Keep the complete agreed
daily-use OS/apps scope; the headless P0 carrier is an intermediate development
environment, not completion or a new final distribution choice.

Implemented reviewed local extraction of pinned host tools, guarded guest setup,
source transfer and actual native C17 owner/storage primitives. Neither upstream
installer was executed. QEMU/UEFI/WHPX and the Fedora 44 ext4 guest now execute;
baseline systemd/SELinux/cgroup/ProcessFD prerequisites have actual observations.
Build tooling uses the recorded GCC/CMake/Ninja/sanitizer versions. Original C17
code remains MIT; carrier/tool notices retain their own licenses.

Corrected the new host-control transport before app scenarios: a benign guest
probe could reach loopback TCP QMP through slirp. Replaced TCP with an actual
Windows AF_UNIX socket under a current-user-only directory. IPC access is protected
by the host filesystem, while UUID remains correlation rather than authentication.
No host drive/share, Windows feature toggle or unrelated WSL/project mutation.

Native checks now cover fixed owner bytes, preserved failure outputs, actual
bounded reads and a real same-filesystem bind mount in a private namespace.
Debug ASan/UBSan and Release pass. The core requires a caller-verified protected
root and has no fallback to weaker resolution. This does not approve a finished
Aetheri policy, volume registry, D-Bus authority, app launch or full removal.

## Temporary MAC proof — 26 October 2026

Implemented and exercised an original CIL fixture policy in the disposable
enforcing carrier, then removed its module/root. One app domain uses two fixed,
disjoint MCS category pairs under the same UID. Own data works; sibling/synthetic
broker/unselected-document access and sudo execution are denied. Actual UID,
label, empty capability sets and NoNewPrivileges are verified by the C17 probe.

This is partial implementation evidence within the selected SELinux/namespaces
architecture, not a new production-domain/category-allocation decision or full
A3 acceptance. The explicit root-only fixture controller must preserve uncertain
dispatches and verify owned module bytes/settled units before cleanup. No default
host test activates policy; no permissive mode or automatic app SIGKILL fallback.

## Native catalogue startup — 26 October 2026

Implemented C17 v2 creation/opening and startup-context transactions against the
selected SQLite library. Canonical SQL stays authoritative and is embedded at
build time; no duplicated handwritten schema or runtime SQL-file loading. Unknown
versions/drift fail closed, without implicit recreation or migration. The full
v1-to-v2 migrator remains development Python until its native port is reviewed.

One stable writer-lock file serializes cooperating native writers; same-UID MAC,
registered root/volume identity and global service readiness remain authority
responsibilities. Preserve owned files/receipts when commit or directory flush
acknowledgement is uncertain. Existing gate closure and recovery marking do not
prove actual process/descriptor drain. No public service capability is added yet.

## Confined protocol endpoints — 26 October 2026

The selected system/user boundary now has a C17 prototype service with two roles,
fixed protected bootstrap registries, separate enforcing domains/catalogue writers
and actual boot/runtime sourcing. XML method tables are generated at build time.
Only discovery is enabled; no lifecycle capability is advertised. Internal calls
remain unauthorized until real broker registration/principal mapping exists.
This is implementation progress within the approved architecture, not approval of
production policy, category allocation, registered volumes or all P0 runtime gates.

Use the existing login user manager for the user endpoint; do not enable lingering
to keep a headless test artificially alive. Bind only its session socket onto a
precreated private-namespace mount placeholder, keeping home directories hidden.
RestrictSUIDSGID=no preserves required openat2; startup still verifies enforcing
type, NoNewPrivileges and empty capabilities. The exact integration failures,
compensating boundaries, actual checks and limits are in
[runtime](../../spec/runtime.md#confined-protocol-services).

The root controller operates only the dedicated empty prototype. It refuses
uncertain starts, remaining processes/jobs, altered inputs or unexplained entries.
Administrative update/cleanup interruption requires inspection; do not present
this tool as the production recovery/removal engine. No dependency release or
library family was changed for this service implementation.

## Process-context observation — 26 October 2026

Implemented a separate C17 Linux observer joining the existing bus ProcessFD to
required kernel credentials/cgroup ID, current SELinux label and trusted systemd
service/invocation metadata. Exact cgroup path/inode and repeated live observations
must agree. No caller-PID reopening or missing-feature fallback. The self-pin API
is limited to this calling process; System1 uses it for its own supervised context.

This implements evidence collection/expected-context matching within the accepted
principal contract. It does not register a broker/app or enable lifecycle methods.
Protected launch/account/catalogue binding and admission remain next. The backend
currently requires host PID/proc/cgroup-v2 views and a system-managed .service;
arbitrary user units, scopes and delegated descendants are not generalized.

Actual process tests deliberately run outside the app sandbox, with empty caps
and NoNewPrivileges. Separately, the confined system authority's self-check passes
after narrow read/status/cgroup policy additions. No ptrace, cgroup writes, new
capability, dependency family or package upgrade was adopted. Keep that distinction
when assessing later full app/run and target-exec acceptance.

## Fixed P0 broker enrollment — 26 October 2026

Implemented private Broker1 1.0 without changing public wire 1.1 or schema v2.
Root-owned account/epoch configuration and a selected supervised unit enroll the
sole eligible test broker. Native connection credentials, current context and
invocation bind its declared catalogue runtime. This implements the accepted
registration direction; general eligibility, app/run admission and launch remain.

Registration can pin the known Type=notify process while activating, but every
use requires active state. Only the trusted root controller selects the unit;
copied IDs cannot choose a principal. Same binding is idempotent; live replacement
conflicts. Targeted name/exit subscriptions invalidate identity without claiming
app drain or releasing leases. A name-presence fence after subscription closes
the already-disconnected race; state is rechecked after nested bus calls.

The user broker gets a fixed system-bus connection and only bus-daemon/System1
messaging, not raw supervisor access. A root restart can re-enroll the existing
broker on demand; no timeout-driven service restart. The new probe and slot are
explicit development inventory, with no new dependency family, capability or
SDK/package version. Keep the single-account bootstrap and unfinished production
policy/account/launch boundaries visible.

## Durable test-account enrollment — 26 October 2026

Back the protected P0 account registry with the existing system scopes row before
root readiness or broker registration/use. Only explicit initialization or stopped
administrative enrollment may insert the expected account. Normal startup requires
it already; it must not repair a missing/retired identity as a side effect.

Account insertion and catalogue revision advance share one SQLite transaction.
Exact active retries are idempotent. A retired/differently bound scope cannot be
rearmed or rebound; a live UID collision fails. New identity after UID reuse never
changes or adopts the old lifetime. This implements the accepted identity model
without schema/wire changes, a public account API or a new process/dependency.
General eligibility and OS-account retirement coordination remain future work.

## Canonical native fixture decoding — 26 October 2026

Implement the accepted AETHFIX1 metadata contract in the existing core module,
without introducing a new runtime language, parser library, service or format.
Use fixed bounded arrays and borrowed immutable text spans; reject noncanonical
order/paths/UTF-8 and preserve caller output on failure. Keeping decoding separate
from trust, payload hashing and publication prevents a parsed manifest from being
mistaken for an approved installation. The proposed hash-library selection remains
an open user discussion, not an adopted dependency decision.

## Runtime root references — 26 October 2026

Complement accepted owner headers with current held/named directory observations:
require STATX_MNT_ID_UNIQUE rather than relying only on device/inode, and keep
private storage lookup beneath a trusted anchor using the derived StorageRef.
Expected allocation identity must be independently trusted; observing a candidate
does not authorize adopting it. Keep these runtime structs separate from future
durable volume/allocation records, which still need enrollment/recovery contracts.

Use the same root observer for catalogue directory bindings. Parent/mount writers
remain controlled; observations do not lock out a concurrent privileged mutator.
An internal file FD retains its own holder obligation after a root reference closes.
P0 native transport returns bounded bytes and closes that FD; direct app descriptors
remain deferred. Actual fstatfs denial justified only authority fs_t getattr access.

## Independent allocation object witness — 26 October 2026

Keep the owner/operation binding in an independent immutable record outside the
allocated tree and ordinary operation-receipt lifetime. One 168-byte record per
StorageRef joins owner IDs with inode/birth/filesystem witnesses; mutable mount
instances stay runtime-only. This preserves the accepted lifetime boundary without
silently assigning unimplemented registry semantics to an existing SQL column.
The owning operation still needs intent-before-effects and journal reconciliation;
record publication alone does not admit an app or authenticate a volume.

Publish an unnamed fully written/flushed inode with a create-only link and directory
flush. Preserve uncertain results, compare exact intent on retry and reject copies/
rebindings. Retain the newly allocated test fixture; an empty-bootstrap cleaner is
not authorized to remove managed private data. Reviewed retirement remains next.

## Delegated technical recommendations and OpenSSL — 26 October 2026

The user explicitly accepted OpenSSL 3.5 LTS/libcrypto for native hashing. Review
the exact package/source/license/security/footprint before installation/linking;
do not adopt the older transitive carrier package as the new direct pin by default.
The user is temporarily unavailable and authorizes well-supported recommended
technical choices with recorded rationale. Personally important product/UI and
destructive choices wait for their participation. This supersedes pending-answer
notes for the OpenSSL family; it does not change earlier foundational requirements.

## Native hashing backend and minimum release — 26 October 2026

Adopt Fedora openssl-libs and openssl-devel 1:3.5.9-1.fc44 after actual package
resolution, full-key RPM signature verification, upstream maintenance/security and
Apache-2.0 review. Prefer the maintained LTS patch over the existing 3.5.5 runtime;
do not introduce the newer major family. This updates one runtime and adds one
development package, about 5 MiB net according to the reviewed DNF transaction.
No new resident service is required. Exact inputs remain in vm-inputs.json.

Use EVP SHA256 with provider=default in a private synchronous context, rather than
original cryptographic code, legacy digest APIs or caller-selected plugins. Link
the Crypto component only; require >=3.5.9 within the 3.5 family at build/runtime.
Treat failures as errors and preserve output. Hash the held FD, not a later path
lookup. Metadata checks detect drift but cannot exclude writers; the importer must
copy/hash into protected staging and independently trust its expected ArtifactId.
No signature protocol, general publisher enrollment or package installation is
implied by these primitives or the synthetic enforcing storage check.

## Stage copied payloads before publication — 26 October 2026

Build one bounded C17 payload stager, using the existing canonical manifest,
openat2, root observations and EVP backend. Check the exact inventory before
effects, classify pinned objects before opening file content, and hash protected
copies rather than reopening the source for execution. Keep an operation-owned
0700 root, exclusive files and read-only completed children; never publish or
silently clean up on error. A separate validated resync repeats file/directory
flushes; matching content alone does not settle a previous failed flush.
Caller-owned authorization, independent root/intent bindings and same-UID MAC
writer exclusion are explicit prerequisites. Retain these gates when wiring the
installed authority; a tested filesystem primitive does not replace them.

## Journal-bound administrative fixture staging — 26 October 2026

Use the existing v2 plans/operations/steps and staged/preparing metadata, through
the single native catalogue writer. Do not add a parallel installer database or
mark a stage as a completed installation. A fixed native AETHINT1 install projection
must match the independently protected digest; boot/deadline apply to new acceptance,
while exact accepted retries retain IDs and revalidate scope/UID/revision.

Use root-provisioned preallocated P0 carrier infrastructure to exercise this
integration before implementing general dynamic allocation/public installation.
The StageSlotId is distinct from private-data lifetimes. AETHFIC1 binds the selected
source/stage inode/birth/filesystem witnesses; SHA-256(configuration) commits to
the executing step before payload writes. Content-identical candidates do not
replace independently expected roots. Preserve uncertain stages and require repeat
validation/flush. Keep the operation publish-pending and deny executable mappings
for source/stage types; publication, activation and admission remain separate gates.

## Publish one sealed manifest/payload object — 27 October 2026

Keep the original staged payload inode and move it into a separately preallocated
P0 carrier container. Publish the whole container under the canonical ArtifactId
using same-mount RENAME_NOREPLACE; do not overwrite, copy on EXDEV or adopt matching
unbound bytes. Bind stage/store/container witnesses through immutable AETHSTO1 and
the existing operation's publish-step evidence before IO. No new schema family.

Every retry rechecks data/identity and repeats all flushes, including both previous
parents after visibility. Seal modes and final MAC types before committing verified/
committed metadata. Deny final mutations even to the system role; keep apps without
execution permission until launch admission is implemented. Publish with one owned
RWP ancestor because separate bind-mounted leaves break rename's mount constraint;
the source stays explicitly read-only and MAC still limits writes.

Publication leaves installation preparing and operation running/reconcile with
activation_pending. It is not app activation, an installation result or a lease.
Root-provisioned infrastructure exercises the boundary without silently claiming
general allocation, package trust, registered volumes or complete recovery.

## Complete first installation with closed admission — 27 October 2026

Treat first activation as the last step of the fixed P0 install intent, keeping
the original operation/install/generation identities. Bind the publication and
expected install/generation/gate revisions in protected AETHACT1 evidence. Verify
the immutable store before metadata effects and commit activation, its applied
step and the original install result together in the single owner database.
Do not add a second installer database or a partial cross-system commit protocol
for a step whose only effects are SQL.

Require current boot/runtime, an exact cause-free closed gate and no conflicting
leases/dependencies/dispatches. Leave that gate closed; activation is not launch
readiness or data admission. No registration effects are claimed by this fixture.
Preserve failed/lost commit outcomes and immutable completion stamps/evidence.

Completed retry returns history. It must not rearm a generation, restore an old
active pointer, close a subsequently opened gate or undo removal. Separate the
historical receipt from current installation_active and all future launch checks.
Public activation/update/rollback, general recovery and real app launch remain
distinct implementation gates.

## Observe installations across owner boundaries — 27 October 2026

Use one typed private system observation and the existing user install_bindings
table, through its native owner writer. Do not share SQLite connections/files or
add another metadata database. Bind source epoch/runtime/boot and recipient account;
return actual install scope separately. Store only an exact active observation.
Changed or revoked bindings require explicit reconciliation, including higher
install revisions. Repeats after a lost commit acknowledgement must not duplicate
the row or advance the revision again.

Keep observations separate from admission: no data epoch, attachment, launch,
grant or opened gate. Later launch/data work must recheck system authority.
Private Catalogue1/Development1 are independently versioned 1.0; existing SQL/XML,
public protocol/capabilities and dependency pins remain unchanged.

For the fixed P0 trigger, use a root-selected short-lived UID-1000 user-domain
helper. This carrier rejects root authentication on its user bus. Preserve the
existing bus authentication and verify the selected helper's actual held process,
active unit, invocation, context, cgroup and boot before the local write. Clear
its protected selection after the controller drains it. Narrow read-only manager
status and same-domain IPC permissions support that check, without a new service
or app-domain system-bus access. This is test infrastructure, not a general app API.

## Enroll the first app's private data explicitly — 27 October 2026

Use fresh DataEpochId, StorageRef and AllocationId for the original installed P0
app, with the fixed account and carrier machine/volume references. Preserve the
independent retained storage fixture. Root preallocates the new protected carrier
infrastructure and freezes an independently observed object witness before native
allocation publication; do not infer ownership from a matching candidate header.

The native user writer then inserts the data row, exact install attachment and
closed data gate in one SQLite commit. The temporary open gate produced by the
existing insertion trigger is closed inside that same transaction and never
committed open. There is no external effect inside this transaction, so it needs
neither a second database nor a fabricated install/launch operation. Keep the v2
schema and public API unchanged. General dynamic allocation needs its own explicit
recoverable protocol before production use; this P0 path does not claim one.

Freeze the original user catalogue baseline in protected configuration for first
creation. A changed baseline fails conservatively instead of guessing after
unrelated work or pruning. Exact currently attached retries are read-only and
preserve later gate changes. Detached, retained, purged, changed or conflicting
rows fail; no retry reattaches or resurrects them. This is metadata enrollment,
not a historical completion receipt or a replacement for live launch admission.

## Reserve exact code before process dispatch — 27 October 2026

Use the existing root plans/operations/steps, generation_leases and launch_dispatches
tables. One owner transaction records canonical AETHINT1 launch intent, a stable
request fingerprint, one code lease and reserved dispatch. Stable broker/lease/run
correlations retain the original root IDs on retry; new proposed IDs or timestamps
cannot replace an accepted reservation. Changed tuples conflict. A runtime restart
makes unfinished dispatch uncertain and keeps its holder accounted for.

GetDispatch is account/broker-authority scoped. CloseLaunch currently withdraws
only a reserved tuple with no ready-data/exec observation; cause, terminal state,
released lease and cancelled operation commit together. It must refuse uncertain
or potentially dispatched work. Later stop/drain support needs actual supervisor
evidence. No timeout, lost bus name or metadata-only check proves process exit.

Keep the original private Runtime1 signatures and SQL v2. Development1 gains the
protected P0 test at minor 1.1. Do not advertise runtime.dispatch.v1 until the whole
capability works. The wire profile is explicitly the original fixture with
main/p0/none, not a claim of general action/logind-session authorization.

For real enforcing tests, use an explicit system service startup profile with a
root-protected AETHADM1 bound to current boot and original pre-start catalogue
revision. Recheck exact sealed store witnesses, final labels/modes, inventory,
manifest and content before opening its metadata gate. Ordinary startup keeps
admission closed; a stale profile cannot reopen it. This permits code reservation,
not data readiness or execution: app executable mappings remain denied. The P0
driver persists its test correlations before IPC; normal durable user launch
acceptance and target dispatch remain separate implementation work.

## Journal user acceptance before root RPC — 27 October 2026

Use existing v2 plans/reads/effects, operations/steps/links and launches for the
user side. Commit immutable local intent and proposed LaunchId/LeaseId before any
ReserveLaunch call, so an unknown reply cannot erase the accepted obligation.
Store the exact root request fingerprint in pending step 0; acknowledge the actual
root plan/op/intent through the new private receipt method instead of inventing
remote IDs before RPC. This avoids a second journal or cross-database transaction.

Opening the exact verified private-data gate is separate from readiness. The
original allocation, filesystem identity, mode and MAC must be checked first;
no app view or executable mapping is implied. User startup explicitly marks live
run records recovery_required while existing rules close open admission. Keep
the old request and remote lease, and do not automatically retry a reservation
under the new broker incarnation.

Permit the current authenticated broker of the same authority/account to withdraw
an old tuple only while root dispatch/lease remain reserved, on the original boot,
without invocation or ready-data evidence. This is a metadata-only withdrawal
before dispatch; original broker identity is not changed. It removes an otherwise
unnecessary stranded reservation after user restart. Root restart uncertainty and
potential process work still require separate recovery/drain evidence.

Finish the user operation only from the exact authenticated withdrawn receipt,
including matching stored fingerprint and local-operation cause. A missing root
link after lost reply can be filled from that actual receipt. Existing exec,
readiness, grants or further setup steps require richer reconciliation. Preserve
terminal stamps/revisions and private data. No additional schema, process, library
or policy permission is needed for this slice; public launch remains unsupported.

## Prepare views from admitted handles — 27 October 2026

Use Linux's FD-based mount API for the small trusted setup component. Clone only
the admitted code/data mount, add restrictions without removing existing ones,
then attach to exact empty protected app/data slots. Omit nested mounts rather
than recursively importing another mounted tree. A dedicated child namespace with
private propagation keeps setup effects out of the authority/host namespace.

Do not put namespace entry into the permanent system or user authority. The new
native primitive is for a fresh single-threaded setup process, to be connected to
the durable launch journal/supervisor next. Source/MAC/content admission remains
the caller's responsibility. Readonly code is a view property; other backing-file
aliases still require immutable publication and policy against writers.

A move attempt consumes the local attachment opportunity; uncertain results cannot
be retried as new setup or used to infer drain. Closing a handle is not mount/app
release. Test actual kernel enforcement and an independent parent observation
while the mount exists. Keep SQL/private/public APIs and deployed policy unchanged
until the process/journal/readiness integration accounts for those effects.

## Bind setup before implementing target execution — 27 October 2026

Connect the view primitive through a short-lived, fixed no-exec helper first.
Keep Root's physical setup journal separate from target dispatch InvocationRef and
app readiness. User's existing pending run holds private D; Root's original code
lease remains held while the helper or its effects may exist. A live ProcessFD,
actual unit invocation/cgroup and independent terminal supervisor observation are
different evidence, and none alone proves target execution.

Retain the exact PID1 unit with RefUnit. On this carrier, PID1 authorizes that as
system start; grant it only to trusted Root, explicitly acknowledging the broader
MAC permission. Do not expose arbitrary unit management to apps/User. Use a
separate setup domain for mount privileges and retain cap-zero authority/app rules.
Clear allowed inherited capability bits before registration/effects. No app code
may run under this setup domain or with these capabilities.

For the known immutable no-exec helper's early failure, permit a separately tagged
supervisor-only proof after checking exact image/unit/argv/exit, no automatic
restart/extra commands and independent drain. It does not claim a registered peer.
This narrow recovery depends on protected provisioning and the original Root
runtime/boot. Unknown jobs, changed images, Root restart or a target-executing
implementation require richer recovery; do not extend this proof by analogy.

Keep SQL versions and Runtime1 frozen. Add private Setup1 1.0 and protected
Development1 1.3 only. Preserve old main reservations and the run/c31,c32 fixture;
the c33,c34 data profile needs a new compatible real-exec fixture next. No browser,
desktop or personally important UI decision is implied by these technical choices.

## Build a one-way exec boundary before durable target dispatch — 27 October 2026

Keep the installed no-exec helper and its withdrawal proof unchanged. Introduce
small internal Linux image/execute primitives, tested with owned inert children
and then a separate temporary enforcing policy. A fresh trusted starter verifies
the admitted ELF/held backing object, seals all internal descriptors, removes
credential/capability authority and selects the mandatory target MAC label before
execveat. There is no privileged return or fallback after failure.

Treat the error pipe as failure diagnostics only. EOF, including exit 125 before
or after exec, remains unknown. Use a held ProcessFD and current kernel executable
object/hash observation separately from readiness. Same-UID cap-zero observation
is supported by the actual test; production User/Root integration still needs
unit/run/invocation and durable outcome contracts. Do not add SYS_PTRACE or KILL to
the authorities to copy the privileged test harness.

Keep NNP/nosuid and the restricted namespace setup. The trusted test starter
retains SETUID explicitly until the execution boundary; systemd's bounding set
alone did not retain it under the selected v259 filters. Only exact temporary
test source/target MAC transitions are permitted. All target capabilities and
inherited internal handles are removed. No public API/SQL/new dependency is needed
for this primitive. A new compatible fixture and richer executing-process drain/
recovery are the next integration step; old immutable tuples stay historical.

## Keep compatible execution fixtures independent — 27 October 2026

Use a separate aetheri.p0.exec component/install/generation/artifact and native
operation, with independent protected registry files and a fresh preallocated
slot. Reuse the proven stage/publication/activation implementation and existing
content-addressed store/catalogue; do not fork their protocols or overwrite the
original package, config, main reservations or private data. The two administrative
profiles are compiled selections, never app-supplied paths. Keep both gates closed
until their own admission prerequisites hold. Existing live broker/data/setup
methods continue to select the original profile.

Resolve an exact action from verified canonical manifest bytes into owned literal
argv and expected file metadata. Preserve empty/UTF-8 arguments and allow the
manifest's eight arguments plus argv[0] at the exec boundary. Unknown action means
ENOENT, never main-to-run substitution or a shell fallback. This metadata helper
does not confer publisher trust, filesystem/run admission or readiness.

The compatible inert ELF is test infrastructure for the real start path, not an
installed desktop app or proof of full launch. Give this installation its own user
binding, DataEpoch/allocation and executing-starter recovery/drain contract next.
No new dependency, policy grant, wire or schema version is needed for this step.

## Bind the second installation to separate private data — 27 October 2026

Reuse the authenticated projection and owner-scoped enrollment implementations
for the second selected fixture. Add one no-input protected Development1 1.4
method, RefreshExecFixtureInstall; preserve all original methods/selections and
public versions/capabilities. Use distinct root-owned data/owner config files and
fresh StorageRef, DataEpoch and allocation-operation IDs. Reuse only independently
enrolled common parents and the machine/volume/account identity. Never adopt the
old data from matching paths/content, rebind it or recreate the shared parents.

Assign the exec data the separate fixed c35,c36 profile rather than sharing
original c33,c34. The compatible payload can test that later process context.
Check complete raw object context plus UID/GID/mode and selected account/install/
publisher before allocation publication/SQL. Type-only checking did not bind the
intended category. Keep category selection in the compiled protected test profile;
no new general category allocator or policy grant is implied. Existing header,
allocation, enrollment bytes and SQL remain unchanged.

Remove only the controller's global first-data restriction: first enrollment is
per selected component/publisher/account/install. Observe selected rows and retain
all unrelated data/attachments/gates/business history during retries. An enrolled
binding or closed data gate does not imply app admission/readiness. Connect the
new exact tuple to a distinct executing-start contract next; the no-exec withdrawal
proof remains limited to the immutable non-executing helper.

## Journal execution separately and never replay an unknown effect — 27 October 2026

Use a distinct AETHEXE1 profile and a stage/generation step for the potentially
executing starter. The old stage/launch no-exec proof cannot match it. Bind the
entire canonical profile, exact declared run request and independent allocation
digest; do not reinterpret main or accept caller-selected paths/labels. Reuse v2
SQL vocabulary with native owner transactions rather than adding another database
or service. User reads its own accepted run/data fence; Root owns only its journal.

Only a fresh successful REQUEST/ARM result permits its next external effect.
Retries after a lost acknowledgement observe the receipt and never repeat that
effect. This can leave an unstarted run needing reconciliation, but avoids
silently executing a possibly started run twice. Preserve leases and uncertainty
through replacement. Profile bytes/digests are not authority or actual process
evidence. Native journal tests establish this internal behavior; live starter,
error channel, actual image/readiness and drain integration remain separate work.

## Let the supervisor issue the error writer — 27 October 2026

Root should create the anonymous error pipe and keep its reader, binding it to
the admitted profile/journal and original held starter/invocation before sending
only the writer in an authenticated registration reply. This avoids accepting a
client-selected reader or weakening incoming-FD method-call denials. Native
creation/reading and actual sd-bus writer transfer now have tests; live protocol
and policy binding remain to implement. sd-bus copies the outgoing descriptor
and borrows the incoming one, so both message and local writer lifetimes matter.

Keep the 16-byte AETHXER1 record. Bound reading to two nonblocking calls and 17
retained bytes, require EOF after a valid frame, and retain malformed/unknown
outcomes. Empty EOF is never execution/readiness/drain. Remove terminal watches;
do not introduce a polling process. Keep the explicit enforcing execution probe
on Release binaries, with ASan/UBSan in the separate normal native suite. An
observed LeakSanitizer same-domain ptrace denial does not justify broader policy.

## Coordinate effects after commit and retain terminal diagnostics — 27 October 2026

Use one native Root coordinator around the existing owner journal, held original
starter and sd-event channel watch. Compiled request/release adapters run only
after fresh commits; a lost acknowledgement never authorizes a second effect.
Re-observe the selected UID/domain/boot/unit/invocation under the ProcessFD and
unit reference. Quarantine observed drift permanently within that coordinator.
Sample the channel before arm, not just when its queued event is dispatched.

Record normalized terminal evidence as AETHEXD1 in existing v2 operation step 4
and a bounded canonical reason, atomically. Distinguish unknown, reported failure,
malformed data and observer failure. Keep dispatch/lease/outcome unchanged until
actual observation/readiness/drain contracts permit a transition. Explicitly
retry only a diagnostic acknowledgement, never a process effect. Releasing local
memory/FD/unit references is not a persistent operation or lease release.
These are implemented native component choices; live wire, provisioning, starter
policy/runtime and target readiness/drain acceptance remain separate work.

## Bind the executing starter to protected sources — 27 October 2026

Use the compiled EXEC selection and AETHEXE1 rather than app-supplied paths/action/
labels. Join the independently witnessed publication, exact manifest inventory/
declared run ELF and private allocation digest/record/owner/root witnesses/full
c35,c36 context. Return held CLOEXEC sources for private views and one-way exec;
active catalogue/User admission and exclusion of future mutation remain separate
caller obligations. File observations do not grant those rights.

Share pure registry/config decoders in fixture_profile.c, excluding administrative
write/effect code from the future starter. Reuse import checking with a read-only
no-fsync entry point; preserve publication repeat flushes. The shared 0700 store
parent is distinct from sealed 0555 packages. Reserve a separately labeled/hash-
bound starter and execution.conf, but do not provision/advertise them before live
protocol/policy/supervisor integration. No new dependency, database or process.

## Pin the original Root before accepting starter replies — 27 October 2026

Resolve the trusted system bus owner once, then send only to that unique name.
Correlate method returns to the retained request by bus/sender/cookie/type; require
bus-authenticated ProcessFD/UID/PID/full label and the independently selected
active Root unit/invocation/cgroup/boot. Compare discovery epoch/runtime against
AETHEXE1 and recheck original context on each operational reply. Never follow
a replacement or repeat an uncertain registration/release request.

Use a separate private Execution1 1.0 contract, generated only for the native
client/test producer until actual handlers are ready. Registration transfers one
checked anonymous writer; prepared release repeats the full binding with an
advanced revision. The producer must hold that reply until fresh durable ARM.
Do not reinterpret authenticated receipt as app execution/readiness or reuse
no-exec drain. Keep two/five-second bounded calls and retain uncertainty on timeout.

The explicit protocol fixture reuses the actual Root domain with temporary Root
pipe and bus-daemon write permissions. Root retains its reader; no daemon read
grant, permissive mode or ptrace broadening. Remove only after independently
observed exact process/unit/cgroup/job drain. This tests the consumer, not the
future producer's catalogue/admission logic or the actual starter domain.

## Hold the original prepared call until fresh durable arm — 30 October 2026

Add a separate native Execution1 producer around the existing coordinator. It
owns release transport, so only fresh ARM after commit can consume the retained
original prepared call. Bind replies use the actual journal binding/revision;
no caller-selected paths, descriptors or release callbacks are accepted. Consume
before send and retain send uncertainty rather than attempting another response.

Use one four-second monotonic deadline inside the client's five-second wait,
sampled both before ARM and after its commit. Expiration attempts one recovery
error and leaves durable obligations intact, including a commit that crossed
the deadline. No new database, dependency family, resident process or installed
method table is needed. Close remains local cleanup, never starter drain.

Test the real journal/transport component on the existing broker37 with an owned
private socket/config and no activation entries. Its launcher uses system scope
only for the normal PID1 subscription. Actor/PID1 observations are explicit
controlled adapters. Do not weaken global bus rules or mistake these tests for
actual starter-domain/live-launch admission. Protected provisioning, live
supervisor request and fresh broker/data mediation remain integration gates.

## Use a separate executing image and independently witnessed workspace — 30 October 2026

Build a distinct starter around the existing authenticated client, verified
source loader, private mount primitives and one-way execute boundary. Keep the
old no-exec helper/proofs unchanged. The fixed P0 Root unit and target UID/domain/
environment are compiled technical fixture selections, never app-supplied paths
or policy. General account/environment integration remains separate work.

Bind preallocated runtime workspace objects independently through AETHVWS1 and
the complete AETHEXE1 digest. A matching name/type/owner alone cannot justify
adopting a recreated directory. Keep exact attachment FDs held, reopen sources
inside the new namespace and recheck named/held mounts plus image after the
original release. No fresh code/data/UID/context should be selected after it.

Use the existing bounded error frame for pre-exec failures, then exit without
retry or fallback. EOF/exit125 remains unknown; an executing image always needs
independent executing drain/recovery, even when a particular error occurred early.
Actual protected provisioning/Root publication and positive starter-domain tests
remain gates. No library family, schema/XML change or resident process is added.

## Prove the connected runtime in a separate owned fixture — 30 October 2026

Join the actual native Root journal/endpoint, PID1 dispatch, unchanged starter,
private mounts and target exec before changing normal authority provisioning.
Use a separate private root bound only into those two units, new authority/runtime
epochs and a controlled administrative broker/data context. Never reinterpret
the old normal catalogue/immutable package as this test's state.

Select a separate aetheri_exec_app_t MCS domain. The old app test domain forbids
store execution and Root messaging; preserve those boundaries. The compiled
target context changes the starter digest and is bound by the protected profile.
Scope actual PID1 status/start access to a dedicated temporary unit-file type.
Keep the five-capability starter and cap-zero Root; cross-UID image observation
and exact process termination belong only to the administrative test driver.

Use a fresh post-exec bus connection for current credentials and join it to the
original held starter process/invocation. Observe the actual running ELF rather
than interpreting error-channel EOF as success. Independently drain exact owned
processes/units/cgroups/jobs before test retirement, archive the native unresolved
lease/operation and never apply the old no-exec release proof. Unknown starts
preserve fixture/policy/intent. This evidence does not complete production
admission, readiness, lease recovery or removal, and enables no public capability.

## Persist User execution preparation before crossing owners — 30 October 2026

Add a User-owned step1 binding the complete independently admitted execution
profile to the accepted launch before its first executing Root request. Reuse
the v2 operation journal rather than a parallel database or mutable config flag.
Atomically recheck User metadata/data fences and record the obligation. Keep
preparing/unknown/no invocation; durable intent is not evidence of execution.

Only a fresh successful commit may return submit_required=1. Exact retries return
zero and unchanged revisions; lost commit acknowledgement never permits replay.
Runtime/gate/data drift rejects, while the existing step survives startup recovery.
The old User pre-dispatch withdrawal path must refuse this operation, including
when Root has independently withdrawn its own still-reserved tuple. Future
executing reconciliation must settle the User record explicitly.

Do not treat this record as a filesystem check, authenticated RPC, cross-owner
ARM grant or readiness/drain proof. Current Root/broker/reservation provenance,
independent allocation/MAC checks and mutation exclusion remain required before
the caller uses it. No new schema/wire/dependency/policy/service is needed for
this internal intent boundary; actual User admission wiring remains next.

## Connect owners through the original registered process — 30 October 2026

Replace the isolated synthetic broker context with a real UID1000 confined User
service and its own native catalogue. Keep Root out of User SQLite. Split code
and private-allocation readers so each owner can check its own boundary; retain
the combined reader for the five-capability starter. Code must remain inaccessible
to User, private metadata inaccessible to cap-zero Root, with no tracing grants.

Bind the executing endpoint's complete profile to a Root admission adapter. Hold
an independent original broker ProcessFD before REQUEST, and re-verify its current
connection/unit/invocation and profile identity before ARM. Unknown submission
never permits another request; replacement cannot inherit delegation. Root trusts
the protected User code to perform User intent/data checks, not an app-supplied hash.

Keep live control in the explicit disposable fixture. Preallocated package/data/
scaffold and the fixed test system-scope User unit do not establish normal session
or dynamic allocation policy. Acknowledge fresh User files only after independent
slot/context/ownership checks; archive both unresolved catalogues after exact
test drain. No no-exec release proof, public capability or normal profile change.

## Separate app registration from explicit readiness — 30 October 2026

Use User-owned native journal steps for verified app registration and a subsequent
explicit readiness report. Reuse v2, retain the full original preparation/profile
and current owner/data fences, and never turn READY into lease release or operation
completion. Exact retries preserve later ready state; changed evidence, another
connection, stale gate or owner replacement cannot inherit registration.

Have actual cap-zero User independently check the app's authenticated ProcessFD,
current unit/invocation/cgroup/MCS and running executable against original Root's
authenticated protected-code observation. Root remains outside private User data
and User SQLite. AETHIMG1 encodes only the bounded file observation; process/Root
provenance remains independent. Permit temporary proc/executable reads needed for
this same-UID path, without ptrace, capabilities or direct store traversal.

Keep AppSource/Register/Ready in the explicit isolated test before designing a
normal app SDK/session registration API. The fixed test name and inert payload
establish protocol readiness only; UI responsiveness, general apps and owner
recovery/drain remain separate acceptance. Archive unresolved owner operations
and Root's running lease after exact test drain. No new dependency, schema/XML,
resident service or normal launch capability is introduced.

## Persist Root's actual execution observation separately — 30 October 2026

Record a separate Root step5 only after original held starter/app process context
and the original authenticated User image/registration evidence agree. Move the
native dispatch from dispatching to observed and mark ARM applied, keeping the
generation lease and launch operation running. Reuse v2 without schema/XML changes.
An old exact receipt must preserve later closing/terminal/uncertain states and
closed gates; it cannot become another start or a release authorization.

Permit late immutable channel diagnostics after actual observation. Preserve the
bounded diagnostic reason regardless of ordering; the separate dispatch state/
step5 retains observation. Channel EOF and actual target evidence answer different questions.
Keep their journal steps separate. Bind the live proof through the original local
connections and pins; a packet/digest or self-declared PID alone is insufficient.

Use test-only PinApp/ConfirmApp in the explicit enforcing fixture before exposing
a normal API. The actual User READY record precedes the Root confirmation; losing
that reply retains both obligations for later query/recovery. General launch-result
acknowledgement and eventual executing drain remain separate implementation gates.
No product/UI or destructive hardware choice is needed for this internal step.

## Complete start operations independently of run lifetime — 31 October 2026

Give Root a canonical immutable AETHLCR1 receipt after its original actual execution
observation and settled UNKNOWN channel record agree. UNKNOWN settles only the
channel; it never replaces target/process/image evidence. Preserve its bounded
diagnostic reason. Complete only the original start operation with LaunchId;
leave the observed dispatch and running generation lease unchanged.

User authenticates that original Root reply and compares the exact profile,
observation/run/invocation with its own original registered/ready app and current
private-data fences. Complete its own operation/link in a separate transaction.
Lost replies resolve the original receipt without replaying a start. Historical
retries preserve later states/times and cannot reopen a run, gate or lease.

Reuse existing v2 operation steps and typed internal codecs; no schema/XML or
dependency change is needed. The 200-byte receipt gets an independent frozen
vector. Keep FinishLaunch/Complete in explicit test interfaces until normal
session/app provisioning, lifetime recovery/drain and production policy are proven.
No extra user product choice is needed for this internal lifetime separation.

## Observe original executing lifetime before journal settlement — 31 October 2026

Keep lifetime observation separate from both start-result receipts and durable
run/lease release. Hold the originally admitted leader's ProcessFD, exact cgroup
objects and event reader while the caller retains its original unit reference.
Join terminal PID1 state/job absence with the original invocation and recursive
group emptiness; repeat observations and quarantine identity drift. Use no new
daemon or resident polling loop.

The pinned kernel reports st_nlink=2 even after group removal. Require missing
name plus deactivation/ENODEV from the already-held event reader instead. A missing
name with a readable original rejects. Avoid ambiguous deleted-path strings.
Other kernel behavior needs separate acceptance and fails closed. Actual cgroup-v2
rename returns EPERM; missing/replaced lookups are explicit fault injections,
not a claim that such an actual rename test passed.

Exercise the primitive in the existing explicit enforcing execution fixture only
after Root has joined original User image/readiness confirmation to its process
evidence. The administrative driver authenticates original Root's reply and keeps
the historical lease/run obligations unchanged. Durable closure, restart recovery,
future-dispatch fencing and external/delegated holder settlement remain work.
No library family, frozen schema/XML or normal lifecycle capability changes.

## Keep run closure separate from completed start results — 31 October 2026

Add one v3 run_closures table to each owner's existing database. The start
operation and immutable AETHLCR1 result already have a completed lifetime; run
shutdown needs an independent journal. Store its full original profile and
evidence rather than appending unfinished work to that completed operation.
Keep v1/v2 definitions byte-for-byte, normal creation on v2 and opening free of
migration. An explicit exclusive v2-to-v3 upgrade adds only the new table/guards
and preserves every existing row, owner identity, revision and gate.

Use an authenticated receipt chain: Root closes its dispatch, User fences its
run and acknowledges quiescence, Root independently verifies actual end and
atomically releases the generation lease, then User acknowledges exited. Each
owner commits locally; lost acknowledgements resolve original records without
another process effect. Cleanup can proceed with closed global admission gates
and never restores them. Original owner replacement stays a separate recovery
case. The natural successful-run journal does not invent a removal/stop operation
or rewrite an existing close cause.

Keep this integration inside the fixed inert enforcing test until general
holder/revocation/session policy is implemented. Root rechecks the original
broker after the supervisor/end observations, immediately before its mutation.
The fixed unit excludes restart triggers, delegation, FD stores and exports;
arbitrary app or external-holder drain is not inferred. The User P0 boundary
requires no grant/access rows. No public API, library family or normal installed
catalogue upgrade is added. UI/unsaved-work/stop interaction remains a user choice.

## Retain original boot-bound kernel process identity for recovery — 31 October 2026

Extend the existing process module with a 32-byte AETHPID1 record: original boot
plus non-recycled 64-bit pidfs inode. Capture only after actual bus ProcessFD and
independently admitted current-context checks, repeat those checks afterward and
reject unsupported kernel evidence. A replacement observer may compare a fresh
authenticated connection against the independently protected original identity
and context. It cannot use candidate bytes/PID/name to create its own authority.

Use direct fstat on the accepted x86-64 pidfs kernel after the mandatory
PIDFD_GET_INFO checks. The reviewed libsystemd helper performs extra self-pidfd,
filesystem and file-handle probes; its filesystem getattr is denied in the actual
User domain. A separate actual confined read probe proves direct fstat works.
Do not broaden policy for that optional helper path. No third-party code/family,
frozen schema/wire change or new resident service is needed.

Keep durable run/context enrollment, replacement-owner journal settlement,
already-dead process recovery and lost original cgroup/event/unit handles as
separate obligations. Identity survives ordinary exec and is never target-image
or readiness proof. UI/unsaved-work choices remain deferred to the user.

## Persist original process evidence before fresh execution transitions — 31 October 2026

Add one immutable execution_processes table per owner through explicit v4
additions after frozen v3. Preserve all published v1/v2/v3 definitions and existing
rows/gates; normal creation stays v2 and opening never migrates. The table belongs
to the existing owner lock/database, not another service or file hierarchy.

Root stores the actual starter before ARM, then its actual app under the same
boot-bound kernel identity/PID/invocation/cgroup. User stores its registered app
before READY. Each record binds the complete original execution profile, current
process context, original journal evidence and canonical AETHPRC1 bytes/hash.
V4 fresh ARM, Root target observation and User READY require the corresponding
validated record. Keep metadata evidence distinct from actual kernel/image/data
admission; caller-supplied packets do not authorize their own enrollment.

Enrollment requires the original owner runtime. Exact retries preserve bytes/time;
changed records conflict. A same-authority/account/boot maintenance getter may
cross a runtime replacement, returning original history without state adoption,
readiness restoration, another effect or lease release. Keep actual recovery
transactions and already-dead/lost-handle proof as separate work. Prevent pruning
until run/lease settlement, then prune evidence before referenced launch/start
records. Retention/removal integration remains required.

Use the existing SHA-256, SQLite owner boundary and native C17 modules; no public
wire, dependency family, daemon, automatic installed migration or UI choice is
needed. A legacy pending start without this evidence is not silently backfilled
or accepted merely because its schema was upgraded.

## Retain original observation handles through the existing supervisor — 31 October 2026

Use the existing PID1 descriptor store as the bounded transport for a component
restart test, before adding a resident custodian. Export exactly original pidfd,
cgroup anchor/group and event reader, with independently protected original
expectations. Never export an error-channel writer or app code/data views. Keep
the native checkpoint distinct from canonical persistence and recovery authority.

Permit explicit restored observation through the exact retained root mount when
the new service has a fresh PrivateMounts namespace. Preserve ordinary capture's
strict named-mount quarantine and recheck original host PID/cgroup namespace
bindings on restored observations. Current host admission remains the caller's
protected obligation; no PID1 ptrace permission is added to inspect its links.

The test alone enables a four-entry store, preserves it across service exit and
disables automatic error/hangup retirement. Actual delivery/object checks, not
names or a processing barrier, establish the tested set. Remove stored entries
explicitly after successful test observation. Preserve uncertain state. Normal
Root/app policy and installed inputs remain unchanged; production handoff
journaling, original unit retention and Root/User settlement need separate work.

## Journal the original app-observation handoff in Root v5 — 31 October 2026

Add a single Root-only execution_handoffs table linked to the existing immutable
v4 app witness. Keep User on v4, normal create on v2, opening free of migration
and all published v1/v2/v3/v4 sources unchanged. The explicit v4-to-v5 extension
retains the owner lock/DB object and changes no existing rows, gates or revisions.
No new database, daemon, dependency family or public wire is needed.

Use canonical AETHHND1 for original process/cgroup/event/namespace and custodian
observations. Persist the full original frame/hash and profile/process bindings
before transport. Preserve full-width identifiers and signed observed birth time;
native padding is never a record format. Custodian and app invocations are
distinct from Aetheri runtime incarnations and never admit themselves.

Keep intent, actual delivered-set observation, retirement intent and actual
absence acknowledgement distinct. Only confirmed fresh intent commits permit a
transfer/retirement effect. Existing-intent retries preserve history with no fresh
effect flags. Replacement runtimes can record the last actual observation while
the origin stays immutable and the old run/lease remains quarantined/held.
Retirement requires separate native terminal/released closure and actual holder/
PID1 evidence; never infer them from a barrier, stored name, history or reboot.

The real administrative component uses the canonical frame with synthetic test
digests and UID0. Native Root P0 matching still requires its actual UID1000 app
profile. Keep installed confined producer/receiver admission, exact partial-set
reconciliation, continuous original unit retention, early-starter/error-channel
recovery and Root/User settlement as remaining integration work.

## Test process-free original-unit retention before installing it — 31 October 2026

Use a separately provisioned protected target with explicit ordering references
to the original custodian/app for the next component proof. It retains PID1's
original unit metadata without a new resident custodian or activation dependency.
Keep the original controller-pin variant as a separate regression. The new variant
must use zero controller RefUnit calls, discard the old host-bus connection and
reap the original services' clients before inspecting the same original evidence.

Bind the exact created fragment/inode and nonzero target invocation, recheck
loaded dependencies/job/reload state, and never retry an uncertain start. Preserve
uncertain inputs. After actual restoration/end observation and explicit store
retirement, stop the target, remove only its exact fragment and require all three
units to be collected. A target stop alone is never drain or lease release.

This administrative enforcing-carrier test now passes; it does not authorize a
normal Root store, production configuration mutation or recovery settlement.
Integrate protected provisioning/reload exclusion, interrupted effects and the
v5 handoff journal separately. Runtime code, SQL/wire contracts and policy stay
unchanged. Personal UI/product decisions remain deferred.

## Join original observations to v5 before transfer — 31 October 2026

Use one private Root bridge in the existing execution-service library. Bind its
admitted current self context/runtime, compare actual lifetime export to the
immutable enrolled app witness and commit PREPARE before returning transfer FDs.
Only a confirmed fresh intent returns handles; close temporary duplicates on
errors/lost acknowledgement. Existing intent never authorizes another transfer.

On replacement, load original expectations from protected SQL before examining
delivered objects. Require a new invocation of the same custodian, actual object
restoration, repeated current actor/runtime checks and confirmed OBSERVE before
publishing an observer. Preserve caller-owned incoming FDs and error outputs.
Keep exact supervisor-inventory/transport admission outside this native bridge;
do not pretend a four-element array proves supervisor acceptance.

First integrate actual PREPARE into the confined fixed-run fixture, with no
transport enabled, then wire a separate replacement receiver. Root alone uses
explicit v5 there; User remains v4 and normal provisioning v2. Require an active
custodian after readiness; do not weaken process proof for startup timing.
Namespace identity needs only a temporary Root nsfs getattr rule, supported by
an actual enforcing denial. Do not add ptrace, namespace entry/open or capabilities.

Keep failed live-test native state intact even when its independently inspected
dead test infrastructure is retired. Quarantine is not lease release or successful
recovery. The unsubmitted successful handoff may retire only after native run
release, local-copy closure/fencing and actual zero-capacity/empty store checks.

## Exercise confined PID1 transport before active-run recovery — 31 October 2026

Add SEND/RECEIVE/RETIRE to the same native bridge and existing Root process.
The P0 store has exactly four entries, main-only notification and preservation
across exit. Derive routing labels from the original profile; admit kernel
objects against protected SQL, never against their supplied names. Unknown
effects preserve intent; partial retirement cannot automatically resend removal.
Completed retry is historical and cannot act on later store contents.

Use a protected process-free target to retain Root/app unit history. Start it
before the services and verify it activates neither. Select original versus
replacement startup from admitted catalogue history before checking incoming
inventory. A replacement uses a fresh runtime, closes gates and exposes only its
test observation/retirement endpoint. Keep normal/public launch inputs disabled.

For the first confined integration, complete the run with its original owners
before replacing Root. This isolates actual handle transport/restoration and
retirement; recovery of an active run still needs separate Root/User transactions.
Keep original start receipts/runtime distinct from the replacement's observation.
Do not recreate app/operation IDs or settle a lease from store/descriptor closure.

PID1 needs descriptor use and the barrier pipe's write permission, without a
Root error-reader read grant. Root needs pidfs filesystem getattr for mandatory
retained-pidfd type observation. Review/test these narrow temporary rules; do
not add capabilities, ptrace or namespace entry as a shortcut. A failed receiver
with a complete retained set may revalidate the same intent in a later admitted
invocation; this never authorizes another SEND.

## Separate close-only Root recovery from original-runtime APIs — 31 October 2026

Keep original v3 close/get APIs strict. Rewriting AETHEXE1 to the current runtime
would destroy its original identity/digest and receipt joins. Broadly accepting
any current owner would omit the new observation binding. Instead add an explicit
Root v6 path, using the existing v3 receipt/closure chain with an immutable current
recovery-owner row. Its entry requires current v5 OBSERVE of the exact original
objects and complete original successful-start evidence. Original User identity
and same boot remain mandatory; replaced-User/early-start recovery is separate.

BEGIN journals the new owner and closure atomically, retaining an uncertain old
closure and its immutable receipt when present. RELEASE still requires fresh
original end, holder/future-effect fences and authenticated original User quiet.
A further replacement must OBSERVE and BEGIN anew. No new execution effect,
capability, resident daemon, dependency or gate reopening follows from these
transactions. Prove native faults first, then wire a separate confined active-run
fixture; do not relabel the completed-run restart test as that proof.

## Preserve the interrupted active run and separate User recovery — 31 October 2026

Use an explicit active-test mode with After ordering but no BindsTo from App/User
to Root. The normal fixture keeps its existing dependencies. Before transfer,
original Root captures its authenticated User's actual ProcessFD identity/context
in one create-only, flushed protected record bound to the original profile and
Root invocation. Independently check that record against the controller's original
User pin. A later current unit/domain or claimed runtime cannot replace it.

Keep current target-image verification in the same-UID reidentified User, matching
the original split of responsibilities. Root rechecks protected package sources
and the original app process/lifetime. Do not add ptrace or capabilities so Root
can read a different UID's proc/exe. The first combined check failed before the
later cross-owner sequence; its correction does not retroactively accept it.

After the original User expires, preserve the actual pending transfer/lease and
all retained observations. Build new-User recovery next; do not weaken the v6
original-User rule, pretend the dead User's UUID is live, clear the store or create
fresh app/operation IDs for a green test. The current failed image/unit inputs
stay independently protected until a reviewed same-intent continuation exists.
