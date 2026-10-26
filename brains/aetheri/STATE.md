# State

26 October 2026 — reviewed OpenSSL 3.5.9 and native fixture SHA-256 verification.

## Confirmed

The user accepted the minimal extension/P0 plan and requested implementation of
versioned database/API contracts and migration tests.
The active user goal is to implement the full Aetheri project. The headless carrier
and core below are progress toward that scope, not a smaller completion target.
All earlier architecture, ownership, lifecycle and language decisions remain.
P0 carrier/resource selection is accepted as the development direction, not the
final Aetheri distribution or runtime acceptance.

## Implemented

[Versioned contracts](../../spec/README.md): complete common/system/user v2 SQL,
five new table designs, unchanged v1 references and two additive XML interfaces
with nine methods. Schema version 2 is independent from wire major/minor 1.1.

The Python development harness performs empty bootstrap, canonical validation,
consistent backup and atomic v1-to-v2 migration. Old rows/IDs are preserved;
new gates are closed and runtime incarnation unset pending service reconciliation.
This harness is not a privileged Linux service or deployed C17 migrator.

The [native C17 store](../../src/catalogue/store.h) now implements explicit v2
creation, canonical opening/validation, writer lifetime and atomic runtime/boot
updates. SQL is embedded at build time from the same canonical sources. Existing
open gates close and recovery triggers run; no holder is settled or gate reopened.
Root/MAC/registry identity, actual boot/runtime generation and service admission
remain caller duties. The prototype service below now supplies its protected fixed
registry, SELinux type and actual kernel boot; volume registration and full
admission/recovery remain pending. No native v1 migration is implemented.

[P0 runtime contracts](../../spec/runtime.md): bounded fixture manifest,
104-byte private-root header and canonical plan-intent encoding reference, with
three independently constructed fixed byte/digest vectors. Payload verification
checks supplied bytes and inventory; it is not a privileged filesystem importer.
Principal/volume/supervision/recovery obligations remain pending; the native
header/file primitives below implement only their bounded storage portion.

[Carrier input lock](../../spec/vm-inputs.json), verified cache and Windows-local
archive extraction now provide QEMU 11.1.0 Windows build
v11.1.0-12130-ge470268ff4. The unsigned 7-Zip MSI and expired-signature QEMU NSIS
installer were read as data, never executed as installers. Expected hashes and
third-party licenses were retained. Actual QEMU/firmware execution now works.

Installed Fedora Server 44 carrier: UEFI, 2 vCPUs, 8 GiB RAM, one 48 GiB sparse
qcow2 disk, ext4 root/boot. Installer validates guest product, exact virtual disk
serial/size and disk count excluding RAM zram. No raw host disk or folder sharing.
Host orchestration uses scoped keys, guarded starts and an AF_UNIX QMP endpoint
under the Windows-user-only VM directory. TCP QMP was removed after a real guest
reachability test; no guest-reachable QMP network listener remains.

[Native core](../../src/core/owner.h): C17 owner encode/decode/matching and
[bounded storage](../../src/core/storage.h) with required openat2 containment,
regular-file/size/link checks and protected-header UID/mode/identity validation.
No public RPC or app grants are exposed by these primitives.

[Root observations](../../src/core/root.h) now require the kernel's non-recycled
unique mount ID and repeat held directory identity/ownership observations around
fstatfs. Native catalogues retain and recheck the held/named root on use.
[Storage references](../../src/core/storage.h) bind an independently expected owner/
directory identity to its derived StorageRef under a trusted anchor. Before/after
internal bounded file opens they check held/named root, exact header and private
payload UID/permissions. They own duplicate anchor/root FDs and reject inherited
fork use. Closing a reference does not revoke an already returned internal FD.
These are runtime primitives; persistent volume/allocation enrollment, same-UID
MAC admission and public native-byte transport remain caller/integration duties.

