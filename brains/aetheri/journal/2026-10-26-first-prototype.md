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

### Native inventory and private payload staging

Implemented core/import's bounded inventory/copy/hash/flush and independently
bound staged resync, following c-specialist/security-auditor guidance. It keeps
the root private, rejects nonempty fresh targets and preserves failures. Source
data/owner/mode/xattrs are not written; normal read-atime changes remain possible.
The caller still owes trusted fixture/root/operation bindings, genuine procfs,
MAC and writer exclusion before using this primitive in an authority.

Final native suites: Debug ASan/UBSan 19/19, 47.59 s; Release 19/19, 43.60 s.
Both include 42 ordinary fixtures and real private-namespace mount/device probes.
Tests prove copied bytes, limits, failed-IO retention, repeat flushes and borrowed
FD/offset lifetime. Initial xattr test setup failed because its read-only fixture
could not accept the test attribute; setup now temporarily grants owner write,
sets the attribute, restores the intended mode, then exercises actual rejection.
The final complete suites pass that case. Toolchain/dependency pins, frozen
contracts and policy are unchanged; no installed authority action was added.
Final review strengthened the source-replacement case to three copy chunks and
different same-length replacement content. The staged bytes still match the held
original inode; source drift rejects the operation. Additional focused import/
mount repeats pass Debug 18.80 s and Release 19.65 s, after the complete suites.

Next: connect the stager to fixed protected fixture setup, independent operation/
root identity and the real system-domain journal/publication path. Preserve all
existing allocation data and do not let empty-bootstrap cleanup retire it. Public
API capabilities remain discovery-only; the full OS/apps objective remains active.

### Protected system-domain fixture journal

Used the C, database and security workflows. Added the fixed native AETHINT1
install projection and atomic plan/operation/preparing metadata on the existing
v2 owner connection. Exact retry retains IDs; executing evidence binds the protected
configuration before payload effects. READY leaves running/publish plus staged/
preparing metadata, never a completed/active installation.

Added internal AETHFIC1, native object observation/sealed root provisioning and
system --fixture-observe/--fixture-stage. The root controller preallocates trusted
carrier infrastructure and expected bytes; native C handles actual operational
rows and stage IO. Added source/stage MAC types, deny executable mapping/execution,
and keep caps empty/NNP. The Release compiler flagged a potentially uninitialized
empty-slot result; explicit initialization resolved the warning before deployment.
Both complete native suites now pass 21/21, including lost COMMIT acknowledgements,
scope/identity/deadline, accepted cross-boot replay and configuration bounds;
Release takes 45.53 s. Host guards: 29/29, latest 0.231 s. Isolated validator:
374 assertions on host SQLite 3.53.1; native guest SQLite remains 3.51.2.

Actual native ELF copy/hash/label/flush/journal succeeds in enforcing system units;
exact repeats retain step/operation revision and file objects. Filtered AVCs prove
source/stage map denial. The first non-TTY ausearch waited on stdin; inspection
confirmed its own anon_pipe_read, and only that pidfd-pinned read-only query was
terminated. --input-logs returned actual audit evidence. All owned units stopped;
graceful QEMU exit 0/PID absence preceded offline boot. Fixture check reports real
cross_boot_reopen=true; regular endpoint/app-denial and private storage checks pass.

An identical-content same-filesystem source copy is bound only inside a probe
unit's namespace. Native ESTALE occurs before journal modification; the verified
temporary clone is retired and the real fixture rechecks successfully. Existing
private allocation data/configuration remain intact. Public SQL/XML and old vectors,
SDK pins and dependency families are unchanged. General allocation/recovery and
atomic publication/activation/admission remain next; the full daily-use OS goal
is active and no original desktop/system image is claimed.

27 October handoff: final reviewed state retains the real fixtures, with all
owned service/probe units stopped. Next implement journaled atomic publication;
do not restart from another slot or turn staged metadata into active code early.

### Sealed store publication — 27 October

Implemented core/publish plus the existing catalogue/system administrative path.
The original payload moves into a preallocated manifest/payload container, then
same-mount RENAME_NOREPLACE publishes that container under ArtifactId. Independent
AETHSTO1 binds stage configuration, store anchor and container inode/birth/filesystem
witnesses to step-1 evidence. Separate root provisioning and native journal ownership
remain explicit; general allocation and publisher enrollment are not implemented.

Retries validate both expected objects and exact inventory/content and repeat all
file/directory/old-parent/new-parent flushes. Errors retain effects. New final MAC
types deny system-role mutations and all current app store access/execution.
One owned RWP ancestor preserves the mount identity needed for rename; source is
explicitly read-only. No capabilities, dependency, schema or public API is added.
Applied publication means artifact verified/generation committed, but installation
still preparing and operation running/reconcile/activation_pending. No lease/result.

All 23 native tests pass Debug ASan/UBSan (77.94 s) and Release (55.99 s). New
publication cases cover before/after rename failures, process exit, full resync,
copied/unknown targets, conflicting metadata and real same-inode binds. Journal
tests inject before/after real COMMIT acknowledgement failure at every publication
phase. Host guards pass 31/31 and migration/wire tests 27/27 (46.744 s); isolated
validator passes 374 assertions. Final chmod probes were then built and exercised
in the actual installed system role, with unchanged inode/mode/ctime on replay.

Actual enforcing cap-zero native publication/replay succeeds. Audit evidence shows
store-code map and store-dir/code/data setattr denials, permissive=0. An initial
follow-up discovery check was called while endpoints were stopped; explicit start
then check passes. Private allocation reopening remains unchanged. All owned units
drained, graceful QEMU exit 0 and PID absence preceded offline reboot of the same
disk. An early SSH banner timeout was only an observation; live VM was preserved.
After readiness, publication reports cross_boot_reopen=true with original objects,
exact sealed labels and unchanged operation revision. Endpoint/app-denial and
private-storage checks pass after boot; all owned authority/probe units stopped.
Final review adds a complete byte-identical final-container substitution after
pinning the original FDs: native ESTALE preserves both roots and never adopts the
copy. Focused publication/mount Debug/Release repeats pass (12.00/10.77 s), now
covering 34 ordinary fixtures plus four actual bind probes.

Next: activation and app/run admission/recovery with target-exec evidence. Reuse
the retained published fixture; fixture-store-check replaces original stage/rebind
checks after the move. Preserve private allocation and both journals. Personal UI/
destructive choices remain deferred under the user's delegated recommendation rule;
the full personal OS/apps goal is active.

### Atomic first activation and retained receipts — 27 October

Implemented first activation as step 2 of the original fixed install intent.
Protected 72-byte AETHACT1 binds published-store evidence, install/generation/
closed-gate revisions and an administrative completion stamp. The system role
revalidates the exact immutable object before first activation. One SQLite commit
activates the original generation and completes the original operation/result;
existing triggers advance activated_once and its revision. The launch gate remains
closed, with no app process, data epoch, grant or launch lease admitted. No new
policy permission, dependency, schema/XML version or resident process.

Completed retries return historical evidence. A separately reported current
catalogue pointer prevents a past result from claiming present readiness. Tests
switch to a new generation, remove/reclaim the old install/code, and open a later
gate; retry of the old request changes none of those states. Those mutations are
controlled database test fixtures, not production update/removal implementations.
First activation rejects unpublished work, wrong scope/boot, missing runtime,
stale revisions, open/closing/owned gates and live recovery/dependency references.

Complete native suites pass 23/23 Debug ASan/UBSan (72.63 s) and Release (65.20 s).
Expanded statement/COMMIT faults, fresh-process exits before/after COMMIT, retained
receipts and malformed configuration then pass focused journal/config repeats in
Debug 18.80 s and Release 15.57 s. The independent Python encoder supplies AETHACT1
bytes/hash. Host guards pass 31/31, frozen runtime tests 14/14, migration/wire 27/27
(31.065 s), isolated validator 374 assertions. No native or host SQLite pin changed.

Actual enforcing cap-zero/NNP system units prepare, activate and replay with the
original generation/result, closed gate, unchanged published inode/mode/ctime and
unchanged catalogue hash on completed retry. Follow-up native store verification
passes with activated/completed metadata. Normal endpoint/app-denial and existing
private allocation checks pass. All owned units drain; graceful QEMU exit 0 and
PID absence precede offline boot of the same disk. Activation then reports real
cross_boot_reopen=true; its original receipt and gate persist with no dispatch.
Store, endpoints/app denials and retained private storage pass after boot; units
are stopped again. Carrier configurations and test infrastructure remain tracked.

