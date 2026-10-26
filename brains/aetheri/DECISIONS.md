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

## Identity approval and concrete schema/API draft — 25 October 2026

The user accepted the explained identity/lifecycle recommendations and asked for
concrete SQLite and API contracts. This supersedes the earlier pending status of
that model. Distinct component/artifact/install/generation/data/run/operation IDs,
scope epochs/revisions, exact-target removal, retry/expiry, cancellation and recovery
are accepted in direction.

The [schema/API draft](knowledge/schema-and-api.md) proposes byte encodings,
18 distinct table designs (14 system / 11 user including common definitions),
14 D-Bus methods, explicit errors and initial resource/deadline/receipt budgets.
These new details remain recommendations; no approval is inferred in advance.
Executable SQL/XML fixtures validate selected constraints, not production services.

## Schema/API approval and storage discussion — 25 October 2026

The user accepted the initial concrete SQLite/API recommendations and requested
filesystem structure, storage ownership and permission boundaries next.
The initial SQL/XML, table relationships, operation semantics and starting budgets
are accepted. Canonical artifact/intent byte encodings, runtime authentication
and integration gates remain unresolved as already recorded in that contract.
Earlier pending schema/API status is superseded; no runtime acceptance is implied.

The [storage/permissions proposal](knowledge/filesystem-and-permissions.md)
recommends a conventional Linux technical hierarchy, managed code/data roots,
independent documents, explicit grants and mandatory same-UID separation.
Ext4/LUKS2 and SELinux plus namespaces are new recommendations for discussion,
not inferred dependency approval. No on-disk installation or host policy change.

## Storage approval and launch/access protocol — 25 October 2026

The user accepted the filesystem, storage ownership and permission recommendations.
This includes the proposed Linux-compatible layout, exact private roots versus
independent documents, ext4/LUKS2 and SELinux plus namespaces as foundations.
This supersedes their earlier pending status, not the remaining release/policy/
boot/integration and runtime acceptance gates. No install or host policy change.

The new [protocol proposal](knowledge/launch-access-and-removal.md) describes
root/data/grant admission and drain, native brokered document access, explicit direct
compatibility grants, controlled save/revocation and graceful removal.
These new detailed protocol choices remain recommendations.
The original 18 table designs and 14 methods do not yet implement the identified
grant/edit/gate/dispatch extensions; no silent schema/API expansion is recorded.

## Lifecycle protocol approval and P0 proposal — 26 October 2026

The user accepted the combined launch/file-grant/revocation/removal recommendations.
Durable fences/reservations, native brokered versus direct access, scoped grants,
controlled saves and honest closure/recovery are accepted in direction.
Interactive timeout does not automatically authorize forced termination.
This supersedes pending protocol status, not the unimplemented SQL/API extensions.

The [minimal extension/P0 proposal](knowledge/first-linux-prototype.md) recommends
five new table designs, schema v2 and nine additive public/private methods.
It limits the first implementation to confined launch, small native reads,
revocation and exact fixture removal; edits/bulk/direct/persistent grants follow.
A QEMU/Fedora Server 44 headless development carrier and initial VM resources are
new proposals. They do not select the final Aetheri base or install dependencies.

## P0 approval and contract implementation — 26 October 2026

The user accepted the minimal schema/API/P0 recommendations and explicitly requested
versioned contracts plus migration tests. The five-table first slice, schema v2,
nine additive methods and headless carrier scope are accepted.

Implemented v2 canonical snapshots, XML and a development bootstrap/migration
harness with named SQL/wire/migration tests. Kept v1 SQL/XML byte-for-byte frozen.
The migrator rebuilds under one transaction after a verified backup and preserves
old identities; migration backfills closed gates rather than silently admitting work.

No VM/service implementation or runtime security acceptance follows. Python here
is contract/test tooling; C17 remains the production language direction.

## Next implementation slice and hardware clarification — 26 October 2026

The user asked to begin the recommended next work. Added bounded P0 fixture,
private-root owner and plan-intent encoding references with independent fixed byte
vectors and negative host tests. They do not change frozen v1 SQL/XML or implement
general signed packages. Runtime principal, storage, target-exec and recovery
obligations are recorded in [P0 runtime contracts](../../spec/runtime.md);
their Linux backends remain unimplemented and require guest evidence.

Pinned QEMU 11.1.0 Windows build 2026-08-11 and Fedora Server 44-1.7 network media
for development-carrier preparation. QEMU was linked by the upstream download
page; the provider's SHA-512 matches, but Windows Authenticode reports an expired
certificate. Do not call it a valid Windows signature or silently execute the
installer. A suitable local tool-provisioning path remains to be reviewed.
The Fedora checksum signature was verified against the official full Fedora 44
fingerprint. Downloads/pinning do not select the final Aetheri base or prove a VM.

The user confirmed that the present development workstation is also the main-PC
target. Read-only Windows queries identify Xeon W-2123, nominal 32 GB RAM,
RTX 2080 Ti and three active monitor records. User further requires Aetheri to
work after a future AM5 upgrade. Preserve generic x86-64/UEFI and CPU feature
detection; no Intel-only baseline or unconditional -march=native. Exact future
CPU/mainboard/GPU and Linux physical acceptance remain open.

## Active implementation and installed carrier — 26 October 2026

The user set the continuing goal to implement Aetheri. Keep the complete agreed
daily-use OS/apps scope; the headless P0 carrier is an intermediate development
environment, not completion or a new final distribution choice.

Implemented reviewed local extraction of pinned host tools, guarded guest setup,
source transfer and actual native C17 owner/storage primitives. Neither upstream
installer was executed. QEMU/UEFI/WHPX and the Fedora 44 ext4 guest now execute;
baseline systemd/SELinux/cgroup/ProcessFD prerequisites have actual observations.
Build tooling uses the recorded GCC/CMake/Ninja/sanitizer versions. Original C17
code remains MIT; carrier/tool notices retain their own licenses.

Corrected the new host-control transport before app scenarios: a benign guest
probe could reach loopback TCP QMP through slirp. Replaced TCP with an actual
Windows AF_UNIX socket under a current-user-only directory. IPC access is protected
by the host filesystem, while UUID remains correlation rather than authentication.
No host drive/share, Windows feature toggle or unrelated WSL/project mutation.

Native checks now cover fixed owner bytes, preserved failure outputs, actual
bounded reads and a real same-filesystem bind mount in a private namespace.
Debug ASan/UBSan and Release pass. The core requires a caller-verified protected
root and has no fallback to weaker resolution. This does not approve a finished
Aetheri policy, volume registry, D-Bus authority, app launch or full removal.
