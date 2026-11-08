# Working on Aetheri

Latest USB desktop boundary: read `spec/usb-desktop.md` and the 2026-11-08 USB
desktop journal/STATE. Full-size single-medium two-boot/persistence and independent
ext4 checks pass. USB2's OBS2 binds the complete root/data profile before handover;
real-root data checks require the original boot/device and same current parent.
USB1 stays fixed. Current bochs/serial evidence is not laptop acceptance. Actual
wrong-parent testing is still in progress here; retain incomplete/failed fixtures
and require unchanged bytes on both media. Native image inputs are fenced; two
host-only SGR parser/test updates were separately reviewed. Sparse regular-image
helpers are never physical writers; flash must write and verify zeros too. Next
finish negatives, add laptop DRM/input/firmware and test without serial before the
already authorized Intenso G: flash. Reidentify, do not ask again, and continue
without waiting for the later user test. No flash yet; NexaUI handoff stays closed.

Root v9 now adds immutable unit_allocations: native prepare precedes filesystem
effects; only fresh confirmed prepare permits one O_TMPFILE/create-only-link
primary fragment. Recipe AND original directory/parent placement bind before
creation. Unknown commit/link/flush preserves pending intent and possible files,
never auto-recreates/adopts/cleans up. Complete selection is witnessed once before
REQUEST/ARM. The Root adapter requires that original witnessed selection/placement.
Read spec/unit-profiles.md; run catalogue.units, authority.unit_custody and the
cross-language/contract tests. Explicit quiesced upgrade_units is Root v8->v9 only;
ordinary create stays v2 and User v9 is unsupported. Preserve frozen v1-v8 SQL/XML.
Recovery inventory includes preparing AND witnessed unit obligations; no custody
retirement/purge or general volume/clone/owner-replacement admission exists.
The old quarantined executing fixture and normal installed v2 inputs stay closed.

authority/unit.h joins fresh native execution REQUEST to exclusive unit
custody. Read spec/runtime.md's Root request section. Constructor moves only an
unused unit against independent source/graph/service and starter-ELF witnesses;
read-only REQUEST-state queries never grant replay. Keep the Root catalogue hold
through Subscribe/Ref/Start, completed jobs and unknown releases. Original SYSTEM
cleanup context plus independently admitted original end/holders governs retire;
source fencing does not require renewed broker start delegation. Abandon drops
only local observation/hold. Keep its bus separate from the coordinator's bus,
and the adapter alive through callbacks. Run explicit authority.unit_custody in
the enforcing carrier. This is inert UID0 administrative/native-journal evidence,
not a normal installed registry/provisioner, production end/volume/recovery admission,
normal launch or lease settlement. Preserve the old execution quarantine.

Local Files creation now uses files/create.h: an immutable literal name/held
parent, preparation without effects, one worker attempt and cached retry results.
Existing entries are conflicts. Unknown mkdir/save/flush acknowledgements retain
possible objects/stages and block new creation for that window; close only frees
memory/FDs. No automatic retry/rollback/cleanup or durable restart reconciliation.
Read spec/files.md and run files.create/files.native. The alternate test_files_fault
binary is test-only; never install its lost-mkdir-acknowledgement hook.
Files rename.h supplies same-folder rename and explicit cross-folder move with
independent selected source/destination parents, O_PATH source and immutable names.
Directory moves reject observed self/descendant targets through bounded repeated
ancestry walks and flush their own original metadata FD plus both parents. Kernel
RENAME_NOREPLACE protects the destination, not source-inode compare-and-swap.
Recheck source/parent, retain unknown outcomes, never auto-retry/reverse/clean up.
Preview invalidation requires cancel/refresh; own document end precedes admission.
Unknown creation/rename blocks both; unknown rename also blocks new document starts.
The separate test_files_fault now includes a lost-rename-reply hook as well.
Window-local Cut/Paste holds an independent selection; clearing it never settles
an unknown move. Menus own their press/release through close. No cross-filesystem
copy/delete fallback exists. Read spec/files.md; run files.rename/files.move/files.native.
No durable restart recovery,
hostile-writer exclusion, normal grant or managed removal follows.

For software development, first read `D:/Skills/skills/AGENTS.md`, select relevant
specialist skills and read their required entry points and knowledge.
Start at `brains/aetheri/START.md`, then PROJECT, STATE, DECISIONS and QUESTIONS.

- NexaUI coordination: the requirement catalogue and initial chat message are
  delivered. Maintain `D:/NexaUI/NEXT_AETHERI_UI.md` in place when new requirements
  or evidence warrant a change; otherwise leave it unchanged. Do not generate or
  repeat a chat/start message or ask the user to paste one unless newly requested.
  After compaction, carry forward **handoff delivered; catalogue maintenance only;
  no pending chat message** and resume the actual task in STATE. Historical handoff
  records are not new assignments. See brains/aetheri/knowledge/nexaui.md.
  Treat routine NexaUI checks as background catalogue maintenance, not a recurring
  deliverable or next step for the user. With no relevant change, omit a NexaUI
  update from the reply. With a relevant change, a brief delta and catalogue link
  suffice; do not repeat the catalogue overview or implementation request.
- Files is an opt-in ordinary-user development view; read spec/files.md.
  Keep the C17 directory model separate from SDL, complete bounded snapshots and
  held parent navigation, raw names separate from display labels, and rejected
  filter intents unable to activate a retained prefix. A selected file's held
  O_PATH witness passes literally through a separately owned Terminal/PTY to the
  editor and is compared before reading. No shell interpolation or symlink
  following. Preserve the original direct child across pending/unknown results;
  UI close retains the editor's dirty-content choice and Files waits for Terminal
  end. These FDs/observations are not document grants, sandbox/volume identity,
  descendant drain or durable recovery. Run files.model/launch/native plus affected
  editor/terminal/shell/desktop regressions. Fixed development desktop launch now
  exists; normal app admission remains work.
- Document now owns bounded delta undo/redo: read spec/editor.md. Keep monotonic
  revisions distinct from logical saved-state identity. New successful edits
  discard redo; failed admission/allocation must preserve it. Preallocate before
  mutation; raw inverse grapheme splits are private original-history only. Reuse
  retained capacity on traversal and evict complete entries within both budgets.
  Modal/UI history never acknowledges or rearms an uncertain save. Run
  editor.history/document/modal/file/native plus provider groups. Signal exit
  now drains remaining queued managed signals without repeating effects after
  its first decision; retain the test-only fsync gate, not a production hook.
- Modal Visual/Lines selection and its bounded private register are app-owned.
  Read spec/editor.md: inclusive grapheme endpoints, original line endings, final
  line removal, linewise-change empty line and paste separator rules. Stage copy/
  assembly/document/index/history growth before mutation; failures preserve the
  old register, selection, text and redo. replace_line_start admits original line
  starts and records that result cursor in the same undo entry. Wipe replaced
  register bytes; never imply platform clipboard or file/save authority. Run
  editor.selection plus document/modal/history/view/native/provider groups.
- Literal document search is app-owned: read spec/editor.md. ae_document_find is
  read-only, bounded, case-sensitive and admits whole-grapheme endpoints only.
  Preview/cancel preserves original cursor and prior accepted query; rejected
  input cannot accept an old prefix. n/N recompute after edits, without wrapping.
  No search settles UNKNOWN or changes text/revision/clean/history/file state.
  Keep minimum-size query hints, inverse match and query focus. Run editor.search,
  view/native/provider plus affected groups; accessible text/richer search remain.
  The old 200-percent no-match image contains zerr, not zero; read the native-key
  input record before reopening it as a renderer/search defect. Use verified
  tap_keys and actual target/text/frame gates; QMP success or stable pairs alone
  do not establish intended text or visual acceptance. No timer cause is proven.
- AETHERI_EDITOR now builds the separate aetheri-editor development executable.
  Read spec/editor.md: main joins existing document/modal/file libraries to the
  pure bounded view. UNKNOWN is independent of clean state; q/CtrlQ must not
  silently close it, save-as cannot rearm it, export is a separate create-only
  copy and discard requires literal DISCARD. Preserve extra uncertain exports.
  Own TTY OFD/foreground checks, alternate screen/modes, bounded UTF-8/key/command
  rejection and no reads beyond quit are required. Below 30x8 refuse hidden
  editing/commands. Stop restores modes; background resume stops again. Emergency
  signal copies are best effort, not durable recovery. Run editor/native plus
  shell/terminal/provider groups. Normal grants, search/panes/accessibility
  and final UI remain work; preserve the quarantined runtime.
- editor/file.h now supplies a separate local development load/save adapter.
  Read spec/editor.md: repeated original byte/metadata checks are not atomic CAS
  or a production document grant. Preserve metadata and pre-rename target bytes;
  uncertain rename/flush/close fences later saves. Uncertainty is independent of
  document dirty state, including an already clean submitted document. Never
  rollback/retry or delete a potential artifact after an unknown outcome. Run
  editor.file plus document/modal/grapheme tests; the application has its own
  view/command layer. Durable brokered/restart reconciliation remains work.
- Shell history search now has its own bounded query/preview: preserve original
  draft/cursor/undo until accept; Enter accepts only, later Enter submits. No-match
  and rejected input cannot reuse an old result. Run editor/search/native groups.
  New src/editor document/modal are pure model libraries, not an editor executable
  or file/save authority. Read spec/editor.md: UTF-8/ending/gap/line/grapheme and
  allocation/revision contracts. mark_clean is only local generation state.
  The app owns its separate view/commands; full reconciliation remains work.
  Keep installed profiles/capabilities and quarantined old intent unchanged.
- The graphical terminal now selects the own `-i --edit` single-line profile.
  Read `spec/line-editor.md`: toolkit-free editor/history, original terminal/OFD
  ownership and whole-draft invalid/overflow rejection. Preserve plain `-i` and
  original child input/modes. Unicode18 generated data uses separate Unicode-3.0
  licensing; reproduce with tools/generate_graphemes.py downloads --check. Run
  text.graphemes, shell groups and relevant terminal/desktop/Compose integration.
  Bounded undo/redo and literal command/path completion now exist. Preserve undo
  grouping/branch/budget and clear/reset/history lifetimes; no undo may revive
  blocked input. Completion only proposes literal quoted tokens, with no exec,
  cwd/env mutation or content reads. Keep budgets/cancellation and real parser
  round trips; filesystem hints never authorize later operations. Run shell.completion
  plus editor/native/terminal/provider groups. No programmable completion/daemon.
  Grapheme segmentation is not shaping; advanced completion/richer search, modal editor app,
  panes and persistent sessions remain work. Preserve the quarantined old runtime.
- Optional built-in Compose is documented in `spec/compose.md`: build and runtime
  opt-ins, reviewed embedded data/license, no external input-method/virtual-keyboard
  global. Retain original field/client/focus admission, including pending enable
  invalidation across leave/re-enter. Run the compose model/input/provider/lifecycle
  checks and relevant desktop tests. This is not external/CJK IME, trusted lock or
  normal app/session admission. Keep source-data licensing separate from our MIT.
