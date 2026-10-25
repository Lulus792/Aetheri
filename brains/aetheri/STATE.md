# State

25 October 2026 — initial schema/API accepted; filesystem/permissions proposal prepared.

## Confirmed

Linux x86-64/UEFI, C17/targeted Rust/adapters, glibc, MIT, modular B,
wlroots, selected systemd/session integration, SQLite, versioned base and managed
apps. Suspend/resume required with independent Intel/NVIDIA acceptance.
Module, identity/lifecycle and initial [schema/API](knowledge/schema-and-api.md)
contracts are accepted, including the SQL/XML, failure semantics and initial budgets.

F: is reported completely clear. Last capacity observation: approximately 232.4 GiB
free. Work stays in F:/Aetheri; no whole-volume or unrelated-path changes.

## Prepared

The [filesystem/permissions proposal](knowledge/filesystem-and-permissions.md)
defines a Linux-compatible host tree, readable ownership views, immutable store,
exact StorageRef roots, private app views and independent documents. It compares
enforcement options and recommends SELinux plus namespaces for same-UID boundaries,
with ext4 and LUKS2 as proposed storage foundations.

These new filesystem/security recommendations remain for user discussion.
No new dependency is adopted, downloaded or installed by this planning work.

## Evidence and limits

This turn: researched primary references and validated project documentation.
No Linux permissions, labels, confinement, encryption, mounts or purge tests ran.

Previous SQL/XML validation: Python 3.12.14 / SQLite 3.53.1, 374 assertions, exit 0,
including selected constraints and structural checks for 14 API methods.
That result is historical; it is not a new security/storage test or runtime proof.
No production database, OS service, VM/kernel or hardware acceptance exists.

Before implementation acceptance, specify and test canonical encodings, registered
volume/root identity, ownership headers, authenticated app/principal mapping,
mandatory policy/category allocation, IPC mediation and cross-authority recovery.
Browser/boot/update integration, shell syntax and UI alternatives remain open.

## Publication and next step

Public repository: https://github.com/Lulus792/Aetheri.
Discuss filesystem ownership and enforcement recommendations, then define the
launch, grant, revocation and removal protocol against concrete Linux primitives.
Do not reopen settled architecture or treat a document as operating security.