Next: actual app/run/data identity and launch admission, supervised dispatch and
target-exec observation, then native document/revocation/removal P0 cases. Reuse
the activated fixture and preserve historical receipts/private allocation. The
full daily-use OS/apps goal stays active; no original desktop or bootable Aetheri
image is claimed. Personal UI/destructive decisions still await the user.

## 27 October — authenticated installation observation

Implemented private Catalogue1/Development1 v1.0 and bounded C17 wire handling.
System resolves one eligible active app/current committed generation in a read
transaction. User caches only its own authenticated install binding in its own
transaction. The system database is never opened by the user service. Exact retry
does not rewrite; changed/revoked bindings require reconciliation. Observations
are not launch/data authority. Existing SQL/XML, public protocol/capabilities and
dependency pins stay unchanged.

The fixed-fixture trigger uses a root-selected transient UID-1000 user-domain
helper with NNP, no capabilities and no writable path exception. Actual held peer,
unit, invocation, UID/context, cgroup and boot are checked and rechecked before
writing. Controller drains it and clears the protected selector. No new resident.
Real testing rejected UID-0 user-bus authentication; retained existing transport
and used the scoped helper. Evidence then identified missing self-domain IPC and
systemd status permission. Added only those and read-only context observations.
Ordinary and same-domain wrong-unit callers remain denied, including copied real
broker IDs; app raw-system-bus and protected-file denials remain.

Complete native suites passed 25/25 Debug ASan/UBSan (99.02 s) and Release (72.47 s).
After final integration, fresh Debug build and six focused projection/codec/config/
peer/broker/process cases pass (6.21 s). Host migration/wire 28/28 (40.352 s),
orchestration 25/25, cache guards 7/7, frozen runtime vectors 14/14 and isolated
constraints 374 assertions pass. New native tests cover exact retry/reopen,
failed/lost COMMIT acknowledgement, revision/identity conflicts, account/machine
scope, wrong contexts and unchanged error outputs. Wire fixtures independently
cover truncations, lengths, integer limits, tokens, enum and extra fields; mocked
request tests are explicitly separate from actual enforcing transport evidence.

Actual projection-check verifies separate native writers, exact unchanged retry
hashes and no data/attachment/grant/launch/gate effects. Broker replacement/root-only
restart pass and projection sees the new system runtime. All owned units drain;
graceful QEMU exit 0 and verified PID absence precede offline boot of the same disk.
Projection reports cross_boot_reopen=true and system_runtime_changed=true. Normal
endpoint/app-denial, original sealed-store/activation and retained-allocation
checks pass again. Owned units are stopped, both tracked fixtures remain.

Next: fresh app DataEpoch/allocation journal and actual admission/dispatch/exec
evidence. Never adopt the unrelated retained private fixture. General binding
reconciliation and public installation remain pending. Full OS/apps goal active;
personal UI and destructive hardware choices deferred until the user returns.

## 27 October — first app DataEpoch and closed attachment

The fixed original app now has a separate private DataEpoch, StorageRef and
AllocationId. The previous storage/read fixture is preserved. Root records fresh
test identities/baseline, provisions a distinct protected anchor/root/payload and
freezes the confined native object's independent AETHALC1 witness in AETHDTA1.
The 368-byte internal configuration also binds the install/publisher, both
authority epochs, install revision, original user catalogue baseline and schema.

Native data enrollment verifies/publishes that exact allocation, then commits
data+attachment+closed data gate through the single user SQLite writer. There is
no filesystem effect inside the metadata transaction. Exact currently attached
retry is read-only; detached/retained/purging/purged, changed or conflicting rows
fail. Later open gates are not closed again by an old enrollment. An advanced
baseline rejects recreation after pruning. This is explicit P0 preallocation and
atomic metadata enrollment, not a general dynamic allocation operation journal,
historical receipt or launch admission. Existing v2 SQL/XML, dependencies, policy
permissions and public capabilities remain unchanged. No new resident process.

The test-only native user-domain observe/enroll one-shots require enforcing
SELinux, NNP and empty capabilities. They cannot be registered as the live user
broker. The new payload uses fixed categories c33,c34; the old storage fixture
keeps c31,c32. Same-UID app probes cannot open the new owner, allocation record,
payload or protected configuration; ordinary public transport and previous raw
system-bus/private-state denials still pass. Cleanup explicitly refuses app data.

Complete native suites pass 26/26 Debug ASan/UBSan (83.90 s), 26/26 Release
(76.33 s). Final expanded config-length/owner/epoch/retirement/overflow tests pass
focused data repeats (4.13/3.02 s). Tests inject failures at each SQL mutation and
before/after COMMIT acknowledgement; fresh child connections exit before/after
real COMMIT and reopen with either no whole enrollment or one whole enrollment.
Inherited connections are rejected. Future lifecycle states are controlled
unconfined metadata fixtures, not implemented production removal APIs.

Host migration/wire 28/28 (36.901 s), orchestration 26/26, cache guards 7/7,
frozen runtime tests 14/14 and isolated constraints 374 assertions pass. Actual
native allocation/enrollment/retry preserves database/record bytes on retry and
the unrelated system catalogue/storage witness. Projection and endpoint/app-denial
checks pass, followed by original store/activation/private-storage checks.
Owned units drain normally; orderly shutdown yields QEMU exit 0 and verified PID
absence before the same disk boots offline. data-check reports cross_boot_reopen=true
with the same data identity, closed gate, preserved old allocation and no app
dispatch. Projection, normal endpoint/denial and previous fixture checks pass
again; owned units are stopped.

Next: code/run reservation and actual data readiness, then supervised dispatch and
target-exec evidence using the new existing data identity. Keep both test fixtures
and deferred general allocation/volume/binding recovery explicit. Full original
OS/apps goal stays active; personal UI/destructive choices still await the user.

## 27 October — private code reservation, observation and withdrawal

Implemented native root reservation and the existing Runtime1 ReserveLaunch/
GetDispatch methods, plus CloseLaunch limited to withdrawal before dispatch.
Current broker/process/account context is authenticated. The wire profile is
explicitly the original protected P0 I/G/D and main/p0/none; it does not claim
general action/logind policy. No arbitrary executable, UID, environment, mount or
SELinux context enters these methods. Public capabilities remain empty and
DispatchLaunch remains unsupported. Development1 adds a protected test at 1.1;
original Runtime1/public XML and SQL v2 stay unchanged.

One transaction records canonical AETHINT1 root plan/read/effect, operation, full
AETHRSV1 request fingerprint, generation lease and reserved dispatch. Exact retry
retains original root IDs/timestamps despite new proposals; changed tuples conflict.
Acknowledging an existing reservation after gate closure never reopens it. Scoped
GetDispatch returns actual state and optional invocation. Withdrawal requires an
original current reserved tuple with no ready-data/exec evidence; terminal cause,
released lease and cancelled operation commit together. Changed cause conflicts;
uncertain or potentially dispatched work requires reconciliation and keeps its
holder. Normal user launch acceptance/data readiness remains next.

Explicit system --admit-fixture startup checks root-protected boot/baseline-bound
AETHADM1, original active fixture and current sealed manifest/content/inventory/
object witnesses before opening metadata reservation admission. Verify-only sealing
checks cannot change labels/modes. Default startup closes admission; old profile
replay fails ESTALE. Executable mappings remain denied. The first integration
attempt found that the bounded small-document open helper rejects manifest's
larger metadata bound; a direct no-follow held manifest open now checks its exact
regular-file owner/mode/size and bounded bytes. No policy permission was broadened.

The P0 driver persists launch/lease/cause before IPC. The selected user-domain
helper invokes the registered user service, which reserves twice, queries reserved,
withdraws twice and queries terminal. Actual committed canonical plan bytes are
independently reconstructed in Python. Root lease is released, operation cancelled,
cause exact, user DB unchanged and derived app unit absent. Same-UID unconfined and
same-domain wrong-unit controls reach transport but are denied all three runtime
handlers and the protected test. No new resident process/dependency/policy permission.
Prepared failed startup can resume only before acceptance with the original IDs;
new explicit tests require prior terminal/released evidence and retain history.

Native complete suites pass 28/28 Debug ASan/UBSan (104.93 s), 28/28 Release
(82.47 s). Tests cover all ten reservation mutations, all four withdrawal mutations,
failed/lost COMMIT acknowledgement, fresh-process exits before/after real COMMIT,
original IDs, canonical/fingerprint bytes, scoped reads, fence closure, terminal
withdrawal and restart uncertainty with a nonreleased holder. Independent wire
fixtures cover all field truncations, array lengths, numeric/token bounds, extras
and unchanged error output. A final guard avoids converting rejected unsigned
overflow values; focused wire repeats pass Debug 0.07 s and Release 0.02 s.
Host migration/wire tests 28/28 (45.848 s), orchestration 27/27, cache guards 7/7,
runtime vectors 14/14 and isolated constraints 374 assertions pass.

