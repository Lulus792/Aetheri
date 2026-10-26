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

## Native bus credential foundation

Published carrier/core milestone 8fdaf5c to the public repository; remote main
matched and working tree was clean before this next slice.

Reviewed the selected sd-bus API at v259 and actual Fedora metadata before adding
systemd-devel 259.10-1, sqlite-devel 3.51.2-2 and libselinux-devel 3.11-2.
The signature-checked transaction installed eight/upgraded twelve packages, about
16 MiB inbound/7 MiB extra. The explicit version pins are recorded in vm-inputs.
systemd/libselinux/SQLite runtime packages updated accordingly. No Windows or
other-project changes, no new Aetheri daemon or policy activation.

Added optional C17 Linux peer library: unique sender/bus ID, authenticated UID,
PID, SELinux connection label and owned CLOEXEC ProcessFD. Required fields and
augmentation masks are checked with no numeric-PID fallback. Liveness observes
the held process; current-domain, app/run and supervisor authorization remain open.

Real user-bus test uses separate known client/server processes. It checks actual
credentials, 128 captures without FD growth, invalid message/output preservation,
exit/reaped-process detection and owned FD close. Negative test-only linker
wrapping supplies full-but-augmented or incomplete credentials, both rejected.
No fake positive capture or Aetheri app-confinement claim.

Graceful carrier shutdown and offline boot succeeded after the SDK update.
Four Debug ASan/UBSan tests pass in 0.26 s, four Release tests in 0.05 s, exit 0.
All nine carrier prerequisites pass again; SELinux remains Enforcing and systemd
running. Full P0 and the daily-use OS/apps goal remain unfinished and active.
The unchanged 23 catalogue and 14 format tests pass again after the SQLite package
update in 42.982 s and 0.016 s respectively, exit 0.

## Partial enforcing MAC proof

Published peer milestone 9d557e1 and verified remote main. Added one original
CIL fixture module, one C17 probe and an explicit root-only carrier controller.
No new tool family/package was needed: the installed carrier semodule supports CIL.
Production policy, protected writers, category allocation/reuse and the full
P0 authority/launcher/grant/removal implementation remain pending.

The first attempt failed before target exec at mount-namespace setup: enforcing
AVC denied init_t mounton of the new data directory. Added only that supervisor
permission. The next attempt failed before target exec at entry-file execute;
added init_t read/open/getattr/execute/map for the exact new executable type.
Neither correction added app mount rights or disabled enforcement. Both failed
owned fixtures were inspected, their inactive/failed/no-process units and loaded
CIL bytes verified, then their exact module/files removed before retry.

Both probes subsequently passed under UID 1000 and aetheri_p0_app_t with category
pairs c21,c22 and c23,c24. The same data type/UID serves both roots. Each reads,
appends and creates its own file with the exact inherited label; sibling read,
synthetic broker state, unselected document and sudo exec return EACCES. Eight
enforcing AVCs corroborate category search/type read/execute denials. Outside-app
same-UID controls read all targets and can use the fixture administrative sudo.

The final rebuilt Release probe also checks actual empty effective/permitted/
inheritable capability sets and NoNewPrivileges. That repeated two-fixture run
passes and removes its exact module/root. Build result/attempt records stay in
the guest build tree. Eleven host guard tests pass in 0.164 s, including refusal
to infer safe cleanup from an unacknowledged dispatch. No arbitrary recursive
purge, foreign policy replacement or production-removal claim.

Partial A3 is evidenced; real authorized broker IPC, ptrace/FD-transfer cases,
production domains, app/run mapping, exec-event observation and complete P0 gates
remain open. Full daily-use Aetheri/apps goal stays active.
After cleanup, all four existing Debug/Release tests pass again (0.19/0.05 s,
exit 0). Cleared only the two verified stopped failed-test unit records; their
diagnostic attempt files/audit history remain. Systemd reports running, SELinux
Enforcing, with no owned policy module/root left. The existing semodule stack is
policycoreutils/libsemanage 3.10-1 and libsepol 3.11-1.

## Native SQLite catalogue startup