- Follow the user's agreed foundations: Linux x86-64/UEFI, C17 primary, targeted
  Rust and small C++ adapters where justified, existing open development tools,
  glibc and the discussed technical-library boundary. Exact dependency versions
  and substantial new families need evidence, rationale and compatibility/security
  review. While temporarily unavailable, the user authorizes well-supported
  recommended technical choices; document them and defer personally important
  product/UI/destructive choices. Do not repeatedly ask noncritical questions.
- Approved architecture: controlled modular system (B), original wlroots-based
  Wayland compositor, selected systemd/session/sleep components, SQLite metadata,
  versioned read-only system base and separately managed apps/tools with enforced
  ownership boundaries. Do not reopen these choices without material new evidence.
  Exact releases and remaining sandbox/update/browser integrations need review;
  architectural selection is not implementation or runtime acceptance.
- Approved module direction: privileged system authority plus per-user mediation,
  compositor/UI separation, typed local D-Bus/sd-bus control, owner-scoped SQLite
  writers and recoverable operations. The identity/lifecycle model is accepted.
  The initial SQL/XML management contracts and limits are accepted in
  brains/aetheri/knowledge/schema-and-api.md. Run tools/validate_contracts.py for
  isolated constraints; acceptance does not imply installed APIs.
- Filesystem/ownership/enforcement direction is accepted in
  brains/aetheri/knowledge/filesystem-and-permissions.md, including ext4, LUKS2 and
  SELinux plus namespaces as foundations. Exact releases, policy and runtime
  acceptance are pending. Do not create the Linux tree or change policy on Windows.
- Launch/document/revocation/removal protocol and P0 scope are accepted in
  brains/aetheri/knowledge/launch-access-and-removal.md and
  brains/aetheri/knowledge/first-linux-prototype.md.
- Versioned v2 SQL, additive P0 XML and development migration/tests are implemented;
  read spec/README.md and run tools/test_contracts.py. Keep v1 contracts unchanged.
  This contract portion does not implement Linux lifecycle or document transport. Native
  edits/bulk/direct/saved access remain later work. Dedicated carrier/tool setup now
  exists; read STATE for the exact executed boundary.
- P0 fixture/owner/intent byte references and frozen vectors are in spec/runtime.md;
  run tools/test_runtime_contracts.py. VM input pins/cache tooling are in
  spec/vm-inputs.json and tools/prepare_vm.py; run tools/test_vm_inputs.py.
  QEMU's checked Windows installer has an expired Authenticode certificate; the
  reviewed provision_windows.py reads MSI/NSIS archives as data without installer
  execution. Do not equate source hashes with valid Windows codesignatures.
  Native core/fixture now decodes canonical AETHFIX1 into borrowed bounded text
  views. Retain immutable input while using them; never treat views as C strings.
  It validates metadata, not content hashes, publisher trust or installation.
  OpenSSL 3.5 LTS/libcrypto is accepted and reviewed at Fedora 1:3.5.9-1.fc44;
  exact RPMs/hashes are in spec/vm-inputs.json. CMake/runtime reject other families
  and versions below 3.5.9. core/digest uses fixed EVP SHA-256 in a private context;
  fixture verification does not replace publisher trust, inventory or staged import.
  core/import now checks complete trusted-fixture inventories, copies/hashes private
  staged payloads and repeats required flushes. A protected system-domain P0
  administrative path now binds a preallocated carrier slot and native install
  intent/journal before payload writes; it reopens after a guest boot. Read
  spec/runtime.md's administrative stage journal and store publication. The trusted
  fixture now publishes a manifest/payload container through no-replace same-mount
  renames, immutable AETHSTO1 object evidence, full repeated flushes and final MAC
  sealing. Publication alone leaves install preparing/running-reconcile; the
  separate AETHACT1 first-activation path now atomically completes the original
  install operation, with its gate closed and no lease/run. Completed receipts
  are historical: never rearm/restore a generation, change later gates or undo
  removal on retry. Read current STATE/runtime contracts. General dynamic allocation,
  updates, caller admission and complete recovery coordination remain pending. After moving
  the payload use fixture-store-check, not the original stage/rebind checks.
  The slot ID is not a private-data StorageRef. Never adopt a candidate root from
  matching content/name; retain independent configuration/operation bindings.
- Native C17 ownership/storage primitives and tests exist under src/core and tests.
  core/root requires STATX_MNT_ID_UNIQUE and joins held/named mount/inode/filesystem
  observations. These are runtime facts, never durable volume/clone authentication.
  storage_ref derives its leaf from StorageRef and binds an independently expected
  owner/root under a trusted anchor; caller still owes account/volume/MAC admission.
  Returned file FDs are internal bounded-read obligations, not native app transport.
  Closing a storage reference never revokes an already returned FD.
  core/allocation adds immutable AETHALC1 object witnesses (168 bytes, frozen vector).
  Birth time is mandatory for these records, optional for generic root snapshots.
  Capture requires independently authorized, flushed allocation setup; never adopt
  a candidate tree from its own header. Publishing uses O_TMPFILE/create-only link/
  fsync; unknown acknowledgements preserve records. This is not volume/clone
  authentication or a completed allocation/purge journal. The optional protected
  storage fixture is now retained in the guest; empty-bootstrap cleanup must refuse
  it until a reviewed storage-retirement protocol exists. Use storage-check for
  native same-intent reconciliation, never mint replacement IDs after a lost reply.
  They are not an installed authority, public grant API or Aetheri app sandbox.
  Dedicated guest runs Fedora enforcing policy. A temporary Aetheri app-domain/MCS
  fixture probe is tested and removes its module afterward; production authority/
  broker/app domains and category allocation remain unimplemented.
  QMP must stay host-local AF_UNIX under the protected VM directory: guest slirp
  networking can reach a host loopback TCP listener through 10.0.2.2. Never restore
  unauthenticated TCP QMP. Use run_vm.py status/live process evidence before retries.
- Native src/linux/peer captures bus-authenticated connection credentials and a
  process pin. This is not complete app/run authorization or current-domain proof.
  Keep required ProcessFD/provenance checks; do not replace them with UID/PID paths.
- Native src/linux/process joins that held ProcessFD to current UID/label, an
  active system service invocation and cgroup-v2 identity. Read the process proof
  in spec/runtime.md. Trusted host system bus/proc/cgroup views are prerequisites.
  Its self-pin API is only for the calling process, never a substitute for a
  missing caller ProcessFD. Snapshots are not full app/run admission or target-exec
  evidence. The root endpoint checks its own supervised context on discovery;
  complete app registration, mutation fences and production policy still remain.
- Native src/catalogue now creates/validates v2 and commits startup context using
  build-embedded canonical SQL. Read spec/README.md#native-catalogue-boundary.
  No production registry/MAC/authority or native v1 migration is implied. Keep
  writer.lock stable, never close independent DB-inode FDs during SQLite locks,
  and never infer rollback from a lost commit/flush acknowledgement.
  System-owner account enrollment/check now uses existing scopes rows. Exact
  active retries are idempotent; never rebind/reactivate an old scope or adopt its
  private state on UID reuse. Only explicit initialization/admin enrollment may
  insert the fixed test account; normal startup and broker use require it already.
- tools/test_linux_policy.py is an explicit root-only disposable-carrier test,
  never a default host test or production installer. It must stay enforcing,
  preserve uncertain dispatches and refuse foreign fixtures/modules. Fixed test
  categories are not a production category allocator or complete A3 acceptance.
- src/authority implements two confined prototype protocol endpoints, generated
  from accepted XML. Read spec/runtime.md#confined-protocol-services. Public discovery
  and private P0 Broker1 enrollment/check now work; no lifecycle capabilities are
  advertised. Read the broker enrollment section in spec/runtime.md. Bootstrap is
  one protected UID-1000 account/unit, not general account/launch admission. Losing
  a name/process drops identity only; never infer app drain or release leases.
  tools/p0_services.py explicitly
  provisions/checks/stops the dedicated guest bootstrap. lifecycle-check removes
  and recreates only an inspected empty bootstrap, never a production app install.
  Preserve unknown/pending dispatches, processes, jobs and unexplained entries.
  Its admin update/cleanup interruption needs manual reconciliation; do not claim
  production power-loss recovery. User roles follow the login manager, no lingering.
- src/authority/broker owns the retained peer pin and narrow name/exit subscriptions.
  Registration may observe a known Type=notify service while activating; every
  use still verifies active state. Caller-supplied IDs cannot select the unit.
  broker-check proves departure FD cleanup, user replacement and root-only restart.
  Cache tests mock context but use real name-loss/pidfd events; separate enforcing
  integration rejects unconfined and same-domain/wrong-unit impersonators.
- Use English for project documentation. Conversation can remain German.
- The opt-in C17 graphical terminal is in src/terminal; read spec/terminal.md and
  run terminal.model/pty/native with the reviewed UI profile. Fork its original
  shell before SDL/Mesa/worker creation. Retain independent child/pidfd and stream
  observations; neither EOF nor a close request proves descendant/cgroup/lease
  drain. Failed event posts must not abruptly abandon ownership. TERM=dumb is
  intentional; do not claim complete VT/Unicode/editor/IME/a11y or persistence.
  Its test-only event-failure executable is not a normal app dependency. The
  carrier Mono font has explicit font/OFL hashes and limited installed-RPM
  evidence, not a new independently verified RPM/distribution package. Normal
  development owner/dock integration now has independent Settings/Terminal/Files
  slots.
  Normal admitted app/session launch remains separate. Keep app/model
  state toolkit-independent for later NexaUI migration and preserve old fixtures.
- Private Catalogue1 1.1 / Development1 1.4 support a bounded authenticated install
  observation and fixed-fixture refresh. Public capabilities remain empty. A user
  binding is not admission, storage ownership or current generation authority.
  projection-check uses a root-selected short-lived user-domain helper with actual
  process/unit/invocation/cgroup checks; clear its protected slot after use. Keep
  the retained private allocation fixture separate from future app DataEpochs.
- The first app DataEpoch/StorageRef is now enrolled through a separate protected
  P0 fixture, not adopted from the old storage fixture. Read catalogue/data.h and
  spec/runtime.md's initial data enrollment. AETHDTA1 freezes both authority epochs,
  exact binding/baseline and independent AETHALC1 object witness. data-check repeats
  filesystem verification/publication then atomic native data+attachment+closed
  gate enrollment. Retired/detached/conflicting data is never reactivated; a live
  exact retry never changes later gate state. Root setup remains preallocated test
  infrastructure, not a general allocation operation journal. Preserve both fixtures.
- Distinguish confirmed requirements, architecture proposals, implementation and
  test evidence. No runtime function exists merely because it is documented.
- Native catalogue/reservation.h and Runtime1 now reserve/query/withdraw code.
  DispatchLaunch remains unsupported and public capabilities empty. The P0 wire
  profile is only the protected original I/G/D, main/p0/none. Never treat a lease
  receipt or gate observation as data readiness or process execution. Only a
  still-reserved tuple can be atomically withdrawn; uncertain/observed/dispatching
  state requires reconciliation, never inferred drain. Development1 is now 1.4.
  Explicit system --admit-fixture rechecks the current sealed store using a
  boot/original-catalogue-baseline-bound AETHADM1 before opening reservation
  metadata. Ordinary startup stays closed; stale admission config must fail.
  reservation-check/new-check/continue/replay-check preserve protected intent and
  terminal history. These are fixed P0 tests, not normal user launch acceptance.