[Allocation witnesses](../../src/core/allocation.h): canonical immutable 168-byte
records retain all six owner IDs, anchor/root inode and birth time, and filesystem
observations. O_TMPFILE/create-only link/fsync publication never overwrites a
committed key. Exact retries retain its bytes/inode; mismatches fail. A protected
existing record supplies expected object facts when rebuilding a current reference.
This is not genuine-volume authentication or a complete allocation-operation journal.
An optional fixed allocation fixture now exercises publication/verification through
the confined user authority's one-shot administrative modes and catalogue writer lock.
It is tracked private data, so empty-bootstrap cleanup intentionally refuses it.

[Fixture metadata](../../src/core/fixture.h): canonical AETHFIX1 decoding now
returns bounded borrowed text views and checked file/action/provider records.
Strict UTF-8, token/path/order/ancestor/mode/count/size/action checks reject bad
metadata without changing output. There is no allocation, filesystem operation,
payload hash verification, publisher enrollment or app installation in this API.

[Linux peer capture](../../src/linux/peer.h) now collects required authenticated
bus credentials and an owned process pin without /proc augmentation or numeric-PID
fallback. It rejects missing/augmented provenance and preserves outputs on error.
This is connection identity, not app/run authorization or current-domain proof.

[Process observer](../../src/linux/process.h): the held process reference now
joins current kernel UID/label, actual system service/invocation and cgroup-v2
path/inode identity. Required PIDFD_GET_INFO fields must exist; observations repeat
under liveness checks and stale contexts are rejected. The system endpoint uses
its self-only pin to corroborate its own invocation/boot on discovery. This is
not itself broker/app registration, catalogue admission or target-exec evidence.

[Broker enrollment](../../src/authority/broker.h): a protected root-owned slot and
account/epoch bootstrap select the sole eligible UID-1000 user service. Register
binds its authenticated bus/sender/ProcessFD, current domain/credentials and actual
unit/invocation to its declared catalogue runtime. Check requires active state and
revalidates enrollment/context. Name/exit events invalidate the retained identity;
no polling or app-holder settlement. This is not general account or app admission.

System-owner native account enrollment/check now uses the existing scopes table.
Creation and catalogue revision advance commit together. Exact active retries do
not rewrite the database; retired or differently bound scopes and active UID
collisions fail. The fixed system service requires its protected account row on
normal startup, Register and Check/Runtime1 authentication. Explicit initialization
or stopped administrative enrollment may create it. No general account creation,
retirement API, NSS synchronization or private-data adoption is implemented.

[Temporary SELinux fixture](../../policy/p0-fixture.cil): tested one MCS-constrained
app domain with two same-UID/disjoint-category data roots, separate synthetic
broker/document types and narrow supervisor setup permissions. The C17 probe and
root-only controller are explicit opt-in tests; their module/root are removed
after success. No production policy/category allocator is implied.

