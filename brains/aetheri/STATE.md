# State

25 October 2026 — storage/enforcement accepted; launch/access/removal protocol proposed.

## Confirmed

Linux x86-64/UEFI, C17/targeted Rust/adapters, glibc, MIT, modular B,
wlroots, selected systemd/session integration, SQLite, versioned base and managed
apps. Suspend/resume required with independent Intel/NVIDIA acceptance.
Module, identity/lifecycle, initial schema/API and filesystem/ownership directions
are accepted. Ext4, LUKS2 and SELinux plus namespaces are accepted starting
foundations; exact releases, tools, policy and runtime acceptance remain pending.

Work stays in F:/Aetheri. No unrelated project, drive or migration is changed.

## Prepared

[Launch/access/removal protocol](knowledge/launch-access-and-removal.md) proposes
durable root/data/grant gates, reservation/ready/dispatch/drain handshakes, native
brokered versus direct access, grant lifetimes, coordinated local saves and
truthful revocation/removal. Interactive stop does not silently escalate to kill.

The proposal identifies grant/resource/access/edit and dispatch/gate persistence
and typed API additions absent from the current SQL/XML nucleus. Existing source
contracts are unchanged; no migration or service has been implemented.

## Evidence and limits

This turn researches primary references and validates documentation only.
No app, grant, stream, policy, mount, encryption, stop or deletion was executed.

Historical SQL/XML check: Python 3.12.14 / SQLite 3.53.1, 374 assertions, exit 0.
It covers selected core constraints and structural checks for 14 methods, not the
new protocol or Linux security. No production catalogue, VM/kernel, OS service
or hardware acceptance exists.

Before implementation, freeze the added record/wire formats, canonical encodings,
registered-root identity, sender/category/IPC mapping, stream/commit ordering,
supervisor/launcher setup and crash recovery. Browser integration, boot/update,
shell syntax, terminal-session lifetime and UI alternatives remain open.

## Publication and next step

Public repository: https://github.com/Lulus792/Aetheri.
Discuss protocol recommendations, then define the minimal grant/dispatch schema
and API additions and a bounded Linux prototype acceptance matrix.
Keep accepted architecture; planning is not operating security or performance evidence.
