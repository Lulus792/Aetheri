# Original supervisor start and exact job observation

7 November 2026. Native C17 in the existing Fedora 44 Enforcing carrier,
systemd 259.10-1.fc44. The full Aetheri OS/application goal remains incomplete.

## Implemented boundary

The new internal Linux observer sends one asynchronous StartUnit/fail to the
independently admitted original PID1 unique connection. It installs watches
before submitting and authenticates both the original correlated reply and
JobRemoved through actual bus credentials and a held ProcessFD. Four bounded
same-unit early events can be retained; only the original reply's canonical
job path/ID selects an event. Matching a unit name never recovers a lost reply.

The Root adapter derives the unit from the original launch and joins the existing
coordinator's fresh REQUEST commit. Compiled provisioning admission and source
checks run twice. A separate cleanup bracket keeps the catalogue/runtime alive
while a call or job is unsettled, including after source fencing. Known exact
job completion retires the watches and local hold; later Manager replacement
cannot rewrite the result. Unknown/quarantined requests retain their obligations.
Explicit abandon retires local observation only, with no Stop/Cancel/Unref or
SQL/lease settlement. The caller still owns its independent unit reference and
Manager subscription on a dedicated host-system-bus connection.

Review identified a final-callout edge: compiled admission can itself process
bus events. A name-loss event can therefore latch quarantine after the preceding
current-manager lookup. The observer now checks the latch after that lookup and
again after the last guard, directly before attempting submission. A separate
case deliberately dispatches name loss during that final guard and requires zero
StartUnit calls. This is a new adapter review correction, not a claim that the
normal installed authority previously exposed this path.

## Executed evidence

Both complete builds pass. All 19 final private-bus cases and the separate actual
PID1 case pass without skips in Release (74.60s) and ASan/UBSan (74.83s). They
cover normal/early/foreign-job/wrong-sender delivery, a lost reply, malformed
paths, conflicting/excessive events, pre- and post-terminal Manager loss,
admission refusal/fencing, uncertain submission before/after actual transport,
pending-source fencing, fork refusal, missing JobRemoved and a failed job.
Checks include callback close/reentry, catalogue close/runtime exclusion,
descriptor baselines, exactly one or zero producer calls, immutable native
REQUEST and unchanged reserved lease/dispatch. The failed job releases only the
local hold; a missing job signal retains it despite a confirmed reply.

The actual PID1 case checks real connection credentials, owned-file identity,
load state, empty dependency graph, original job ID/path/result and active target.
It then independently checks inactive/no-job state and the exact file before
removing that inert fixture. The same test runs in both final profiles.

All three affected existing regression groups pass: linux.peer,
authority.execution_service and authority.execution_fence (92.89s/102.12s).
Those peer/coordinator inputs are unchanged by the later start-observer latch
correction. Four separately compiled defective producers are rejected by the
unchanged selected assertions: missing post-admission source check, discarded
early event, missing pending-owner hold and missing final Manager-loss latch.
Original production/test artifacts remain unchanged by these counterexamples.

Earlier 15-, 16- and 18-case positive transcripts remain attributed to those
revisions. The final guard-edge build initially fails -Wmisleading-indentation;
splitting its unconditional return fixes the warning without relaxing flags.
No tests run from that failed build. The final complete builds and 19-case runs
follow the correction. Original failure output is retained; no timeout, sanitizer,
policy or assertion relaxation is used. Unchanged SQL/vector/migration campaigns
are not represented as rerun by this transport-only change.

The exported local `local/supervisor-start-2026-11-07` bundle contains 65 verified
files / 547,515 bytes, including complete JUnit/logs, compiler commands, artifact
hashes, current source snapshot, historical transcripts and counterexamples.
The audit verifies 413 source inputs, 28 unchanged frozen SQL/XML contracts,
19 protected old-runtime hashes, four untouched foreign files, exact CLOSED DRM
state and unchanged SDK 0.1.18/SDL04 artifacts. No own build/test actor, Aetheri
service or seat0 session remains; no owned target file remains. All 52 normal-P0
files, their inodes/ownership/modes/hashes and controller match the preceding
checkpoint. Normal services are not updated/restarted by this library change;
their earlier seven-stage check is historical, not a new runtime acceptance.
The old executing catalogues are checked as raw files only, never opened through
SQLite or adopted. The existing VM remains running under host-local AF_UNIX QMP.

## Limits and next work

The private-bus cases have actual native SQL, correlated messages, process pins,
descriptor checks and source/owner brackets; PID1 role and unit admission there
are controlled. The separate actual PID1 case independently admits a process-free
target with an empty dependency graph. Its administrative cleanup verifies the
exact owned file and inactive/no-job target. It starts no application and is not
a replacement for the quarantined executing fixture.

The adapter is not wired into ordinary installed authority profiles or public
capabilities. General immutable service/dependency admission, caller-owned
reference/subscription custody, target/private-data admission, production policy,
old-owner/external/delegated holders and admitted owner/volume continuity still
need integration. A StartUnit reply or JobRemoved result does not prove app exec,
REGISTER/READY, executing drain or lease release. Existing no-exec withdrawal
proofs must never settle this executing journal. Lost original observer state
still needs separate durable recovery; do not infer it from a new same-unit job.

The normal prototype, frozen SQL/XML, NexaUI pin/defaults, protected storage/data
fixtures and old executing quarantine remain outside this change.
