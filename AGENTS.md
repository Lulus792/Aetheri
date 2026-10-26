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
  This does not implement Linux authorities, policy or document transport. Native
  edits/bulk/direct/saved access remain later work. VM/tool installation has not occurred.
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
- Review selected files before public publication. Keep credentials, personal
  machine paths, local state, downloaded/build trees and VM images out of Git.
- No laptop installation disk is selected. Verify exact device identity and
  recovery steps before any destructive hardware test.
