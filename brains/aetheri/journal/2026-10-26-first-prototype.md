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

## Protected broker enrollment

Added private Broker1 1.0 Register/Check contracts without changing public wire
1.1, schema v2 or frozen v1 files. Root-owned bootstrap account/epoch records
and a selected supervised service identify the sole eligible test broker. Its
actual bus connection, ProcessFD, current credentials/domain and service invocation
bind its declared runtime. Registration permits the selected Type=notify service
to be activating; subsequent checks require active state.

The cache owns one process pin and targeted name/exit subscriptions. Name loss
and process exit independently invalidate identity. NameHasOwner after subscription
rejects an already-disconnected sender; state is rechecked after nested bus calls.
Invalidation does not settle app holders, leases or uncertain operations.

Ten native tests passed in Debug with ASan/UBSan (30.75 s) and Release (19.18 s).
After the final name-presence fence, the focused cache test passed again in Debug
(0.05 s) and Release (0.01 s). It uses mocked process context with real bus-name
and pidfd events; separate enforcing integration checks supply actual credentials.
Host contract tests passed 27/27 (33.642 s), orchestration guards 18/18 (0.164 s).

Actual offline enforcing service checks reject copied identity claims from both
an ordinary same-UID process and another service in the broker domain. Stopping
the broker reduces system-service FDs from 12 to 11; replacing it restores one
pin. Restarting only the system authority allows the existing user service to
re-enroll without replacing its process/runtime. The final repeat passed after
the name-presence fence. Both owned authorities were then stopped normally.

The guarded empty-bootstrap cleanup/recreation and fresh-identity checks also
passed with the new slot/probe inventory. A 5.024 s idle sample reports one task
per authority and zero reported CPU/context-switch deltas; PSS was 2260/2297 KiB,
RSS 6748/6656 KiB and MemoryCurrent 1904640/1662976 bytes (system/user).
These are short prototype observations, not finished-OS performance acceptance.

No SDK/package version or dependency family changed. General account eligibility,
app/run mapping, registered volumes, admission, target-exec and document/removal
protocols remain next. The full daily-use OS objective remains active; future
AM5 support remains a recorded portability requirement awaiting concrete hardware.

## Durable test-account enrollment

Implemented system-owner account enrollment/check in the existing native store,
using the unchanged scopes schema. Exact active retries preserve the database;
retired/differently bound identities and live UID collisions fail. Row insertion
and catalogue revision advance share a commit. The authority now requires its
protected fixed account on ordinary startup and broker registration/use. Explicit
initialization or a stopped administrative command can enroll it; no app-selected
UID/account/path, account retirement API or private-data adoption is added.

Eleven native tests passed, exit 0: Debug ASan/UBSan 26.96 s, Release 22.61 s.
The account case checks UID/owner/fork boundaries, the largest accepted UID,
idempotence, retired-scope rejection, a new identity after UID reuse, revision
overflow and pre/post-commit error/process exits with reopen and exact retry.
Twenty host guards passed in 0.179 s, including non-bootstrap scope preservation
and administrative unit receipt/role boundaries.

Actual offline enforcing integration: an older bootstrap with no account row
refused root startup with ENOENT, no acquired name and unchanged database hash.
Native enrollment succeeded; an exact retry preserved complete bytes and revision.
The first later controller load rejected the new system-enroll unit because its
receipt grammar had not been extended. Fixed the grammar, excluded administrative
units from active-authority roles and added a roundtrip regression before continuing.

Broker replacement and root-only restart passed again (retained pin 12 to 11 on
departure). Full restart, unknown-entry preservation, guarded empty-bootstrap
removal and fresh initialization/recreation passed with the account row. The
controller permits only the original active revision-1 bootstrap scope for this
administrative cleanup; other account state requires reconciliation.

No dependency/package/policy versions changed. Next: native fixture publication,
protected machine/volume/read references and app/run admission, then actual
reserve/ready/dispatch, target-exec, document access/revoke and removal gates.
General account eligibility and OS-account retirement coordination remain open.
Both owned authorities/probes were stopped normally after integration; the offline
carrier and inspected bootstrap remain available. The isolated validator also
passes its unchanged 374 assertions; no public schema/XML source changed.

## Native fixture metadata

Implemented canonical AETHFIX1 decoding in core/fixture.[ch], with fixed bounded
records and borrowed immutable text spans. Strict UTF-8, text/path/token, order,
ancestor, mode, size/count and action-target checks preserve output on failure.
No dynamic allocation, filesystem access, hash implementation, publisher trust,
catalogue enrollment, package installation or new resident process is added.