- Native catalogue/launch.h now journals user acceptance before root RPC, binds
  actual root receipts (Catalogue1 minor 1.1), and reconciles proven withdrawal
  after broker replacement. User startup marks unsettled runs recovery_required.
  A current same-authority/account broker may withdraw an old still-reserved
  tuple without rebinding it; root uncertainty/readiness/exec must never infer
  release. launch-check/cancel/reopen-check preserve fixed correlations/history.
  Private data gate admission is not view readiness; no actual app dispatch or
  public launch capability exists yet. Keep SQL v1/v2 and Runtime1 XML unchanged.
- Native linux/view.h now clones exact code/data FDs into private mount views.
  It is for fresh trusted single-threaded setup only, not a permanent authority.
  Entry can change namespace on later failure; exit rather than fall back. Move
  attempts are not retryable after uncertainty. Closing view/namespace handles is
  not drain or lease release. Real kernel tests run as administrative unconfined_t
  in the enforcing guest; the separate supervised no-exec setup path below now
  exercises setup_t. App policy and target readiness remain pending. Preserve the
  existing source/MAC/content admission requirements.
- src/launcher and private Setup1 1.0 connect view setup to the original Root
  journal. The fixed helper cannot execute app code. Root pins actual ProcessFD /
  unit invocation and separately observes terminal unit/cgroup/job drain before
  releasing this no-exec attempt. Read spec/runtime.md's supervised setup section.
  Root's ordinary reservation withdrawal rejects additional setup steps. Early
  failed helpers have a distinct supervisor-only proof, never a fabricated peer;
  it depends on the unchanged protected image/unit and original Root runtime/boot.
  Offline --recover-setup keeps that runtime under the exclusive catalogue lock.
  Do not reuse this proof for a target-executing starter or uncertain dispatch.
  Preserve old main tuples and the immutable run/c31,c32 fixture; actual target
  exec needs a new compatible fixture for c33,c34 data. Public capabilities remain
  empty. No production app launch or complete removal yet.
- linux/execute.h and image.h now provide isolated one-way execution and current
  image observation. They are not wired into the installed helper/authority.
  Read spec/runtime.md's execution section. EOF/exit 125 is never exec/readiness
  evidence; never extend the no-exec withdrawal proof to an executing starter.
  test_execute_policy.py installs only its separate temporary test domains and
  retires them after exact fixture and independent unit/cgroup/job drain checks.
  Unknown dispatch preserves pending state. Test-only SYS_PTRACE/KILL belong to
  the parent harness; the exec boundary accepts neither. Actual app admission,
  observation, recovery and drain integration require separate evidence.
- The separate aetheri.p0.exec fixture is now published/activated, gate closed.
  Read spec/runtime.md's separate fixture/action section. Fixed original/exec
  administrative registry selections share the existing store/catalogue but have
  separate immutable inputs, slots, identities and receipts. The controller's
  exec_fixture subtree preserves the original. Normal broker/data/no-exec paths
  still select the original; no implicit main-to-run or DataEpoch rebinding.
  core/fixture's action resolver returns owned literal argv/file metadata only.
  Private Development1 1.4 now adds RefreshExecFixtureInstall. The new fixture has
  its own projected binding/DataEpoch/allocation; both gates remain closed. Native
  data enrollment checks the exact fixed raw MAC context, UID/GID and selected
  install/publisher. Exec data uses c35,c36; original data keeps c33,c34. Never
  reinterpret an allocation-operation ID as policy identity. Read spec/runtime.md's
  scoped execution-fixture data section. Existing owner bytes/SQL stay unchanged.
  Actual executing dispatch/registration/readiness/drain remains pending.
- catalogue/execution.h now defines the distinct AETHEXE1 native execution
  profile and owner request/bind/prepare/arm journal. Read spec/runtime.md's
  durable execution section; run catalogue.execution. Fresh-only effect_required
  is zero on every retry/lost acknowledgement. Its stage/generation step cannot
  use old no-exec stage/launch withdrawal. This is not a live starter/DispatchLaunch
  implementation or image/readiness/drain proof. Do not install/dispatch it as one.
- linux/exec_channel.h now provides the process-owned bounded error reader and
  checked anonymous writer. Only the issuer keeps the reader; future live Root
  must bind it to the exact journal/held starter before exporting a writer reply.
  Incoming-FD method-call denials remain. sd-bus append duplicates, read borrows:
  close all issuer/message copies after transfer and seal the starter's writer.
  UNKNOWN EOF is never execution/readiness/drain; unregister terminal watches.
  The peer/execute tests use it, but no live Execution interface exists yet.
  Run explicit test_execute_policy.py with Release; Debug LeakSanitizer needs a
  same-domain ptrace permission deliberately absent from that test policy. Keep
  ASan/UBSan in the normal native suite; do not weaken policy for instrumentation.
- authority/execution.h now joins native journal/pin/channel/event handling in a
  Root-only coordinator. Its compiled effect adapters run after fresh commits;
  unknown request/release/bind never permits duplicate effects/writer issuance.
  Re-observe original context; observed drift stays quarantined. Sample channel
  before arm. sd-event terminal watches stop; close never frees a lease or stops
  the process. Callback-time close is EBUSY. Run authority.execution_service;
  its process/PID1/effect adapters are controlled fixtures, not live admission.
  AETHEXD1 diagnostics persist immutable step 4 + bounded reason with no outcome/
  lease change. Retry only acknowledgement. Read spec/runtime.md's coordinator
  section. This library has no installed wire/provisioning/starter policy yet;
  do not claim runtime/target readiness or use no-exec drain to settle it.
- authority/execution_profile.h now binds protected config/publication/manifest/
  ELF and private owner/allocation/context into held sources. Read spec/runtime.md's
  protected execution-profile section. The 0700 store parent is not a sealed 0555
  package. Source checks are read-only, not launch/view/volume/runtime admission.
  execution.conf/execution-starter and aetheri_starter_exec_t remain reserved and
  unprovisioned; positive starter admission still needs actual policy/image tests.
  The optional --live-read-only probe uses synthetic build-local request IDs and
  keeps gates closed. Never install it as a real dispatch profile. Shared pure
  fixture config codecs now live in fixture_profile.c. core/import's check_payload
  has no fsync; preserve publication sync/durability behavior.
- launcher/execution.h now supplies the native executing-starter client. It pins
  actual Root discovery/process/unit/invocation/context and verifies same-owner
  correlated replies plus exact binding/revision before accepting writer/release.
  Registration/prepared each run at most once; no owner replacement or retries.
  peer_capture_reply requires the retained sent unique-destination request and
  matching bus/cookies; capture outputs must start empty. Execution1 1.0 generates
  a separate client header only, not default installed method tables. Read the
  authenticated-client section in spec/runtime.md. Explicit test_execution_client.py
  uses a Release producer in the actual Root domain; optional Debug client stays
  outside it. Temporary pipe/bus-write grants retire only after exact owned drain;
  never grant the bus read access to Root's reader or ptrace for instrumentation.
  The synthetic producer has no catalogue/views/target exec. Actual starter,
  protected Root unit/profile provisioning, producer admission/ARM handlers and
  target readiness/drain remain work. Keep ordinary installed inputs unchanged.
- MIT applies to original Aetheri code/documentation. Preserve third-party licenses.
- catalogue/execution.h now persists immutable AETHPRC1 process observations in
  explicit v4 execution_processes. Read spec/runtime.md's durable enrollment;
  run catalogue.processes and contract/migration tests. Freeze v1/v2/v3 SQL/wire;
  normal create stays v2 and open validates exact v2/v3/v4 (Root also v5) without migrating.
  upgrade_processes is an explicit quiesced-owner v3-to-v4 action under the same
  retained lock; no existing rows/revisions/gates change or missing evidence is
  backfilled. Root starter precedes ARM, Root app retains that exact kernel
  identity after ARM, User app follows registration and precedes READY. V4 fresh
  ARM/Root observation/User READY require their validated original record.
  Enrollment needs the original runtime; exact retries keep time/bytes. The
  historical same-authority/account/boot getter permits a replacement runtime
  without adopting a run, restoring readiness or releasing a lease. Current
  process/image/data/holder admission and an actual recovery transaction remain
  separate. End/lease settlement precedes evidence pruning; no full purge exists.
  Actual confined tests enroll all three observations, but production restart
  coordination, already-dead/lost-handle recovery and normal provisioning remain
  work. Ordinary installed catalogues stay v2 and public capabilities empty.
- linux/process.h now captures/reidentifies AETHPID1 (boot + 64-bit pidfs inode)
  from an authenticated live ProcessFD and independently admitted current context.
  Read spec/runtime.md's observer-replacement section; run linux.process_identity
  and linux.process in the explicit carrier suite. Mandatory PIDFD_GET_INFO checks
  bracket direct fstat on the accepted x86-64 pidfs kernel; no numeric-PID fallback.
  Do not widen policy for libsystemd's optional self-pidfd/fstatfs/handle probes.
  Fresh bus names can identify the same original process, never authorize a new
  owner/run or recover an old connection/image/lease/cgroup handle. Persist original
  identity with independently protected run/context before relying on recovery;
  never adopt candidate-supplied evidence. The actual replacement-observer test
  keeps its native context envelope private/test-only. Confined Root/User probe
  identities now have separate v4 enrollment above; restart settlement remains work.
- catalogue/lifetime.h now journals successful original-run closure separately
  in explicit v3 run_closures. Read spec/README.md and spec/runtime.md's separate
  durable run closure section; run catalogue.lifetime. Keep frozen v1/v2 SQL and
  wire unchanged. Native create stays v2, open validates exact v2/v3/v4 (Root also v5) without
  migrating; upgrade_lifetimes is an explicit quiesced-owner action under the
  retained writer lock. It adds no rows/admission/revisions to existing tables.
  Never restore a backup after unknown commit. A completed start operation,
  revision/time/result/steps and AETHLCR1 receipt remain immutable on close.
  Root closing -> authenticated User quiet -> independently admitted original
  end/Root terminal+lease released -> authenticated User exited. Digests/frames
  are metadata, never process/quiescence or bearer proof. Get receipts are
  historical, not fresh observations. Original runtime/owner replacement requires
  separate recovery; unfinished v3 closures become uncertain. The P0 User path
  requires no grant/access rows. Actual fixed test provisioning excludes future
  dispatch/restart/migration/delegation/FD-store/export holders; general holder
  settlement is unimplemented. Root rechecks original broker just before mutation.
  New methods stay only in explicit test vtables; normal installed catalogues
  remain v2 and capabilities empty. Do not use old no-exec withdrawal to close
  an executing attempt, or treat an isolated successful-run close as full removal.
