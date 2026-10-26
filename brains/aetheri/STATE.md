# State

26 October 2026 — installed UEFI/WHPX carrier, native core and bus-peer capture.

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

[Linux peer capture](../../src/linux/peer.h) now collects required authenticated
bus credentials and an owned process pin without /proc augmentation or numeric-PID
fallback. It rejects missing/augmented provenance and preserves outputs on error.
This is connection identity, not app/run authorization or current-domain proof.

## Executed evidence

Python 3.12.14 / SQLite 3.53.1 on the development host:
23 named catalogue/API tests passed again, exit 0, 27.041 seconds.
14 runtime-format tests passed, exit 0, 0.023 seconds.
7 download-cache tests passed, exit 0, 0.085 seconds. Those prior 44 host tests are
unchanged. Ten additional host-orchestration tests passed after the new VM tools.

Native Linux: GCC 16.2.1, CMake 4.3.0, Ninja 1.13.2. Four tests now pass in Debug
with ASan/UBSan (0.26 s) and in Release (0.05 s), both exit 0 after offline reboot.
The peer case uses two actual user-bus processes, checks the known sender and exit,
rejects injected augmented/missing credentials, and balances 128 captures' FDs.
Actual same-filesystem
bind mount is readable by baseline openat but rejected by the core with EXDEV;
the mount probe runs only in its verified private namespace. No leak/UB report.
GCC reports baseline x86-64 / generic tuning, without -march=native.

Guest Python 3.14.3 / SQLite 3.51.2: after the SDK update, existing 23 catalogue
tests pass again in 42.982 s and 14 runtime-format tests in 0.016 s, exit 0.
These remain contract tests.

Guest inspection confirms EFI, ext4, systemd PID 1/running state, SELinux Enforcing,
cgroup v2, root mount namespaces and bus ProcessFD/LinuxSecurityLabel support.
Running kernel is 6.19.10-300.fc44; installed UAPI headers are 7.2.4-200.fc44.
glibc is 2.43-9.fc44; the reviewed SDK transaction updated systemd to 259.10-1,
libselinux to 3.11-2 and SQLite to 3.51.2-2. Policy remains 43.3, dbus-broker 37.
All nine carrier prerequisites pass again after the reboot. Compiler/SDK direct
package pins are in vm-inputs.json; transitive dependencies belong to this carrier.
Offline guest boot and SSH/local native tests also work. This is not physical
hardware or desktop performance acceptance.

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

The carrier uses Fedora's enforcing policy, not Aetheri app domains/categories.
No production catalogue migrated, Aetheri authority/broker/launcher or document
transport installed. No real app start/grant/revocation/purge, Aetheri confinement,
power-loss, physical hardware or desktop acceptance. No original Aetheri image/UI.

Next: C17 authority/broker/launcher/probe, protected registry/read refs, complete
app/run principal mapping, Aetheri SELinux policy and target-exec/recovery evidence.
All P0 Linux gates must pass before expanding to the desktop/apps. The full
daily-use Aetheri requirements, including user UI decisions, remain the objective.

Work remains inside F:/Aetheri; other projects/shared WSL/hardware untouched.
Public repository: https://github.com/Lulus792/Aetheri.
