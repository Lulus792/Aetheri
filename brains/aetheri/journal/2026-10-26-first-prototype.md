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

## P0 approval and versioned contract implementation

The user accepted the P0 recommendation and requested actual SQL/API contracts
and migration tests. Added v2 snapshots, additive Documents1/Runtime1 XML,
development bootstrap/migration utility and a standard-library test suite.
V1 reference SQL/XML hashes remain unchanged.

The migration validates a distinct consistent backup, rebuilds tables under an
exclusive local transaction, preserves all old columns/rows/IDs and backfills
closed gates. It rejects schema drift/unknown versions and detects a writer
between backup and lock. No automatic backup overwrite/restore or downgrade.

Initial wire tests found contradictory size annotations; removed the duplicate
and added a duplicate-annotation check. A misplaced assertion in the expanded
test fixture was corrected. Added constraints/tests preventing pruning of live
access/dispatch/launch obligations and reviving terminal identities.

Final complete run: Python 3.12.14 / SQLite 3.53.1, 23 named tests, exit 0,
27.009 seconds. Includes populated migration and fresh-schema equivalence,
20 checkpoint-failure subcases, six process-exit/reopen cases (pre/post commit),
read-only and page-limit SQLITE_FULL failures, FK validation, scope/context/revision
checks and static wire conformance. V1 regression included.
No live D-Bus, Linux policy, app/document IO, VM/hardware or physical power-loss test.

Final document/source checks passed: 49 UTF-8 files without BOM/NUL, 142 local
Markdown links, balanced fences, Python syntax and valid brain metadata/root.
Git diff checks passed. Contract documentation is in spec/README.md; generated
fixture databases/backups stay under ignored out and are path-checked on cleanup.