- linux/lifetime.h now provides a Root-owned read-only original-process/unit/
  recursive-cgroup end observer. Read spec/runtime.md's lifetime section; run
  linux.lifetime in the explicit privileged carrier suite. Borrow the caller's
  original unit reference and trusted system bus; never Unref/stop/release from
  observer close. Same held objects/mounts, invocation, dead pin, terminal unit/
  no job and empty/deactivated original group are required. st_nlink remains 2
  after removal on this carrier: require missing name AND held event-reader
  ENODEV, never missing-name/link-count/deleted-path inference alone. Other
  kernel behavior fails closed. Restart/namespace/lookup drift quarantines.
  Actual root-only PID1 tests use a private component bus; property/lookup faults
  are controlled (v2 rejects actual rename). The connected enforcing test captures
  after actual Root/User execution confirmation and authenticates ObserveEnd only
  in its test interface. Archive native_end as historical snapshot; the separate
  v3 path above now settles only the successful fixed run. General future-dispatch
  fencing, external/delegated holder settlement, recovery and removal remain work.
  Never use the old no-exec withdrawal proof to settle an executing attempt.
- catalogue/execution.h now separately completes Root/User start results through
  ae_catalogue_complete_execution / ae_catalogue_finish_user_execution. Read
  spec/runtime.md's launch-result completion section; run catalogue.execution_completion.
  AETHLCR1 is a 200-byte immutable receipt, never a lease/drain capability. Original
  Root replies, actual app/User/current-image/private-data admission remain caller
  obligations. Root requires observed execution plus the exact settled UNKNOWN
  diagnostic; UNKNOWN alone is no exec proof. Preserve its reason. Root step6 and
  User step4 acknowledge applied launch results without changing live dispatch,
  launch, gates or lease revisions. Exact receipt/time retries preserve later
  state; owner replacement needs separate recovery. Test-only FinishLaunch/Complete
  and launch-completion.bin are not a normal SDK or production close/removal API.
  Archive completed start results AND the still-running lease/run obligations.
- catalogue/execution.h now has ae_catalogue_observe_execution: original Root
  profile/starter binding plus independently authenticated app/User evidence
  records step5 and observed dispatch, retaining the running lease/operation.
  Read spec/runtime.md's Root observation section; run catalogue.execution_observation.
  A digest is not process/IPC proof. Historical exact retries preserve later
  closing/terminal/uncertain state and gates; never rearm or infer lease release.
  Preserve the bounded step4 diagnostic reason in either ordering; dispatch state/
  step5 separately retain actual observation. The explicit
  live fixture adds PinApp/ConfirmApp only in test interfaces; no installed API/
  capability/policy change. Keep actual original pins/unique connections and User
  current-image/private-data checks. Executing durable drain/recovery remain
  pending; administrative failed-test retirement preserves both
  unsettled catalogues and is never a no-exec release or production recovery.
- catalogue/execution.h now separately journals User REGISTER and READY through
  ae_catalogue_observe_user_execution. Read spec/runtime.md's User-mediated app
  registration section; run catalogue.user_readiness. Original process/Root/
  image/private-data evidence remains the caller's obligation. REGISTER binds
  actual invocation/context; READY requires the same actor/evidence. Exact retries
  preserve later state; restart/gate drift rejects. Neither completes operations
  or releases Root's lease. The explicit live fixture exercises actual cap-zero
  User process/current-image checks and rejects premature/foreign-connection READY.
  AETHIMG1 is a 112-byte bounded observation codec, never provenance by itself;
  AETHOBS1/AETHAPP1 are test-only Root-observation/context envelopes. The temporary
  User policy allows exact proc/executable reads, no ptrace/capability or code-store
  traversal. Normal SDK/session provisioning, Root settlement and executing drain/
  recovery remain pending. Public capabilities and installed profiles stay closed.
- authority/execution_admission.h now connects the original registered User peer
  to native Root request/arm. Read spec/runtime.md's connected-owner section;
  run authority.execution_admission and the explicit live fixture. It verifies
  endpoint/profile equality, retains an independent original peer pin before any
  request, and rechecks active broker identity/unit/context before ARM. Unknown
  requests cannot repeat; replacement cannot inherit delegation. Caller still
  owes protected User code/data and mutation/revocation coordination. Split code/
  private-data readers retain combined starter checks; no User SQLite is read by
  Root. The live fixture now uses an actual UID1000 system-scope User service and
  its own native catalogue/preparation, not a normal login session. Its temporary
  bus name must map to the existing User service type; inbound-FD denials remain.
  Bootstrap acknowledgement requires exact independent slot/context/new-file
  evidence; native SELinux xattrs may have one terminal NUL, frozen afterward.
  Archive BOTH unresolved owner catalogues after exact original process/unit/
  cgroup/job test drain. This is not production readiness, release or removal.
- catalogue/execution.h now has ae_catalogue_prepare_user_execution. Read the User
  preparation section in spec/runtime.md; run catalogue.user_execution. It binds
  complete AETHEXE1 in User step1 before a possible Root execution request. Only
  fresh successful commit returns submit_required=1; retries/lost acknowledgements
  never authorize replay. Old User withdrawal refuses the step; startup retains
  it as recovery_required. Current Root/broker/still-reserved receipt provenance,
  private allocation/MAC and mutation exclusion remain caller obligations. This
  is not current owner RPC, filesystem admission, ARM/readiness/drain or a release
  protocol. No SQL/XML/installed capability changed; preserve the normal inputs.
- launcher/preparation.h and aetheri-execution-starter now join protected self/
  source checking, the authenticated client, private views and one-way exec. Read
  spec/runtime.md's separate starter section. AETHVWS1 (184 bytes, frozen vector)
  binds four independently recorded scaffold objects to the entire AETHEXE1;
  never adopt a recreated/matching-name directory or reuse the no-exec scaffold
  proof. Source checks happen inside the private namespace; held/named mounts and
  image are checked again after release. Failure must exit, with no retry/fallback.
  The fixed P0 Root unit is aetheri-p0-execution-root.service; target UID/GID1000,
  c35,c36, manifest literals and three fixed environment entries are fixture-only.
  No normal starter/profile/scaffold record/policy is installed. Flow adapters
  are controlled; actual confined positive admission, Root provisioning/live
  request/data mediation and executing target readiness/drain still remain.
- An explicit tools/test_execution_runtime.py now joins actual native Root journal/
  PID1 request, unchanged starter/private views and UID1000 executing target in
  the enforcing disposable carrier. Read spec/runtime.md's isolated live test
  section. It uses a separate privately bound root and controlled broker/data
  fixture admission, never normal installed inputs. Temporary aetheri_exec_app_t
  preserves old app-domain denials; compiled target SID changes the profile-bound
  starter digest. Root/starter get no SYS_PTRACE/KILL; the administrative
  driver independently checks the running ELF and terminates the exact app pidfd. Fresh
  post-exec credentials join the original held process/invocation/cgroup. Unknown
  starts preserve intent/fixture/policy. Exact process/unit/cgroup/job drain and
  full owned-input/tree checks precede test retirement; archive native unresolved
  lease/operation history, never use no-exec withdrawal or infer release from EOF.
  No production admission/readiness/recovery/removal or public capability follows.
- authority/execution_wire.h now supplies the separate journal-backed Execution1
  producer component. Read spec/runtime.md's endpoint section. It retains one
  original prepared call until fresh native ARM, consumes before send and checks
  one four-second deadline before and after commit. Expiry/send uncertainty never
  releases leases or proves drain. Run authority.execution_endpoint: real isolated
  owner/bus/ProcessFD tests use controlled actor/PID1 adapters on an owned private
  broker37 socket/config. No global bus/policy change or dbus-run-session dependency.
  The launcher uses system scope for its own PID1 subscription only. This library
  is not connected to the installed authority; actual Root/starter provisioning,
  live supervisor request/current broker-data admission, target readiness and
  independent executing drain/recovery remain pending. Keep normal inputs closed.
- linux/lifetime now exports/restores four private original observation handles.
  Read spec/runtime.md's retained-observation section. The native checkpoint is
  not a portable persistent record or journal adoption. Independently protect
  the original expectation before transport; never adopt incoming names/objects.
  Restore uses the exact retained root across PrivateMounts, rechecks original
  PID/cgroup namespaces and keeps ordinary capture's named-mount quarantine.
  linux.lifetime_handoff tests actual PID1 retention after observer exit and app/
  group disappearance, with capability-empty administrative unconfined_t services.
  Its four-entry store uses FDPOLL=0 and explicit retirement; a barrier is not set
  acceptance. The independent test controller retains the original unit. No normal
  Root FD store, production unit retention or Root/User recovery is implemented;
  the separate journal below does not enable them. The app FD store stays forbidden.
  Preserve uncertainty.
- catalogue/handoff.h adds canonical AETHHND1 and an explicit Root-only v5
  journal linked to the original v4 app witness. Read spec/runtime.md's durable
  handoff section. User remains v4; normal create remains v2, open never migrates,
  and frozen v1/v2/v3/v4 SQL/wires stay unchanged. Existing-intent retries/lost
  successful commits never permit another transfer/retirement effect. Current
  observer incarnations cannot replace the origin, adopt a run, restore readiness
  or release leases; retired retries never rearm. Actual descriptor-set/absence
  and protected custodian admission remain caller obligations. The real component
  uses the canonical frame with explicit synthetic digests/UID0; it is not a fixed
  UID1000 execution-profile admission. No installed Root producer/receiver,
  production unit retention, partial-set or early-starter recovery is implied.
  Run catalogue.handoffs and the existing linux.lifetime_handoff component test.
- linux.lifetime_handoff_retention separately tests a protected process-free
  target retaining original custodian/app metadata after all original probe
  clients exit and the controller closes its host-bus connection. The controller
  uses zero RefUnit calls; actual restoration/end/store retirement precedes exact
  target retirement and all-three-unit GC. Read spec/runtime.md's original-unit
  retention section. This is trusted administrative carrier configuration, not
  production Root provisioning/reload exclusion or v5 recovery integration.
  Preserve uncertain target/file/job state; do not infer drain from target stop.
  Normal inputs, policy, closed gates and public capabilities remain unchanged.
- authority/handoff.h now joins actual lifetime export to the enrolled v4 app
  witness before v5 PREPARE, and protected SQL expectations to restore/OBSERVE.
  Read spec/runtime.md's native bridge section. Only a fresh confirmed intent
  returns transfer FDs; existing/lost-successful-commit retries return none.
  Incoming FDs stay caller-owned; exact supervisor inventory/transport and
  original-unit retention remain caller obligations. Current self admission must
  already be independently established; construct only after the unit is active.
  authority.handoff uses real SQL with controlled kernel adapters. The confined
  default test exercises actual UID1000 PREPARE in Root v5/User v4 but submits no
  transfer. Its default retirement proof is ONLY for that unsubmitted case,
  after native release/local-copy fencing and zero-capacity/empty store checks.
  The temporary Root nsfs getattr grant adds no capability/namespace entry/open.
  Preserve the failed live fixture's original pending/held history in protected
  guest quarantine; infrastructure retirement did not settle it. No normal
  installed receiver, partial-set or full owner recovery is implied.