Previous goal turn made verified progress (published core/peer/MAC milestones).
Revalidated clean checkout 28782a3 and actual live QMP/process state; reused the
same offline guest rather than starting another VM.

Added C17 catalogue creation/opening, exact v2 schema/metadata validation, stable
writer-lock lifetime and startup-context transaction. Python builds an ignored
generated/schema.c from the canonical v2 files; no production interpreter or
editable runtime SQL source. No new runtime library family or SDK transaction.
SQLite SDK remains the reviewed 3.51.2-2; CMake's new imported target name is used
with its documented alias for the older declared build-tool floor.

The store requires protected 0700 directory/0600 singly-linked files, expected
account/authority identities and one writer. It rejects sidecars/path replacement,
uses the normal Unix VFS/NOFOLLOW, closes preflight DB descriptors before SQLite
can hold POSIX locks and rejects fork-inherited handles. MAC/volume/registry and
actual kernel boot/runtime generation remain external caller duties. No authority
daemon/public RPC or native v1 migrator was installed.

Native tests cover both kinds, creation/reopen/restart triggers, wrong identity,
schema/literal/version drift, corruption/FK/CHECK failures and revision overflow.
Page-limit injection causes actual SQLite SQLITE_FULL during creation and leaves
no partial schema/version. Failed directory flush after commit preserves the
visible committed catalogue for explicit existing-file resolution. Test-only
precommit/lost-ack errors and four real child-process exits across both kinds
show old/new runtime and gate state as expected. No physical power-loss claim.

A symlink-alias test initially expected ELOOP; actual openat2 with final
O_DIRECTORY|O_NOFOLLOW returned ENOTDIR. Independent syscall observation confirmed
rejection, so the test now accepts those two valid refusal classes. The terminated
failed fixture was inspected and only its three known files/directory removed.
No containment flag or fallback was weakened.

Added native opening of populated Python-migrated system/user catalogues: every
row and database byte remains unchanged. Native exact comparison exposed the
development normalizer's excessive whitespace handling inside SQL literals;
corrected tokenization to preserve literal bytes/escaped quotes and added two
parser cases plus real SQLite literal-drift rejection. Frozen v1 SQL/XML unchanged.

Latest native run: 8/8 Debug ASan/UBSan in 21.21 s, 8/8 Release in 19.01 s, exit 0.
Guest Python 3.14.3/SQLite 3.51.2: all 26 catalogue/API tests pass in 47.238 s.
Host Python 3.12.14/SQLite 3.53.1: all 26 pass again in 35.068 s, exit 0.
Full daily-use OS/apps goal remains active; the next integration is actual
system/user authorities, protected registry/domain setup and end-to-end P0.

## Confined discovery endpoints and empty-bootstrap lifecycle

Implemented one C17 executable with two authority roles, fixed protected bootstrap
registries, dedicated enforcing types and real boot/runtime sourcing before name
ownership/readiness. Accepted XML generates native method tables. Discovery works;
all lifecycle capabilities remain absent and return explicit errors. No new
dependency family or SDK install. Full scope remains the daily-use OS/apps.

Actual integration exposed separate setup failures: RestrictSUIDSGID seccomp
rejected required openat2, root-directory opens required explicit authority
permissions, SQLite journal mode setup required own-file setattr, and readiness
needed the observed notify-socket/peer permissions. A ProtectHome exception also
needed a precreated, separately typed socket mount target. No weaker openat2 path,
app mount/privilege grant, permissive mode or broad init user-file creation.

Both endpoints and live method shapes pass against the XML. The actual confined
same-UID client verifies its label/UID/NoNewPrivileges/empty caps, discovers the
protocol and accesses its own scratch. Private registry/catalogue, raw user
systemd, system bus, unexpected FD calls and unowned authority-name takeover are
denied; unsupported functions and bad argument types return the expected errors.
Restart preserves durable epoch/boot, changes runtime/events and advances revision.

