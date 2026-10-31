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
The [versioned contracts](../../spec/README.md) and
[native prototype](../../spec/runtime.md) now have a dedicated enforcing Linux
carrier. The fixed test joins package/private-data ownership, separate confined
system/user authorities, authenticated process/image observations, supervised
private views and execution, app registration/readiness and distinct durable
start/run journals. A successful original fixed run can close and release its
code lease. Normal installed lifecycle capabilities remain disabled; general
admission, document transport/revocation, recovery and full removal remain work.
See [State](STATE.md) for current evidence and implementation limits.

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

The native lifecycle prototype is in progress. Boot-bound process identity can
recognize an original still-live process through a fresh authenticated connection.
Separate immutable v4 records now persist the original profile/context in each
owner's catalogue. Actual Root/User restart reconciliation, already-dead process
and lost-handle recovery remain necessary. Current normal catalogues/gates remain
unchanged and closed.

The full desktop and original applications, normal session/app provisioning,
an original bootable image/installer, updates and hardware/performance acceptance
remain required. Backend fixtures and individual software tests do not complete
that scope. Product/UI alternatives will be reviewed with the user later.
