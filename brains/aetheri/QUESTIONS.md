# Open questions

The main language/library/ownership policy is agreed. Do not ask the original
foundation questions again. Remaining decisions belong to architecture or concrete
product behavior.

## Browser

B1. Which everyday sites define acceptance? Are browser extensions, DRM streaming,
WebRTC calls and password management required? Existing engine accepted; CEF versus
a WebKit integration and prebuilt-versus-source delivery still need a measured comparison.
No browser engine can promise universal website or ad-blocking compatibility.

## Shell, editor and sessions

S1. Design the original shell syntax with concrete examples before implementation:
commands, paths/quoting, pipes, values/variables, errors, scripts and background jobs.
Decide whether compatibility with existing shell scripts is needed at all.

S2. Define the editor/plugin capability model and terminal pane/session interaction.
Need exact Vim/Neovim compatibility only if subsequently requested.
Should terminal sessions survive closing every window, logout or reboot? These
are different requirements and affect supervision/persistence design.

## Hardware and daily use

H1. The main PC is now confirmed as the development host: Xeon W-2123, nominal
32 GB RAM, RTX 2080 Ti. Windows reports UG25F (HDMI) and two HP E223i (DisplayPort).
Exact per-monitor modes/refresh/scaling and physical cable/adapter paths still need
collection. A future AM5 upgrade must also work; exact CPU/board/GPU remain unknown.
Keep baseline CPU portability and later separate hardware acceptance profiles.

H2. Suspend/resume is confirmed and no longer an open product choice. Select and
validate sleep modes, lid/idle policy, locking and device recovery on each hardware
profile. Hibernate remains separate. Confirm microphone/special peripheral features later.

## Architecture and security

