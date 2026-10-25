# State

25 October 2026 — module and interface plan prepared for discussion.

## Confirmed architecture

Linux x86-64/UEFI, C17 primary, targeted Rust/adapters, existing development tools,
glibc, MIT; controlled modular B architecture, original wlroots-based compositor,
selected systemd/session/sleep foundation, SQLite metadata and versioned read-only
base plus independently managed apps/tools with controlled ownership.
Suspend/resume is required on independently accepted Intel/NVIDIA profiles.

## Completed

Prepared [module/interface plan](knowledge/modules-and-interfaces.md): responsibilities,
process lifetimes, authoritative data owners, privileged versus per-user mediation,
local interfaces, app launch/document access, install/purge/update workflows, display/
lock/suspend and recovery behavior. Source layout is proposed without creating stub
code directories. Existing architecture choices are retained.

New finer choices are recommendations, not implementation selections: exact authority
process split, D-Bus/sd-bus binding, portal/authentication adapters, schemas, operation
signatures and resource limits. No additional backend is implicitly selected.

## Limits

No runtime dependency installed, schema created/migrated, executable implemented,
VM/kernel built or hardware accepted. Document validation does not certify security,
crash recovery, display behavior or performance.

Use F:/Aetheri only. Physim/NexaCloud files, processes and active migrations untouched.
Future Physim/NexaUI migration is not executed by this planning task.

## Publication

Public planning repository: https://github.com/Lulus792/Aetheri.
The module plan is a discussion draft within an approved architecture, not a release.

## Next

Discuss fine-grained module/interface recommendations, then specify identity and
recoverable lifecycle contracts, trusted launch/document access and compositor/
session integration. Pin actual dependencies and perform bounded prototypes only
after concrete contracts and compatibility checks.