[C17 authority](../../src/authority/main.c): two supervised service roles with
dedicated enforcing domains and separate catalogue writers. Fixed root-owned
bootstrap registries bind account/epochs; process startup verifies its own type,
NoNewPrivileges and empty capabilities. Actual boot/runtime are committed before
the bus name and readiness notification. Generated method tables preserve accepted
method/argument signatures. GetProtocolInfo returns 1.1 and no capabilities;
other public methods return UnsupportedCapability. Private Broker1 Register/Check
work; Runtime1 denies unregistered callers and remains UnsupportedCapability for
an authenticated active broker. See [service boundary](../../spec/runtime.md#confined-protocol-services).

## Executed evidence

Python 3.12.14 / SQLite 3.53.1 on the development host:
The catalogue/API suite now has 27 named tests, including private broker wire shape,
literal-preserving
canonicalization and real schema-drift rejection, with unchanged frozen v1 files.
All pass again on the host in 33.642 s, exit 0.
14 runtime-format tests passed again, exit 0, 0.012 seconds.
7 download-cache tests passed, exit 0, 0.085 seconds. These format/cache sources
are unchanged. The host orchestration/cleanup guard suite now has 22 passing tests
(0.194 s), including pending dispatch, remaining process/job, wrong-owner PID,
administrative receipt roles and preservation of non-bootstrap accounts/allocated data.

Native Linux: GCC 16.2.1, CMake 4.3.0, Ninja 1.13.2. Sixteen tests pass in Debug
with ASan/UBSan (28.27 s) and Release (25.39 s), both exit 0 in the offline guest.
These are functional/sanitizer checks, not latency benchmarks.
The fixture case compares decoded fields against the unchanged frozen vector and
independent expected metadata. Its final focused runs pass with 8 valid records,
81 malformed records and every shorter prefix of the frozen vector, preserving
failed output (Debug 0.10 s, Release 0.08 s). No content hashing is claimed.
Storage-reference tests cover actual roots/payloads, all six owner bindings,
copied-header replacement, UID/mode changes, escaped/oversized reads, fork and 64
open/read/close cycles. Post-open header invalidation rejects the result without
leaking its file FD. Unique-mask/unsupported/changing-directory-metadata faults
preserve failed output. Two actual same-inode/device bind tests reject the changed
mount via storage and catalogue paths, retaining database state. Final focused
storage tests add all nine mismatched root observations and pass again in Debug
(0.08 s total) and Release (0.04 s total).
These primitive tests are unconfined; the separate fixed-allocation integration
now supplies actual enforcing publication/read/metadata-denial observations. Complete
app/run/document admission is still pending.
The allocation case checks independent frozen bytes/all truncations, invalid ranges,
missing birth time, immutable and concurrent same-intent retries, copied roots,
record metadata and actual close/reopen. File-fsync failure/process exit before link
leaves no named record; lost directory-fsync acknowledgement/process exit after link
leaves one unchanged record. Reconciliation preserves that exact identity.
The final close-error acknowledgement case also preserves the published record;
focused allocation repeats pass in Debug (2.14 s) and Release (1.99 s).
The peer case uses two actual user-bus processes, checks the known sender and exit,
rejects injected augmented/missing credentials, and balances 128 captures' FDs.
Actual same-filesystem
bind mount is readable by baseline openat but rejected by the core with EXDEV;
the mount probe runs only in its verified private namespace. No leak/UB report.
GCC reports baseline x86-64 / generic tuning, without -march=native.

The process case uses actual bus credentials from a separately supervised UID-1000
test service. Seven mismatched context fields, exited senders and a real same-name
service restart reject stale identity. Test-only ioctl faults cover missing fields,
unsupported ioctl, wrong cgroup ID and credentials changing during observation;
failed outputs stay untouched. Repeated peer/self observations balance FDs.
These target processes are deliberately unconfined development fixtures; separately,
the actual enforcing system authority passes its new self-check with empty caps.
The broker cache case uses mocked context, real daemon name-loss and separate
pidfd-exit events: transfer/idempotence/conflict and reentrant invalidation pass.
The new native account case covers owner/fork/UID boundaries, exact retry,
retirement without resurrection, fresh identity after UID reuse, the largest
permitted UID, revision overflow and four pre/post-commit error/process-exit
points. Reopen proves account rows and catalogue revision settle together.

Guest Python 3.14.3 / SQLite 3.51.2: updated 26 catalogue/API tests pass in 47.238 s
and the prior unchanged 14 runtime-format tests in 0.016 s, exit 0.
These remain contract tests.

Native catalogue tests exercise both kinds, single-writer/fork guards, owner/
epoch and file binding, schema/version/constraint/corruption rejection, SQLite
page-limit FULL rollback, revision overflow and failed directory flush after
commit. Injected precommit/lost-ack errors and four child-process exits across
both owners preserve or commit the expected runtime/gates. Populated Python
migrations open natively with identical rows and database bytes. These are
Linux file/SQLite observations, not production registry/MAC or power-loss proof.

Guest inspection confirms EFI, ext4, systemd PID 1/running state, SELinux Enforcing,
cgroup v2, root mount namespaces and bus ProcessFD/LinuxSecurityLabel support.
Running kernel is 6.19.10-300.fc44; installed UAPI headers are 7.2.4-200.fc44.
glibc is 2.43-9.fc44; the reviewed SDK transaction updated systemd to 259.10-1,
libselinux to 3.11-2 and SQLite to 3.51.2-2. Policy remains 43.3, dbus-broker 37.
All nine carrier prerequisites pass again after the reboot. Compiler/SDK direct
package pins are in vm-inputs.json; transitive dependencies belong to this carrier.
Offline guest boot and SSH/local native tests also work. This is not physical
hardware or desktop performance acceptance.

Both SELinux probes pass while offline/Enforcing: exact UID/domain/categories,
empty effective/permitted/inheritable capabilities, NoNewPrivileges, own-data
read/write/create and inherited label; sibling/broker/unselected/sudo denied.
Same-UID controls outside the domain read all negative fixtures and elevate via
the carrier's test sudo setup. Eight enforcing AVCs confirm the category/type
denials. Verified no owned test module/root remains afterward. Setup failures
were preserved/reconciled before narrow supervisor-only corrections; no app
mount rights or permissive execution were introduced. This is partial A3 only.

Coverage includes both populated catalogue kinds, fresh-v2 equivalence, ten
precommit failure points per kind, six child-process exits/reopens including after
commit, source drift/corruption/alias/read-only checks, SQLite page-limit
SQLITE_FULL rollback, version gates, scoped grants, stale context/revisions,
outstanding-record pruning and dispatch/drain constraints.
The unchanged v1 regression and frozen SQL/XML hashes are included.

XML checks validate static names/types/limits/signatures. SQL cases validate
database observations, not actual file/process/authorization state.

Runtime-format tests cover fixed bytes, canonical ordering, changed same-size
payloads, exact owner binding, all six intent kinds, old/reinstalled targets,
malformed/duplicate/escaping inputs, bounds and explicit JSON nesting limits.
Cache tests cover verified reuse, corruption, incomplete/oversized acquisition,
exclusive partial-file creation and publication only after successful verification.

The user confirmed this workstation as the main-PC target and added future AM5
portability. Observed Xeon W-2123/nominal 32 GB/RTX 2080 Ti and three active monitor
records are in [test environment](knowledge/testumgebung.md). A read-only WHP
initial capability query returned S_OK without running a virtual processor. Later
guest boots and native execution now supply the distinct WHPX execution evidence.

## Limits and next step

The service integration passed live method-shape/real-boot checks and a same-UID
confined C17 caller: public discovery and own scratch write/read work, while private
catalogue/registry, raw user systemd and system bus access are denied. Unsupported,
bad-signature and unexpected-FD calls return their expected errors. The caller
cannot acquire the User1 name even while unowned. Restart preserves epoch/boot,
changes runtime/events and advances revision. An unexplained file blocks cleanup
and survives; the inspected empty bootstrap, bus files and module were then removed
and recreated with fresh durable identities. This is not app removal acceptance.

A later 5.033 s idle observation after catalogue mount checks recorded one task each,
zero reported CPU/context-switch deltas, PSS 2252/2292 KiB, RSS 6764/6676 KiB and cgroup
MemoryCurrent 1646592/1667072 bytes (system/user). These are a short carrier sample, not a
full-system budget or precise wakeup counter. Full startup/latency/workload evidence
remains pending. Owned units were explicitly stopped after the sample; the
prototype registry/catalogues, policy and bus files remain for further work.
User roles are tied to the existing login user manager; lingering is disabled.

Actual enrollment/Check pass under the two dedicated enforcing domains. Copied
real identities fail from both an unconfined same-UID process and a same-domain
different unit; their public transport works. Existing app raw-system-bus denials
remain. Broker departure reduces root FDs from 12 to 11; replacement restores only
that pin. A root-only restart re-enrolls the still-live user PID/runtime. Full
restart, unknown-entry preservation and empty-bootstrap recreation also pass with
the new slot/probe inventory. These are identity tests, not app launch/removal.

The actual enforcing authority refuses startup when the old bootstrap lacks its
account row; it acquires no public name and leaves database bytes unchanged.
Explicit native enrollment succeeds, and its exact retry preserves the complete
database hash and revisions. Broker replacement/root-only restart and guarded
empty-bootstrap recreation pass again with the durable account row. Normal new
initialization creates it; other account records still block bootstrap cleanup.

The updated enforcing authority initially refused startup on a real fs_t
filesystem/getattr denial from fstatfs. One read-side rule for the two authority
domains resolved it; no mount/write capability or app-domain permission was added.
Actual broker replacement/root-only restart and full restart, unknown-entry
preservation and confined fresh bootstrap initialization pass with the root checks.
Owned services were stopped normally after the final idle observation.

The new user-domain storage-publish/storage-verify one-shots pass with enforcing
SELinux, NoNewPrivileges and empty capabilities. They preserve the published record
on repeat and reopen it in a separate invocation. A same-UID ordinary control can
read the witness; the confined app with its known StorageRef cannot open witness,
owner header or protected configuration. Existing broker/IPC denials still pass.
A graceful guest reboot triggered the configured QEMU -no-reboot exit 0; its PID
was confirmed gone before offline boot of the same disk. A different kernel BootId
and cross_boot_reopen=true then confirm native reopening with unchanged record
hash/inode. Broker replacement/root-only restart passes again after that boot.
Services are stopped; the optional allocation fixture/registry remains for further
work and makes cleanup/lifecycle-check intentionally out of scope until retirement.
Actual cleanup refusal preserved that witness/payload; a subsequent native reopen
passed with the same hash/inode. The isolated validator still passes 374 assertions.

The carrier retains Fedora's enforcing base plus the explicit P0 service module.
Production broker/system/app policy and category allocation remain pending.
No production catalogue migrated, Aetheri launcher or document transport installed.
No complete Aetheri launch/grant/revocation/purge, permanent
confinement, power-loss, physical hardware or desktop acceptance. No original
Aetheri image/UI.

Next: native fixture enrollment and app/run principal mapping, protected read refs,
fixture import/launcher, target-exec/recovery evidence and the remaining P0 policy.
All P0 Linux gates must pass before expanding to the desktop/apps. The full
daily-use Aetheri requirements, including user UI decisions, remain the objective.

The user accepted OpenSSL 3.5 LTS/libcrypto for native SHA-256 and delegates
well-supported recommended technical choices while temporarily unavailable.
The exact review selected Fedora openssl-libs and openssl-devel 1:3.5.9-1.fc44,
Apache-2.0, from the official HTTPS update mirror. Both RPM signatures match the
existing full Fedora 44 key; hashes/sizes are pinned. The offline signature-checked
transaction upgraded the existing libs 3.5.5 and added devel, with no other packages
or service. RPM verification and pkg-config 3.5.9 pass. Runtime ABI remains
libcrypto.so.3; Aetheri requests only the Crypto CMake component.

[Native SHA-256](../../src/core/digest.h) uses a private call-local EVP context and
fixed default-provider SHA256. It preserves outputs on failure and hashes a bounded
held regular FD without changing its offset or closing it. Canonical fixture
verification now checks an independently expected ArtifactId and per-file content;
it does not enroll newly supplied IDs, inventory a tree or publish an app.
Seventeen native cases pass Debug ASan/UBSan (30.46 s) and Release (25.14 s).
Known answers, the frozen fixture, chunk/size limits, changed same-size content,
crypto setup/final/unload faults and read/stat faults pass. Twenty-nine host VM/
cache guards pass (0.272 s); all five cached input hashes pass.
The actual cap-zero enforcing administrative storage check now verifies its fixed
hello-newline SHA-256 and retains the allocation witness unchanged across retries.
No policy permission was added. [Sources](SOURCES.md#openssl-35-lts-adoption--26-october-2026)
records the dependency review and remaining importer boundary.
After graceful shutdown and verified QEMU exit/PID absence, the same disk boots
offline again. All nine carrier prerequisites pass. Additional old/new runtime
version rejection tests pass in focused Debug/Release repeats (0.84/0.43 s).
The enforcing storage check again reports native_sha256=true and cross-boot
reopening with the retained record. No installation or private-data removal occurs.
Personally important product/UI/destructive choices are deferred until the user
can participate; continue independent implementation and document recommendations.

Work remains inside F:/Aetheri; other projects/shared WSL/hardware untouched.
Public repository: https://github.com/Lulus792/Aetheri.