- The same handoff module now has explicit P0 SEND/RECEIVE/RETIRE with a four-slot
  preserved main-only PID1 store. Read spec/runtime.md's confined-transfer section.
  Only fresh confirmed intents permit notifications; unknown/partial effects
  never allow automatic repeat. Completed retirement never touches a later store.
  Labels route only; complete inventory plus actual SQL-bound object validation
  remain mandatory. The opt-in test_execution_runtime.py --handoff proves actual
  confined Root replacement after the original run is already closed/released.
  Its process-free target retains original unit history; startup selects its role
  from protected catalogue history, not from incoming FDs. The receiver exposes
  no executing/broker handlers and uses a fresh runtime without rewriting original
  receipts. Native retirement follows actual local-copy closure/fencing and store
  absence. A pidfs getattr failure was repaired with the same retained intent/FDs;
  no app/operation identities were regenerated. Temporary PID1 fd use/barrier write
  and Root pidfs metadata permissions add no pipe read, capability or ptrace.
  Active-run/cross-owner recovery, general allocation, partial-set integration,
  early starter/error-channel recovery and production retention still remain.
- catalogue/lifetime.h now has explicit Root-only v6 close-only recovery functions.
  Read spec/runtime.md's Root replacement closure section. Immutable current-owner
  rows require same boot, original successful start/app witness, current handoff
  OBSERVE and still-original User identity. Caller must retain that original
  User's independent process/unit/invocation expectation before failure; current
  unit/domain or broker UUID alone is insufficient. Original APIs remain strict.
  A further Root replacement must OBSERVE and BEGIN again; never rewrite AETHEXE1,
  rearm a run/gate or infer fresh end/holder proof from SQL. Native tests use
  controlled evidence, not an installed active-run coordinator. Normal inputs
  stay v2; User remains v4. Actual live Root recovery, replaced-User coordination
  and early-start recovery still need separate integration/evidence.
- The opt-in --recover-active test now captures immutable AETHUSR1 original-User
  metadata under system/recovery-user.bin before Root transfer. Read spec/runtime.md's
  active-test section. Its first actual attempt 0f04cd9ae92d4500a42177e3e14de76b
  reached current Root OBSERVE while App/User remained active, then failed at a
  combined package/process-image check. Root UID0 without capabilities cannot
  dereference UID1000 proc/exe; the source now keeps current image verification
  in the reidentified User. That source correction is NOT installed in the held
  failed fixture. App/User later expired; no actual recovery closure passed.
  On 4 November QEMU was found dead; offline carrier reboot confirms those four
  volatile handles and /run units/target are gone. Preserve the exact persistent
  fixture/controller/module and old evidence. Its Root v6 still has uncertain dispatch, running lease, observed
  handoff revision 2, no closure and no recovery-owner row. Evidence is under the
  guest build's active-recovery-failure-0f04cd9ae92d4500a42177e3e14de76b.
  Never reconstruct old handles from IDs, retire this fixture or rerun
  from fresh app/operation IDs to obtain a passing test. Original User metadata
  is not a live ProcessFD or proof of its complete drain. Next implement separate
  replaced-User admission/quiescence and separate cross-boot/lost-handle recovery,
  preserving v6's strict original-User boundary. Normal v2 inputs remain closed. Native
  Release/Debug metadata checks ran only as administrative unconfined observers;
  they do not prove live User reidentification or the remaining RPC sequence.
- Existing projects are read-only references unless a scoped change is requested.
  Physim's future NexaUI migration is a requirement, not an order to modify it here.
- Native catalogue/lifetime.h now adds explicit v7 paired-owner close-only APIs
  and AETHRCP1 (232 bytes). Read spec/runtime.md's paired-owner section. Root v6
  and User v4 upgrade independently; create stays v2 and open never migrates.
  Original v3/v6 runtime/User checks and frozen SQL/XML stay unchanged. Both
  owners separately enroll the exact current pair after independent original/all
  prior User drain, current actor/image, holder and future-effect admission.
  Digests never prove those facts. Root can forward historical release to a new
  User without changing the lease. Old uncertain receipts never rearm; another
  actor replacement needs new admission. Run catalogue.owner_recovery and host
  migration/vector tests. No installed pair wire, cross-boot/lost-handle or
  post-retirement Root-replacement recovery is implied. Preserve the old failed
  fixture; do not migrate it to turn native metadata into runtime proof.
- linux/process now has optional observe_main/verify_main/observe_self_main APIs:
  active actual MainPID, no ControlPID/Unit.Job, with the original retained peer
  and repeated UID/label/unit/invocation/cgroup checks. Read spec/runtime.md's
  current-service-main section. Generic/activating registration stays unchanged.
  Root discovery and active broker use require strict main checks; cache tests
  mock this explicitly, while linux.process tests actual same-unit subordinate
  rejection and typed property faults. No numeric MainPID reopen/ExecMainPID
  history/remote self-pin substitution, new capability or policy permission.
  Normal v2 owned inputs were updated and enforcing check/broker-check passed;
  services are stopped/gates closed. Old isolated v6/User v4 fixtures stay intact.
  A service-main snapshot is not current image, future-effect/holder fencing or
  old volume/clone admission. Retained host UUID/PID/command files have no original
  independent disk-object witness; never invent it retroactively.
- catalogue/interruption.h now adds explicit v8 cross-boot maintenance after v7:
  immutable AETHINT1 bindings (248 bytes) and four distinct 352-byte receipts.
  Read spec/runtime.md's cross-boot section; run catalogue.interruptions and
  contract/migration/vector tests. Normal create stays v2; open never migrates.
  SQL v1-v7/XML and original/same-boot checks stay unchanged. Original successful
  start/app evidence is mandatory; early/unfinished starts are separate work.
  Further owner/boot replacement requires fresh independently admitted bindings
  at BOTH owners; old stages/results never change or rearm effects. Hashes and
  boot IDs are not continuity/clone/current-actor/holder/future-effect proof.
  Tests use controlled admission with real SQLite; no installed wire/coordinator
  or real retained-fixture settlement follows. Keep its v6/User v4 unchanged.
- linux/boot.h now provides read-only AETHBOT1 current-kernel/proc/self PID/cgroup/
  time namespace observation (48 bytes). Read spec/runtime.md's boot section; run
  linux.boot and the explicitly privileged linux.boot_namespaces carrier checks.
  Independent trusted host scope remains mandatory; self IDs never prove initial
  scope. No PID1 namespace reads/numeric-PID opens/ptrace or new policy permission.
  Different boot is not chronology, machine/volume/clone provenance, original end,
  holder drain or release authority. The build-local administrative inspect-boot
  checks original protected metadata/store without beginning a runtime; it does
  not change normal load checks or settle the retained v6 fixture. Cross-boot
  interruption/closure, current actors and future-effect fencing remain work.
  Inspection requires independent quiescence/fencing and existing regular lock/DB
  with no journal/WAL/SHM; the ordinary SQLite connection is read-write and must
  not perform incidental hot-journal recovery. The explicit --inspection Python
  runner tests the preserved fixture plus private shadow refusals, never dispatch.
- Keep a small number of coherent modules/documents. Explain every major directory,
  dependency, resident service and generated artifact.
- Design ownership and complete app removal from the beginning; read
  `brains/aetheri/knowledge/app-lifecycle.md`. Include plugins, state, credentials,
  registrations, services and multi-user/shared dependencies.
- Avoid unnecessary resident processes and polling. Measure wakeups, memory,
  responsiveness, startup and disk use. Process/file count alone is insufficient.
- VM RAM may use up to 16 GB. F: is available for Aetheri, but use F:/Aetheri only.
  Existing Physim/NexaCloud directories, ongoing migrations and processes are
  protected: do not move, delete, clean, stop or otherwise interfere with them.
  Do not format/repartition the volume or treat it as disposable workspace.
  Do not expand shared WSL storage on D: casually.
- UI remains a user decision. Later prepare multiple alternatives with keyboard,
  focus, scaling, three-monitor, accessibility and error-state behavior.
- User now prefers working UI foundations such as SDL3 instead of waiting for
  NexaUI completion. Read brains/aetheri/knowledge/nexaui.md. Review exact SDL3/
  text/widget dependencies before adoption; the first exact development profile
  and runtime limits are now in spec/settings.md.
  Keep app models/commands/persistence toolkit-independent and wlroots compositor
  separate. NexaUI can be adopted per app when its required scope is ready. The
  authorized local NEXT_AETHERI_UI.md note is a handoff, not permission to alter
  NexaUI implementation, other consumer pins or send another chat a message.
- Early experiment environments are now required: complete Aetheri desktop first,
  persistent changes until explicit reset, and individual personal-file/network/
  device permission (4 November 2026). Read knowledge/experiment-environments.md
  under brains/aetheri. Plan hardware-virtualized isolation for guest root/kernel
  and later AI work, resource limits and complete VM-state ownership/removal.
  The existing Windows carrier is not this feature; no absolute host-crash or
  escape guarantee, TCG security fallback or implicit host-management exposure.
- No invented commands, screenshots, measurements or hardware acceptance.
- The optional private C17 compositor now exists; read spec/graphics.md and its
  separate graphics-inputs.json. Native input and explicit nested output are tested;
  its wlroots pointer-enter patch and fresh build helper are pinned there. Default
  builds stay graphics-OFF. These tests are not physical/trusted-session or app-
  lifecycle acceptance. The input-FD driver is a separate test executable. Keep its
  private socket/captures and unprivileged test children separate from the retained
  old execution intent. Exact wlroots/Wayland dependency changes require review.
- Optional AETHERI_SETTINGS now builds the native development view in src/settings;
  read spec/settings.md and spec/ui-inputs.json. Keep its model toolkit-independent.
  The selected SDL Wayland profile uses OpenGL ES; the initial no-renderer profile
  could not create a framebuffer. Private builds pin SDL/text sources and one
  optional-data-device null guard. Do not advertise clipboard merely to avoid that
  client bug. The VM uses explicitly selected llvmpipe; hardware/AT-SPI/normal app
  admission and settings backends remain work. Supply an explicit owned 0700
  development shader-cache directory; it is not a StorageRef or disk quota.
  Preserve unpatched close-time core dumps and the same-binary negative control.
  Source/headers and guest graphics RPMs are reviewed separately; do not install
  broad GL development packages, change shared host/WSL settings or infer physical
  GPU acceptance. SDK/Mesa Release libraries are not ASan-instrumented by the
  current app Debug check. The default Settings option stays OFF.
- The private compositor now exposes native clipboard selection. Read the
  clipboard section of spec/graphics.md and the additional pinned wlroots patch.
  Use the actual request client/current seat focus and current focus serial floor;
  no UID/source-name inference. Retained offers require current focus at receive.
  Replacement keyboards retain the current window and start fresh focus. The
  private getter/reject symbols require the new ABI before socket publication;
  they are not library provenance. Reject unstarted drags without leaking a
  source-less drag or destroying the active clipboard source. Clipboard uses no
  history/payload cache/host bridge; it is not app admission, consent, FD revocation,
  trusted lock or completed DnD. Preserve the old-ABI/receive-guard negative controls.