The test-only C batch reader and Python interop driver compare every projected
field against independently expected metadata and unchanged frozen bytes. Final
focused runs cover 8 valid records, 81 malformed records and every shorter prefix
of the frozen vector: Debug ASan/UBSan 0.10 s, Release 0.08 s, both exit 0.
The complete native suite passed 12/12 Debug (25.99 s) and Release (21.62 s);
those full runs preceded two additional positive boundary fixtures, which passed
in the final focused repeats. Host runtime-format regression passed 14/14 in
0.012 s. No diagnostics, sanitizer report or format/vector change.

Reviewed the hash-library candidate separately: the offline carrier contains
openssl-libs 3.5.5-1.fc44, without development metadata or the openssl command.
Official upstream lists OpenSSL 3.5 LTS maintenance 3.5.9 and Apache-2.0 licensing.
Presented libcrypto for SHA-256 to the user because they requested library choices
before adoption. The asynchronous choice remains unanswered; no new package,
link dependency, version pin or security approval was introduced. Exact package
provenance/security/compatibility/footprint review still precedes adoption.

The dedicated offline VM remains live; the previously stopped Aetheri authorities
were not started/updated for this pure core addition. Next: native payload copy/
hash and immutable publication, protected machine/volume/read references and app
admission/launch. Library-independent storage work can proceed while the choice
is open; the full daily-use OS goal remains active.

## Retained storage references and mount identity

Added core/root.[ch] with repeated statx observations and required unique mount
IDs, plus held-FD filesystem facts. Catalogue open/create retain this identity;
ordinary uses also reopen the protected named root and reject disagreement.
Same inode/device is insufficient when a bind mount changes the visible mount.

Added opaque internal storage references in the existing storage module. They
derive the allocation leaf from StorageRef, copy independently expected owner/
root identity, retain duplicate CLOEXEC anchor/root FDs and validate the exact
0700/0600 private root/header before/after bounded payload opens. Payload and
file UID checks prevent foreign-owned private content from being returned.
Fork use fails. Closing the reference does not revoke a returned file FD: P0
native transport must settle/close its internal read obligation and return bytes.

Fifteen native tests passed, exit 0: Debug ASan/UBSan 27.91 s, Release 25.02 s.
Actual roots/payloads, six owner mismatches, copied-header replacement, UID/mode,
escape/oversize, fork, post-open header mutation/FD cleanup and 64 open/read/close
cycles pass. Separate injected statx faults prove required-mask/error/metadata
change handling without substituting for actual feature observations. Two opt-in
private mount tests retain identical device/inode while unique IDs differ; storage
returns EXDEV and catalogue operations ESTALE without changing state. Final nine
expected-root field mismatch cases pass in focused repeats (0.08/0.04 s totals).

Actual enforcing startup initially failed with EACCES and an fs_t filesystem/
getattr AVC. Added only authority filesystem metadata read permission; no new app,
mount, write or capability grant. Guarded update preserved the existing catalogues.
Broker replacement/root-only restart and full restart, unknown-entry preservation
and empty-bootstrap recreation/fresh confined initialization then passed.
A 5.032725849 s idle observation shows one task each and zero reported CPU/context-
switch deltas; PSS 2252/2292 KiB, RSS 6764/6676 KiB and MemoryCurrent
1646592/1667072 bytes. Both owned services/probes were stopped normally afterward.

Runtime reference fixtures are unconfined; separate real policy integration proves
only catalogue observer/service behavior. Parent/mount topology, same-UID MAC,
registered account/volume/allocation provenance and admission remain caller duties.
No durable registry format, clone proof, public document grant or purge is added.
No SDK/package/crypto change or other-project/hardware mutation. The OpenSSL choice
remains unanswered; independent storage/enrollment work can continue.

## Immutable allocation object records and cross-boot reopen

Implemented canonical 168-byte AETHALC1 encode/decode, independent frozen vector,
and protected create-only publication/reopening in core/allocation.[ch]. Six owner
IDs and filesystem/inode/birth facts persist; transient mount/device IDs do not.
Generic root observations now carry optional birth time, required for allocation
witnesses. Capture presumes independently authorized/flushed setup; neither a
candidate path nor filesystem ID authenticates a volume or admits an app.

