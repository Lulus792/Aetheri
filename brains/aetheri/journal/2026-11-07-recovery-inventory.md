# Read-only startup recovery inventory

7 November 2026. Native C17 catalogue/authority sources in the disposable Fedora
44 Enforcing carrier. The complete OS/apps/image/installer/update/hardware/full
experiment-desktop objective remains active and incomplete.

## Change and purpose

The next recovery coordinator needs a consistent list of the original owner's
unresolved metadata before it can select separately admitted protocols. The new
[inventory API](../../../src/catalogue/recovery.h) copies that metadata in one
read transaction. It includes held code/access leases and unsettled launches
independently of completed start-operation receipts. Transitional installations,
generations/data, grants, cause-bearing gates and later closure/handoff/
interruption stages retain their original IDs, states and revisions.

The [runtime contract](../../../spec/runtime.md#read-only-owner-recovery-inventory)
defines the exact source predicates, limits and omissions. A result is not a
current actor observation, future-effect fence, global readiness gate or replay
authorization. Historical uncertain closures can outlive a separate terminal
interruption. Empty SQL cannot prove that external holders are gone. The API
never settles a run/lease, migrates, changes a gate or reconstructs old handles.

The authority source invokes the scan after its fresh runtime commit and before
explicit fixture admission or bus/READY publication. Failed scans stop that
startup. Successful scans log only schema, revision and record count with
`metadata_only=1`. The new authority is built, not installed into the protected
normal bootstrap; no new live startup/policy/capability acceptance follows.

## Verification boundary

The [dedicated native test](../../../tests/test_recovery_inventory.c) and
[independent Python reference](../../../tools/test_recovery_inventory.py) use
private populated legacy fixtures, explicit development migrations and actual
SQLite. They compare complete records across every supported Root/User schema,
verify unchanged file bytes/rows and descriptor counts, and exercise record
bounds, multibyte reasons, query/allocation failures and pre/post-success
BEGIN/COMMIT acknowledgement loss. Reentry must leave the original transaction
intact. A second SQLite connection cannot commit during the read snapshot; its
connection is retained until the original transaction ends. Native startup
transitions and a retained gate-closing cause are tested separately from the
read-only scan.

The existing [native lifecycle test](../../../tests/test_launch.c) now checks
populated closure/handoff/interruption metadata at successful native stages,
including its disappearance at the respective terminal stage. The completed
Root start receipt still leaves its running code lease visible. These tests use
controlled process/holder inputs; they do not admit a production recovery actor.

The first run failed an incorrect harness assumption that a migrated/opened Root
lease had already become reconcile. The next explicit-start check established
that native Root startup also leaves that separate legacy lease reserved:
dispatch recovery and lease state are distinct. The corrected test requires
that actual state and independently requires User recovery_required. Production
startup/lease rules were not changed to fit the test. Both original failures
remain in the local/guest evidence. The corrected focused Release test passed.

The first expanded Release campaign also caught a new test-helper assumption:
an exact historical interruption-BEGIN retry after RELEASE was expected to
appear as pending. The query correctly omitted it. The staged assertions now
check the actual intermediate points and separately require terminal absence;
the existing historical retry keeps its original receipt and cannot recreate a
pending obligation. The production query was unchanged. Individual repeated
inventory reads now have distinct evidence filenames rather than replacing
earlier logs in the same test fixture.

The earlier broad `-j2` Release campaign also reached the existing 240s process
group limit. That failed campaign was explicitly interrupted after its actual
ctest process was reidentified; all child actors ended. The final selected
groups ran serially under the original limits, including that process group.
Their success does not prove the earlier timeout's cause. No deadline, policy,
sanitizer, assertion or frozen schema was relaxed.

| Final profile | Selected catalogue groups | Strengthened populated inventory |
| --- | --- | --- |
| Release | 10/10, 617.43s | 1/1, 6.97s |
| Debug ASan/UBSan/LSan | 10/10, 723.70s | 1/1, 11.62s |

All JUnit cases execute without skips. The ten groups are lifetime,
interruptions, processes, handoffs, recovery_inventory, basic, reject, recovery,
accounts and interop. Both complete native builds passed. The strengthened
Python-only fixture was added after that campaign; all native source bytes
remain the same. Its separate inventory execution in each profile
checks 40 owner/schema/bounds/fault cases, now with preparing/staged installs,
grants/access/resources/purging data and a completed operation retaining an
uncertain step. All ordinary SQL guards remain active. It also verifies both
two-byte and maximum four-byte-per-scalar reasons. The 50 host contract/migration
tests passed in 178.72s, and all 24 Linux runtime-vector/contract tests passed.

Evidence: `local/recovery-inventory-2026-11-07`; final guest campaign
`/home/aetheri/build/recovery-inventory-serial`. Populated inventory evidence:
Release `recovery-inventory-qag8belg`, Debug `recovery-inventory-dstad0ni`.
The local collector verifies 135 exports, exact JUnit names, source/binary hashes
and actual C17/Release/ASan compiler commands. Tar SHA-256:
`eebf127f07b4be707520e2f6a6e5060330dcf32aaa021ce183b119acd3f43da5`.

Final audit: 398 source inputs agree between host and guest; all 28 frozen SQL/XML
contracts, 19 protected old-runtime hashes and the four foreign files are
unchanged. The exact historical CLOSED DRM record, SDK 0.1.18 and SDL 04 remain
unchanged. Enforcing/tty1, no protected SQLite sidecars, no native build/test or
private-registry actors, no Aetheri services and no seat0 session. The protected
old runtime remains unsettled. Git history records the publication.

## Remaining work

The live recovery coordinator still needs independent machine/volume/clone
continuity, actual current owner/source admission and complete holder/future-
effect fencing. Do not apply this inventory to redispatch, adopt, settle or purge
the quarantined old executing fixture. Its lost original handles remain lost.
Normal session/image, document mediation, full removal, update/installer,
accepted apps and physical/human/final UI acceptance remain incomplete.