Actual stale-profile replay fails closed with unchanged terminal business history.
Original store/activation, app data and retained storage checks pass; activation's
historical check now accounts for terminal reservation history while requiring
unchanged current closed gate. After orderly shutdown/QEMU exit 0 and verified
PID absence, the same disk boots offline. Replay reports cross_boot_reopen=true;
normal endpoints/denials and original fixtures pass again. Final installed native
codec build and endpoint/denial checks pass. Owned service/probe units are stopped.
No hardware/power-loss or actual app execution/readiness acceptance is claimed.

Next: persist normal user launch intent/run/data readiness, prepare exact views,
then serialize DispatchLaunch and verify supervised target exec. Potentially
dispatched recovery and real stop/drain must precede release. Keep both data
fixtures and reservation receipts; do not rearm old identities. Full OS/apps goal
active; personal UI/destructive choices remain deferred to the user.

## Durable user launch journal — 27 October 2026

Implemented the next owner boundary, using existing v2 tables and the accepted
C17/systemd/SQLite foundations. The user writer opens exact verified data admission,
then atomically stores canonical intent, read set/effect, local operation, requested
run/proposed code lease and complete root request fingerprint before RPC. Unknown
root replies retain that obligation. An actual authenticated root receipt supplies
the remote plan/op/digest for acknowledgement; readiness remains unknown.

Catalogue1 gains additive minor 1.1 GetReservationReceipt. Development1 minor 1.2
adds selected-controller PrepareFixtureLaunch/CancelFixtureLaunch. Public 1.1
capabilities remain empty; Runtime1 XML and SQL v1/v2 are unchanged. Root withdrawal
can now serve a replacement current same-authority/account broker only for an old
still-reserved tuple without readiness/exec evidence on the original boot. It never
rebinds or resends that request; root restart uncertainty stays held.

User runtime startup now marks unsettled runs recovery_required in the same
transaction that changes runtime/closes admission. Testing exposed the missing
run marker in the former native startup, so this is an implemented correction,
not an assumption about a schema trigger. Proven root withdrawal with the exact
local-operation cause completes the release step, link, cancelled local operation
and start_failed/unavailable run. It rejects local readiness/exec/grants/additional
setup and never changes data admission. The initial duplicate request-based finish
helper was removed; normal and recovery cancellation use one owner implementation.

Native tests use an independent Python AETHINT1 digest, all two admission, ten
acceptance, five acknowledgement and seven reconciliation mutation faults, failed/
lost COMMIT replies, fresh-process exits before/after acceptance COMMIT, stable IDs,
changed-tuple/cause/fingerprint rejection, historical retry and broker replacement
with both acknowledged and lost root replies. Independent receipt wire bodies
cover truncations, field lengths, optional-cause pairing, states, integer bounds,
extra arguments and output preservation. A duplicate input/output argument name
was caught by the wire contract test and corrected before installation.

Complete suites pass 29/29 Debug ASan/UBSan (134.29 s) and 29/29 Release (99.29 s).
After expanding admission faults, focused Debug launch repeats pass (16.45 s);
Release includes those added cases. Host migration/wire passes 28/28 (48.137 s),
orchestration guards 28/28, cache guards 7/7, frozen runtime vectors 14/14 and
isolated constraints 374 assertions. These times describe test runs, not product
performance budgets.

Actual offline enforcing cap-zero/NNP services accept/reserve/acknowledge the fixed
fixture, then replace only the user service. The original root reservation and
broker incarnation remain held while the local run becomes recovery_required.
The new broker explicitly withdraws it with the original local operation as cause;
both journals complete and a repeat preserves both database hashes. Independent
Python reconstruction agrees with both committed canonical plan digests. No app
unit/view/readiness exists. Same-UID unconfined, same-domain wrong-unit and confined
app controls cannot call the new protected methods; copied identity cannot read
the new private root receipt. Normal endpoints/denials and later-gate data-enrollment
retry pass. The installed binary was updated again after final native cleanup;
explicit existing-attempt cancellation and endpoint checks pass.

Root-protected AETHLNF1 stores fixed correlations before IPC. launch-cancel never
creates another reserve; launch-reopen-check checks retained business rows. Unknown
supervisor dispatch remains preserved and blocks automatic action. No new library,
SELinux permission or resident process was added. Both independent data fixtures,
sealed code and all historical reservation receipts are retained.

After graceful QEMU exit 0 and verified PID absence, the same disk boots offline.
launch-reopen-check reports cross_boot_reopen=true with unchanged business rows;
normal endpoints/denials, stale-profile rejection, original sealed publication/
activation, app data and retained storage reopen again. Owned units are stopped.

Final native recovery guards also reject synthetic local readiness, invocation and
additional setup, preserving the pending journal. The owner snapshot exposes the
same pre-dispatch eligibility check before CloseLaunch RPC, so local setup cannot
release root code before failing user reconciliation. Focused suites after this
guard pass Debug ASan/UBSan (20.05 s) and Release (19.16 s). The driver now permits
a distinct launch-new-check only after exact prior terminal/released evidence,
retaining the previous protected attempt in history. The installed final guard
also passes that new real enforcing prepare/broker-replacement/withdrawal attempt
and normal endpoint/app-denial checks, without reusing old identities.

Next: exact data/code view preparation, durable readiness and serialized dispatch/
supervisor target-exec evidence. Recovery/drain must account for external effects
before their corresponding lease can be released. No public launch, physical
power-loss/hardware, desktop or finished OS/app acceptance is claimed.

## Native private mount views — 27 October 2026

Added linux/view.h/c to the existing Linux module, with one native test and an
independent parent observer. No new package/library, policy permission, schema,
RPC or resident process. The previous turn was verified progress: committed user
journal/recovery a39278f was present and clean; the same offline VM was live.

The trusted setup primitive enters a fresh recursively private mount namespace,
then clones exact admitted source directory handles without nested mounts. New
unique mount IDs are compared to the same backing object. Code becomes readonly;
data remains writable/noexec; both views are nodev/nosuid/nosymfollow. It never
clears inherited restrictions. Exact empty root-owned setup targets are checked
before attachment. A move attempt becomes uncertain before the syscall; ordinary
and lost-success failures cannot be repeated. Objects reject fork/namespace misuse,
use CLOEXEC descriptors and release only local handles. Closing them deliberately
does not infer unmount/app drain or authorize durable lease release.

Actual tests create detached mounts and attach them to owned disposable slots.
While the views are live, a separate parent process verifies its original empty
targets and nested underlay remain visible. Kernel checks reject code writes,
symlink traversal, data exec, executable mmap and device opens. Inherited readonly/
noexec restrictions are rejected rather than removed. A same-inode bind substituted
at the target fails NO_XDEV; renamed/replaced targets, changed source metadata,
unexpected entries and wrong slots fail. Namespace entry/private-propagation,
clone and attribute failures, move failures before/after actual success, fork and
namespace changes, output preservation and repeated descriptor cleanup are covered.
All library objects can close while the view is still mounted; that remaining
reference is demonstrated before explicit test unmounts. The Python harness retains
failed fixtures, including any unknown child state, rather than recursively deleting.

Complete native suites pass 30/30 Debug ASan/UBSan (113.06 s) and Release
(101.14 s). Final expanded exec-mmap/target-bind cases pass focused Debug/Release
repeats (0.18/0.09 s). Host orchestration 28/28 and isolated constraints 374
assertions pass. These are test durations, not OS performance measurements.
The guest is globally SELinux Enforcing, but these mount tests deliberately run
as administrative unconfined_t. No deployed launcher/app-domain acceptance is
claimed. Existing installed authority launch-history reopening and normal
endpoint/app-denial checks still pass. Owned authority/probe units are stopped.

Next: connect this component to a protected short-lived supervised starter,
persist setup effects before making them, drop privileges/close handles, establish
typed readiness and prove actual target execution. The retained fixture declares
action run; old synthetic main reservations must remain immutable history, not be
silently reinterpreted for execution. No public launch or complete app sandbox yet.

## Supervised no-exec preparation — 27 October 2026

