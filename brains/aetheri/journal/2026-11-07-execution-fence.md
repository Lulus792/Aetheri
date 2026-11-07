# Local execution-source fencing

7 November 2026. Native C17 and normal P0 Root shutdown in the Fedora 44 Enforcing
carrier. The whole Aetheri goal remains active and incomplete.

## Implemented boundary

`catalogue/effect.h` provides one explicit, process-owned, non-reopenable latch
shared by all execution coordinators borrowing the original SYSTEM catalogue.
It neither changes SQL nor derives admission from the recovery inventory.
Its local status counts entered adapter/transport brackets separately from the
latch. An entered adapter or send can still complete or return unknown after
fencing. Zero brackets never proves external job/process/descriptor-holder drain.

Native fresh REQUEST/BIND/PREPARE/ARM stages reject after fencing; historical
acknowledgments still return no renewed effect permission. REQUEST/ARM adapters
obtain current-context permission after their fresh native commit. BIND holds a
bracket across original supervisor reference/channel setup and verifies again
before returning a writer. Execution1 writer/release reply construction and
sending hold separate brackets. Every exit balances them; a permit requires
SQLite autocommit. The catalogue cannot close or replace its runtime while a
bracket is active, and a fenced handle cannot begin another runtime.

Root fences on SIGTERM/SIGINT before event-loop exit and again during cleanup.
No SQL/XML/public capability, dependency or policy permission changes. This
normal shutdown hook is installed; the separately tested native executing
endpoint is still not a normal public launch implementation.

## Tests and counterexamples

Both complete native builds pass. Twelve selected groups pass without skips in
Release (390.50s) and ASan/UBSan (357.01s): execution journal/reservation/recovery
inventory, the four basic catalogue groups, connected admission, existing
execution service/endpoint and the two new fence groups. The source and transport
adapters remain controlled; private-bus peers, ProcessFDs, channels and native SQL
are real. These component cases do not establish actual app-domain execution.

Nine new source scenarios check fencing before REQUEST, after its actual commit,
inside an entered request, during supervisor reference acquisition, after BIND
commit, before PREPARE/ARM, after ARM commit and inside an entered release.
They preserve original rows, output markers and generation leases, exercise a
preexisting sibling coordinator, reject new coordinators, and retain read-only
historical acknowledgments with no new permission. Callback checks reject
catalogue destruction/runtime replacement; fork/wrong-owner-role calls cannot
alter the latch or balance the parent's brackets.

Six new actual private-bus scenarios check post-BIND fencing, the gap before
writer reply, an entered writer send, pre-/post-ARM fencing and an entered release
send. Real clients receive rejection or the already entered reply as appropriate;
there is no duplicate dispatch/reply or implied lease release. Local counts include
nested brackets, not distinct processes or globally enumerated holders.

The first build rejected a shadowed test variable; it was renamed. The first
wire run lacked the private-bus environment because the new test mode was not
included in the runner's bus condition. That harness condition was corrected;
no SDK/policy/deadline relaxation followed. Originals remain retained.

CTest's initial successful JUnit output was limited to 1,024 bytes. The twelve
suite results remain valid, but those logs cannot enumerate every new scenario.
The two new groups were therefore repeated with a bounded 1 MiB success-output
limit: Release 56.20s and ASan/UBSan 60.48s, both 2/2. The collector verifies all
nine source and six wire completion markers/profile, no truncation/skips/failures
or sanitizer findings. Original short logs are preserved separately.

A broad deliberate mutation omitting the effect check is rejected at an earlier
direct-API assertion. A more targeted producer moves REQUEST permission before
the native intent commit. The unchanged final test detects an actual callback
after intervening fencing at the intended post-commit boundary. It exits 1 in
`exec-service-fence-request-commit`. The production archive and test executable
remain byte-identical. Mutant output/dependency files stay in their own build
trees; no ordinary build metadata or program is replaced.

All 50 host contract/migration cases pass (176.500s), as do all 24 Linux runtime
vectors. Existing frozen SQL/XML and ordinary v2 creation stay unchanged.

## Installed normal-service evidence

The reviewed controller updated the verified stopped normal bootstrap. Selected
authority SHA-256:
`0cc2e74e6bec4c41e24bdb3d34c549a619219848f2d079a52a9c1f2a44c2072e`.
All seven stages pass: update/start/check/connection-check/broker-check/idle/stop.
Actual Root logs `EXECUTION_FENCE fenced=1 active=0 local_only=1` when stopped.
This is the actual cap-empty Root domain's local latch, not a live executing-app
or complete external-drain proof. Both normal services are stopped.

The comparison covers all 52 normal files with explicit program/controller/
selected-unit/runtime exceptions. Owner/epochs, writer-lock inodes and every
non-runtime table remain unchanged. Policy hash is unchanged. User replacement
and Root restart with the original User retained pass; Root's User pin drops
12 to 11 on departure. Both one-task services have zero CPU/context-switch deltas
in the 5.032s idle sample. No polling worker or resident process is added.

## Evidence and remaining work

`local/execution-fence-2026-11-07` retains scripts, all earlier/final logs,
actual service snapshots and the separate targeted producer. The final collector
verifies 53 exported files including complete per-scenario transcripts, exact
JUnit groups, compiler commands and native binaries. Final archive SHA-256:
`46139700bb29b89069c26c4fc87679fea8ac31702e607c88f684bebfcd10fefc`.
The earlier 45-file archive is retained. Audit checks 406 source inputs, 28 frozen
contracts, 19 protected old-runtime hashes, four foreign files, exact CLOSED DRM
history and SDK/SDL pins. No native/build actor, Aetheri service, protected sidecar
or seat0 session remains; Enforcing/tty1. Git history records publication.

This local fence does not survive handle/process replacement, authenticate a
replacement machine/volume/clone or enumerate other effect/holder families.
An entered send, PID1 job, old/intervening owner, starter, exported view/channel,
FD store or delegated holder still needs independent coordination and drain.
Connect those sources and durable per-run gates/receipts before recovery or
removal. Never use this latch or an empty inventory to settle the old quarantined
v6/User v4 run; its original lost handles/provenance remain lost. No reopening,
migration, adoption, release, purge or replacement target test is performed.
Full OS/apps/image/installer/update/hardware/experiment-desktop work remains.