- The user confirmed the current workstation is the main-PC target and requires
  future AM5 portability. Read brains/aetheri/knowledge/testumgebung.md.
  Keep the x86-64/UEFI baseline vendor-neutral; no unconditional -march=native.
- Review selected files before public publication. Keep credentials, personal
  machine paths, local state, downloaded/build trees and VM images out of Git.
- No laptop installation disk is selected. Verify exact device identity and
  recovery steps before any destructive hardware test.
- The private desktop connection now exists in compositor/desktop.c; read
  spec/desktop.md. Only the independently inherited original stream connection
  can discover/bind layer-shell/control; no UID/title/app-ID or numeric-PID proof.
  Keep view state, compositor focus policy and fixed-app supervisor launches
  separate. Neither private global exists without --shell-fd. The current native
  protocol consumer stays diagnostic; a separate SDL development consumer and
  fixed two-app owner channel now exist in spec/desktop-ui.md. No production
  session/lock/app authority is implemented. Unexpected shell loss
  fails this development session; retire acknowledges only orderly departure,
  never lifecycle/lease settlement. Preserve old intents and normal empty caps.
- AETHERI_DESKTOP is opt-in and requires the compositor. Read spec/desktop-ui.md.
  Keep models/commands toolkit-independent, SDL custom roles/high-density scaling
  in the adapter and fixed app paths/process handles in the development owner.
  Initial configure precedes EGL allocation; retain strict logical geometry and
  correct premultiplied clear colors. The selected carrier needs one bounded
  follow-up repaint after resize. Idle is event-driven. Both channel directions
  reject ancillary FDs/truncation; no unknown request is replayed. The private
  three-byte v3 channel uses L3S/L3T/L3F, A3/E3/X3 plus S/T/F, and Q30. App/window
  selection is tagged; keep per-app pending state and fixed owner-selected paths.
  User Q30 fences new launches and waits for Files (including its document) ->
  Terminal -> Settings -> desktop ->
  compositor, including actual process AND output-pipe end before the next role.
  Preserve this order on exception too; post-spawn pipe errors are not no-effect
  replies. Close unused parent client copies when stopping before desktop startup;
  a late compositor READY cannot create the first UI after the stop fence. Closed
  format1 records without launch_protocol stay historical v1;
  historical protocol2 remains readable without rebinding; new desktop records
  bind protocol3, seven binaries and Mono font/license hashes. The session-local
  documents root is independently prepared. Search may still activate existing
  windows during closing, but cannot launch new ones.
  desktop.owner runs the real loop with test-only host adapters and independent
  pidfds; its /proc writer retention and force cleanup are never app admission.
  Journal stdout can use a PAM session scope: bind current boot/original process
  and captured stream for evidence, never adopt historical PID/log identity. A3 is
  only an original child observation, never app READY/admission or lease release.
  App-ID grouping grants no privilege. SDL/SDK/Mesa instrumentation limits and
  unresolved native a11y/text/physical/session gates remain explicit.
- Window desired maximize/fullscreen modes are compositor-owned, separate from
  client requested/committed state. Super+Up/Down requires actual app focus and
  refuses shell focus, pointer interactions and fullscreen; consume both arrows.
  Initial v4+ bounds are advisory and exclude dock/margins, not client clipping.
  Reconfigure against the current selected output, never only the home index.
  Read spec/desktop-ui.md's later work-area evidence; physical/general off-screen
  recovery and fullscreen dock policy remain open.
- AETHERI_SHELL is opt-in; read spec/shell.md before extending its experimental
  profile0. Parsed programs own arena strings and cannot be copied without pointer
  rebasing. Keep scalar expansion nonrecursive and reject whole invalid pipelines
  before effects. Retain the all-forks start barrier, direct original-child waits,
  stdio-only child FD export and explicit execve (no execvp shell fallback).
  Parent builtins require a singleton without redirection. This is not a graphical
  terminal, app admission, descendant drain or a sandbox; -i is scoped below.
  Final language/product choices and normal provisioning remain open.
- Shell -i now supplies private canonical N_TTY/group/SID checks and ephemeral
  jobs through jobs.h. Retain original direct children and monotonic IDs, terminal
  handoff before barrier release, default child actions and mask unblocking before
  any blocking redirection. Continue requests are not running observations; a new
  STOP may coalesce with CONT. Refuse foreign foreground takeover. Exit/EOF refuses
  open jobs; HUP/TERM signals still-waitable original direct children and waits
  without force-kill. Do not infer descendant/app/lease drain. Final persistence/
  raw editing and normal terminal app integration remain open. Reject saturated raw canonical records
  before CR/delimiter trimming; no lossless kernel-editor claim. PTY test cleanup
  enrolls original pidfds while the private leader is unreaped; never rediscover
  a session from a stale numeric SID. Read spec/shell.md's interactive evidence.

- AETHERI_TERMINAL supplies a separate graphical PTY owner; read spec/terminal.md.
  Native selection retains every row's content-incarnation token through history
  movement. Edits/erase/wrap changes, eviction, resize/reset/screen replacement
  cannot retarget it. Tokens are process-local observations, never security or
  durable identity. Copy sizing precedes writes; preserve wide/combining UTF-8,
  real soft-wrap joins and atomic undersized-buffer failure. Ctrl+Shift+arrows/A
  and selection Escape never enter the PTY. Accepted input clears selection;
  focus loss cancels drag. UI handles stay outside the cell/selection model.
  UTF-8 export is not native clipboard transport. The selected SDL Wayland read
  path is synchronous without a complete-transfer byte limit; do not call its
  main-thread-only API from the readiness worker or add unbounded UI paste.
  The bounded clipboard adapter now borrows SDL's actual display/surface/queue;
  keep one owner reader/dispatcher and the existing readiness worker. Preserve
  fixed source/offer/send/memory budgets, nonreused epoll cookies, total deadlines,
  immutable in-flight exports and the one-unacknowledged-receive sync barrier.
  Old offers are dropped on native focus loss. Run terminal.clipboard: real peers,
  stalled/trickled IO, blocked exports/replacement and close must stay bounded.
  Toolkit-free paste validates all <=4096 bytes before queueing; multiline/tab
  review starts on Cancel and requires fresh input after review-frame submission.
  Recheck original focus/shell/foreground/screen state; no retry after acceptance.
  This remains TERM=dumb, not bracketed paste/full VT/IME or app admission.
  Add no clipboard daemon/host bridge implicitly. Closing FDs, EOF and protocol
  sync never prove remote revocation, process drain or lease settlement.
  Native tests retain exact pixel/byte and stale-row mutation evidence; virtual
  input and software rendering do not establish hardware/a11y acceptance.
- terminal/composition now retains <=1024 bytes of preedit separately from PTY
  cells/history. Scalar offsets are not bytes/cells/graphemes. Preserve complete
  validation, bounded horizontal viewing and current candidate coordinates.
  Hold editing/confirmation keys while composing; Escape/blur/context/end/close
  clear the draft. Commit is validated single-line input with no speculative
  PTY bytes or replay. Stop native text input during blur/end/close/paste review.
  SDL's pinned Wayland ClearComposition can succeed as a no-op; retain actual
  Stop/Start transitions and queued-event fencing, never infer provider/serial
  admission from that local timestamp. Run terminal.composition and terminal.ime
  plus affected terminal/desktop regressions. The controlled native v3 producer
  is linked only into the test server and explicitly enabled by inherited-socket
  I on; it is not installed compositor/provider IME integration. Keep normal
  production globals/services/policy unchanged until that separate route is reviewed.

- The own NexaUI Terminal now has original-session tabs/splits. Read
  spec/terminal.md#original-session-tabs-and-split-panes and the multiplex journal.
  workspace is a bounded value tree, never a PTY/model owner. Keep live sessions
  in stable slots, one worker and a rotating shared 16 KiB turn. Prepare spawner
  sources/environment before threads; pidfd_spawn's unprivileged bootstrap is
  distinct from the privileged execution starter. A post-spawn error with a pin
  retains original child/stream duties; never retry or infer descendant drain.
  Named pane IDs must remain disjoint across equal-token models, hide/resize and
  slot reuse. Focus transfer cancels old paste/preedit and fences queued commits.
  Owner v2 adds exact bootstrap/source binding without rewriting v1/closed history;
  Files' document editor receives no multiplexer helper. Run terminal.workspace,
  terminal.spawn, terminal.panes_view, terminal.multiplex and affected owner/input
  regressions. Persistence, normal admission and complete assistive use remain work.
- catalogue/recovery.h now provides one bounded read-only owner metadata snapshot
  on the retained SQLite connection. Read spec/runtime.md's recovery-inventory
  section and run catalogue.recovery_inventory. It never authorizes replay,
  holder drain, gate reopening or old-volume/process adoption; empty counts are
  not service readiness. Preserve historical uncertain closure rows and separate
  terminal interruption semantics. Source startup scans before protocol
  publication, but the protected installed bootstrap is not updated by that build.
- linux/image.h now has a self-only image matcher supporting capability-empty
  UID/GID0. Keep remote peer UID0 rejection; never turn self pins into remote
  authority credentials. Normal P0 selects the independently protected role ELF
  before catalogue access and at context/User-delegation use, latching failure.
  The separate offline setup-recovery image selection remains explicit. Read
  spec/runtime.md's current-authority-executable section; run linux.image_self
  and affected execution/peer/process/broker tests. The explicit actual-domain
  shadow test has no catalogue and cannot adopt normal state. The normal v2
  bootstrap is now updated/checked enforcing, including the earlier startup
  inventory. Policy and non-runtime owner tables stay unchanged. This does not
  prove loaded-dependency/remote-owner/continuity/future-effect/holder admission
  or settle the old quarantine. Preserve its lost-handle boundary.

- authority/system_connection.h now binds normal User to the protected current
  Root unit/epoch using actual correlated successful replies, an original
  ProcessFD and repeated active MainPID/unit/invocation/cgroup/boot checks. Read
  spec/runtime.md's reciprocal-current-Root section; run
  authority.system_connection and the explicit controller connection-check.
  Management event incarnation is not Root runtime. Private normal User Root
  RPCs use the same unique destination. Bounded pending-message dispatch retains
  name-loss evidence through same-name reacquisition; nested use/close is EBUSY.
  Live drift quarantines, actual departure drops identity only; never replay a
  sent call or infer old-run adoption/drain/lease release. The normal v2 bootstrap
  now includes the protected selected-Root slot and minimal reciprocal proc
  metadata policy, with explicit User ptrace/Root-ELF denials. No SQL/XML/public
  capability change. Interrupted controller probe/update needs manual review.
  Continuity, prior-owner/future-effect/holder recovery remain separate work;
  preserve the old lost-handle quarantine and all unsettled owner journals.