Connected the private mount primitive to the original User/Root launch journal,
through a short-lived C17 setup helper, protected AETHSUP1 profile and private
Setup1 1.0 / Development1 1.3. No SQL version, Runtime1, dependency or package was
changed. The helper links pure codecs rather than a SQLite writer. User's pending
run holds D; Root owns the physical setup journal. Additional setup steps block
the old simple reservation withdrawal.

Root holds the actual authenticated setup ProcessFD, sender and observed
InvocationID/cgroup/boot, plus a PID1 unit reference, before writing its binding.
View preparation and READY remain separate from target exec/readiness. The helper
has no app-exec, arbitrary command or privilege-changing path. It retains only
SYS_ADMIN/DAC_READ_SEARCH for trusted namespace setup. Setup teardown requires
held process death plus independently observed same-invocation inactive/failed
unit, empty cgroup, MainPID 0 and no job. Native terminal receipt then releases
the original code lease and lets User finish cancelled without touching D.

The first real helper stopped before registration. The old Root service lacked
the new early-failure recovery code, so a separately selected confined cap-zero
native administrative writer recovered the same original runtime under its
exclusive catalogue lock. It checked the retained failed PID1 invocation, exact
protected no-exec image/unit/argv/context/restart/exit and independent drain.
It recorded distinct supervisor-only AETHSTE1 evidence, never a held peer. The
controller verified the durable receipt before replacing old executables; it
never patched SQL, changed the failed helper or repeated its dispatch. Later
early failures were reconciled through the live Root endpoint. General Root
restart/boot or target-exec recovery is deliberately not enabled by this proof.

Observed integration issues and repairs:

- PID1 requires system/start for RefUnit. Actual audit and v259 source agree;
  this broader permission is confined to trusted Root. Public unit management
  remains absent. SELinuxContext is a mandatory (bs) tuple, not a string.
- The policy refresher initially changed the .cil basename, creating a separate
  owned module. After verifying both staged/installed hashes, the intended module
  was updated and the unintended one removed. Staging now preserves the basename.
  A diagnostic dontaudit rebuild was restored; enforcing mode never changed.
- PID1's NNP/SELinux transition left SYS_ADMIN inheritable. The helper now drops
  only allowed inherited bits and checks exact effective/permitted rights again,
  before registration/effects. No SETPCAP or app permission was added.
- Actual audit identified namespace-FD reads, private root propagation mounton
  and the kernel-labeled notify socket. The responsible setup-only permissions
  were added; code/data execute/map prohibitions remain.
- Procfs's current label included one terminating NUL. Independently reconstructing
  the context digest from the retained controller observation without that
  terminator matched native evidence. The controller now accepts exactly the
  expected bytes, with optional single NUL, rather than hashing it as label text.

The final enforcing setup-check passed all reported checks: actual supervised
views, independent live context digest, live-withdrawal denial before/after User
replacement, observed drain, byte-identical terminal retry and removal of only
the exact known empty scaffold. Target execution was false throughout. The old
publication, private payload/allocation/owner and operation histories remain.
The final normal endpoint/app-denial check also passed after updated installation.

Complete native CTest suites pass 31/31 Debug ASan/UBSan (154.94 s) and Release
(118.88 s). The profile test separately checks every truncation, UUID variant/version,
signed-range boundary and unchanged failure output. The supervisor test rejects
22 independent image/transport/unit/argv/context/restart/exit/drain faults. Native
launch tests cover each mutating SQL step, failed/lost commits, exact retries,
changed evidence, two revision overflows, terminal-only recovery and retained
Root-restart uncertainty. Final bounds changes pass focused Debug/Release repeats
(0.07/0.02 s).

Host migration/wire 29/29 (50.747 s), orchestration 30/30 (0.235 s), frozen runtime
vectors 14/14, cache guards 7/7 (0.091 s) and isolated constraints 374 assertions
pass. A final added false-recovery-receipt guard passes its focused repeat. Test
durations are not OS performance figures. Public capabilities remain empty.

The final broker-check rejects copied identities from unconfined/same-domain
wrong-unit callers, including the new Root setup and User development methods.
Departure reduces Root FDs 12 to 11; replacement and Root-only restart with the
User retained pass. launch-reopen-check reports terminal_history_preserved=true
and cross_boot_reopen=false: this last check is a normal service restart, not a
new guest-boot claim. All owned authority/probe/setup units were then stopped;
getenforce still reports Enforcing. The dedicated offline VM and known fixtures
remain for the next implementation step.

Next: a compatible real-exec fixture and exact target-dispatch/privilege-drop/exec
evidence. Preserve the old immutable main reservations and run/c31,c32 package;
enrolled data uses c33,c34. Document transport, full recovery/removal, original
desktop/apps/image and hardware acceptance remain part of the active full goal.

## Native execution boundary — 27 October 2026

Added internal linux/image and linux/execute, with separate owned-child tests,
an inert post-exec payload and a temporary MAC test controller/policy. Only tests
call the execution primitive. Installed authorities/helper, their catalogues and
the original no-exec withdrawal proof are unchanged. No SQL/XML/public capability,
external dependency or resident service was added. Runtime contracts describe the
limited ELF profile, trusted caller prerequisites and unfinished launch integration.

The primitive revalidates the held image/object/hash, selects the pending mandatory
MAC label, seals every internal FD, clears supplementary groups and capability
sets/bounding/ambient authority, locks securebits, changes all IDs, enters held
private data as its owner and execs the held ELF. It never returns after failure.
Its error frame records the actual phase and immediately saved errno. Tests change
errno during successful reporting helpers and still recover the original ENOEXEC.
The absent-reader case closes before fork, making EPIPE deterministic. Malformed
ELF/capability/alias fixtures are inspected as data, never kernel-executed.

An independent observer checks actual ProcessFD credentials/cgroup/label and the
backing /proc executable, including exact object/hash and repeated observations.
The real same-UID worker with zero effective/permitted/inheritable capabilities
also passes this observation. Future production authority integration must preserve
its own run/unit/invocation admission; this snapshot is not readiness or a claim
of continuous image identity. No SYS_PTRACE/KILL was added to those authorities.

Observed integration issues and scoped repairs:

- Reopening source/target handles after namespace entry was necessary: inherited
  directory FDs still referred to the parent's mounts and failed clone admission.
- The owner-data CWD change belongs after final UID change, avoiding a need for
  root DAC bypass after setup capability trimming.
- PID1's protected namespace preparation needed mounton on the temporary data
  type. Only the temporary policy gained that exact permission.
- Actual setup capabilities lacked SETUID despite the configured bounding ceiling.
  v259 source explains its conditional removal during seccomp setup. The trusted
  test unit now explicitly retains ambient SETUID; the boundary clears it before
  UID change. NNP and restricted namespaces were retained.
- Actual execveat returned phase 10/EPERM; audit identified nosuid_transition on
  the private code mount. The test policy now permits only the exact test source/
  target pair for both NNP/nosuid transitions. No unconfined target fallback.
- The real target and cap-zero observer succeeded before the parent harness failed
  to signal its changed-UID child. Test-only KILL now permits owned-child shutdown;
  it and the parent's test-only SYS_PTRACE are stripped before the exec boundary.

Each known failed attempt was independently drained and retired through the
controller's scoped cleanup after matching retained policy/image/inventory bytes.
No failed dispatch was resubmitted under its old name. Unacknowledged dispatch
continues to forbid cleanup/retry. Five known failed transient unit records were
later reset only after independent drain; their journal/audit evidence remains.
No broad reset-failed, recursive cleanup or original authority mutation was used.

Final Release MAC controller result: Enforcing, real target domain c33,c34,
UID/GID 1000, capability sets empty, FD seal and exact executable observation,
private data read/write/unlink and executable data-mmap denial all pass. Five
cases cover success, wrong image, explicit exec failure, pre-exec exit 125 and
actual payload exit 125. Both empty-channel exit-125 cases remain unknown rather
than accepted as exec/readiness. Every owned test unit drained; exact fixtures and
the temporary module were removed. registered_app=false. A final check confirms
module/fixture absence and Enforcing; installed authority/probe units remain stopped.

Complete native CTest suites pass 32/32 Debug ASan/UBSan (237.03 s) and Release
(223.30 s), including linux.execute (1.38/0.34 s respectively). These two builds
ran concurrently in the same two-vCPU guest; timings are test-run evidence, not
OS benchmarks or a comparison to earlier serial runs. The inert exec_payload
does not use sanitizers so their own startup FDs cannot mask the inherited-FD test.
Host migration/wire tests pass 29/29 (53.841 s), orchestration 33/33 (0.214 s),
runtime vectors 14/14 and isolated constraints 374 assertions. Final cleanup-only
guards also cover an unresolved dispatch, independent PID/cgroup/job drain and
resetting only observed failed owned units; the latter ran against the five real
retained failed units. No change to native code followed its complete test suites.

