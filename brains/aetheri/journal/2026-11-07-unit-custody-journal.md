# Durable unit connection and RPC history

7 November 2026. Follow-up to the original unit-allocation/selection work at
`6899001`. The full Aetheri OS/apps/image/installer/update/hardware/experiment
desktop goal remains active and incomplete.

## Change and boundary

Root v10 adds immutable connection origins and ordered per-node RPC intents with
separate acknowledgements. The Root adapter installs compiled journal hooks on
its exclusive Linux unit owner. Each real Subscribe/Ref/Start/Unref/Unsubscribe
attempt follows a fresh confirmed intent; the original admission is checked
again before sending. Unknown replies or journal acknowledgements quarantine
fresh use. Asynchronous Start acknowledgement records only the authenticated job
reply. Historical retries cannot repeat effects or alter later state.

The same owner SQLite transaction/lock is preferable to a separate sidecar:
one writer can bind these rows to the existing allocation/REQUEST and preserve
unknown commits without a second cross-file commit protocol. The Linux layer
depends on small shared C types and compiled hooks, not SQL or catalogue code.
There is no new dependency, installed endpoint, public capability or UI change.

Migration is explicit Root v9-to-v10 and adds no origins/history for old runs.
Normal creation/installed catalogues remain v2. All earlier SQL/XML stays frozen.
The canonical AETHUBS1 origin is a connection observation, not a kernel process,
supervisor, image, liveness or volume proof. Even a fully acknowledged sequence
remains in recovery inventory. No run/lease/allocation retirement is inferred.
See the [contract](../../../spec/unit-profiles.md#root-v10-connection-custody-journal).

## Validation

Final candidate: `local/unit-custody-journal-2026-11-07/source-v5.json`, 430
explicit source inputs in a separate guest source/build tree. Both complete
core/IPC Release and ASan/UBSan builds pass strict C17 warnings as errors.
Both profiles pass 32 actual PID1/inert administrative cases and 11 native
catalogue cases, with both independent frozen codecs: 330.573s Release and
367.794s ASan/UBSan. All nine affected regression groups also pass without
skips: 350.932s Release and 396.772s ASan/UBSan. They cover legacy Linux
unit/PID1 start, execution/preparation/completion, the Root start adapter, recovery
inventory, original unit allocation and populated Python/native v2-through-v10
validation. No old schema is migrated merely by native open.

The checked export contains 250 files / 907,882 bytes, including 190
retained administrative cases across v3/v4/v5 and both profiles. Raw histories
preserve original REQUEST/User preparation and reserved leases; neither source
retirement nor acknowledged custody settles them. The evidence-index SHA-256 is
`1f374050b86fb513eef4b29e5146aa8dbf4bc5c83a895d2cae083429c2c3c2b0`.
Final audit verifies 430 source inputs, all 29 published SQL/XML, 19 protected
quarantine hashes, 52 normal files/controller, exact CLOSED DRM/SDK0.1.18/SDL04
and vendor drop-in. No own program/argument path or recorded unit/image remains
live. The three original foreign planning files are unchanged; the parallel UI
note update is preserved and excluded. This is a single-agent self-review and
controlled fault/native evidence, not an independent agent review or four new
deliberately defective binary campaigns.

The subsequent self-review found that asynchronous quarantine needed to reach
the Root coordinator, not just the unit owner. Candidate v5 adds one guarded
first-quarantine notification and fences fresh Root effects immediately. The
existing lost journal/async-ack cases additionally require ESHUTDOWN at the Root
effect boundary. This is a source-level integration finding; no normal installed
runtime exploit or independent reviewer is claimed. V4's passing checks remain
historical, not evidence for v5's corrected propagation.

The new cases cover original connection/manager binding, fresh-only preparation,
ordered effects and absent Start, eight-node/19-step bounds, all native write
faults, migration DDL/commit/process-exit faults, origin/intent/confirmation lost
acknowledgements, source/gate fencing and replacement-owner historical reads.
A test wrapper reads the original durable unconfirmed intent before each tested
RPC, checks its actual bus/issuer/destination and rejects duplicate attempts.
It does not seal, read or alter an unsent request's body. A separate lost async
Start-confirmation case preserves both the real job observation and quarantine.

Host contracts/migration pass 55/55 (206.749s); unchanged frozen runtime tests pass
25/25. The first build exposed incorrect positional initialization of an internal
four-field SQL helper; designated fields corrected it. The first targeted Python
command used the wrong import root; a missing json import then affected only the
new vector test. The raw v1 compile log and earlier native evidence remain preserved; scoped host/snapshot diagnostics are recorded in preliminary-failures.json. The earlier v3 native
suite passes both profiles, but is not attributed to final v4's expanded checks.

The 32-case aggregate timeout was set to 420s before the final expanded run;
individual 40s case limits and production RPC deadlines are unchanged. No
timeout failure was converted to success by widening a deadline.

## Storage and preservation

The source snapshot initially failed because the guest's unprivileged free space
was exhausted; no v4 source directory was created by that failed attempt. After
checking exact own inactive CMake source bindings, 1,200 replaceable object files
from earlier own unit builds were removed: 144,928,096 bytes. Sources, libraries,
programs/debug symbols, original logs, SQLite histories and foreign work remain.
The audit manifest is `local/unit-custody-journal-2026-11-07/space-reclaimed.json`.
Root free space was 2,632,314,880 bytes immediately afterward; ordinary-user
available space was only 123,809,792 bytes. More build capacity still needs care.
The same UID1000 space constraint recurred before v5. Its new source extraction
now uses the existing administrative build boundary with an explicit 512 MiB
Root-free-space guard and create-only directory; disk size and ext4 reservation
settings are unchanged. No missing snapshot was treated as a successful build.

The old executing quarantine is never opened/migrated/adopted. Native fixtures
are separate UID0 administrative true programs, not replacement UID1000 app runs.
Their original Root/User journals and reserved leases remain retained. Normal
services, SDK pin, SDL profile and NexaUI handoff are unchanged.

## Next

Persist and independently reidentify the original issuing process/unit/image,
then reconcile original connection loss and retire custody with independently
admitted original end/holders. Continue volume/owner continuity and external/
delegated-holder settlement. Do not replay from the journal or reinterpret
acknowledged RPCs, closed FDs or completed jobs as app drain or lease release.
