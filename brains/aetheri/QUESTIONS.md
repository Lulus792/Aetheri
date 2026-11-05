# Open questions

The main language/library/ownership policy is agreed. Do not ask the original
foundation questions again. Remaining decisions belong to architecture or concrete
product behavior.

UI foundation: the user permits suitable available choices such as SDL3 now,
with later NexaUI migration and no global toolkit-completion blocker. The current
review selects SDL3 as the preferred early app platform direction, while retaining
per-app NexaUI adoption when justified. Exact SDL/text releases and the first
native development view are now tested in [Settings](../../spec/settings.md).
Complete widget/accessibility, normal session and physical-driver acceptance
remain engineering work; no additional permission question is needed.
The delivered [NexaUI catalogue](D:/NexaUI/NEXT_AETHERI_UI.md) maps all 103 current
UI requirements, beginning with a Wayland-only consumer profile and native
Settings view. The initial list and chat handoff are complete; ongoing checks
update the existing catalogue. Another chat message requires a new explicit user
request, including after context compaction. Exact candidate
adoption follows the relevant app scope; the whole catalogue need not be finished
before Aetheri can have working UI.
Independent private Wayland compositor/UI bring-up can progress while old-boot
recovery provenance is reviewed. It must not redispatch/replace the quarantined
app attempt or claim lifecycle/hardware acceptance. Final product designs remain
deferred for user review; dependency/renderer/input plumbing is engineering work.
The private wlroots 0.20.2/Pixman prototype now passes native input/focus, nested
presentation and clipboard transport checks, with a working development Settings
view. Normal app/session and accessible UI integration remain engineering work;
physical/trusted-session acceptance remains open. The separate optional
[DRM session](../../spec/session.md) now exercises the carrier's actual connector
and PS/2 input with logind. A reusable development controller/native console now
exists in the [development guide](../../spec/development-session.md). Host-window
input acceptance, normal admitted session and lock/resume remain engineering work.
The [SDL3 desktop](../../spec/desktop-ui.md) now adds native dock/search, current
window selection and fixed Settings launch with real carrier supervision. Final
visual design, complete editing/IME, assistive semantics and physical acceptance
remain open; no personal design approval is inferred from this prototype.
Both the coordinate and private
clipboard integration patches are pinned. See [current scope](../../spec/graphics.md).

## Browser

B1. Which everyday sites define acceptance? Are browser extensions, DRM streaming,
WebRTC calls and password management required? Existing engine accepted; CEF versus
a WebKit integration and prebuilt-versus-source delivery still need a measured comparison.
No browser engine can promise universal website or ad-blocking compatibility.

## Shell, editor and sessions

S1. Design the original shell syntax with concrete examples before implementation:
commands, paths/quoting, pipes, values/variables, errors, scripts and background jobs.
Decide whether compatibility with existing shell scripts is needed at all.
An experimental profile0 now supplies concrete [grammar/examples](../../spec/shell.md)
and a tested command runner with explicit `-i`/ephemeral job control. Its spelling is a reversible engineering
recommendation, not final user approval of the language. POSIX compatibility,
typed values/functions and final interactive job/session behavior remain open.
The [own single-line editor](../../spec/line-editor.md) now supplies grapheme
editing, bounded in-memory history/undo and literal command/path Tab completion.
Bounded history search is now implemented with reversible preview and two-stage
acceptance/submission. Final bindings, advanced completion/richer search,
the full modal editor app, panes and durable session/history policies remain work; its
provisional controls do not settle those product questions.
The [own file-editor core](../../spec/editor.md) has tested document/Normal/Insert
models plus a tested local file adapter with observed-conflict and unknown-save
fences. Interactive conflict/reconciliation and unsaved-close handling, brokered
access, document undo and actual view/key integration remain engineering work before user UI
review; no running editor app or native document-grant transport is implied.
The prototype refuses exit/EOF with open jobs; hangup waits for original direct
children without forced killing. Final window/logout/reboot persistence and richer
editor/multiplexer behavior remain product decisions, not implied by these tests.

S2. Define the editor/plugin capability model and terminal pane/session interaction.
The [first graphical terminal](../../spec/terminal.md) now has native PTY/grid/
input/job evidence and actual fixed desktop launch/reuse/close/reopen. The optional
[built-in Compose path](../../spec/compose.md) now has native and virtual DRM
keyboard-to-shell evidence. Next engineering work includes external IME routing,
full text editing and the app-owned editor/pane/session
model, then normal admitted session/app integration. It does not
decide persistence by merely closing a window. A first Release llvmpipe sample
is about 163 MiB RSS at 200% text; profile app/driver/cache/thread contributions
and startup/peak/long-run behavior before choosing memory/performance changes.
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

