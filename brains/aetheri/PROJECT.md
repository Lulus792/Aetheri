# Project

## Purpose

Aetheri is an original Linux-based personal daily-use desktop system, developed
through Codex. The user wants to understand each major component, directory and
running process. Priorities: original code, polished UI, speed, efficiency and
an understandable installation/runtime footprint.

## First useful workflows

- Normal web browsing in an original lightweight browser UI using a maintained
  external engine. No default news/feed/promotional clutter; user-controlled
  content/ad filtering. Required special sites, extensions and DRM remain open.
- An original terminal and modal-editor-inspired experience, own plugin/design
  system, image/PDF viewing and integrated tmux-style panes/session management.
  Exact Neovim compatibility is not required by the stated inspiration.
- An original shell and shell language from the first shell milestone.
- Physim, intended to migrate from its current UI to NexaUI when the toolkit is ready.
  This project does not silently start that migration.
- NexaCloud Desktop, confirmed as the desired NexaOS-associated data application.
- Up to three monitors across 1080p, 1440p and 4K, with flexible scaling.

Apps also include Files, Settings/device management, software/development-tool
management and image/PDF viewing. Prefer one coherent management UI for monitors,
audio, Bluetooth, keyboards and mice over separate vendor-style utilities.
Supported device capabilities still require an explicit hardware matrix.

## Experience and visibility

Free windows, dock/taskbar, global search; macOS-inspired calm, compact dark themes,
glass/transparency, accents and animation. Later present multiple visual designs.
German/English UI, German/US layouts, extensive keyboard operation and scaling.
The user retains product/UI design choices.

Provide a component view explaining why something is installed, what it owns and
what services it uses. Offer clear app removal versus complete managed-state
removal. Include configuration, caches, plugins, registrations, credentials and
background services, with previews and honest shared/retained-state reporting.
See [application lifecycle](knowledge/app-lifecycle.md).

Normal system use works offline, without cloud accounts or online activation.
Internet is needed only for relevant app use and chosen downloads/updates.
There is no general requirement to run arbitrary Linux/Windows software.

## Agreed foundations

Linux x86-64/UEFI; C17 primary, narrowly justified Rust and small C++ adapters;
existing open compiler/build/debug/VM tools; glibc. Reuse the discussed graphics,
input, crypto, browser and document foundations where justified. Useful maintained
audio/Bluetooth/WLAN backends need not be replaced merely to increase original code.
Exact component versions and transitive footprints must be reviewed before adoption.
NexaUI remains subject to suitability and performance acceptance.

Selected architecture: controlled modular system (B), an original Wayland
compositor based on wlroots, selected systemd/session/sleep components and SQLite
for component metadata. The system base uses versioned read-only deployments;
apps/tools are separately managed with controlled private state and shared runtimes.
This selects responsibility boundaries; exact releases, boot/update engine,
on-disk deployment format and browser integration still need their contracts.

The accepted [module/interface plan](knowledge/modules-and-interfaces.md) separates
system installation authority, per-user mediation, compositor, desktop UI and apps,
with typed local control and owner-scoped metadata writers. The
[identity/lifecycle model](knowledge/identity-and-lifecycle.md) is accepted,
including distinct installation/generation/data/run/operation lifetimes.
The initial [schema/API contract](knowledge/schema-and-api.md) is now accepted:
SQL constraints, D-Bus signatures, failure semantics and starting budgets.
Isolated validation and two prototype protocol endpoints exist; normal lifecycle
capabilities remain disabled. The explicit fixed-run test has separate native
start and v3 lifetime journals; general provisioning/recovery/removal remain work.
The [filesystem/permissions direction](knowledge/filesystem-and-permissions.md)
is accepted, including managed paths/ownership, independent documents and
ext4/LUKS2/SELinux plus namespaces as starting foundations.
The accepted [launch/access protocol](knowledge/launch-access-and-removal.md)
defines admission, native versus direct grants, revocation, save and removal.
The accepted [P0 scope](knowledge/first-linux-prototype.md) narrows the first
implementation to confined launch, small native reads, revocation and fixture
removal on a dedicated Fedora/QEMU development carrier.
The [v2 SQL/XML and migration harness](../../spec/README.md) implement the contract
portion. An installed headless development guest and tested C17 ownership/storage
primitives now exist. System/user service roles also expose protocol discovery
under dedicated enforcing domains. A native observer now joins held process
references to current credentials, service invocation and cgroup identity; the
system endpoint verifies its own supervised context. A protected single-account
broker enrollment now binds the actual user-service connection and invocation.
The system catalogue also enrolls the fixed test account without UID/scope rebinding.
Native fixture decoding and SHA-256 now validate canonical package descriptions
against an independently expected ArtifactId and verify held-file content.
The native payload stager checks complete inventory and copied bytes. The protected
system-domain administrative path now binds a trusted native fixture to its
preallocated carrier slot and operation journal, including cross-boot reopening.
The trusted fixture can now be published as one sealed manifest/payload object
under its ArtifactId, with immutable operation evidence and exact retries.
First activation now completes that original fixed installation atomically while
leaving its launch gate closed. General dynamic allocation, updates and admission
remain separate work.
The user authority can now observe this active fixture over authenticated private
IPC and persist an owner-scoped installation binding. App starts still require
fresh run identities and admission checks.
The explicit P0 administrative path now enrolls a fresh private DataEpoch and
StorageRef for that installation, with an independently verified allocation and
closed data gate. This does not implement app launch or general dynamic allocation.
The private system API now records exact code reservations and permits scoped
status/withdrawal before dispatch. It is exercised over the real broker connection;
durable user launch acceptance and pre-dispatch withdrawal after broker replacement
also work for the fixed fixture. Native private code/data mount views now have
real kernel tests; connecting them to supervised setup/readiness, public launch
and process dispatch remains pending.
Retained private-root references join expected owner headers with current
held/named filesystem identity. Immutable allocation object records now persist
that binding and reopen after a guest boot. Genuine volume enrollment and complete
allocation-journal/admission integration remain pending.
App/run registration and admission, launch and transport remain next.