Next: connect a new compatible real-exec fixture/action at c33,c34 to durable
dispatch, actual target registration/image evidence and a ready-data fence.
Use an executing starter with its own drain/recovery contract; do not extend the
immutable no-exec helper's release shortcut. Preserve old main reservations,
run/c31,c32 package, private data and all histories. The full OS/apps goal is active.

## Compatible execution fixture — 27 October 2026

The original fixed package expects c31,c32 and its synthetic main reservations
cannot become declared run starts. Added a second compiled protected registry
selection, independent exec-fixture config/manifest/store/activation inputs and
a separate controller subtree. The new aetheri.p0.exec component uses a fresh
install/generation/slot/artifact and native installation operation. Shared owner
catalogue, content-addressed store and all existing stage/publication/activation
code remain common. The controller never recreates the shared store for it and
rejects old immutable identity/artifact reuse. No SQL/XML/version/library/policy
change or additional resident process is introduced.

Added ae_fixture_resolve_action: verify canonical bytes against an independently
expected artifact, resolve the exact declared action, and copy owned executable,
literal argv, file size and SHA-256. No shell/PATH expansion or main-to-run fallback.
Native vector cases check all declared actions, limits, empty/Unicode arguments,
hash mismatch, invalid/absent actions and unchanged failure output. This is metadata,
not publisher enrollment, a code FD, a live broker admission or readiness.
The exec boundary now accepts argv[0] plus eight literal manifest arguments. A real
post-exec payload checks all nine argv entries unchanged; a tenth entry is rejected
before exec. The payload also accepts its manifest-relative bin/probe argv[0].

Before installing updated owned authorities, actual state showed pending=None,
setup withdrawn and zero live generation leases/dispatches. An ignored guest-build
snapshot, exec-original-before.json, captured every old install/generation/operation/
step/dispatch/lease row and original config/store object inode/mode/ctime/hash.
The guarded update-owned path then updated stopped authority/probe/helper images.
The helper's implementation remains no-exec; terminal setup/profile history was
not repurposed. No unacknowledged request, failed setup or catalogue was patched.

Executed controller sequence: exec-fixture-prepare, exec-fixture-check,
exec-fixture-store-prepare, exec-fixture-store-check, exec-fixture-activation-prepare
and exec-fixture-activation-check. Native root observations bind independently
preallocated objects. Staging is nonexecutable; publication retains exact container/
payload inodes, seals MAC/modes/content and denies further setattr; first activation
completes the original new operation atomically. Each check also repeats the exact
accepted operation without changing receipt/content bytes or identities.

Actual before/after comparison confirms every old business row remains present
with identical column values, and all original protected config/publication objects
retain inode/mode/ctime/hash. Both aetheri.p0.probe and aetheri.p0.exec are active,
their generations committed and install gates closed. No live lease/dispatch is
present. Original private data/bindings were not reassigned. The new package is an
inert execution fixture, not an app run or new desktop feature. Its own private data
and projected user binding do not exist yet; ordinary broker/data/setup selection
still uses the original profile. Normal enforcing endpoint/app-denial checks pass
after updating the authorities, and owned authority/probe units were stopped.

Full native suites pass 32/32 Debug ASan/UBSan (137.62 s) and Release (125.85 s).
After adding only maximum/overflow argument probes to tests, focused linux.execute
passes Debug (1.52 s) and Release (0.31 s). Its 23 scenarios now include all eight
literal extra arguments and deterministic ninth-extra-argument rejection. No
production C code changed after the full suites. Host migration/wire tests pass
29/29 (28.552 s), orchestration 36/36 (0.254 s), frozen runtime vectors 14/14 and
isolated constraints 374 assertions. Timings are test evidence, not OS performance.

After QMP system_powerdown, run_vm status verified recorded_process_alive=false
and qmp_verified=false before a new offline boot. An initial SSH banner timeout
was only a boot-readiness observation failure; the same live QEMU process/QMP
remained verified and was not restarted. The subsequent SSH session succeeded.
Normal enforcing endpoints/app denials passed on the new boot, then both owned
services stopped. exec-fixture-store-check and exec-fixture-activation-check report
cross_boot_reopen=true with original object inodes, sealed types, completed exact
receipts, unchanged publication, closed gate and app_dispatched=false. This is
normal reboot evidence, not power-loss recovery or target execution acceptance.

Next: project the compatible installation into the user catalogue, allocate/enroll
its own private DataEpoch, and connect durable target dispatch, actual registered
process/image identity, readiness and independent drain/recovery. An executing
starter needs a different contract from immutable no-exec setup withdrawal.
The full OS, original desktop/apps/image and hardware acceptance goal stays active.

## Separate execution data — 27 October 2026

Added a no-input protected RefreshExecFixtureInstall method, private Development1
1.4, using the same controller ProcessFD/unit/invocation/boot admission and typed
authenticated Root projection as the original method. Original methods/signatures
and their compiled selections remain unchanged. Only the selected user writer
stores the second binding; Root never reads the user catalogue. Generated XML
tables, probe helper, ordinary app and same-domain/wrong-unit denial cases include
the new method. Public capabilities remain empty; SQL and other XML stay frozen.

Extended the existing protected original/exec registry selection to separate
owner/enrollment files and exact raw data contexts. Original data keeps c33,c34;
the new installation receives its own c35,c36 profile and fresh StorageRef,
DataEpoch and allocation-operation IDs. The final ownership field is allocation
operation, not a MAC policy ID. Machine/volume/account and independently enrolled
common app-data/app-allocations parents are reused. No parent or old private leaf
is adopted/recreated. Fixed P0 categories do not implement general allocation.

Native data administration now checks full raw label, UID/GID, 0700 and agreement
with the protected selected fixture's account/install/publisher before publishing
an allocation or entering SQL enrollment. Existing AETHOWN1/AETHALC1/AETHDTA1
bytes and owner transaction are unchanged. Only the controller's global empty-user-
catalogue restriction was removed: absence is scoped to the selected component/
publisher/account/install. Checks now observe selected rows and independently
retain unrelated data/attachments/gates, bindings and launch/grant/operation rows.

The guarded owned update ran only after verified pending=None/setup withdrawn.
Before it, exec-data-before.json in the ignored guest build captured all old
system/user table rows except authority_meta and both packages/configurations plus
the original private root/payload/header/config/allocation inode/mode/ctime/hash.
Actual exec-projection-check persisted the second binding and proved exact retries
and same-UID unselected-caller denial without data/gate or Root-catalogue effects.
exec-data-prepare independently created/observed the distinct root and sealed its
owner/enrollment configuration. It did not publish an allocation or enroll SQL.

A newly added controller alias guard used policy instead of the frozen
allocation_operation field. load rejected the prepared record before the negative
test changed any object or submitted a unit. Corrected the field, added valid-owner,
cross-install and alias regression cases, and reused the same prepared identities.
No data/config recreation or native SQL repair occurred.

Actual wrong-category test: with all services stopped, stable carrier lock held
and allocation record absent, a recorded Root action changed only the known new
empty payload to c37,c38. The cap-zero enforcing user-domain --exec-data-enroll
returned native EACCES (-13). Catalogue bytes were identical and no allocation
record existed. The exact failed unit independently drained; Root checked the
same payload inode before restoring c35,c36. The passed probe remains in the
controller record; only its observed drained failed unit was reset, with journal
evidence preserved. No Enforcing exception or new policy permission was used.

Normal exec-data-check then published the immutable allocation, atomically enrolled
data/attachment/closed gate and proved byte/inode/revision-identical retries.
Original data-check also passed with the second data present. Independent snapshot
comparison found every old system/user row with identical columns and both
packages plus old private objects unchanged in inode/mode/ctime/hash. Both apps
have separate active attachments and closed data gates. No app dispatch occurred.

Updated actual endpoint/app-denial checks and broker-check pass: copied identity,
unconfined and same-domain wrong-unit calls cannot use the new method; departure
reduces Root FDs 12 to 11; user replacement and Root-only restart pass. Owned
authority/probe units were stopped, with Enforcing retained. No additional
resident service, external dependency, schema or policy grant was added.

Full native suites pass 32/32 Debug ASan/UBSan (136.26 s) and Release (120.78 s).
Host migration/wire 29/29 (28.172 s), orchestration 39/39 (0.235 s), runtime
vectors 14/14 and isolated constraints 374 assertions pass. These are test-run
times, not OS benchmarks. Native code did not change after the full suites.