Experiment environments: the full Aetheri desktop is the first priority, changes
persist until explicit reset, and personal files/network/devices require separate
permission (user confirmation, 4 November 2026). No remaining product question
blocks controller/isolation work. Later choose name/UI, optional disposable
sessions, detailed AI workflow and supported imported guests/devices. Exact
backend/resource limits and enforcement need engineering review and measurements.
See [experiment environments](knowledge/experiment-environments.md).

Current implementation priority: reconcile retained attempt
0f04cd9ae92d4500a42177e3e14de76b after both original Root/User were lost. Actual
active Root replacement/handle OBSERVE worked before a direct cross-UID image
check failed; the original User later expired. On 4 November the carrier was
found stopped; the new boot has lost the four volatile PID1 handles and /run
target. Persistent catalogues and original User expectation remain protected.
Separate new-User admission/quiescence and cross-boot/lost-handle settlement are
required; same-boot pairing cannot settle that quarantined attempt. This is
engineering work, not a pending personal product decision.
The read-only native boot observer and administrative protected-metadata/store
inspection now pass in Release and ASan/UBSan. Different boot metadata alone
cannot close the intent. Separate v8 native interruption bindings/stages now pass
controlled-evidence SQLite tests, including further actor/boot replacements.
Next implement actual same-machine/catalogue/volume and snapshot/clone provenance,
current Root/User admission and all-holder/future-effect fencing. Preserve successful
start receipts and old evidence; do not fabricate original live handles or User
quiet receipts. See [boot boundary](../../spec/runtime.md#read-only-current-boot-observation).
The [v8 contract](../../spec/runtime.md#cross-boot-interruption-transactions) does
not itself settle the retained fixture. Early/unfinished launch interruption,
general exported/delegated grant/holder settlement and coordinated pruning remain
separate engineering gates, with no new personal product question required.
Strict live main-process admission now works in the normal Root/User endpoints,
including actual replacement/restart checks. This is a different authority realm
from the quarantined test. Bind protected current image/unit/epoch selection and
reviewed continuity into recovery before any v8 mutation of its old catalogues.
The retained host UUID/PID/command files lack an original independent disk-object
witness; engineering review must address that legacy provenance gap explicitly.
Do not silently synthesize it from the current candidate's matching metadata.
See [active test](../../spec/runtime.md#interrupted-active-root-test).

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
protected AETHHND1 bytes and a Root-only v5 handoff journal now exist. Connecting
their exact-set validation to an installed confined producer/receiver, reconciling
partial/unknown transfers, retaining the original unit and settling Root/User
journals remain necessary. Early starter/ARM/error-channel recovery also remains.
No normal Root FD store is enabled; the fixed app's FD store remains forbidden.
See [retained observation handles](../../spec/runtime.md#retained-observation-handles-across-a-component-service-restart).

Original-unit retention now has a separate actual component test: a protected
process-free target preserves custodian/app metadata after original process/client
exit and the controller's original host-bus connection closes. That controller
makes zero RefUnit calls. After actual handle restoration/end and empty-store
verification, target retirement permits all three units to be collected, without
restarting the app. The default handoff variant still uses controller pins.
This proves the mechanism on the trusted carrier, not production retention
admission. Protected provisioning/reload exclusion, interrupted target creation/
retirement and its integration with v5 exact-set reconciliation remain required.
See [unit retention](../../spec/runtime.md#original-unit-retention-without-controller-pins).

The native authority/handoff bridge now joins real export to the original v5
intent and restores from protected SQL expectations before OBSERVE. Its actual
confined fixture exercises UID1000 PREPARE and unsubmitted-handoff retirement;
replacement/commit/ownership cases use controlled kernel adapters with real SQL.
The separate --handoff variant now proves actual confined transfer/replacement
and retirement after original run closure. Active-run Root/User recovery,
production admission and partial-set reconciliation remain required. Preserve the namespace-
denied test's original held history in guest quarantine; it was not settled by
infrastructure retirement. No personal product decision blocks this work. Native Root-only v6 now permits
close-only recovery with a current handoff OBSERVE and the still-original User;
actual active-run wiring, replaced-User settlement and early-start recovery remain.
See [Root recovery transactions](../../spec/runtime.md#root-replacement-closure-transactions).

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
