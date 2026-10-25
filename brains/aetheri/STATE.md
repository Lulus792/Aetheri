# State

25 October 2026 — module direction accepted; identity/lifecycle contract prepared.

## Confirmed

Linux x86-64/UEFI, C17/targeted Rust/adapters, glibc, MIT, modular B,
wlroots, selected systemd/session integration, SQLite, versioned base and managed
apps. Suspend/resume required with independent Intel/NVIDIA acceptance.
The user accepted the module/interface direction: system authority, per-user broker,
compositor/UI separation, typed local control and owner-scoped metadata writers.

F: is reported completely clear. Read-only observation: approximately 232.4 GiB free.
Use only F:/Aetheri; do not infer permission for volume formatting or unrelated paths.

## Prepared

[Identity/lifecycle contract](knowledge/identity-and-lifecycle.md) distinguishes
logical components, immutable artifacts, installation lifetimes, activations,
private-data epochs, actual app runs and durable mutation intents.
It proposes scoped revisions, retry/expiry rules, launch/removal admission,
recovery, cancellation, shared references and bounded receipts.

Examples cover update/rollback, close/reopen, retained-data reinstall, old purge
versus new install, missing startup acknowledgement and catalogue restore.
These are contract examples and future acceptance cases, not executed tests.
Fine-grained recommendations remain for user discussion.

## Limits

No runtime component installed, schema created/migrated, app/VM/kernel implemented
or hardware accepted. UUIDs and SQLite do not by themselves supply authorization,
filesystem identity, cross-service atomicity or exactly-once side effects.
Exact schema/wire types, limits, browser/boot/sandbox/authentication integration and
shell syntax still require their contracts and compatibility work.

## Publication and next step

Public planning repository: https://github.com/Lulus792/Aetheri.
Discuss identity/lifecycle recommendations, then define concrete schema constraints,
request formats and failure fixtures. Keep accepted architectural/module decisions.
