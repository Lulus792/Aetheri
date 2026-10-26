# State

26 October 2026 — P0 accepted; versioned SQL/XML and migration tests implemented.

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

## Executed evidence

Python 3.12.14 / SQLite 3.53.1 on the development host:
23 named tests passed, exit 0, 27.009 seconds in the final full run.

Coverage includes both populated catalogue kinds, fresh-v2 equivalence, ten
precommit failure points per kind, six child-process exits/reopens including after
commit, source drift/corruption/alias/read-only checks, SQLite page-limit
SQLITE_FULL rollback, version gates, scoped grants, stale context/revisions,
outstanding-record pruning and dispatch/drain constraints.
The unchanged v1 regression and frozen SQL/XML hashes are included.

XML checks validate static names/types/limits/signatures. SQL cases validate
database observations, not actual file/process/authorization state.

## Limits and next step

No production catalogue migrated, VM/tool/policy installed, Linux authority,
document transport or supervision implemented. No live D-Bus, SELinux enforcement,
power-loss, native performance or hardware acceptance.

Next: verified VM/toolchain setup and real C17 authority/broker/launcher/probe,
with fixture/intent formats and policy/peer mapping. All remaining P0 Linux gates
must pass; contract tests alone do not complete P0.

Work remains inside F:/Aetheri; other projects/shared WSL/hardware untouched.
Public repository: https://github.com/Lulus792/Aetheri.
