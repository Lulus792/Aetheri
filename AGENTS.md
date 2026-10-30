# Working on Aetheri

For software development, first read `D:/Skills/skills/AGENTS.md`, select relevant
specialist skills and read their required entry points and knowledge.
Start at `brains/aetheri/START.md`, then PROJECT, STATE, DECISIONS and QUESTIONS.

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
  current-image/private-data checks. Launch-result completion and executing drain/
  recovery remain pending; administrative failed-test retirement preserves both
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
- Existing projects are read-only references unless a scoped change is requested.
  Physim's future NexaUI migration is a requirement, not an order to modify it here.
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
- No invented commands, screenshots, measurements or hardware acceptance.
- The user confirmed the current workstation is the main-PC target and requires
  future AM5 portability. Read brains/aetheri/knowledge/testumgebung.md.
  Keep the x86-64/UEFI baseline vendor-neutral; no unconditional -march=native.
- Review selected files before public publication. Keep credentials, personal
  machine paths, local state, downloaded/build trees and VM images out of Git.
- No laptop installation disk is selected. Verify exact device identity and
  recovery steps before any destructive hardware test.
