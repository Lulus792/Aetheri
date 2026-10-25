# First Linux prototype planning — 26 October 2026

The user accepted the launch/access/revocation/removal protocol and requested
minimal database/API changes and the first Linux prototype's scope.
Recorded protocol acceptance while keeping new scope recommendations separate.

Prepared one consolidated P0 document with five new table designs, narrow changes
to the existing operation/plan metadata, schema-v2 migration strategy and nine
additive public/private method roles. P0 proves real confinement, supervised launch,
small native read access, revocation and private fixture removal. Edits/streaming,
saved/direct/project grants and graphical work remain later product work.

Defined a headless QEMU/UEFI development carrier recommendation and explicit
acceptance gates. Fedora Server 44 is a test-carrier candidate, not an adopted
production base. Initial proposed allocation: 2 vCPUs, 8 GiB RAM, 48 GiB sparse disk.

Read SQLite migration/version and D-Bus/QEMU/Fedora primary sources. Fedora's
SELinux Quick Docs fetch was challenged; it does not prove runtime enforcement.
Read-only host observations: QEMU tools not on PATH, WSL Alpine-Dev, F: about
232.4 GiB free. No WHPX availability assertion and no downloads/installation.

No SQL/XML/source implementation changed, production data migrated or runtime test
executed. Existing projects, shared WSL and hardware disks remain untouched.
The next actual implementation must establish enforcing Linux boundaries and pass
all P0 gates; initial SQL/XML fixture success is not sufficient.

Documentation validation passed, exit 0: 41 UTF-8 files without BOM/NUL,
115 local Markdown links, balanced fences and valid six-field metadata/project
root. Checked unchanged executable SQL/XML/tool sources. Git diff checks passed.
No new migration/runtime/security or performance test result is claimed.
