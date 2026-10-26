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

## Runtime-format implementation and carrier input preparation

The user asked to begin the next recommended work. Added one runtime specification,
one encoding/payload reference, fixed independent vectors and a format test suite.
The three records bind fixture artifacts, exact private-root ownership and immutable
plan intent. P0 manifests deliberately exclude installation scripts, arbitrary
environment/policy and general signed packages. Vectors use synthetic text instead
of pretending to contain a runnable probe.

Tests reject duplicate/escaping/oversized inputs, changed equal-size payloads,
wrong owner fields and changed plan targets/revisions/effect order. The first run
showed that relying on Python's recursion limit did not enforce the intended JSON
depth. Added explicit depth <=16 and reran successfully. All six intent kinds and
optional bindings are exercised; the codec does not implement business authorization.

Added a carrier input lock and download/verification tool. It never executes or
installs inputs and retains interrupted partials instead of overwriting them.
Cache tests include corruption, oversized downloads, exclusive creation and
publication only after verification. QEMU's downloaded SHA-512 matches; product
metadata says 11.1.0, Windows Authenticode says certificate outside validity.
Inspected installer source requests admin and machine registration; did not run it.
Local portable provisioning/source review remains before VM execution.

Verified Fedora's checksum signature against the official full 44 fingerprint
using an isolated ignored GPG home. The initial Windows-style GPG home was not
understood by Git's MSYS GPG; corrected to its /f/... notation, import/verification
then succeeded. No personal keyring was changed. Media bytes require their own
size/digest verification independently of the signature-list check.

User clarified the development host is the main PC: read-only queries show Xeon
W-2123/4C8T, nominal 32 GB, RTX 2080 Ti and UG25F + two HP E223i monitor records.
Connection codes were interpreted using Microsoft's enum, with no per-monitor
mode/scaling or physical-cable acceptance. Added future AM5 portability requirement
and baseline CPU feature rules, leaving exact upgrade hardware/GPU open.

Read-only WHP query returned S_OK/HypervisorPresent=true (4 bytes written to an
8-byte buffer). This created no partition and ran no virtual processor.
No VM/guest disk, compiler/library installation, Linux policy, C17 service, actual
document IO or private purge exists through this work.

Final host runs: 14 format tests in 0.023 s, 7 cache tests in 0.085 s, existing
23 catalogue/API tests in 27.041 s, all exit 0. Total 44 named tests, with frozen
v1 regression included. Linux principal/storage/exec and recovery gates remain.

Review corrected the header's StorageRef description to match the actual BLOB16
SQL column (hex is its path rendering) and made the lifetime of the future
protected allocation binding explicit. Added rejection of Python's oversized
JSON-integer parsing error to the same bounded input error family. The final
format run above includes that regression.

Carrier acquisition completed, exit 0: QEMU matches pinned size/SHA-512 and
Fedora netinst matches 1228384256 bytes/SHA-256. Fedora's checksum-list signature
had separately matched the official full 44 fingerprint. The isolated GPG agent
was stopped using its verified project-local socket/home, without touching other
agents. No media/installer was executed; VM setup remains pending tool provisioning.

Independent PowerShell Get-FileHash reads matched both pinned digests. Final F:
free space observed about 231.1 GiB; acquisition stayed under ignored downloads.
Text/source validation passed for 56 UTF-8 files, 168 local Markdown links,
balanced fences, JSON and Python syntax. Reviewed selected public files and Git
diff checks; no downloads, keyring, local state, user paths or generated test data
are staged. Frozen SQL/XML and prior migration sources remain unchanged.

## Installed carrier, private host control and native C17 storage

Active goal is full Aetheri implementation. Prepared and actually executed a
dedicated Fedora 44 x86-64 UEFI/WHPX guest, 2 vCPUs/8 GiB RAM/48 GiB sparse disk,
with no shared host folder or physical disk. Reviewed archive extraction reads
unsigned MSI/expired-signature NSIS as data; no installer actions/registry changes.
Pinned 7-Zip 26.05 host-only extraction tool and preserved upstream notices.
The reproducible helper verifies 3324 extracted files and refuses changed outputs.

Observed firmware #GP at MSR 0x3a with unfiltered host VMX before disk writes;
advertising host,vmx=off,svm=off booted. First disk precheck stopped because zram0
was counted as a second disk. Live installer-shell inspection confirmed product,
virtio serial, exact 48 GiB and RAM device; corrected only the RAM exclusion.
The next installation completed/powered off, then the installed disk booted.

SSH inspection initially stalled due to Windows child standard-handle inheritance.
After a direct native SSH control succeeded, fixed explicit handles, ignored
personal configuration/agents and disabled forwarding. Stopped only the verified
own read-only stalled SSH client. Existing shared WSL/other projects were untouched.

Guest baseline checks passed: EFI, ext4, systemd PID 1/running, SELinux Enforcing,
cgroup v2, root mount namespace and bus ProcessFD/LinuxSecurityLabel availability.
Kernel 6.19.10, systemd 259.5, dbus-broker 37, libselinux 3.10/policy 43.3,
Python 3.14.3/SQLite 3.51.2. Reviewed/pinned GCC 16.2.1-2, CMake 4.3.0-1,
Ninja 1.13.2-2, matching sanitizers, glibc-devel 2.43-9 and UAPI headers 7.2.4-200.
DNF installed with signature verification; mirror 404s fell back successfully.
Transaction installed 20/upgraded six packages; glibc is now 2.43-9. No C17 sd-bus/
SQLite/SELinux development packages or Aetheri policies installed yet.

Implemented native C17 owner codec and openat2-based bounded file/header reader,
plus CMake and native tests. GCC baseline reports x86-64/generic, no host-native
ISA assumption. Added real private-namespace same-filesystem bind containment.
The first mount test used a stale binary: deterministic archive times made Ninja
miss changed source. Corrected extraction to use guest times; rebuild then compiled
the actual new mode and all three tests passed.

Debug ASan/UBSan: 3/3, exit 0, 0.06 s. Release: 3/3, exit 0, 0.14 s, including
private mount namespaces. Filesystem checks include symlink/FIFO/hardlink/type,
size, UID/mode and owner mismatch; failed byte decode preserves outputs.
Guest catalogue tests: 23/23, 50.456 s; format tests: 14/14, 0.017 s, exit 0.
These checks do not implement or prove Aetheri app isolation, launch, grants,
revocation, purge, production migration, power-loss or physical hardware behavior.

Security review of the new VM helper found actual guest access to loopback TCP
QMP through 10.0.2.2. Before any untrusted app scenario, replaced it with tested
host-local AF_UNIX/Winsock control. The guest was gracefully powered down before
transport replacement and booted offline afterward. Verified host directory ACL:
inheritance disabled, one current-user-only rule. Old TCP listener is absent;
guest probe returns ECONNREFUSED. Repeated local QMP queries and host SSH pass.
Offline Release tests also pass. The host IPC adapter is separate from Linux
Aetheri D-Bus contracts and is not treated as application security acceptance.

Next: actual authority/broker/launcher/probe, protected registry and peer mapping,
Aetheri app policy and full P0 lifecycle/race/recovery gates, then desktop/apps.
The full user goal remains active; this carrier/core milestone does not complete it.
