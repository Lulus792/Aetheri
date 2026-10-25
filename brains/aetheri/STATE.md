# State

25 October 2026 — identity/lifecycle accepted; concrete schema/API draft prepared.

## Confirmed

Linux x86-64/UEFI, C17/targeted Rust/adapters, glibc, MIT, modular B,
wlroots, selected systemd/session integration, SQLite, versioned base and managed
apps. Suspend/resume required with independent Intel/NVIDIA acceptance.
Module split and the [identity/lifecycle model](knowledge/identity-and-lifecycle.md)
are accepted, including scoped installations, fresh activations/runs, private-data
epochs, durable requests, retry/expiry, removal fencing and honest recovery.

F: is reported completely clear. Last read-only observation: approximately 232.4 GiB
free. Use only F:/Aetheri; no volume formatting or unrelated path changes.

## Prepared

[Concrete schema/API draft](knowledge/schema-and-api.md): three SQL source files,
one D-Bus XML contract and a disposable standard-library validator. System/user
catalogues have separate trusted writers; common plans/operations/delegation
correlation preserve exact target lifetimes. The proposed API has 14 methods.

Executed with Python 3.12.14 and SQLite 3.53.1: 374 assertions, exit 0.
Selected positive/negative SQL constraints and XML structure/type signatures pass.
Both disposable catalogues pass foreign-key and database integrity checks.
The assertion count is mostly structural validation, not 374 lifecycle scenarios.

## Limits

No production database created/migrated, dependency installed, OS/app service,
VM/kernel, live D-Bus exchange or hardware acceptance. The SQL bootstraps test
catalogues only. Runtime security, concurrency, durable filesystem publication,
power-loss recovery and schema upgrade/downgrade are not verified.

Detailed SQL/XML and proposed budgets remain under discussion. Before services,
freeze canonical artifact/intent bytes/test vectors, peer identity/authorization,
sandbox/storage ownership and cross-authority lease/admission/recovery.
Browser/boot/update integration, shell syntax and UI alternatives remain open.

## Publication and next step

Public planning repository: https://github.com/Lulus792/Aetheri.
Explain and review the schema/API proposal, then design the security/storage and
recovery boundaries needed for a small Linux authority prototype. Keep settled
architectural choices; implementation gates are not new foundation questions.
