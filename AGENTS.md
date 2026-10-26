# Working on Aetheri

For software development, first read `D:/Skills/skills/AGENTS.md`, select relevant
specialist skills and read their required entry points and knowledge.
Start at `brains/aetheri/START.md`, then PROJECT, STATE, DECISIONS and QUESTIONS.

- Follow the user's agreed foundations: Linux x86-64/UEFI, C17 primary, targeted
  Rust and small C++ adapters where justified, existing open development tools,
  glibc and the discussed technical-library boundary. Exact dependency versions
  and substantial new families need evidence and discussion before adoption.
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
- Native C17 ownership/storage primitives and tests exist under src/core and tests.
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
- Distinguish confirmed requirements, architecture proposals, implementation and
  test evidence. No runtime function exists merely because it is documented.
- MIT applies to original Aetheri code/documentation. Preserve third-party licenses.
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