The explicit root controller serializes actions, preserves pending dispatches and
checks recorded process/cgroup/job drain without SIGKILL. An unknown fixture file
blocks cleanup and survives. After removing only that exact synthetic file, the
inspected empty bootstrap, bus files and module were removed and prepared again;
new owner/epochs and both fresh endpoints pass. This is not app removal or
power-loss acceptance. Interrupted admin update/cleanup still needs inspection.
User roles follow the login manager; logout can stop them, no lingering enabled.

Native Debug ASan/UBSan 8/8 in 20.25 s and Release 8/8 in 19.86 s, exit 0.
Host orchestration/cleanup 17/17 in 0.198 s, exit 0. A 5.023 s Release idle sample
has one task each, zero reported CPU/context-switch deltas, PSS 2184/2147 KiB,
RSS 6532/6340 KiB and cgroup MemoryCurrent 1634304/1650688 bytes (system/user).
Accounting scopes differ; this is not a precise wakeup counter, long-term workload
measurement, startup/latency acceptance or finished-OS budget. Units were stopped;
the recreated bootstrap/policy/bus files are retained inside the guest.

Next: real broker/app/run registration and authorization, volume/read references,
fixture import/exec supervision, then document access/revocation/removal/recovery.
No original desktop, apps or Aetheri image is claimed. Existing projects and
physical disks remain untouched; logs/receipts stay under ignored local state.

The unchanged contract/frozen-reference suite was rerun on host Python 3.12.14 /
SQLite 3.53.1: 26/26 in 29.508 s, exit 0.

## Live process and invocation context

Added src/linux/process C17 observations from a held bus ProcessFD: required kernel
PID/credential/cgroup fields, current SELinux label, actual systemd .service
identity/active invocation, exact cgroup-v2 path/inode and repeated liveness/context.
Error outputs remain unchanged. Expected records belong to trusted supervision;
this does not register a broker/app or authorize any lifecycle operation.

The root authority now verifies its own supervised invocation/boot on discovery.
Its explicit self pin cannot replace a caller ProcessFD. Actual enforcing checks
required root-domain manager status and cgroup directory/filesystem reads. No
capability, tracing, cgroup write, user-manager access or permissive mode added.
Existing app denials/discovery and actual service restarts still pass. Units were
stopped after the final check; the dedicated bootstrap remains in the guest.

Tests launch a unique system service as UID 1000 using the developer's unconfined
context, NoNewPrivileges and empty cap bounds. Direct init_t execution of the
home-directory binary was denied; the final fixture enters through the existing
env executable without granting init_t broad home-file execution. It remains a
test fixture, not the Aetheri launcher or proof of target exec. Unit/Service
property separation was corrected after the live ControlGroup query showed the
wrong interface. One fault case initially retained a deliberately changed peer
label; restoring it isolates the intended second-observation credential fault.

Actual kernel/bus positives and seven stale expected bindings pass. The same unit
name is started twice after verified prior completion: new invocation accepted,
old expected context denied. Missing fields, unsupported ioctl, wrong cgroup and
changed second-observation credentials are test-only injected; actual exit and
32 peer/eight self FD-balance checks also pass. Native full suites pass 9/9 Debug
ASan/UBSan (32.56 s) and 9/9 Release (27.86 s), overlapping policy compilation.
Final focused tests after guard/string/self-pin changes pass in 0.72/0.60 s.

A later 5.030 s idle sample after self-check integration records one task each,
zero reported CPU/context-switch deltas, PSS 2184/2267 KiB, RSS 6508/6640 KiB,
MemoryCurrent 1630208/1658880 bytes (system/user). This is a short sample with
different memory accounting scopes, not a finished-OS or workload budget.

The selected SDK/library versions are unchanged. Glibc's sys/pidfd.h supplies the
ABI without the Linux/libc fcntl redefinition conflict. Current credentials are
separate observations, never claimed as additional bus-authenticated provenance.
Next: bind evidence to protected broker/app/run records and current fences, then
fixture import/reserve/ready/dispatch/target-exec. Full daily-use OS goal remains active.

Unchanged frozen contracts/migration regression rerun on host Python 3.12.14 /
SQLite 3.53.1: 26/26 in 29.069 s, exit 0. The isolated validator also passes its
374 assertions. These add no process/policy claims beyond the native evidence.