- catalogue/effect.h now supplies a process-owned, one-way local execution latch
  shared by coordinators on the retained SYSTEM store. Read spec/runtime.md's
  local-execution-source-fence section; run authority.execution_fence and
  authority.execution_fence_wire plus affected execution/catalogue tests. Fresh
  stages and post-commit adapter/transport permits reject when fenced; historical
  acknowledgments never regain effect_required. Balance all entered brackets;
  close/runtime replacement is EBUSY while active. Actual Root shutdown fences
  before event-loop exit. This is memory-local, not a durable/restart/volume or
  global holder fence. Entered calls/sends may still finish or return unknown;
  active==0 never proves external drain or lease release. Other effect families,
  supervisor/old-owner/export/delegated sources require separate coordination.
  No SQL/XML/policy/public capability change. Keep the old lost-handle quarantine
  untouched; no normal executing launch or complete recovery follows.

- authority/handoff now shares that local source fence for raw observer export
  and every fresh four-slot FDSTORE notification. Root-custodian permits match
  current epoch/boot/runtime without inventing a historical User delegation.
  Hold brackets across source effects; recheck after PREPARE and before each
  send. An entered last send/barrier may still finish; partial/unknown sets stay
  pending and are never filled or replayed. Receive and independently authorized
  retirement use cleanup brackets allowed after fencing, with all original
  exact-object/terminal/holder prerequisites intact. Read spec/runtime.md's
  original-observer-export section; run authority.handoff_fence and affected
  catalogue/handoff/execution-fence regressions. These new component scenarios
  use real SQL/local FDs with controlled lifetime/PID1 adapters, not joined live
  app/supervisor acceptance. No installed public launch/recovery or old-volume
  provenance follows; preserve the existing quarantine and frozen contracts.

- The same retained SYSTEM latch now rejects fresh install reservation-gate
  opening, new reservation intent/lease/dispatch and first no-exec setup intent.
  Check after admission reads and before mutation. Preserve exact historical
  acknowledgments and existing independently admitted observation/withdrawal;
  none authorizes a new unit/start/helper reply. Read spec/runtime.md's fresh-
  reservation-and-no-exec-intent section; run catalogue.admission_fence and
  affected reservation/launch/source-fence tests. The new twelve cases use real
  native metadata with synthetic caller evidence, not live supervisor/drain
  admission. Keep all original cleanup proofs and frozen SQL/XML; the latch
  remains local and does not resolve old-owner/volume/holder continuity.

- authority/setup methods now bracket the current owner for the whole method,
  reject reentrant use/clear, and source-permit Prepare/Register/Prepared replies
  after native work and again before send. Entered uncertain sends are handled;
  never generate a replacement error reply or drop original pins/journals.
  Existing admitted observations and proven no-exec withdrawal remain cleanup
  while fenced. Read spec/runtime.md's no-exec-setup-reply section; run
  authority.setup_fence plus setup-supervisor/launch/admission/transport regressions.
  Its private-bus/process pins are real, actor labels/PID1 context controlled;
  no live MAC/view/drain acceptance follows. Clear now returns EBUSY while active.
  No SQL/XML/policy/public capability changes; preserve old quarantine and all
  remaining supervisor/volume/old-owner/holder obligations.

- linux/start and authority/start now submit/observe one original asynchronous
  PID1 StartUnit/fail after the coordinator's fresh REQUEST commit. The compiled
  caller still owes immutable unit/dependency-graph/exclusive allocation admission,
  its own RefUnit/Subscribe on a dedicated host bus, and current app/data context.
  Authenticate original reply and JobRemoved; never adopt matching-unit jobs
  after a lost reply. Keep the catalogue cleanup hold for pending/quarantined
  work; ordinary close is EBUSY, explicit abandon is local only. Known job result
  is neither target exec/readiness nor process/unit/holder drain or lease release.
  Terminal watches stop and historical results survive later Manager replacement.
  Read spec/runtime.md's original-supervisor section; run authority.supervisor_start
  and explicit linux.start_pid1. The first controls PID1 role on a real private
  bus; the second uses actual PID1 with an owned process-free target, not a new
  executing app fixture. No installed source/profile/capability or SQL/XML change.
  Keep the old quarantine; normal provisioning/restart/volume/holder recovery remains.

- linux/unit now owns an exclusive moved bus, original PID1 pin, witnessed source/
  drop-in FDs and one-shot Subscribe/RefUnit/UnrefUnit custody. Read runtime.md's
  protected-unit section. Its bounded P0 graph has one executable starter and
  non-executing infrastructure; only literal perpetual root/system slices can
  omit files. Current file observations are not durable volume/allocation trust.
  Caller still supplies independent original allocation/global mutation admission
  and journals custody intent. Drive its borrowed bus only; never mix independent
  Ref/Subscribe/Start owners or the older job-only adapter on that connection.
  Fresh config checks fence on source drift; original end/holder admission is
  mandatory before retirement. Unknown acknowledgements never retry. Ordinary
  close rejects unsettled custody; abandon closes without flushing and cannot
  cancel work, prove drain or free a lease. Run explicit linux.unit_custody and
  linux.start_pid1. Actual PID1/admin true starts and controlled lost replies do
  not establish normal aetheri_system_t provisioning, app target readiness or
  executing drain/recovery. Preserve all old fixtures/quarantine and installed
  inputs; live registry/coordinator/end integration is still pending.

- Root v10 now journals original unit connection origins and ordered per-node
  Subscribe/Ref/Start/Unref/Unsubscribe intent/acknowledgement history. Read
  spec/unit-profiles.md#root-v10-connection-custody-journal. The Root adapter
  requires v10 and compiled hooks; isolated Linux owners can omit them only for
  their explicit tests. Only fresh confirmed intent permits an RPC; unknown
  replies/acknowledgements quarantine use. Start confirmation is an authenticated
  job reply, never target exec or drain. AETHUBS1 is connection metadata, not
  issuer process/supervisor/image or volume proof. Full sequences stay visible
  and immutable; there is no custody retirement/recovery transaction yet.
  Explicit upgrade_unit_custody migrates only quiesced Root9->10; normal create/
  installed owners stayv2, User9/10 unsupported, published v1-v9 SQL/wire frozen.
  Run catalogue.unit_custody and explicit authority.unit_custody. Preserve the
  old executing quarantine and all original histories; no replay/adoption/lease
  release follows from this journal or local connection closure.
  First-quarantine notification also fences fresh Root execution effects; this
  does not settle metadata or prohibit independently admitted historical cleanup.
  Once published, keep v10 SQL and unit-origin-v1 unchanged; future schema changes
  need a new explicit version, never an edit to a published development contract.

- Root unit construction now requires explicit Root v11 original issuer
  enrollment before REQUEST. Read spec/unit-profiles.md#root-v11-original-issuer-identity.
  linux/issuer retains an actual bus-authenticated own ProcessFD and checks the
  independently selected main service/label, invocation/cgroup, pidfs identity
  and original self ELF. Same-unit children, wrong image/label/role and inherited
  connections reject. Local self-image pinning never replaces the required bus
  ProcessFD; remote UID0 image admission remains forbidden. AETHUIS1 and getters
  are historical metadata, never current proof, recovered handles or new effects.
  Fresh effects and independently admitted original cleanup repeat issuer checks;
  close only drops local handles. Explicit quiesced upgrade_unit_issuers adds no
  old evidence or gate/runtime/revision changes. Normal create/installed owners
  stay v2; open validates exact Root2–11/User2–4,7–8 without migrating. Preserve
  published v1-v10 SQL/XML and unit-issuer-v1 once published. Run core.unit_issuer,
  catalogue.unit_issuers, tools/test_unit_issuer.py and explicit enforcing-carrier
  authority.unit_custody / authority.issuer_barriers. Their UID0 administrative
  drivers are not production confined Root provisioning. All owner histories and
  reserved leases remain; operator cleanup of known terminal test configurations
  never settles native obligations. No bus-loss/owner/volume/clone recovery,
  external holder settlement or durable issuer/custody retirement is implied.
  Preserve the old executing quarantine, normal inputs and SDK/UI defaults.

- The NexaUI consumer pin now selects 0.1.23/97a4264 through
  spec/nexaui-inputs.json. Read the 2026-11-07 NexaUI upgrade journal and STATE:
  private Release/ASan app evidence is separate from shared-source/build
  promotion. Preserve old 0.1.18/CLOSED inputs and normal P0/quarantine. Never
  weaken owner/artifact checks for test permissions, or remove foreign /tmp
  data to resolve quota failures. The delivered chat handoff remains closed.

- The normal development source and session-release/session-debug builds now use
  the reviewed NexaUI 0.1.23 inputs. Read the 2026-11-07 development-promotion record.
  New Root starts persist tmpfs-v1 and verify the original unit option; supervisor
  preflights a separate 1 GiB temporary filesystem. Preserve older records/history.
  A source .aetheri-promotion.pending fences new starts during a partial refresh;
  never clear unexplained pending state. Native owner fixtures can use an explicit
  canonical UID1000/mode0700 artifact root, with binary bindings unchanged. Export
  private tmpfs evidence before retiring it. Carrier root capacity is limited;
  do not delete foreign caches/data or protected lifecycle fixtures to make room.

- A separate 8 GiB developer qcow2 and original-object registration are prepared
  on the host. Read spec/development-storage.md and the 2026-11-07 journal/STATE.
  Future normal cold boots attach it; installer boots refuse it. Host QEMU probes
  and enforcing guest loop tests pass, but actual fixed-disk guest enrollment is
  pending. A live foreign NexaUI helper defers reboot; never terminate that work.
  Host create/guest format have distinct pending records and no automatic replay.
  initialize-guest requires the actual original QMP block/image/serial; mount-guest
  never formats. Keep old build paths, root reserve, policies and protected state.

- editor/direct.h now supplies the separate graphical Editor's toolkit-free
  non-modal input controller over the original document kernel. Read
  spec/graphical-editor.md; run editor.direct with existing editor regressions.
  Input tokens are process-local correlations, not native clipboard/grant proof.
  Host focus/tab/file changes must cancel original input without losing selection.
  The native tab/save/close coordinator and opt-in window below preserve the
  separate modal Terminal editor and normal launch paths.

- editor/workspace.h now owns bounded graphical-editor documents/direct inputs/
  local file adapters and original close/destination correlations. Read the
  current graphical-editor contract and 2026-11-07 workspace journal; run
  editor.workspace and affected editor regressions. First saves are create-only;
  UNKNOWN retains the original adapter/name and blocks save/discard completion,
  even for clean text. Normal UI shutdown must resolve tabs before destruction.
  The opt-in native window below uses that backend; complete Save As/export/
  recovery, responsive I/O and normal grants remain unfinished.

- The opt-in aetheri-text-editor now has a native NexaUI0.1.23 window; read
  spec/graphical-editor.md and the 2026-11-07 editor-window journal/STATE.
  Selected private Wayland workflows and nine editor groups pass Release/ASan.
  Each original native event passes through the direct controller/focus frame;
  never route the unexamined remainder directly to the toolkit. The local picker
  passes the original held file witness for both new reads and duplicate reuse.
  Rendering may query selection while original close review blocks input handles.
  Save I/O now has the explicit original-worker integration below; loading and
  folder enumeration still need responsive original operation ownership.
  Clipboard/focus cancellation now has the separate native integration below.
  Text snapshots are read-only, not a
  complete accessible editor. Search, IME, native chrome/reflow/design, Save As/
  recovery and normal app admission remain work. Sticky SDK native close is
  latched once; repeated native-close/management acceptance is unproven (W01).
  tests/compositor_input_driver.c's Z command commits test-only integer output
  density, absent from installed compositor input; no fractional-scale claim.
  Preserve normal sources/builds/authority/quarantine, protected VM and foreign
  work. Use private /tmp backing for native tests; never remove foreign quota data
  or treat tmpfs as the known SDK sparse-mmap failure fix. Goal remains incomplete.