After QMP powerdown, the original runner returned exit 0 and status verified PID
absence before a new offline boot. exec-projection-check reports
cross_boot_reopen=true and system_runtime_changed=true; exec-data-check and
original data-check both report cross_boot_reopen=true with exact retries and
closed gates. These are actual normal-reboot checks, not power-loss recovery.

Next: durable executing dispatch and actual registered target/image/readiness/
drain for the new exact I/G/D/storage/run tuple. Keep Root/User cap-zero and keep
the executing setup boundary separate from the immutable no-exec helper/release
proof. Public launch and DispatchLaunch remain unsupported. Original desktop,
apps/image, document transport, complete removal/recovery and physical hardware
acceptance remain required by the active full project goal.

## Durable execution intent and arm — 27 October 2026

Implemented a distinct 384-byte AETHEXE1 profile/codec and native owner APIs for
the exact run/p0/none reservation. The profile freezes both authority runtimes,
boot/account, I/G/D/storage, operations, revisions and artifact/starter/request/
allocation digests. User reads its own accepted run, Root receipt and current
private-data fence. Root journals REQUEST, actual-invocation BIND, PREPARE and
ARM without any external effect inside SQLite. ARM freezes the invocation/data
fence and holds the running code lease; it does not record actual target readiness.

The new stage/generation journal cannot match the no-exec stage/launch proof,
even if its hash is passed deliberately to the old API. Ordinary cancellation
also refuses its extra steps. Fresh successful REQUEST/ARM returns effect_required=1;
all retries return zero, including a completed commit reported as failed. An
unknown outward result stays held for reconciliation rather than resending.
The live caller must enforce these preconditions; profile/digest bytes grant no
authority and Root cannot read a private User gate. No release/completion path
or live DispatchLaunch implementation was added.

The new catalogue.execution case runs 16 isolated scenarios: independent Python
canonical profile/hash, every mutating-statement failure with identical whole-DB
bytes afterward, pre/post-commit errors, actual store-owner exits before/after
REQUEST and ARM commit, exact earlier-stage retries, changed evidence, no-exec
withdrawal rejection, closed install gate, four revision overflows, all-stage
Root-runtime restart and User replacement. Controlled owner-file reopen tests
preserve their registered runtime; the distinct runtime-restart tests reject
reuse and retain uncertainty/leases. No power-loss test is claimed.

An initial C build found an invalid SQL macro/string concatenation at the reason
update. Prefixing each literal before selecting it fixed that compiler error;
the next build and focused execution test passed. No test failure was concealed.
The complete suites then passed 33/33 Debug ASan/UBSan (193.91 s) and 33/33 Release
(177.66 s). Host migration/wire: 29/29 (34.240 s); orchestration: 39/39 (0.320 s);
runtime vectors: 14/14; isolated constraints: 374 assertions. These are test times.

Read-only controller verification under the stable carrier lock passed after
both suites: installed protected objects/configuration/policy match the owned
record, Enforcing, no pending request, both installation and data gates closed,
all existing live dispatches terminal and leases released. The compiled new API
was not installed into the authorities; the normal fixtures/catalogues were not
used for these new journal tests. No schema/XML release, dependency, policy grant
or resident service changed. Reviewed source/documentation is published separately
from ignored guest build/test logs and generated canonical vectors.

Next connect this journal to a distinct executing starter's authenticated error
channel and held actual process/image. Follow with app readiness, independently
proven drain and conservative recovery; never reuse no-exec teardown for that
starter. The full OS/apps/desktop/image and hardware goals remain active.

## Issuer-owned execution error channel — 27 October 2026

The previous goal turn made verified progress in b64055e. This step implements
the bounded Linux error channel needed to connect that journal to actual exec.
The issuer creates an anonymous pipe and retains its process-owned reader; only
the NONBLOCK/CLOEXEC writer is exported. The one-way boundary now shares the
anonymous pipe/Root creator/mode/flag validator. No pipe pathname, persistent
state file, background process, dependency, SQL/XML or policy grant is added.

The reader retains at most 17 bytes and performs at most two nonblocking reads
per call. A complete frame without EOF remains pending. Exact frame plus EOF
preserves phase/errno; empty EOF stays unknown; invalid, truncated or extra bytes
are terminal malformed evidence. Terminal reports perform no more reads, and
their future event-loop watch must be disabled. I/O errors preserve outward
output and the consumed prefix. No channel result releases a lease or claims
target execution/readiness/drain.

linux.peer now issues the writer in an actual sd-bus reply to the exact captured
child ProcessFD/name/UID/label, with an independently delivered unique destination.
It closes the issuer copy; the child checks and duplicates the borrowed message
FD before unref, writes fragments and closes its copy. The retained reader gets
the exact UID/EPERM failure. This user-bus test proves transport/FD lifetimes,
not live Root execution admission. The native actual-exec harness uses the same
reader, including while an executed payload is alive and after both exit-125
cases. Independent image observation remains necessary.

linux.exec_channel adds fragmentation/overflow/truncation, phase/error bounds,
retained-writer EOF, flag/creator mismatch, named FIFO rejection, descriptor reuse,
fork-owner rejection and closed-stdin cases. Allocator, pipe, fcntl, metadata,
read and close faults retain outputs and balance resources. The first focused
Debug peer/channel/exec checks passed 3/3 (1.61 s).

An explicit policy probe was initially given the Debug binary. Its LeakSanitizer
exit check failed: actual audit event 925 denied same-domain ptrace for
aetheri_exec_setup_test_t. The controller retained state=failed, pending=null and
the exact one known invocation/fixture/module. After independent process/cgroup/
job drain, --cleanup verified identities/inventory/hashes and retired those
objects. An ignored guest failure receipt plus audit/journal evidence remain.
Release then passed; no instrumentation permission was added. The policy test's
instructions now require Release while ordinary CTest retains ASan/UBSan.

Final source passes 34/34 Debug ASan/UBSan (199.11 s) and 34/34 Release (176.38 s),
29/29 migration/wire (39.113 s), 39/39 orchestration (0.612 s), 14/14 runtime vectors
and 374 isolated constraint assertions. A final enforcing Release probe also
passes all five actual target-domain cases with exact image, UID/GID 1000,
empty capabilities, sealed FDs and unknown empty channel, then drains/resets only
its known units and removes its temporary module. Timings are test-run evidence.

Read-only verification under the stable carrier lock confirms systemd259.10,
Enforcing, matching protected normal objects/configuration/policy, two closed
install gates and two closed data gates, no pending/unresolved live dispatch or
held lease, and absent temporary execution fixture/module. The normal authorities
were not updated and no managed app was registered/dispatched.

Next bind the issued channel to the live executing profile/journal, original
held starter/invocation and authenticated current Root reply. Follow with actual
target registration/image/readiness and independent drain. The no-exec proof
still cannot settle an executable starter. Desktop/apps/image/hardware and the
full OS goal remain active; UI/personal choices stay deferred to the user.

## Native execution coordination and durable diagnostics — 27 October 2026

Implemented authority/execution as a Root UID/GID 0 native library joining the
existing journal, original starter pin/context, unit reference, issued pipe and
sd-event watch. It copies one protected/admitted profile; live caller capture,
image/unit/profile provisioning and current User data admission remain adapter
obligations. The reserved starter type has no installed policy yet. The archive
libaetheri_execution_service.a is a build artifact, not another resident service.

Compiled request/release adapters run after fresh commits, outside SQLite, once.
Unknown results never reissue them. A genuine original starter may resolve an
unknown request through binding; it gets one writer only after successful BIND.
Ambiguous binds retain pins/reader but issue no replacement. Re-observation under
the unit reference binds AETHEXC1 context evidence, and observed drift remains
quarantined even if later restored. The pre-arm channel sample requires pristine
observed state; queued EOF or partial bytes block release before event dispatch.
Callbacks cannot destroy their own coordinator: close returns EBUSY while active.

Terminal event handlers disable their watch and atomically record AETHEXD1 in
existing integration step 4 plus bounded canonical reason. Kind/phase/errno stay
distinct for unknown, starter failure, malformed data and observer failure.
The digest binds exactly 100 canonical bytes to the whole profile, actual
invocation and observed context. Diagnostic acknowledgement can retry without
another external effect, revision change or evidence replacement. Restart
retains evidence and the lease; old-runtime reporters cannot write. Closing
local objects never stops a process, completes an operation or releases a lease.
Request/release/context error details are still local status; their persistent
request/bind/arm remain unresolved. This is not complete production recovery.