Original Aetheri code/documentation: MIT. External components retain their conditions.

## Hardware, storage and security

First target: dedicated HP EliteBook 840 G4 with Intel graphics and 16 GB RAM.
Main-PC target is confirmed as the current development host: Xeon W-2123,
four cores/eight threads, nominal 32 GB RAM and RTX 2080 Ti. Three active monitor
records were observed; precise per-monitor modes/scaling still need collection.
A future AM5 upgrade is an additional x86-64/UEFI portability requirement; exact
board/CPU/GPU and physical acceptance remain pending.
WLAN, Bluetooth, wired/Bluetooth input and 3.5 mm/Bluetooth headphones are required.
Suspend/resume is required, with separate laptop and main-PC acceptance. Hibernate
has not been requested.

VM tests run on the current development machine, up to 16 GB RAM.
F: is now available for Aetheri. Use F:/Aetheri; protect existing Physim/NexaCloud
data and ongoing migrations/processes. The initial 32 GB discussion no longer
defines a permanent total ceiling, but no unspecified storage is disposable.

Separate users, permissions/sandboxing, encryption, recovery and backups are goals.
Secure Boot may be included; it is no longer excluded. Security mechanisms and
acceptance gates require plain-language explanation and technical design.
Updates: notify, install on explicit choice, verify signatures and retain a
recoverable previous system version.

## Current stage

The user is temporarily unavailable and authorizes well-supported recommended
technical choices with documented reasons. Defer personally important product/UI
and destructive hardware decisions until they can participate; continue independent
work. OpenSSL 3.5 LTS/libcrypto is explicitly accepted and adopted after exact
Fedora 1:3.5.9-1.fc44 RPM/signature/license review. Existing foundational choices
remain unchanged.

Implementation in progress: contract tooling, dedicated Linux carrier and native
C17 core, confined endpoints, supervised no-exec setup and isolated one-way
execution/image primitives. Their connection to real app starts remains next.
An independent compatible execution fixture is now published/activated alongside
the original. Its separate user binding/private data are now enrolled with closed
gates. Separate native execution intent/binding/preparation/arm transactions are
now implemented and tested; live target-start integration remains next.
An explicit enforcing-carrier integration now joins the real native Root journal,
supervisor request, authenticated starter, witnessed private mounts and target
exec. A fresh post-exec connection supplies actual credentials and a separate
administrative driver checks the running ELF. Its isolated trusted package,
allocation, catalogue and broker/data context remain controlled test inputs.
Actual original process/unit/cgroup/job drain permits exact test retirement;
native unresolved lease/operation history is archived without claiming release.
Normal installed inputs/gates remain unchanged and closed. Normal Root/User
provisioning, current admission, target registration/readiness and executing
recovery/removal remain work. The old non-executing helper/proofs stay separate.
The User owner now also has a native execution preparation transaction: it
records the complete profile before a possible Root executing request, with no
submission permission on exact retries. Startup retains the obligation, and the
old pre-dispatch User withdrawal path refuses it. Actual authenticated owner
wiring and executing recovery remain work; no installed launch is enabled.
The subsequent enforcing fixture connects the actual registered User owner,
authenticated Root projection/receipt, independent allocation checks and native
preparation to Root request/arm and exact target ELF observation. Root keeps its
original broker pin; normal owner/session provisioning, mutation/revocation and
executing recovery remain separate gates. No normal launch capability is enabled.
The fixed live fixture now also authenticates the actual app to User using its
original connection/ProcessFD, unit/invocation/cgroup and Root-observed executable.
Separate native registration and explicit readiness commits preserve both open
owner operations and Root's lease. This is tested fixed-fixture behavior, not
normal session/app provisioning, UI responsiveness or executing recovery.
Full Linux app lifecycle, the desktop, an
original bootable image and hardware/performance acceptance remain unimplemented.
Root now also persists the actual observed execution after matching its original
starter/app pin and service context to the original User's image/registration
confirmation. Both owner operations and the running generation lease remain;
launch-result completion and eventual lifetime/drain are separate upcoming steps.
Historical retries preserve later states instead of becoming another start.
Separate authenticated Root start-result receipts and User acknowledgements now
complete both start operations in the fixed live fixture. They keep the observed
dispatch, running generation lease and running/ready User launch intact. Execution
recovery/run closure, complete removal and normal session provisioning remain
separate work; the full desktop/apps/image/hardware objective is unchanged.
