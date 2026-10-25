# Open questions

The main language/library/ownership policy is agreed. Do not ask the original
foundation questions again. Remaining decisions belong to architecture or concrete
product behavior.

## Browser

B1. Which everyday sites define acceptance? Are browser extensions, DRM streaming,
WebRTC calls and password management required? Existing engine accepted; CEF versus
a WebKit integration and prebuilt-versus-source delivery still need a measured comparison.
No browser engine can promise universal website or ad-blocking compatibility.

## Shell, editor and sessions

S1. Design the original shell syntax with concrete examples before implementation:
commands, paths/quoting, pipes, values/variables, errors, scripts and background jobs.
Decide whether compatibility with existing shell scripts is needed at all.

S2. Define the editor/plugin capability model and terminal pane/session interaction.
Need exact Vim/Neovim compatibility only if subsequently requested.
Should terminal sessions survive closing every window, logout or reboot? These
are different requirements and affect supervision/persistence design.

## Hardware and daily use

H1. Main PC CPU model, RAM and monitor connectors/dock; monitor refresh rates and
whether mixed scaling must work simultaneously. No new hardware inspection implied
by the user's broad four-core/eight-thread/RTX 2080 Ti description.

H2. Suspend/resume is confirmed and no longer an open product choice. Select and
validate sleep modes, lid/idle policy, locking and device recovery on each hardware
profile. Hibernate remains separate. Confirm microphone/special peripheral features later.

## Architecture and security

A1. Architecture is approved: B, wlroots, selected systemd components, SQLite and
versioned base/managed-app separation. Do not reopen these settled choices.
Remaining work: exact versions/build options and compatibility, sandbox/update/boot
mechanisms, browser embedding, filesystem implementation and plugin interfaces.
The [comparison](knowledge/architecture-options.md) preserves rationale and alternatives.
The [module/interface plan](knowledge/modules-and-interfaces.md) is now accepted
in direction. The initial concrete management contract is accepted, not installed.
The [identity/lifecycle model](knowledge/identity-and-lifecycle.md) is accepted.
The [schema/API contract](knowledge/schema-and-api.md), including SQL/XML and
starting error/deadline/retention rules, is now accepted.
Discuss the [filesystem/permissions proposal](knowledge/filesystem-and-permissions.md):
technical paths versus readable UI, private-data roots and documents, narrow
grants and SELinux as the recommended mandatory enforcement candidate.
Ext4/LUKS2 and SELinux are new recommendations, not assumed dependency approval.
Before implementation, freeze artifact/intent canonical encodings with test vectors,
authenticated peer/principal mapping, filesystem identity and cross-authority
admission/recovery. Isolated SQL checks do not settle these integration gates.

A2. Explain the proposed daily-use gates: encryption protects data on storage,
sandboxing restricts app access, backups recover lost/corrupt data, and trusted boot
checks early software. Secure Boot may be included, but no keys/settings are changed.
Design separate VM/test-data and personal-data acceptance.
The storage proposal recommends encrypted personal-use storage; boot/unlock,
recovery keys, policy maintenance, label/category allocation, exact ownership-header
bytes and registered-volume identity still require concrete implementation contracts.

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
