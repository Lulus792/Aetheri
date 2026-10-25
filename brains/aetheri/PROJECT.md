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
Isolated validation exists; no runtime service is implemented.
The [filesystem/permissions direction](knowledge/filesystem-and-permissions.md)
is accepted, including managed paths/ownership, independent documents and
ext4/LUKS2/SELinux plus namespaces as starting foundations.
The [launch/access protocol proposal](knowledge/launch-access-and-removal.md)
now defines admission, native versus direct grants, revocation, save and removal
handshakes. New protocol/schema/API extensions remain for discussion.

Original Aetheri code/documentation: MIT. External components retain their conditions.

## Hardware, storage and security

First target: dedicated HP EliteBook 840 G4 with Intel graphics and 16 GB RAM.
Later target: main computer, four cores/eight threads, RTX 2080 Ti; CPU model,
RAM, ports and precise monitor modes still need collection.
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

Requirements and architecture planning. No implementation, hardware compatibility,
performance or completion-date guarantee follows from this document.