authority.execution_service uses real owner transactions, owned-child ProcessFDs,
pipe/event dispatch and cleanup with controlled process/PID1/effect adapters.
It covers 16 scenarios: wrong peers/contexts, inherited-owner/unprivileged creation
denials, drift, request/bind/prepare/arm/report commit losses, callback timeout,
early EOF/partial bytes and non-release on close. GetDispatch inside the effects
starts a separate transaction, proving commit precedes the callback. Unread
malformed data causes no further event dispatch; the child stays alive after
coordinator close until the harness ends it. It proves no live unit/domain
admission. Catalogue execution now covers 20 scenarios, including independent
Python diagnostic bytes/hash/reasons, every diagnostic write/pre/post-commit fault,
idempotence, changed-evidence rejection and restart retention.

The initial coordinator group passed 13 cases in 46.11 s; the expanded owner/
diagnostic groups then passed 2/2 (136.85 s). One compiler shadow-name error in
the expanded test was corrected. A first full Debug run had 34/35 passes: the
older peer fixture assumed its bounded reader reached terminal state immediately
after child exit. It now waits for actual stream progress/EOF under one monotonic
five-second deadline; transport references and bounded reads are not child-death
evidence. The corrected peer passed 24 consecutive roundtrips; its reader's
pending/unknown semantics were not relaxed. Focused service/peer/channel passes
3/3 (64.06 s), including the new partial-data arm guard.

Final suites pass 35/35 Debug ASan/UBSan (278.09 s) and 35/35 Release (250.86 s).
Migration/wire passes 29/29 (38.208 s), orchestration 39/39 (0.433 s), runtime
vectors 14/14 and constraints 374 assertions. The installed259.10 headers/man
pages and actual event tests substantiate ownership/disable behavior; online
version-specific fetches were unavailable, and the current262 page was only a
cross-check. No new library version/family, schema/XML or policy grant was adopted.

After both suites, read-only verification under the stable carrier lock confirms
the normal protected executables/configuration/policy match the owned record,
Enforcing, two closed install and two closed data gates, no pending/unsettled live
dispatch or lease, and no temporary execution module/fixture. The normal authority
binaries were not updated. No actual managed app was dispatched by this component.

Next implement live wire/supervisor adapters, protected exec-profile/image/unit
provisioning and the selected executing starter. Then bind actual target image/
registration/readiness and independently proven drain/recovery. These remain
implementation gates within the full OS/apps/desktop/image/hardware goal; no
user product/UI decision blocks the remaining backend work.

## Protected execution-profile/source binding — 27 October 2026

Implemented authority/execution_profile.[ch] for the fixed EXEC selection. It
reads canonical AETHEXE1 from a reserved protected path, reserves a separately
labeled/hash-bound starter ELF, and joins the witnessed store publication,
verified manifest/complete inventory/exact declared run image and independent
private allocation/owner/root witnesses/full c35,c36 object context. Successful
source output owns three CLOEXEC FDs. Failure leaves output unchanged and closes
temporary handles. There is no SQL/gate/process effect or lease release.

Moved unchanged pure registry selections and AETHFIC1/AETHSTO1/AETHACT1 decoders
to fixture_profile.c, allowing reuse without administrative staging/publication
entry points. A review compared all four relocated definitions byte-for-byte
with HEAD. Added a no-write/no-fsync inventory check through the existing import
validator; existing sync/publication durability paths retain their flushes.
No new library family, schema/XML version, policy grant or resident process.

Tests use Root administrative access in the enforcing carrier, real openat2/
statx/inventory/ELF/digest and actual file-context observations. Only the initial
fixed root path is redirected to owned scratch fixtures. They do not prove the
future starter domain's policy permissions. Repeated successful/failing calls
check output preservation, FD balance and unchanged tree metadata/content.
Positive starter admission remains untested: its reserved file/type/profile are
not provisioned. Absence and a binary labeled for the different setup purpose
are rejected. Source checks still owe live catalogue/User admission, protected
topology/mutation exclusion and private noexec view preparation before exec.

Two initial test expectations confused different link-rejection paths: the
inventory classifies an O_PATH final link as EBADMSG, while O_DIRECTORY payload
opening returns ENOTDIR. Both expectations were corrected without weakening the
production checks. Failed scratch trees/logs remain under ignored guest builds.
An initial actual-installation source check also caught an incorrect new loader/
test assumption: the shared store parent is 0700 aetheri_store_parent_t, distinct
from sealed 0555 package containers. Direct metadata and existing provisioning
code established that shape; corrected source checks then pass on actual paths.

Full regression passes 36/36 Debug ASan/UBSan (305.95 s) and 36/36 Release
(251.79 s). A final review added an explicit fixture account-scope comparison;
scope/generation negatives keep the publication digest consistent so that only
the tuple guard rejects them. The affected native group then passes with all
35 fixtures on final code: Debug 7.66 s, Release 2.48 s. A test-only vararg wrapper
also now retrieves the exact struct open_how pointer type. No production change
followed those targeted runs. Core import tests verify zero file/directory syncs
for read-only checking, while existing publication/resync tests still pass.
Host migration/wire 29/29 (46.893 s), orchestration 39/39 (0.386 s), runtime vectors
14/14 and isolated constraints 374 assertions also pass. Times are test-run
durations, not OS performance benchmarks.

The explicit --live-read-only probe locks the carrier, verifies original owned
inputs, requires closed gates/no unsettled launch/lease, and creates only a
build-local observation profile with synthetic request IDs. The final Release
binary opens the actual retained EXEC code/private data eight times without root
mapping; catalogues/protected inputs remain unchanged and Enforcing. No normal
authority/helper was updated, gate opened, new protected config/image installed
or managed app dispatched.

Next connect the source/profile prerequisites to actual authenticated Root
replies and the distinct executing starter, with reviewed protected unit/profile/
image/policy provisioning and native coordinator adapters. Preserve original
no-exec contracts. Target image registration/readiness, independent executing
drain/recovery and full OS/apps/desktop/image/hardware acceptance remain work.
No personal product/UI choice blocks these next backend steps.

Final read-only verification under the stable carrier lock confirms Enforcing,
unchanged owned inputs, no pending request, two closed install gates and two
closed data gates, no unsettled live dispatch/lease, no temporary execution
module/fixture and no execution.conf/execution-starter provisioned.

## Authenticated executing-starter client — 27 October 2026

Implemented the native client in launcher/execution.[ch], private Execution1 1.0
XML and separate build-generated names/signatures. Default authority method
tables, installed binaries, public capabilities, SQL and older XML remain
unchanged. The actual executing starter and journal-backed producer are not
installed or implemented by this consumer/fixture step.

Constructor discovery resolves System1 once, then pins actual reply UID/PID/
full Root domain and active selected unit/invocation/cgroup/boot. It compares
Root epoch/runtime against protected AETHEXE1. New peer_capture_reply binds an
actual METHOD_RETURN to its retained sent unique-destination call by bus, sender
and cookies, without consuming the body or accepting augmented/missing process
credentials. Both capture APIs refuse occupied outputs. Current original Root
context is rechecked before/after exchanges; no replacement owner is followed.

Registration compares all binding fields/revision and validates/duplicates only
the borrowed anonymous Root writer. Client close cannot close that transferred
writer, stop a process or settle a lease. Prepared release accepts only an exact
same-owner receipt with advanced revision; the future producer must hold that
reply until fresh durable ARM. Each call runs once per client with bounded two/
five-second waits. This is not durable duplicate prevention: callers must not
reconstruct to retry uncertainty, and the issuer journal/profile stays authoritative.
Receipt acceptance does not prove target exec, registration, readiness or drain.

The explicit probe uses a protected Release synthetic producer in the actual
existing Root domain, NNP, cap-zero, selected notify unit and one-task limit.
It has no catalogue, mount setup, target exec or app registration path. Its 28
protocol cases cover field/revision errors, unsuitable writers, method errors,
unknown timeout, exact call counts/no retry, output/FD ownership, original peer
departure, fork ownership and non-Root constructor denial. A separate Debug
ASan/UBSan client also passes against that producer without extending confined
LeakSanitizer ptrace permissions. The normal peer test separately exercises
actual correlated replies, mismatched call cookies, missing/augmented credentials,
occupied output and repeated-query descriptor balance.

