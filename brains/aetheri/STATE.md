# State

26 October 2026 — lifecycle protocol accepted; minimal extension/P0 proposed.

## Confirmed

Linux x86-64/UEFI, C17/targeted Rust/adapters, glibc, MIT, modular B,
wlroots, selected systemd/session integration, SQLite and versioned base/apps.
Filesystem/ownership with ext4/LUKS2/SELinux plus namespaces is accepted.
The user now accepted the launch/access/revocation/removal protocol, including
native/direct distinctions, truthful closure and graceful versus explicit force.

Work remains inside F:/Aetheri. No unrelated project or shared WSL changed.

## Prepared

[Minimal extension and P0](knowledge/first-linux-prototype.md) proposes schema v2
with five new table designs and nine additive method roles. The first Linux slice
covers real confined launch, a small per-run native read grant, revocation and
exact private fixture removal. Native edits/streams, saved/direct/project grants
and graphics follow later. These scope details remain for discussion.

Proposed carrier: dedicated headless QEMU/UEFI VM, Fedora Server 44 x86_64 as a
development test carrier, 2 vCPUs / 8 GiB RAM / 48 GiB sparse disk.
This is not selection of Fedora as the final Aetheri distribution/base.

## Evidence and limits

26 October read-only observation: qemu-system-x86_64/qemu-img not found on PATH,
WSL lists Alpine-Dev, F: approximately 232.4 GiB free. WHPX status is unverified.
No tool/VM/policy installed and no guest/hardware operation performed.

This turn validates planning documents only. Schema v2, migration, added XML/API,
services and P0 acceptance gates are not implemented or executed.
The existing SQL/XML/tools are unchanged; the historical 374-assertion result
covers only the initial contracts. No new SQL/security/performance claim.

Before P0 implementation, freeze its fixture/header/intent and wire formats,
migration and policy/peer-identity details. Real enforcing Linux isolation,
admission/recovery, read/revoke and old-purge/new-install cases must all pass.
Broader browser/boot/shell/UI/hardware goals remain in project scope.

## Publication and next step

Public repository: https://github.com/Lulus792/Aetheri.
Discuss the concrete P0 proposal, then implement its versioned contracts/tests
and verified VM setup before connecting the actual Linux services.
A SQL-only test or an unconfined run does not satisfy P0.