O_TMPFILE holds incomplete metadata unnamed. After full write/file fsync, a
create-only link publishes the owned FD and directory fsync acknowledges its name.
No named-temp residue, replacement, new capability or crypto implementation.
Native tests cover codec truncations/ranges, unavailable birth time, immutable/
concurrent same-intent retries, owner mismatch, copied root, metadata and reopen.
Injected failure/exit before link leaves zero named records; lost acknowledgement/
exit after link leaves exactly one. Same-intent reconciliation preserves identity.

Sixteen tests pass: Debug ASan/UBSan 28.27 s, Release 25.39 s, exit 0. Twenty-two
host orchestration guards pass again in 0.194 s. SQL/XML/v1 sources are unchanged.
The new optional controller storage-prepare fixture records intent before effects,
creates/flushes protected roots/header/payload/config and tracks every location.
Native user --storage-publish/--storage-verify one-shots hold the owner writer lock
under enforcing user-domain/NoNewPrivileges/empty caps. Immutable repeat and separate
invocation reopen succeed. Same-UID control reads the witness, while app-domain
known-ID witness/header/config access is denied; existing broker/IPC checks pass.

Performed a real graceful guest reboot. QEMU's configured -no-reboot exited 0;
verified recorded PID absence, then booted the same disk offline without reinstall.
SSH/QMP guards initially refused reads while no VM was live; no duplicate start
was issued from an observation timeout. After new boot identity, storage-check
reports cross_boot_reopen=true and retains record hash/inode. Broker checks pass
again; services/probes were stopped normally. The new boot helper remains live.

The allocated fixture/registry remains intentionally: it is now tracked private
data and cleanup/lifecycle-check must refuse empty-bootstrap deletion. Reviewed
retirement, full operation-journal/volume provenance and actual app/grant/purge
integration remain next; this metadata feature is not their acceptance.

During this work the user accepted OpenSSL 3.5 LTS/libcrypto and then delegated
well-supported technical recommendations while unavailable. Updated current
instructions/questions/decisions; personally important product/UI/destructive
choices wait. Exact crypto package review/install/linking follows; no dependency
or SDK version changed here. The full daily-use OS/apps goal remains active.
Final acknowledgement review also reports an error if closing the published
writer FD fails, preserving the record rather than claiming rollback. The injected
close-error case passes in Debug/Release focused repeats (2.14/1.99 s). Actual
empty-bootstrap cleanup was deliberately refused with allocated data present;
native storage-check afterward retains the same witness/hash/inode and payload.
The isolated validator still passes its unchanged 374 assertions.

### Reviewed native SHA-256 backend

Applied c-specialist, dependency-manager and security-auditor workflows. The user's
accepted OpenSSL 3.5 LTS choice now has exact Fedora 1:3.5.9-1.fc44 libs/devel pins,
official HTTPS cached RPMs and verified full Fedora 44 key signatures. Reviewed
source-RPM identity, Apache-2.0 license, advisory/maintenance branch, dependencies
and the actual transaction. Offline signature-checked installation upgraded one
existing runtime and added one development package, about 5 MiB net. No service or
policy permission was added; installed RPM verification and pkg-config pass.

Added core/digest: fixed EVP/default-provider SHA256 in a private synchronous
context, unchanged output on failure, checked cleanup, 16 MiB bounded held-FD
hashing that preserves offset/ownership and detects ordinary metadata drift.
Fixture verification checks canonical metadata against an independently expected
ArtifactId and per-file bytes against that decoded manifest. It does not trust
newly supplied hashes or implement inventory, copy/publication or app admission.
The existing administrative storage check verifies its fixed hello-newline digest
inside the real enforcing cap-zero user domain without new permissions.

Executed 17/17 native Debug ASan/UBSan cases (30.46 s), 17/17 Release (25.14 s),
29 host VM/cache guards (0.272 s), and all five cached input hashes. Added exact
old/new runtime-family rejection cases then passed focused Debug/Release repeats
(0.84/0.43 s). Crypto setup/update/final/unload and read/stat failure paths preserve
output/offset/FD lifetime; changed same-size content and wrong ArtifactId fail.
Public ELF needs show libcrypto.so.3 without direct libssl linkage.

Actual graceful VM shutdown finished with QEMU exit 0; PID absence was checked
before booting the same disk offline. All nine carrier prerequisites pass after
boot. Native enforcing allocation publish/retry/separate reopen again reports
native_sha256=true and cross_boot_reopen=true, with the existing witness retained.
The optional private allocation fixture and protected registry remain intact.
Next: protected fixture inventory/copy/hash, publication and app/run admission,
then the remaining P0 launcher/grant/revocation/retirement work. No original
bootable OS, desktop, app, general signing protocol or hardware acceptance claimed.