Two initial protocol attempts reset the Root producer connection on the first
writer transfer. Added focused diagnostics rather than broad permissions. Actual
VM AVCs identify system_dbusd_t write denial on the aetheri_system_t pipe; upstream
Linux6.19 file_receive corroborates received-file permission checks. Temporary
policy now permits Root's own pipe operations and daemon write only, with no
reader grant. The corrected probe passes. Incidental certificate-directory search
remains denied; no crypto/TLS permission was needed. Known failed processes/
unit/cgroup/jobs were independently checked before retirement; their failed unit
metadata was reset only after drain, with logs retained under ignored builds.
The first XML contract test also caught duplicate input/output argument names;
bound output names now remain unambiguous without changing wire types.

Final native regression passes 36/36 Debug ASan/UBSan (305.47 s) and 36/36
Release (249.52 s). The final explicit Release protocol probe passes after those
suites and removes its owned module/files after original PIDFD death and unit/
cgroup/job drain. Host migration/wire passes 30/30 (52.959 s), orchestration
39/39 (0.258 s), runtime vectors 14/14 and constraints 374 assertions. Timings
are test durations, not OS performance measurements. No new dependency family
or permanent policy grant/resident process was introduced.

Final read-only carrier verification confirms Enforcing, unchanged normal owned
inputs, no pending request, two closed install gates/two closed data gates, no
unsettled dispatch/lease, no temporary reply/exec fixture/module and System1
unowned. execution.conf/execution-starter remain unprovisioned. Other projects,
host storage, hardware and ordinary installed authority/helper were untouched.

Next provision the independently protected Root unit/profile and actual selected
executing starter, implement the journal-backed Root handlers and native request/
release adapters, then bind actual target image/registration/readiness and
independent executing drain/recovery. Keep the old non-exec helper/proofs separate.
The full OS/apps/desktop/image/hardware goal remains active; no user product/UI
decision blocks the next backend work.

## Journal-backed execution producer — 30 October 2026

Added a separate native Execution1 endpoint around the existing owner coordinator.
It borrows catalogue/bus/event, owns one deferred prepared call and permits only
the compiled request effect. Registration sends the actual journal binding and
one copied writer after BIND. Prepared retains the original request; fresh ARM
after commit alone consumes it and sends release. Consume before allocation/send
and return handled after uncertain sending, preventing an automatic second reply.

One four-second monotonic timer fits inside the client's five-second timeout.
Fresh time is checked before ARM and again after commit. Timeout/send uncertainty
keeps dispatch/lease obligations, including a successful commit followed by an
expired release. Closing the component cannot stop its starter, free a lease or
complete an operation. Callback-time close is busy. No installed method table,
normal service/policy, public capability, XML/SQL version or dependency changed.

The component scenarios use actual isolated owner catalogues, ProcessFDs, pipes,
descriptor transfer and correlated deferred replies. Actor label and PID1 context/
reference observations are explicitly controlled adapters. The child remains
alive and its lease held after component close. Lost BIND/PREPARE/ARM commit
acknowledgements, pre/post-send failures, actual timer/pre/post-commit deadlines,
wrong peer/tuple, drift and broker/Root replacement are covered.

The initial system-bus test was rejected by routing policy before handler entry.
dbus-run-session is absent. Reused existing dbus-broker37 on an owned private
Root-only session-type socket/config without activation entries. An initial user-
scope launcher failed ENOMEDIUM; actual source/man review shows it still opens
the selected manager bus. System scope supplies its normal PID1 subscription;
tested calls stay private. No global rules were weakened or package installed.
The owned launcher/broker group is retired by the harness. Logs remain under
ignored builds. The focused Debug ASan/UBSan group passes 16 scenarios (67.52 s).

Full Debug ASan/UBSan regression passes 37/37 (370.85 s), Release 37/37 (314.36 s).
Host migration/
wire passes 30/30 (52.400 s), orchestration 39/39 (0.399 s), vectors 14/14 and
constraints 374 assertions. These are test durations, not OS benchmarks. A host
PowerShell final-line CR prevented the first Release CTest invocation; the corrected
direct invocation passes, without a native-code change.

Final read-only carrier observation confirms Enforcing, verified normal protected
inputs, pending=None, two closed install gates/two closed data gates, no unsettled
dispatch/held lease, no temporary execution fixture/module/private test broker and
System1 unowned. No normal authority/helper/profile was updated or app dispatched.

The dedicated VM was initially stopped. Verified its recorded PID was dead and
the QEMU process inventory empty before removing only the orphan local QMP
socket and booting the existing disk offline. No reinstall, host/other-project
cleanup or hardware action. Normal protected execution inputs remain unprovisioned.

Reviewed transaction/effect ordering, retained-message/FD/timer lifetime, fork/
busy ownership and unknown-result paths. This is component evidence, not actual
confined starter/live-launch admission. Next connect protected Root/starter
provisioning, native supervisor request and current broker/private-data admission,
then actual target registration/readiness and independent executing drain/recovery.
The non-executing helper/proofs remain separate. Full Aetheri remains the goal.

## Separate executing starter and witnessed views — 30 October 2026

Implemented a distinct aetheri-execution-starter image and a coherent preparation
module. Startup checks Root UID/GID, single thread, Enforcing/NNP, complete starter
domain and exact five setup/credential-drop effective/permitted capabilities,
then clears inherited/ambient bits. Protected AETHEXE1/current ELF checks precede
RPC. The original-Root authenticated client uses a compiled fixed P0 Root unit;
it registers once before private setup, waits for the original release and
rechecks target handles before the existing one-way exec boundary. No arguments,
paths/UID/context/environment or Root unit come from an app or inherited variables.
The old setup-only binary and its proof remain unchanged.

Added AETHVWS1: 184 canonical bytes bind the complete profile digest to four
independently recorded temporary scaffold objects. Witnesses use filesystem type/
ID, inode and mandatory birth time, never durable mount IDs or filename adoption.
The independently packed frozen vector and strict reference encoder cover the
format without changing older vectors/SQL/XML. Native views retain exact directory
FDs through attachment, reopen sources inside their private namespace and join
held/named mounts plus image on recheck. A recreated directory or later replacement
mount cannot supply another target. Closing handles cannot prove namespace drain.
Root's admitted/flushed pre-dispatch scaffold publication/journal remains work;
the reader is not a creator or recovery adoption path.

The inert EXEC fixture receives exact manifest literals and three fixed variables,
UID/GID1000 and c35,c36. Its eventual actual execution is still unaccepted. Starter
failure uses the issued bounded AETHXER1 frame and exit125, with invalid internal
phase/errno normalized to PROFILE/EINVAL. No writer/EOF/exit125 proves no execution;
never extend the old no-exec withdrawal/drain proof to this image.

Actual enforcing-guest filesystem tests cover the previous35 source fixtures plus
15 new scaffold/private-mount cases, with only the initial anchor mapped. They
check missing/bad records, exact profile/object binding, recreated directories,
nonempty slots, private source/view flags, repeated FD balance, fork ownership,
parent namespace isolation and a later replacement mount. Controlled flow tests
cover 13 ordered success/fault cases plus three real frame/normalization cases;
context/client/views/target execution are explicitly replaced there. The first
flow test correctly hit the process-owned channel accessor's ECHILD guard after
fork; preserving the raw reader number before fork fixed only that test.

The actual Debug starter rejects unprivileged/default administrative contexts
with125 and extra arguments with2, before profile/RPC. Those negative observations
do not establish positive starter-domain/self-image admission. Focused Debug
source/flow/execute groups pass (12.54 s); full Debug ASan/UBSan passes 38/38
(379.63 s), Release 38/38 (312.22 s).
Host migration/wire passes 30/30 (54.772 s), orchestration 39/39 (0.296 s), vectors
15/15 and constraints 374 assertions. Timings are tests, not OS benchmarks.

Final read-only observation confirms Enforcing, verified normal protected inputs,
pending=None, two closed install/two closed data gates, no unsettled dispatch/held
lease, no temporary execution fixture/module/private test broker and System1
unowned. The starter image/profile/scaffold record/fixed Root unit remain absent
from the normal installation. Release negative context/argument probes also pass.
No normal authority/helper/profile was updated or managed app dispatched. No host
storage/hardware or other project was changed.

Reviewed unknown/failure paths, exact profile/scaffold binding, held descriptors,
namespace changes and local-close semantics. No dependency family, policy grant,
normal service/profile or public capability was added. Positive confined starter/
Root admission, protected provisioning/live supervisor request and fresh broker/
data mediation remain next, then actual target image/registration/readiness and
independent executing drain/recovery. The full OS/apps/UI/image/hardware objective
remains active; no personal product decision blocks that backend work.