- The graphical Editor now uses the pinned public Wayland clipboard asynchronous
  begin/take/cancel/release API. Read the updated graphical-editor contract and
  2026-11-07 editor-clipboard journal. Original document/direct token/canvas/focus
  admission precedes take, after all queued native events and focus frames; a wake
  is advisory. Retire only the original app-owned request, never a field's other
  request. Empty results do not delete selection; native EOF does not prove full
  source intent. Use the SDK wiping release with the exact returned length.
  Fourteen independent native workflows pass Release/ASan with original pipe
  cancellation, real file bytes and overall timeout evidence. There is no new
  thread/service for clipboard, installed permission or normal desktop acceptance.
  Save work below adds a bounded on-demand worker; loading/scanning, final design/
  accessibility and the full OS goal remain pending.

- The graphical Editor now prepares one original save job through workspace.h.
  Read spec/graphical-editor.md and the 2026-11-07 editor-save-work journal/STATE.
  Document snapshots freeze the captured saved-history boundary; actual OK may
  acknowledge that state after newer edits, preserving dirty state/newer typing.
  Ack retries never restore later clean markers. Worker owns only its private
  document/file adapter; never read the live workspace/document from it. First
  saves borrow fixed incumbent adapters for checks; their close is blocked.
  The trusted host MUST join the original worker before finish/free; done/wake
  alone is not termination or save success. No started-write cancellation/replay.
  Finish binds only the retained original tab, including UNKNOWN even for clean
  text. Destruction refuses active work. A later save reply never closes a newer
  or cancelled review. All ten native editor groups and actual window/clipboard/
  controlled save workflows pass Release/ASan. test_editor_save_pause contains
  the separate test-only fsync pause/fault, not the normal app. Its UNKNOWN actor
  is pidfd-administratively retired only after join, not production close/recovery.
  TSan compiler probe fails because its runtime is absent; do not claim TSan or
  change global packages/policy. Loading/scanning and full OS acceptance remain
  work; preserve normal inputs, protected history and foreign planning/design.

- tools/build_boot_probe.py now builds a separate ordinary-user diskless
  initramfs from spec/boot-probe-inputs.json. Read spec/boot-probe.md and the
  2026-11-07 boot journal/STATE. systemd remains PID1; the native probe has no
  reboot or installation effect. Its generated image-only unit requests immediate
  poweroff, never suitable for the normal host/user shutdown path. Preserve vendor
  licence files; these development pins are not a production security/base or
  source-distribution approval. tools/test_boot_image_contract.py checks actual
  archive metadata and no-overwrite; tools/test_boot_probe.py uses separate
  diskless/no-network/no-QMP q35/TCG with original extracted tool hashes and fresh
  private NVRAM. Retain original handles/attempt logs; exit0 alone is not PASS.
  Two positive cold boots and a real wrong-nonce rejection pass. Do not call this
  a USB/disk image, Secure Boot, production policy, desktop or hardware acceptance.
  Next test normal removable-media firmware boot, then root/session integration.
  Never attach/format host or carrier disks to make that proof easier.

- tools/build_uefi_probe.py now makes the standalone GPT/FAT32 probe with a
  pinned systemd259.10 stub/ukify and build-only pefile. Read spec/uefi-probe.md
  and the 2026-11-07 UEFI journal/STATE. Verify exact RPM signatures/payloads and
  preserve licences; do not install these private extracted tools globally.
  Formatting accepts only the newly created regular boot.raw under a new output,
  never a device. UKIs reproduce byte for byte; FAT directory timestamps do not.
  tools/uefi_probe_image.py independently checks GPT/FAT/PE; run the seven media
  contracts and five older boot-image contracts. tools/test_uefi_probe.py boots
  only the read-only USB artifact plus fresh NVRAM, no direct kernel/initrd/append,
  system/data disks, network, QMP or shared directory. The shared original-child
  observer now requires exact native result, service outcome and kernel poweroff.
  Two final positive cold boots and an actual wrong embedded nonce rejection pass.
  Valid RPM signatures are not an EFI signature; no Secure Boot/production policy
  or hardware acceptance follows. This is still the immediate-poweroff diagnostic
  image, never a normal user desktop. Next independently assemble a read-only
  system root and integrate the existing session/desktop, without adopting old
  quarantined runtime state or treating image GUIDs as durable volume proof.

- tools/build_root_probe.py now adds a readonly ext4 root in the combined 512MiB
  GPT file. Read spec/root-probe.md and the 2026-11-07 root journal/STATE. It runs
  only as an ordinary Linux builder, formats only new regular files and never
  mounts or installs them. Capture the mke2fs configuration/tool hashes; require
  all 72 selected inode ownership reports and e2fsck acceptance. The C17 root
  mode confirms actual ext4/ST_RDONLY, mounted device PARTUUID and create EROFS;
  the normal RAM mode must preserve its original ELF bytes. Run five root tests
  plus affected old boot/media regressions. Kernel builtin virtio/ext4 performs
  root mounting with no /init; do not claim initrd userspace handover, laptop USB
  root, LUKS, production policy, writable data or desktop acceptance. The new image
  has no production MAC policy; carrier Enforcing evidence is separate. Immediate
  poweroff units stay image-only. Root GUIDs/UUIDs/nonces are fixture correlations,
  never clone authentication or catalogue/data admission. Preserve normal sources,
  old quarantine/CLOSED records, unattached developer disk and foreign work.
  Next writable runtime/data and existing desktop/session integration; full goal
  remains active and incomplete.

- tools/build_data_probe.py now adds a separate persistent-document image profile;
  read spec/data-probe.md and the 2026-11-08 journal/STATE. Native UID1000 writes
  survive two cold boots with a readonly root; root syncs and unmounts the data
  before diagnostic poweroff. Six controller contracts and native record
  Release/ASan/UBSan adversaries pass. UUID/20-byte virtio serial and a retained
  host file handle are bounded fixture observations, never volume/clone/account
  admission. The original data source is immutable; every sequence owns a NEW
  copy and NVRAM. Pending/unknown attempts preserve files and forbid replay.
  Normal app DataEpochs, catalogues and quarantined attempts are unrelated. The
  image has no production MAC policy/login/hardware or power-loss acceptance.
  Minimal udev rules lack full module/device integration; fixed test-account
  paths are not the final account/session design. Independent read-only fsck/
  debugfs verifies actual bytes and ownership after unmount. Next integrate the
  existing desktop/session in this base; full OS/apps/installer/hardware and
  experiment goals remain active. Preserve the original carrier and foreign work.

- tools/build_desktop_probe.py now assembles the existing desktop/apps into a
  separate768MiB system/512MiB readonly-root profile plus256MiB data. Read
  spec/desktop-image.md and the2026-11-08 desktop journal/STATE. Fixed development
  PAM autostart is not production login/MAC/app admission. Keep GUI UID1000,
  NoNewPrivileges/cap-zero and real active-seat/input checks. The bounded root
  VT oneshot uses only SYS_TTY_CONFIG; broker PrivateDevices stays enabled.
  Runtime inventory includes locale/input/module/licence data, not libraries
  alone. Require bounded pinned copy validation and exact fixed image geometry.
  Native owner uses pidfds and a pre-exec barrier; keep unknown attempts/data.
  Accepted two-cold-boot sequences prove real app input, persistent documents
  and normal unmount/poweroff. Separate read-only fsck/debugfs confirms exact
  bytes/UID/mode/link count without changing input. Settings uses SETTLED and
  original-owner exit acknowledgement, not invented READY/CLOSED strings.
  Normal lifecycle/catalogue capabilities and protected carrier stay unchanged.
  The virtio/bochs image is not laptop/USB acceptance. Next early USB-root/device
  selection, module/firmware and single-medium data integration. User explicitly
  authorizes flashing the identified Intenso Speed Line G: when ready; reidentify
  the physical device before writing but do not ask again or wait for their later
  hardware test. Internal disks are not authorized. No USB write yet. Full goal
  remains active; preserve foreign work and the closed NexaUI handoff.

- tools/build_usb_root_probe.py and src/boot/initrd.c now prove early USB root
  and standard systemd initrd handover. Read spec/usb-root-probe.md, STATE and
  the2026-11-08 USB-root journal. Two actual cold boots and wrong-transport/UUID
  pre-mount negatives pass. The fixed512MiB diagnostic is not the physical stick
  desktop. Keep original root bytes and independent input-manifest pins; selected
  libkmod is a udev dlopen dependency, not covered by kmod CLI ldd. Import block
  properties in a rule before matching them. OnSuccess requires an inactive
  oneshot; PID1 OnSuccessJobMode=isolate stops udev before checked SwitchRoot.
  Current boot/devnums/sysfs/diskseq facts are not durable volume/clone trust.
  The prior graphical image lacks libbpf and explicitly disables cgroup BPF;
  its configured DevicePolicy is not accepted as enforced. Add reviewed runtime
  and actual allowed/denied device-open evidence before USB desktop acceptance.
  Next join graphical root/data on one removable medium with independently
  checked common parent/actual stick geometry and laptop DRM/input/firmware.
  VM-test that profile before the already authorized flash. Reidentify Intenso G:
  without asking again; continue work after flashing, never write internal disks.
  Preserve all failed attempts, original carrier/normal inputs/quarantine/CLOSED
  and foreign files. Full goal remains active; NexaUI handoff stays closed.

- tools/device_filter_image.py now adds signed Fedora44 libbpf2:1.6.3-2 and its
  pinned matching BSD licence as data to new graphical images. Read
  spec/device-filter.md and the2026-11-08 device-filter journal/STATE. Carrier
  library stays unchanged; known object-parser reports are not claimed cleared.
  This is trusted PID1-generated instructions, not caller ELF/BPF loading.
  src/boot/device_filter.c checks baseline read-only root block open versus EPERM
  under DevicePolicy=closed, with actual null access in both cap-zero/NNP probes.
  Both oneshots gate VT/desktop startup. The root PARTUUID alias needs a separate
  blkid-import then match rule; old data rules only tag. Two cold boots also pass
  GUI/document/normal unmount-poweroff. This proves fixed probe policy, not all
  app/MAC/device policy or retroactive old-image acceptance. Keep PrivateDevices
  and original runtime pins. Controller requires exact ordered same-boot results
  and rejects mixed/failed/disabled-backend output. Independent ordinary-user
  fsck/debugfs verifies14 document bytes, ownership/mode/link and unchanged hash.
  Preserve failed/unknown attempts, original carrier and foreign work. Next
  single-medium USB root/data, independently checked parent/physical geometry and
  laptop DRM/input/firmware; repeat relevant tests before the authorized G: flash.
  Continue development afterward without waiting for hardware tests. No flash yet.
