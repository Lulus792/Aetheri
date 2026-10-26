# State

26 October 2026 — runtime-format reference and tests added; VM input preparation.

## Confirmed

The user accepted the minimal extension/P0 plan and requested implementation of
versioned database/API contracts and migration tests.
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
Linux peer/storage/exec/recovery obligations are described, not implemented.

[Carrier input lock](../../spec/vm-inputs.json) and preparation tool fetch/verify
exact development inputs without installing or executing them. QEMU 11.1.0 Windows
installer downloaded and SHA-512 checked. Its Windows signature reports an expired
certificate; provisioning remains under review. Fedora's signed checksum list
verified against the official full Fedora 44 fingerprint. The complete
1228384256-byte Fedora net installer downloaded and matched its SHA-256.

## Executed evidence

Python 3.12.14 / SQLite 3.53.1 on the development host:
23 named catalogue/API tests passed again, exit 0, 27.041 seconds.
14 runtime-format tests passed, exit 0, 0.023 seconds.
7 download-cache tests passed, exit 0, 0.085 seconds. Total: 44 named tests.

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
capability query returned S_OK, HypervisorPresent=true; no virtual processor ran.

## Limits and next step

No production catalogue migrated, VM/tool/policy installed, Linux authority,
document transport or supervision implemented. No live D-Bus, SELinux enforcement,
power-loss, native performance or hardware acceptance.

Next: finish reviewed local QEMU/UEFI provisioning, install the disposable guest,
pin/review actual toolchain libraries, then implement the C17 authorities, broker,
launcher/probe and real policy/peer mapping. Protected registry/read refs and
target-exec evidence remain live-integration gates. All P0 Linux gates must pass;
host contract tests and verified downloads alone do not complete P0.

Work remains inside F:/Aetheri; other projects/shared WSL/hardware untouched.
Public repository: https://github.com/Lulus792/Aetheri.
