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