Current implemented boundary: the explicit successful fixed run now closes through
separate v3 owner journals, an authenticated receipt chain and actual Linux end
evidence. Normal installed catalogues remain v2. Real owner-restart recovery,
early failed starts, general exported/delegated holders, document access/revocation
and full removal remain implementation gates. No product choice blocks those
backend tasks. See current [State](STATE.md) and
[closure contract](../../spec/runtime.md#separate-durable-run-closure).
Original live process reidentification has actual observer-exit/reconnect evidence.
The confined fixture now persists that identity with the original profile/context
in immutable v4 owner records. A replacement runtime can read those records;
actual Root/User restart reconciliation and lost original unit/handle recovery
remain separate gates. The component handoff below can observe an already-dead
original only when all its originally captured handles were retained.

The private lifetime module now exports/restores four exact original observation
handles. An actual capability-empty component service exits, then receives the
retained handles from PID1 in a new private mount namespace after its app/group
have disappeared. It proves original end and explicitly empties the bounded
store. No extra resident custodian is needed for that tested transport. A
processing barrier alone still does not prove exact-set acceptance. Canonical
protected handoff enrollment, partial/unknown transfer reconciliation, continuous
original unit retention and confined Root/User journal recovery remain necessary.
No normal Root FD store is enabled; the fixed app's FD store remains forbidden.
See [retained observation handles](../../spec/runtime.md#retained-observation-handles-across-a-component-service-restart).

Native SHA-256 is no longer an open library choice: reviewed OpenSSL 3.5.9 is
installed and native byte/held-FD verification is tested. Native payload staging
now checks inventory/copied bytes and resyncs. Protected P0 administrative staging
and its native journal are exercised across a guest boot. General dynamic
allocation, publisher trust and recovery coordination remain work. Journaled
store publication and first fixture activation now exist; actual app admission,
updates and launch remain separate gates.
Authenticated system-to-user installation projection now exists for the protected
fixture. The persisted binding is only an observation. Explicit first data
enrollment now binds a distinct protected allocation, DataEpoch and attachment
with a closed gate. It preserves the unrelated retained storage fixture.
Private native code reservation/status and durable user acceptance/pre-dispatch
withdrawal now survive broker replacement. Native FD-based private code/data mount
views now have real Linux tests and an installed no-exec supervised setup path.
Setup has separate journal/peer/invocation/drain evidence, including narrowly
checked early-failure recovery. Separate native privilege dropping, exec and exact
current-image observation now pass isolated and temporary enforcing tests. Their
durable target-dispatch integration and actual app readiness remain next; setup
READY is not app readiness. The no-exec release proof cannot cover executing code.
The retained trusted fixture's declared action is run; older journal-only probes
used synthetic main. New executable starts must resolve the actual manifest action
and preserve old immutable tuples instead of interpreting main as run. The retained
binary's c31,c32 expectation differs from enrolled data's c33,c34. A separate
aetheri.p0.exec fixture is now published/activated with a closed gate; the old
package/tuples remain unchanged. Exact manifest-action resolution now exists as
an internal metadata helper. Its own user binding and private data/allocation now
exist with closed gates. The fixed exec data profile uses c35,c36, preserving
original c33,c34; no dynamic category allocator is implied. Separate native
execution-profile/fence and durable request/binding/preparation/arm transactions
now exist, including explicit no-replayed-effect receipts. Next connect them to
the live executing starter, process/image registration, readiness and drain.
The issuer-owned error channel and actual sd-bus writer reply now have native
evidence; current Root reply/profile/runtime authentication and binding to the
live journal/starter remain work. These are implementation gates, not requests to reopen the accepted
architecture.
Native journal/pin/channel coordination and durable diagnostic recording now
have component tests with controlled process/PID1 adapters. Complete the live
wire/provisioning/supervisor adapter and executing starter, then target readiness
and independent drain. No user product decision blocks these implementation gates.
Protected source opening now has actual kernel/file-context tests and a read-only
existing-installation observation. Positive starter image/domain admission and
live data/launch mediation remain integration work.
The native consumer now verifies Root replies and original supervisor context,
including actual confined-service tests. Complete protected Root unit/profile
provisioning, live producer/supervisor wiring and the actual executing starter;
the synthetic reply fixture does not establish producer admission/readiness.
The native journal-backed producer component now has isolated owner/bus tests,
including deferred release and deadlines. Controlled actor/PID1 adapters do not
establish actual starter policy or live current-data/broker admission.
A separate starter now joins the authenticated client/private views/one-way exec.
The temporary scaffold has its own profile-bound AETHVWS1 witnesses; publishing
these from independently admitted/flushed Root setup and integrating actual
starter-domain/Root unit supervision remain implementation gates. No extra user
product decision is needed for these fixed P0 internal choices.
The subsequent isolated live test now connects actual Root journal/PID1 dispatch,
starter/profile/self-image admission, private views and target exec. Fresh target
credentials and running ELF are observed independently; exact test process/unit/
cgroup/job drain retires only that fixture and archives its unresolved lease.
This supersedes the earlier missing-integration observations for the explicit
controlled fixture only. Normal Root/User provisioning, current broker/data
admission, dynamic scaffold/category allocation, actual app registration/readiness
and production executing recovery/lease settlement remain next. These are
implementation gates, not user product questions. No normal launch capability
has been enabled; UI/browser/shell choices above remain deferred to the user.
User-side native execution preparation now persists intent before the first Root
executing request. Fresh-only submission, exact profile binding and recovery/
withdrawal guards are tested; these are not current RPC or filesystem admission.
Next connect the actual registered broker, User private-data checks and native
Root dispatch, retaining independent owner catalogues and existing closed normal
inputs until that path has enforcing evidence. No extra user choice is required.
That fixed-fixture owner connection now has actual enforcing evidence: real User
catalogue/process/registration, private-data checks and preparation feed Root
request/arm; changed unit/connection and same-UID app impersonation are rejected.
Normal session/owner provisioning, mutation/revocation fencing and production
registration/readiness/recovery remain work. The isolated fixture now
proves actual User-mediated app registration and explicit readiness, including
the live current-image check and original-connection requirement. Root observation
settlement is now implemented for the isolated fixture: Root joins its original
starter/app context to original User confirmation and persists observed without
releasing the lease. Separate immutable Root launch-result receipts and exact User
acknowledgement now complete the start operations, while run/lease remain active.
Independently proven executing drain/lease recovery remains next.
No personal product choice blocks
the next backend integration; UI/browser/shell decisions remain with the user.
General dynamic allocation/recovery and binding reconciliation after
install changes remain pending; the P0 infrastructure is preallocated by root.
General distribution signing and key rotation still require release contracts.

A1. Architecture is approved: B, wlroots, selected systemd components, SQLite and
versioned base/managed-app separation. Do not reopen these settled choices.
Remaining work: exact versions/build options and compatibility, sandbox/update/boot
integration, browser embedding, filesystem implementation and plugin interfaces.
The [comparison](knowledge/architecture-options.md) preserves rationale and alternatives.
The [module/interface plan](knowledge/modules-and-interfaces.md) is now accepted
in direction. The initial concrete management contract is accepted, not installed.
The [identity/lifecycle model](knowledge/identity-and-lifecycle.md) is accepted.
The [schema/API contract](knowledge/schema-and-api.md), including SQL/XML and
starting error/deadline/retention rules, is now accepted.
The [filesystem/permissions direction](knowledge/filesystem-and-permissions.md)
is accepted, including ext4/LUKS2 and SELinux plus namespaces.
The [launch/access protocol](knowledge/launch-access-and-removal.md) is accepted.
The [minimal extension/P0 scope](knowledge/first-linux-prototype.md) is now accepted.
Its five new table designs, additive P0 interfaces and development migration/tests
are implemented in [versioned contracts](../../spec/README.md).
VM/tool setup now has actual guest evidence and pinned build tools. Next: the
C17 app/run authorization/launcher, remaining policy and transport. The two confined
protocol endpoints now run, with discovery only. Saved/direct/write/bulk document access remains
in the accepted longer-term protocol, outside this first implementation slice.
The bounded P0 fixture/owner/intent encodings now have a
[reference and frozen vectors](../../spec/runtime.md). This does not finalize
general signed packages or implement registered-volume identity. C17 owner codec
and bounded reads now have native tests. The native catalogue store also creates/
validates v2 and commits startup context; it receives trusted identity/boot values
from its caller and does not implement the registry, kernel-context sourcing or
native v1 migration. Current peer/process observations now exist, but binding them
to app/run identity remains pending. Protected P0 broker enrollment now works for
the fixed eligible test account, now backed by a durable system scope/UID row;
general account eligibility and complete principal mapping,
policy/registry identities, target-exec evidence and actual cross-authority
admission/recovery remain Linux integration gates. Isolated host tests do not
settle these gates.

A2. Explain the proposed daily-use gates: encryption protects data on storage,
sandboxing restricts app access, backups recover lost/corrupt data, and trusted boot
checks early software. Secure Boot may be included, but no keys/settings are changed.
Design separate VM/test-data and personal-data acceptance.
Encrypted personal-use storage is accepted in direction; boot/unlock,
recovery keys, policy maintenance, label/category allocation and durable allocation/
registered-volume identity still require implementation contracts. Exact owner
bytes and runtime private-root reference checks now exist; they do not settle
persistent enrollment, clone detection or public document access.

A3. Define full removal for multiple users, shared dependencies, rollback snapshots,
exported documents and externally stored data. Draft in app-lifecycle.md is a
proposal to satisfy the requirement, not an implemented guarantee.

## Resolved foundation round

| Item | Current answer |
| --- | --- |
| T1 | C17 primary, small adapters, targeted Rust allowed |
| T2 | Existing open development tools accepted |
| T3 | glibc |
| T4 | Proposed technical-library boundary accepted; exact integrations pending |
| T5 | Existing service backends allowed; no forced replacement |
| A1 | Custom lightweight ordinary browser, no feeds/promotional clutter, content filtering |
| A2 | Original editor design/plugins inspired by modal editors |
| A3 | Own shell and language immediately |
| A4 | NexaCloud Desktop confirmed |
| H1 | Up to three 1080p/1440p/4K monitors, wired/Bluetooth input, jack/Bluetooth headphones |
| H2 | F: allocated and now ready; protect ongoing Physim/NexaCloud migrations |
| H3 | Security goals accepted in principle; Secure Boot allowed if useful, explain details |
| M1 | Explicit signed update installation and recoverable previous version |
| M2 | MIT |
