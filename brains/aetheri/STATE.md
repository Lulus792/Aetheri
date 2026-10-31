# State

31 October 2026 — confined PID1 handle transfer and Root replacement.

## Confirmed

The user accepted the minimal extension/P0 plan and requested implementation of
versioned database/API contracts and migration tests.
The active user goal is to implement the full Aetheri project. The headless carrier
and core below are progress toward that scope, not a smaller completion target.
All earlier architecture, ownership, lifecycle and language decisions remain.
P0 carrier/resource selection is accepted as the development direction, not the
final Aetheri distribution or runtime acceptance.

## Implemented

The private handoff module now sends only after fresh PREPARE, admits an exact
four-name PID1 inventory before actual restore/OBSERVE, and removes stored handles
only after fresh retirement intent. Unknown/partial effects remain pending with
no automatic repeat. Completed retirement is historical and never changes a later
store. The new adapter cases use real owner SQL and controlled supervisor edges;
the actual confined test supplies real kernel/descriptor-store evidence.

The opt-in --handoff integration sends actual UID1000 app observation handles
from capability-empty Root while the app is live. Original Root/User complete
normal run closure before Root replacement. A protected process-free target
retains original unit history; the new Root receives/validates the four originals,
observes original end in a new PrivateMounts namespace, and retires the pending
handoff/store at revision 4. No app restart or native lease adoption is performed.
The default unsubmitted integration still retires at revision 3.
See [confined transfer](../../spec/runtime.md#confined-transfer-after-a-completed-run).

The first replacement's pidfs_t filesystem getattr denial preserved the same
intent, original witness and four stored handles. A recorded minimal policy
repair allowed a later invocation to finish that same handoff. A fresh automated
handoff run and the normal baseline both pass after a test-auditor variable
shadowing fix. The temporary extension permits PID1 fd use/barrier pipe write
and Root pidfs metadata observation, with no capability, ptrace, namespace entry
or Root error-pipe read grant. The exact test infrastructure is retired.

Recovery while a run is active, Root/User settlement, partial-set reconciliation,
early failure and production provisioning/admission remain work. Normal v2 gates,
public capabilities and the app's no-store boundary remain unchanged. The older
namespace-denied attempt's held history remains protected in guest quarantine.
The desktop/apps/image/hardware requirements remain the full objective.

Final automatic normal and --handoff runs both pass. The original failure was
also continued successfully with the same intent, not replaced by a fresh run.
Debug ASan/UBSan bridge/transport groups pass 2/2 (68.54 s); Release bridge,
transport and existing coordinator/endpoint groups pass 4/4 (172.69 s).
Read-only checks confirm enforcing policy, no temporary execution/hold units or
module/bus configuration, normal v2 Root/User with two closed gates each and
pending=None/no unsettled normal lease. These are functional/confinement checks;
active-run recovery, production integration and hardware acceptance remain open.
Host VM/tool guards pass 49/49 (3.129 s). All 186 host/snapshot/guest source hashes
agree; local Markdown validation passes 594 file targets, excluding anchors and
external URLs. No new dependency family or resident daemon was added.

Previous native-bridge checkpoint (31 October 2026):

The private authority/handoff module now joins actual lifetime export to the
immutable enrolled app witness and existing v5 PREPARE transaction. Only a fresh
confirmed intent returns four transfer handles. Existing intent and retries after
a lost successful commit return none. RESTORE loads original expectations from SQL before
actual object validation, rechecks the current custodian/runtime and then commits
OBSERVE. Its caller still owes complete supervisor-inventory/transport admission
and continuous unit retention. It does not send notifications, adopt a run or
release a lease. See [native bridge](../../spec/runtime.md#native-bridge-from-original-handles-to-the-owner-journal).

The actual enforcing Root/User/app integration now uses explicit Root v5/User v4
and exercises PREPARE from the real UID1000 app. It checks the canonical frame
against the immutable app witness and actual Root invocation. No store transfer
is submitted. Exported copies close, retry returns none, and normal successful
run closure is followed by retirement of this unsubmitted handoff with a Root
store of capacity zero and no entries. Normal v2 authorities/gates/capabilities are unchanged.
The temporary policy adds only namespace-object getattr for the Root observer.

The earlier before-ready setup failure was reconciled before any app/lease/intent.
The later namespace-denied fixture retains its original pending dispatch/held
lease, catalogue bytes and complete inputs in protected guest quarantine. Only
its exact dead test infrastructure was retired; no native settlement was invented.
The corrected confined integration passes. An installed confined replacement
receiver, actual supervisor transfer/partial-set reconciliation and full Root/User
recovery remain work. All product/desktop/apps/image/hardware requirements remain.

The affected Release sweep passes 3/3 (180.17 s), including the new bridge, original
handoff catalogue and existing execution coordinator. The actual confined run
again passes Root/User/app start/end and native closure; the temporary fixture,
policy extension and bus configuration are retired. The IPC-disabled producer
builds and rejects bridge mode explicitly. Final read-only checks confirm normal
Root/User v2, two closed gates each, pending=None and no unsettled normal lease/
dispatch. Quarantined failed native state remains unchanged. These results do
not cover an installed replacement receiver or full owner recovery.
The reviewed focused bridge repeats pass Debug ASan/UBSan (15.48 s) and Release
(14.07 s). Host VM guards pass 48/48 (2.360 s); all 186 host/guest source hashes
agree. Local Markdown validation passes 591 file targets, excluding anchors and
external URLs. No new general performance or hardware acceptance is claimed.

Previous process-free retention checkpoint (31 October 2026):

The separate opt-in linux.lifetime_handoff_retention test now preserves original
custodian/app units through a protected process-free target with explicit ordering
references. The controller makes zero RefUnit calls and closes its original host
bus connection after both original processes and their systemd-run clients exit.
A fresh connection still sees the original settled service metadata and four
stored handles. A replacement observer validates the original objects and end,
then empties the store. The target keeps the same nonzero invocation. Only after
successful observation does the test stop it, remove its exact owned fragment
and require all three units to disappear from PID1. No app restart is involved.

The four affected Linux cases pass Debug ASan/UBSan 4/4 (6.36 s) and Release 4/4
(4.73 s), with enforcing policy and no new policy grant or dependency. This is an
administrative unconfined_t component test with capability-empty observers, not
confined Root/User recovery. The trusted carrier's configuration and exclusion
of competing administrative changes are prerequisites. Protected production
provisioning/reload/retirement and exact-set reconciliation still need integration
with the existing v5 journal. See [unit retention](../../spec/runtime.md#original-unit-retention-without-controller-pins).
Read-only checks find no remaining handoff unit/target fragment; normal Root/User
catalogues remain v2 with two closed gates each, pending=None and no unsettled
normal dispatch/lease. No normal FD store or public lifecycle capability opens.
Host VM guards pass 47/47 (2.319 s); all 183 host/snapshot/guest source hashes
agree. Local Markdown validation passes 586 file targets, excluding anchors and
external URLs. No full database or confined-runtime suite was rerun for this
test/tool/documentation change.

Previous canonical-journal checkpoint (31 October 2026):

Canonical AETHHND1 now binds the complete original app-process witness to cgroup
root/group/event objects, PID/cgroup namespaces and the original custodian unit/
invocation. Native padding is never stored. Full-width unsigned identifiers,
optional birth and signed birth seconds are preserved; the independent frozen
vector agrees between Python and C. The actual capability-empty component restart
now uses this portable record before PID1 transfer instead of a native structure.
Its UID0/synthetic profile is explicitly outside fixed UID1000 P0 admission.

One new Root-only execution_handoffs table in explicit schema v5 records immutable
origin plus pending/observed/retiring/retired state. User stays v4; create still
defaults to v2 and opening never migrates. Frozen v1/v2/v3/v4 sources and old rows,
gates/revisions are preserved. Only fresh confirmed intent commits permit transfer
or retirement effects; existing-intent/lost-successful-commit retries permit none.
Later runtime observations never replace the origin or adopt/release the old run.
Retirement requires separate terminal/released closure and actual PID1 absence.
See [durable handoff](../../spec/runtime.md#durable-original-observation-handoff).

The native sixteen-case journal matrix covers every DDL/mutation write, lost
commits, eight actual mutation-process exits, repeated runtime replacement,
retention/immutability, overflow and real database-byte corruption. Migration
tests also exit actual processes before/after commit. Host contracts pass 44/44
(91.991 s) on SQLite 3.53.1; the guest uses SQLite 3.51.2. Independent byte tests
pass 21/21 (0.022 s), including reserved bytes, text/bounds and signed-time extremes.

The affected Release sweep passes 14/14 (1024.59 s). Concurrent Debug passes
13/14; its lifetime group hits the existing 240-second limit. With no parallel
suite, the reviewed Debug lifetime group passes in 202.45 s; together with the
final signed-time handoff/component checks, that repeat passes 3/3 (352.11 s).
Final reviewed Release handoff/component checks pass 2/2 (110.13 s). The timed-out
metadata-only fixture/log remain preserved; no functional assertion failed there.
Run database-heavy carrier suites serially under the current resource limits.
The IPC-disabled native build also compiles and passes the codec smoke check;
the catalogue handoff test is registered without an IPC requirement. Host VM
orchestration tests pass 47/47 (2.418 s). This is affected-boundary verification,
not a new full-suite, performance or hardware acceptance claim.
All 183 host/guest source hashes match. Local Markdown validation passes 583
file targets; anchors and external URLs were not checked by that validation.

The existing enforcing confined Root/User/app regression still passes actual
start/readiness/end, three original v4 witnesses and successful native closure.
This journal is not wired into installed confined Root producer/receiver paths;
its metadata actor/transport inputs remain controlled native fixtures. The
component proves canonical expectations in a real bounded handoff, not Root/User
recovery. Full-set reconciliation, original-unit retention, early starter/ARM/
error-channel recovery, app documents/revocation/removal and the desktop/apps/
image/hardware goal remain work. No normal Root store or public capability opens.

Previous retained-handle checkpoint (31 October 2026):

The private Linux lifetime module now exports four original observation handles
with independently protected expectations and restores them after observer exit.
It checks the original boot/pidfs identity, exact root/group/event objects,
original PID/cgroup namespaces and current original unit/invocation/end facts.
The explicit restore path can retain the original mount across a new PrivateMounts
namespace; ordinary capture still quarantines named mount replacement. Namespace
drift latches quarantine. This native checkpoint is not a portable persistent
record, fresh bus peer or owner-journal recovery transaction.

An actual capability-empty administrative component service stores exactly four
handles in PID1, exits, and restarts after its app has died and the original group
has disappeared. The new invocation observes original end using the retained
handles, explicitly removes all stored entries and verifies the store is empty.
The independent controller retains the original app unit. No normal Root store
or production unit-retention path is enabled. Policy remains enforcing; this is
an unconfined_t component test, not production Root recovery-domain admission.
See [retained handles](../../spec/runtime.md#retained-observation-handles-across-a-component-service-restart).

Final affected native tests pass 4/4 Debug ASan/UBSan (4.42 s) and 4/4 Release
(3.95 s). They cover live/dead restore, wrong/missing handles and bindings, output
ownership/FD balance, actual mount and cgroup namespace changes, existing process
identity/reconnect/end behavior and a real PID1 store handoff. Host orchestration
tests pass 47/47 (2.342 s). The existing enforcing Root/User/app integration also
passes: immutable start results and three original process witnesses remain
correlated, Root closes terminal/released and User exits. Exact temporary
infrastructure is retired; normal v2 catalogues retain two closed gates each,
pending=None and no unsettled normal dispatch/lease.
All 179 host/guest source hashes match. Local documentation validation passes
575 file targets; anchors and external URLs were not checked by that validation.

Initial component setup failures were diagnosed before any stored handoff:
capability-empty root cannot traverse the developer's private home or inspect
PID1 namespace links. The test now uses a protected root-owned /run tree and
preserves an already authorized host namespace binding using self observations.
No capabilities or policy grants were added. The six exact failed transient
units were retired only after independent no-process/job/group/store checks;
their original inputs/journals remain preserved as failed evidence.

Canonical durable handoff enrollment, partial/unknown-set reconciliation,
continuous production unit retention and actual Root/User restart settlement
remain work. The full desktop/apps/image/hardware objective stays active.

Previous immutable v4 process-evidence checkpoint (31 October 2026):

The native execution module now persists canonical AETHPRC1 observations in one
new execution_processes table per owner. V4 is an explicit additive upgrade from
frozen v3 under the same writer lock. Published v1/v2/v3 SQL and existing rows,
identities/revisions/gates remain unchanged; native create stays v2 and open
validates exact v2/v3/v4 without migrating or backfilling old starts.

Root enrolls its original starter before ARM and its actual app under the same
kernel identity/PID/invocation/cgroup afterward. User enrolls its actual registered
app before READY. In v4 the three fresh transitions require their corresponding
validated original witness. Enrollment changes only the new immutable row and
catalogue revision; retries retain original bytes/time. Historical maintenance
reads allow a replacement runtime on the same authority/account/boot, without
adopting an old run, restoring readiness, repeating an effect or freeing a lease.

The first sixteen-scenario native matrix passes Debug ASan/UBSan (142.19 s):
all enrollment writes/commit uncertainty, six actual enrollment child exits,
runtime replacement/read-only reopening, missing transition barriers, closed
gates, revision limits, immutability/retention and real database-byte corruption.
Both owners additionally fault all six native v4 DDL/version boundaries and lost
commit directions. Source-contract tests pass 40/40 (79.796 s), including six
actual migration process exits, full-page rollback and frozen v3/source drift.
Independent byte checks pass 20/20 (0.071 s); host orchestration passes 47/47
(2.435 s), including negative archived User-binding checks.

The actual enforcing Root/User/app fixture now passes native enrollment/get/retry
for all three observations, start/readiness/end and successful run closure.
After both authorities exit, its independent controller decodes and correlates
the archived records, immutable start results and actual original process/group
end. Both authorities had all capability sets empty. No policy grant or public
capability was added. Normal installed catalogues remain v2; the explicit test
alone selects v4. Actual Root/User restart coordination, already-dead/lost-handle
recovery, general holders/documents/revocation/removal and normal provisioning
remain work. The full desktop/apps/image/hardware objective remains active.
See [durable process evidence](../../spec/runtime.md#durable-original-process-enrollment).

Final affected regressions pass 12/12 Debug ASan/UBSan (658.41 s) and 12/12
Release (567.72 s), including the new v4 migration of populated closing/quiet
records and unchanged older start/observation/completion/service behavior.
These are affected-boundary groups, not a new full-suite claim. The reviewed
capacity assertion derives its bound from actual fields; rebuilding reproduces
all 76 tested executables byte-for-byte. All 178 guest source hashes match the
host snapshot; 570 local Markdown file targets pass.

Final read-only carrier inspection confirms enforcing policy, unchanged ordinary
v2 catalogues with two closed gates each, pending=None and no unsettled normal
dispatch/lease. Test service names are unowned; exact temporary files/module/units
are absent. Normal executing profile/starter/view scaffold remain absent. Both
archived v4 databases pass integrity/foreign-key checks, contain the three matched
original witnesses and retain completed start results with Root lease released
and User exited. No actual Root/User restart or general removal is inferred.

Previous live-process identity checkpoint (31 October 2026):

The existing Linux process module now captures a boot-bound non-recycled pidfs
identity from its authenticated ProcessFD and independently admitted current
context. A new observer can compare a fresh connection with the original protected
identity/context. No numeric-PID fallback, new daemon, permission or dependency
is added. The portable AETHPID1 frame is 32 bytes; its frozen full-uint64 vector
agrees between Python and C. It is maintenance evidence, not executable/readiness
or recovery authority.

The actual root-only component test exits the first observer, execs a second,
reidentifies the still-live app through a new bus connection, rejects a different
same-context child and observes the original run's end. Native tests also reject
same-name service replacement, current-context drift, denied/missing kernel facts,
denied fstat, zero inode and a dead process, preserving outputs and FD balance.
Final affected Debug ASan/UBSan groups pass 3/3 (3.60 s); Release 3/3 (3.16 s).
Independent byte tests pass 19/19 (0.036 s). This is affected-boundary regression,
not a new full native suite or OS/hardware acceptance claim.

The final actual enforcing Root/User/app integration passes identity checks in
both capability-empty authorities, along with actual image/data/peer/start/end
checks and successful fixed-run closure. Its temporary fixture is retired. Normal
catalogues remain v2 with two closed gates each, pending=None, no unsettled normal
dispatch/lease, unowned test names and absent test units/module/profile/starter.
Policy stays enforcing. Identities in this test remain in memory: protected
durable witness enrollment, real Root/User restart reconciliation and already-dead
process recovery still require implementation. No old runtime/run is adopted.
See [process identity](../../spec/runtime.md#original-live-process-identity-after-observer-replacement).

Two earlier confined attempts failed because libsystemd's optional filesystem
probe was denied; a supervised read-only diagnostic proved direct fstat works
in that same User domain. The final path retains the existing mandatory
PIDFD_GET_INFO checks around direct 64-bit fstat, without loosening policy.
Both unresolved historical fixtures/catalogues were archived after independently
verified original process/unit/invocation/group/job drain and complete owned-tree
inspection. Their running leases were not released or declared recovered.
See [sources](SOURCES.md#live-process-identity-after-observer-replacement).
All 174 guest source hashes match the tested host snapshot; 559 local Markdown
file targets pass. The full desktop/apps/image/hardware goal remains active.

Previous separate v3 closure checkpoint (31 October 2026):

Successful original-run shutdown now has its own v3 run_closures record per owner.
The explicit additive upgrade preserves frozen v1/v2 definitions, all existing
rows/revisions/gates and the original completed start result. Normal creation
stays v2 and opening never migrates. Root closes its dispatch, User records quiet
and stopping/unavailable, Root atomically records end/terminal/released lease,
then User acknowledges exited. Start operation revisions/times/results/steps stay
unchanged. Exact retries are historical; owner replacement marks unfinished close
uncertain and needs separate recovery.

Canonical 136-byte Root-closing/User-quiet/Root-release receipts form a SHA-256
chain from the original 200-byte start receipt. A separate 120-byte end observation
binds actual original boot/invocation/cgroup/PID. Frames remain metadata, never
bearer/process/holder capabilities. No IPC/filesystem/supervisor effect runs inside
SQLite. The native metadata tests cover all writes, commit uncertainty, real child
exits, owner replacement, closed gates and revision overflow. Source-contract
tests cover preserved populated rows, all eight native DDL/version failures,
six migration process exits, source/version/owner drift and SQLITE_FULL.

The actual enforcing fixed test now joins original Root/User IPC and independent
Linux end observation to the journals: Root dispatch terminal/lease released,
User exited/unavailable. It rechecks the original User before mutation and verifies
the protected one-shot unit has no restart/activation/delegation/FD-store holders.
The fixed inert profile excludes other exports/migration; the User boundary
requires no grant/access rows. The controller independently decodes both journals
and their receipt chain and verifies the unchanged start result. Historical data
now also includes run-close-receipts.bin (408 bytes). Only temporary test stores
are upgraded. General holders, early failed starts, real owner-restart recovery,
documents/revocation, full removal and normal session provisioning remain work.
The full desktop/apps/image/hardware objective remains active.
See [run closure](../../spec/runtime.md#separate-durable-run-closure).

The complete native suite passes 45/45 in Debug ASan/UBSan (791.20 s) and
45/45 in Release (711.73 s). These runs include the original twenty closure
scenarios. An additional focused grant case then passes in both builds: pending,
active and even historical revoked grant rows prevent this P0 quiescence path.
Its metadata-only fixture does not claim real document/FD revocation. Final
source-contract, byte-contract and host-orchestration checks pass 35/35
(83.519 s), 18/18 (0.013 s) and 46/46 (2.335 s), respectively. The final actual
enforcing integration repeats successfully after the immediate broker recheck.

Read-only final inspection confirms schema v2 and two closed gates in each normal
owner catalogue, pending=None, no unsettled normal dispatch/lease, unowned test
service names and absent temporary inputs/module/units. Normal execution profile,
starter and view scaffold remain absent. Both actual Root/User test processes
had all five capability sets empty. The archived v3 catalogues pass integrity and
foreign-key checks; their released records and three-receipt chain agree with
the unchanged start receipt. All 172 guest source hashes match the tested host
snapshot. These are fixed-profile software tests, not daily-use OS or hardware
acceptance. All 548 local Markdown file targets pass the final file-link check.

Previous original-process observation checkpoint (31 October 2026):

The new linux/lifetime module holds the original admitted process pin, exact
cgroup root/group and original cgroup.events reader. A current end observation
requires the original inactive/failed unit invocation, no main PID/job, original
ExecMainPID and an empty recursive group or independently deactivated original
group object. It repeats the observations; missing names alone, replacement,
restart and mount drift reject. It adds no thread, resident service or dependency.
Caller-held unit reference, trusted PID1/filesystems and future dispatch/migration
exclusion remain explicit obligations. Closing a handle releases no lease or data.

The final five-fixture linux.lifetime test covers successful/failed services,
an inactive job-free leader with a live nested child, actual same-name restart,
namespace replacement and controlled property/lookup faults. Fork/busy-close/error
outputs and FD balance pass. Together with linux.process, final Debug ASan/UBSan
passes 2/2 (6.64 s) and Release 2/2 (4.73 s). The affected coordinator/endpoint
Debug groups pass 2/2 (143.48 s); host orchestration passes 46/46 (2.803 s).
These are scoped checks, not full-suite or OS/hardware acceptance.

The actual enforcing Root/User/starter/app test also obtains an authenticated
native end snapshot for its exact UID1000 app and retains it as native_end.
The historical Root lease stays running/dispatch observed; User stays running/ready.
No native run-closure journal or release is inferred from that snapshot. The test
policy adds only read/open/getattr for Root's cgroup.events, then retires it with
the exact owned fixture. See [lifetime observation](../../spec/runtime.md#original-process-lifetime-observation).
Final read-only carrier checks confirm Enforcing, unchanged normal inputs,
pending=None, two closed gates per owner, unowned service names and absent test
inputs/module/units. Normal execution profile/starter/scaffold remain absent.
Both actual Root/User capability sets are empty; their original start receipt
is retained. All 166 guest source hashes match the tested host snapshot; 533
local Markdown file targets pass. The final connected repeat captures the observer
only after original Root/User execution confirmation.
Executing durable closure/recovery, holder settlement, documents/revocation,
complete removal and the full desktop/apps/image/hardware goal remain active.

Previous start-result completion checkpoint (31 October 2026):

Root now completes the known start postcondition through an immutable 200-byte
AETHLCR1 receipt, after original observed execution and the settled canonical
UNKNOWN channel diagnostic agree. Actual execution observation remains independent
of that diagnostic; UNKNOWN alone proves no execution. User authenticates the original Root reply and
checks the complete profile/run/invocation/observation against its own registered,
ready context before completing its operation/link. Both results are applied and
bound to the exact LaunchId. Root dispatch stays observed/lease running and User
launch stays running/ready; no gate or run/lease revision is changed.

Exact receipts/times survive lost acknowledgements and later state changes.
Original owner replacement, closed fresh admission, missing readiness or a
contradictory diagnostic reject success. The nineteen native completion scenarios
pass Debug ASan/UBSan (93.89 s), including all three Root/four User writes, commit
uncertainty, actual process exits, overflow and historical terminal/exited retries.
Independent byte vectors pass 17/17 (0.038 s); host orchestration passes 45/45
(3.680 s). The eight affected-owner groups pass 8/8 Debug ASan/UBSan (437.53 s)
and 8/8 Release (410.29 s). This is affected-boundary regression, not a new
full-suite or OS/hardware acceptance claim.

The actual enforcing live test passes both original-process IPC acknowledgements
and repeated Complete calls. Its controller independently reconstructs the Root
receipt and verifies the same digest in Root step6 and User step4, preserving
the running lease and original diagnostic. launch-completion.bin is historical
test evidence, not a capability. No normal service/profile/policy or public
capability is enabled. Final read-only carrier checks confirm Enforcing, unchanged
normal inputs, pending=None, two closed install/two closed data gates, no unsettled
normal dispatch/lease, test names unowned and temporary inputs retired. Normal
executing profile/starter/scaffold/Root unit remain absent. All 162 actual guest
source hashes match the tested host snapshot; 446 local Markdown targets pass.
The next work is executing recovery/run closure, native
documents/revocation and complete removal; normal session/dynamic provisioning
and the full desktop/apps/image/hardware goal remain active.
See [start results](../../spec/runtime.md#launch-result-completion-without-run-or-lease-release).

Previous Root observation checkpoint (31 October 2026):

Root now separately journals actual target execution as step5 and an observed
dispatch. The original full profile, starter invocation/context, current owner/
account/gates and running generation lease must match. ARM becomes applied; both
launch operations and the lease remain running. Exact retries preserve later
closing/terminal/uncertain state without reopening gates or repeating effects.
Channel diagnostics remain independent: their bounded reason survives whether
recorded before or after the separate actual-observation step/state.
See [Root observation](../../spec/runtime.md#root-execution-observation-journal).

The enforcing fixed fixture now joins Root's actual held starter/app process,
current service/invocation/cgroup/MCS and original app connection to actual User's
independently checked image/registration/READY evidence. User authenticates Root's
observed receipt. A deliberately wrong bus GUID rejects before mutation; exact
valid retries preserve history. The successful live run archives Root steps0..5
applied and User running/ready while preserving both incomplete operations and
the running Root lease. No normal API/profile/policy/capability is enabled.

The initial thirteen focused observation scenarios pass (55.58 s). Baseline native
regression passes 42/42 Debug ASan/UBSan (497.98 s) and 42/42 Release (432.28 s)
before the final diagnostic-retention correction. Host orchestration passes 45/45
(2.526 s). After that correction, the final fourteen-case observation and affected
coordinator groups pass 2/2 Debug (120.05 s) and 2/2 Release (112.04 s). Tests include
all five write faults, commit uncertainty/actual child exits, restart/closed gate,
revision overflows, both diagnostic orderings and later-state receipt preservation.
The first connected negative probe expected the wrong existing error name;
its unsettled histories were inspected/archived before exact test retirement,
then the corrected connected run passed. A final live repeat confirms Root observed
with steps0..5/applied while preserving its bounded channel diagnostic reason,
User running/ready, both incomplete operations and the running generation lease.
Final read-only checks confirm Enforcing, unchanged normal inputs, pending=None,
two closed install/two closed data gates, no unsettled normal dispatch/lease,
test names unowned and temporary inputs retired. Normal executing profile/starter/
scaffold/Root unit remain absent. All 161 actual guest source hashes match the
current host inventory; 437 local Markdown targets pass. This is not production
executing recovery, launch-result completion, removal or OS/hardware acceptance.

Next implement launch-result completion independently of lifetime/lease release,
then executing recovery/drain, native documents/revocation and complete removal.
Normal session provisioning, dynamic allocation and mutation fencing remain.
The full desktop/apps/image/hardware objective stays active; no user choice blocks
this backend work.

Previous actual User registration/readiness checkpoint (30 October 2026):

The actual User owner in the isolated enforcing fixture now authenticates the
executing app's original bus connection/ProcessFD, service/invocation/cgroup/MCS
and running executable against original Root's authenticated current-code
observation. User independently rechecks private allocation/MAC. It retains the
original app pin before registration commits, including uncertain replies.
The new bounded AETHIMG1 codec has an independent Python/C frozen vector and
rejects malformed shape without changing outputs. No dependency was added.

Native User REGISTER and READY transactions reuse v2: registration records the
verified invocation and leaves readiness unknown; READY from that exact actor
records running/ready. Exact retries preserve later state. Premature READY,
another connection, changed evidence/invocation, stale gate/runtime and revision
overflow reject. Every write/commit boundary and actual child exits around both
commits are covered. Neither owner's operation completes and Root's lease remains
running. Readiness is the inert app's protocol declaration, not UI responsiveness.
See [registration/readiness](../../spec/runtime.md#user-mediated-application-registration-and-readiness).

Full native Debug ASan/UBSan passes 41/41 (450.68 s), Release 41/41 (383.41 s).
Host migration/wire passes 30/30 (55.259 s), orchestration 45/45 (5.456 s), runtime
vectors 16/16 and 374 isolated constraint assertions. The twelve native readiness
scenarios exercise all five REGISTER/four READY writes and commit boundaries.
These are test results, not OS responsiveness or performance measurements.

The final live repeat passed the actual cap-zero User current-image check and
original-connection handshake, including REGISTER after READY and repeated READY.
Root's test observation also checks the exact original installation revision.
Temporary proc/executable reads add no ptrace or capability and direct code-store
opening remains denied. Owned processes/units/config/policy were retired after
independent drain, with both unsettled owner histories archived. Final read-only
verification confirms Enforcing, unchanged normal protected inputs, pending=None,
two closed install/two closed data gates, no unsettled normal dispatch/lease,
System1/test User name unowned and temporary inputs retired. Normal executing
profile/starter/scaffold/Root unit remain absent. All 161 actual guest source
hashes match the current host transfer inventory. No normal capability is enabled.
Normal SDK/session provisioning and executing recovery/drain/lease settlement
remain work, followed by documents/revocation and complete removal. The full
desktop/apps/image/hardware objective remains active; no personal choice blocks
that backend work.

Previous connected-owner checkpoint (30 October 2026):

The isolated enforcing test now connects an actual registered UID1000 User
process to Root dispatch. User owns a separate native catalogue/runtime, verifies
authenticated original Root replies and private allocation/MAC, then performs
native projection/cache, first data enrollment/admission, launch acceptance,
Root receipt binding and durable execution preparation. Its original connection
submits the exact profile to Root. Root never reads User SQLite. Root code and
User private-data readers are separated; actual cross-boundary opens are denied.

The new native execution admission adapter retains the original registered peer
before REQUEST and re-verifies it before ARM. Account/epoch/runtime/boot drift,
departure and replacement cannot inherit delegation; unknown request outcomes
cannot create another attempt. Complete endpoint/profile equality is checked.
Close is local cleanup only. Controlled native tests cover replacement/lost-
request/fork/callback and independent pidfd lifetime; actual enforcing negatives
reject the wrong unit, another connection from the same User process and the
same-UID app's privileged broker call.

The connected live run passes actual starter/private-view/target ELF observation
and exact app/User/Root process/unit/cgroup/job retirement. Both unresolved owner
catalogues are archived, preserving User preparation and Root running lease.
Earlier startup/observer failures were reconciled and archived without releasing
executing obligations. No normal capability or installed profile was enabled.
This is one fixed system-scope UID1000 test broker, not normal session provisioning,
general mutable admission, genuine app readiness or production recovery/removal.
See [connected owners](../../spec/runtime.md#connected-user-and-root-execution-admission).

Full Debug ASan/UBSan passes 40/40 (390.60 s), Release 40/40 (359.30 s).
Host migration/wire passes 30/30 (53.310 s), orchestration 45/45 (3.726 s),
runtime vectors 15/15 and 374 constraint assertions. Final live repeat passes
with actual Root/User effective/permitted/inheritable/bounding/ambient capability
sets observed empty. Both owner histories are archived; no lease/operation is
settled from process death. These are test results, not OS performance metrics.

Final read-only carrier verification confirms Enforcing, unchanged verified
normal inputs, pending=None, two closed install/two closed data gates, no unsettled
normal dispatch/held lease, System1/test User name unowned and temporary execution
inputs retired. Normal executing profile/starter/scaffold/Root unit remain absent.
Next implement genuine User-mediated target registration/readiness and executing
recovery/drain/lease settlement, then native documents and complete removal.
Normal session/owner provisioning and mutation/revocation remain required; full
desktop/apps/image/hardware scope stays active. No user decision blocks that work.

Previous User preparation checkpoint (30 October 2026):

Native User execution preparation now atomically checks the accepted launch,
original Root link and current binding/attached-data fences, then binds complete
AETHEXE1 in its own operation step1 before a possible executing Root request.
Only a fresh confirmed commit returns submit_required=1. Exact retries retain
bytes/revisions with zero; unknown commit replies expose no permission to replay.
The launch remains preparing/unknown with no invocation. The step disables the
old User pre-dispatch withdrawal path and survives restart as an unsettled
recovery_required obligation. Changed profile and stale owner/data fences reject.

The focused native Debug ASan/UBSan catalogue.user_execution group passes nine
scenarios (31.40 s), including four write-statement faults, lost commit replies,
actual process exits on both sides of COMMIT, restart/gate closure, overflow,
identity substitutions, full-profile conflicts and byte-stable retries. These
are owner transaction tests, not actual User RPC/process or power-loss evidence.
No SQL/XML/dependency/policy or installed input changes. Current Root/broker/
reservation admission, independent allocation/MAC and owner mutation exclusion
remain caller obligations. This is not an ARM/readiness/drain capability.
See [User preparation](../../spec/runtime.md#user-execution-preparation-journal).

Full native Debug ASan/UBSan passes 39/39 (399.86 s), Release 39/39 (339.37 s).
Host migration/wire 30/30 (49.053 s), orchestration 43/43 (2.339 s), runtime
vectors 15/15 and 374 constraint assertions also pass. Timings are tests, not OS performance.
Final read-only carrier verification confirms Enforcing, verified unchanged normal
protected inputs, pending=None, two closed install/two closed data gates, no
unsettled normal dispatch/held lease, System1 unowned and temporary execution
inputs absent. Normal executing profile/starter/scaffold/Root unit remain absent.

Next connect a real registered User broker and its independently checked private
allocation to Root request/arm in the enforcing integration, preserving separate
catalogue writers and current peer/unit/invocation evidence. Genuine target
registration/readiness, executing recovery/lease settlement, documents and complete
removal remain P0 work. The desktop/apps/image/hardware goal remains active.

Previous live execution checkpoint (30 October 2026):

An explicit enforcing-carrier test now runs the actual native Root journal and
Execution1 endpoint, real PID1 StartUnit, unchanged executing starter, authenticated
writer/prepared release, witnessed private mounts and UID/GID1000 target exec.
A fresh post-exec connection supplies actual current credentials; Root joins it
to its still-held starter process pin and original unit/invocation/cgroup. The
administrative driver authenticates Root and independently checks the actual
running ELF against the manifest Artifact before exact pidfd termination.
Root/starter receive neither SYS_PTRACE nor KILL. The target checks zero caps,
empty supplementary groups and absence of inherited authority descriptors.

Temporary policy uses a new MCS-constrained aetheri_exec_app_t:s0:c35,c36 target,
preserving the old non-executing app domain's store/Root-messaging denials. A
dedicated unit-file type grants only the actual fixture status/start needs.
The separate native test root has new authority/runtime epochs and actual boot;
both units privately bind it to the fixed anchor. Broker/data admission, trusted
package/catalogue/allocation/scaffold setup are controlled test inputs. No normal
Root/User launch path or public capability is enabled.

The successful test independently observes original app/Root process death and
unit/cgroup/job drain, verifies exact inputs and the complete owned tree, archives
the native incomplete operation/running lease, and retires only its fixture,
unit files, bus config and policy. Process exit/EOF never releases that lease.
Unknown dispatch preserves the complete pending fixture for inspection. This is
test retirement, not production execution recovery or complete app removal.
See [live test contract](../../spec/runtime.md#isolated-live-rootstarterexecution-test).

Full native Debug ASan/UBSan passes 38/38 (388.84 s), Release 38/38 (337.97 s).
The earlier explicit enforcing execution-policy probe also passes all five
scenarios with the payload's added libsystemd linkage. The final live integration
repeat passes with the completed normal-state guards and retires its fixture.
Host migration/wire passes 30/30 (50.281 s), orchestration 43/43 (2.428 s), runtime
vectors 15/15 and constraints 374 assertions. These are test timings, not OS
performance measurements.

Final read-only observation confirms Enforcing, unchanged verified normal inputs,
pending=None, two closed install/two closed data gates, no unsettled normal
dispatch/held lease, System1 unowned and temporary execution roots/config/module/
units retired. Normal starter/profile/scaffold/Root unit remain unprovisioned.
No normal managed app, hardware installation or other project was changed.

Normal authority/starter provisioning, fresh current User/private-data admission,
mutation exclusion, dynamic category/scaffold allocation, genuine registration/
readiness and executing recovery/lease settlement remain implementation gates.
The full OS/apps/UI/image/hardware objective remains active; no user decision
blocks the next backend work.

Previous starter/scaffold checkpoint (30 October 2026):

The separate aetheri-execution-starter now joins protected profile/self-image
checks, the authenticated Root client, private code/data views and one-way exec.
Its fixed P0 Root unit is aetheri-p0-execution-root.service. Original registration
precedes setup; the original release and a second target check precede exec.
Literal manifest argv and three fixed inert-fixture environment entries replace
inherited variables. Failures report through the issued channel and exit without
retry/fallback. The old no-exec helper/proof is unchanged.

AETHVWS1 independently binds the four temporary scaffold objects to the complete
AETHEXE1 digest. The 184-byte contract has a frozen vector/reference encoder.
Recreated directories and changed records are rejected, source observations are
made inside the private namespace, and held/named mounts plus the actual image
are checked before handoff. Closing local handles is never namespace/app drain.
This is separate from persistent private-data allocation. Root's pre-dispatch
scaffold creation/flush/publication journal remains to implement.

Focused Debug ASan/UBSan source/view and flow/execute tests pass. Source fixtures
now include 15 scaffold/mount cases in addition to the earlier35; only the initial
fixed anchor is mapped, with actual file contexts/mounts/ELF checks. Flow faults
use controlled context/client/views/execute adapters and real error channels;
they are not live starter-domain or target-exec acceptance. Actual negative CLI
probes reject unprivileged/default administrative contexts and extra arguments.
Full Debug ASan/UBSan regression passes 38/38 (379.63 s), Release 38/38 (312.22 s).
The flow group includes 13 ordering/fault cases and three direct failure-frame
cases. Host migration/wire passes 30/30 (54.772 s), orchestration 39/39 (0.296 s),
runtime/reference vectors 15/15 and constraints 374 assertions. These are test
durations, not OS performance measurements.

Final read-only carrier verification confirms Enforcing, unchanged verified normal
protected inputs, no pending request, two closed install/two closed data gates,
no unsettled dispatch/held lease, no temporary execution fixture/module/private
broker and System1 unowned. Starter/profile/scaffold record and fixed Root unit
remain unprovisioned. Release negative entry/argument probes also pass. No normal
managed app was dispatched or hardware/storage installation performed.

No starter/profile/execution-views record, Root unit or policy was installed.
Positive confined starter/Root admission, protected provisioning/live supervisor
request/current broker-data mediation, actual target registration/readiness and
independent executing drain/recovery remain required. The full OS/apps/UI/image/
hardware goal stays active; no user decision blocks the next backend work.
See [starter/scaffold contract](../../spec/runtime.md#separate-executing-starter-and-witnessed-mount-scaffold).

Previous endpoint checkpoint (30 October 2026):

The separate native Execution1 producer now joins actual BIND/PREPARE/ARM owner
transactions to correlated descriptor/deferred replies. It retains the original
prepared request and answers only from fresh ARM after commit. A four-second
deadline is checked both before and after the transaction; expiry/send uncertainty
never releases a lease or proves drain. Local close preserves process/durable
obligations. Its vtable is not linked into the installed authority or default
method tables; public capabilities, existing XML/SQL and normal protected inputs
remain unchanged. No new dependency family, installed policy or resident service.

The focused Debug ASan/UBSan group passes all 16 scenarios (67.52 s), using real
isolated owner catalogues, ProcessFDs, pipes and bus transport with explicit
controlled actor-label/PID1 adapters. The carrier's existing broker37 serves a
private Root-only bus without activatable services. Its launcher uses system
scope for its own PID1 subscription. Initial system-bus routing denial, absent
dbus-run-session and user-scope manager connection failure were test-environment
issues resolved without weakening global rules or adding a package. Full Debug
ASan/UBSan regression passes 37/37 (370.85 s), Release 37/37 (314.36 s).
Migration/wire passes 30/30 (52.400 s), orchestration 39/39 (0.399 s),
runtime vectors 14/14 and constraints 374 assertions. These are test timings,
not OS benchmarks. See the
[endpoint contract](../../spec/runtime.md#journal-backed-execution-endpoint).

Final read-only carrier verification confirms Enforcing, verified normal protected
inputs, no pending request, two closed install gates/two closed data gates, no
unsettled dispatch/held lease, no temporary execution fixture/module/private test
broker and System1 unowned. execution.conf/execution-starter remain unprovisioned.
No normal managed app was dispatched or hardware/storage installation performed.

Actual protected Root unit/profile/starter provisioning, live supervisor request
and current broker/private-data admission, target execution/registration/readiness
and independent executing drain/recovery remain pending. No user product decision
blocks that backend work. The full OS/apps/UI/image/hardware goal stays active.

Previous authenticated-client checkpoint (27 October 2026):

The native executing-starter client now pins the actual Root discovery/process/
active unit/invocation/cgroup/boot and checks profile epoch/runtime. A correlated
reply capture requires the retained sent request's unique destination, bus and
matching cookies plus unaugmented UID/PID/label/ProcessFD. Each operational call
addresses the original owner and runs at most once per client; timeout, drift,
bad binding/revision or unsuitable writer cannot authorize execution or retry.
The returned writer is separately owned and outlives client close. Duplicate
prevention across objects/processes still belongs to the issuer journal/profile.

Private Execution1 1.0 names/signatures generate a separate client header; default
authority tables, installed services, public capability set, SQL and older XML
remain unchanged. Actual starter, protected Root unit/profile provisioning and
journal-backed producer handlers/adapters remain work. An authenticated fixture
release does not establish actual app execution, registration or readiness.

The explicit 28-case protocol fixture passes with actual confined Root supervisor
and descriptor transport, both with Release and an external Debug ASan/UBSan
client. The producer serves synthetic replies only: no catalogue/views/target
exec. A missing bus-daemon pipe-write permission initially reset its connection;
an actual AVC identified the cause. Corrected temporary Root pipe/bus-write
permissions retire only after original process/unit/cgroup/job drain; no daemon
read, permissive or instrumentation ptrace grant. Full regression passes 36/36
Debug ASan/UBSan (305.47 s) and 36/36 Release (249.52 s), followed by another
successful confined Release protocol probe. Migration/wire passes 30/30
(52.959 s), orchestration 39/39 (0.258 s), runtime vectors 14/14 and constraints
374 assertions. These are test timings, not OS benchmarks.
Final read-only verification confirms Enforcing, unchanged normal protected
inputs, both install/data gates closed (two each), no pending/unsettled launch/
lease, no reply/exec temporary fixture/module and System1 unowned. No normal
authority/helper/profile was updated or managed app dispatched.
See [client protocol](../../spec/runtime.md#authenticated-executing-starter-client).

Read-only execution-profile/source loading now binds the compiled EXEC selection
to protected config/publication witnesses, exact manifest inventory and declared
run ELF, and independently recorded private allocation/owner/full c35,c36 context.
Success owns three CLOEXEC source FDs; failures retain output and close temporary
handles. It performs no SQL/gate/process effects. Source data still needs private
noexec views; current catalogue/User admission and mutation-excluding policy are
separate caller obligations. See [source-opening contract](../../spec/runtime.md#protected-execution-profile-and-source-opening).

Existing pure registry/config decoders moved to fixture_profile.c. The shared
inventory checker now also has a no-write/no-fsync entry point; publication
durability is unchanged. The reserved execution.conf/execution-starter/type are
not provisioned. Positive starter image/domain admission is still untested;
absence/wrong-purpose labels are rejected. No installed binary, schema/XML,
policy grant, dependency family or resident process changed.

The new native group covers 35 owned fixtures with real kernel/file-context checks;
only its initial anchor path is redirected. An explicit Root read-only probe also
opens the actual retained exec code/private data eight times with no mapping.
Protected inputs/catalogues stay unchanged, gates closed and no live dispatch/
lease exists. Its synthetic build-local request IDs are not a launch profile.
Previous source-binding checkpoint: 36/36 Debug ASan/UBSan (305.95 s) and 36/36 Release
(251.79 s). After final review added explicit account-scope binding and a
publication-consistent negative case, the affected group passes again with all
35 fixtures: Debug 7.66 s, Release 2.48 s. The final actual-installation observation
also passes. Migration/wire 29/29 (46.893 s), orchestration 39/39 (0.386 s), runtime
vectors 14/14 and constraints 374 assertions pass. These are scoped test timings,
not OS performance measurements. Earlier evidence below belongs to its stated
checkpoint. Actual starter/wire/supervisor mediation, target registration/readiness
and independent drain remain work.
Final read-only carrier verification confirms Enforcing, no pending request,
both install gates and both data gates closed, no unsettled live dispatch/lease,
unchanged protected inputs, no temporary execution module/fixture and no reserved
starter/profile file provisioned.

A new native Root coordinator now joins the existing execution journal, original
starter ProcessFD/invocation, issuer-owned pipe and sd-event watch. Request and
release adapters run only after fresh commits, outside SQLite, and never repeat
after uncertainty. Binding pins/refers/reobserves the exact starter before its
writer is issued. Context drift is retained; ARM samples queued channel state
before any release. Terminal watches stop, and coordinator close cannot free a
lease or imply process drain. Callback-time close returns EBUSY.

Canonical AETHEXD1 diagnostics now persist actual invocation/context/profile,
observation kind and phase/errno through an atomic step-4/reason transaction.
Unknown EOF, starter failure, malformed input and observer failure stay distinct.
Exact acknowledgement retry is safe and cannot repeat an external effect; Root
restart preserves evidence/held obligations. No schema, XML, library family,
resident process or policy grant changed.

The coordinator tests use real owner SQL, ProcessFDs, pipes and sd-event with
controlled process/PID1/effect adapters. They do not prove live starter/domain
admission. The library is not connected to the installed authority/wire or a new
starter binary yet. Protected profile/image/unit provisioning, current authenticated
reply/broker/data mediation, actual target readiness and independent drain remain
work. See the [coordinator/diagnostic contract](../../spec/runtime.md#native-execution-coordinator-and-durable-diagnostics).

Previous coordinator checkpoint: 35/35 Debug ASan/UBSan (278.09 s) and 35/35 Release (250.86 s).
The coordinator group covers 16 scenarios; catalogue execution covers 20, with
four independent diagnostic vectors. Migration/wire passes 29/29 (38.208 s),
orchestration 39/39 (0.433 s), runtime vectors 14/14 and isolated constraints
374 assertions. These are test timings, not OS benchmarks. A full-suite peer
fixture failure exposed its immediate-EOF assumption; it now waits for actual
terminal evidence under one deadline and passed 24 consecutive roundtrips.
Known partial error data also blocks arm before event dispatch.

Read-only protected-input verification passes afterward: Enforcing, no temporary
execution fixture/module or pending request, both install/data gates closed and
no unsettled live dispatch/lease. No new authority binary, interface, helper,
policy grant or resident process was installed. Callback/context error details
remain local status while their durable intent/arm/lease obligations remain held;
complete control-plane history/recovery and live integration are still work.

The Linux execution subsystem now has a process-owned anonymous error pipe:
the supervisor keeps its reader and exports only the checked NONBLOCK/CLOEXEC
writer. The one-way boundary also requires an anonymous pipe created by UID/GID
0. The bounded reader retains fragments, rejects invalid/duplicate/truncated
records and treats empty EOF as unknown, including while an actual executed
payload remains alive. Terminal channels need no further polling. Constructor/
I/O/cleanup failures, FD identity/flags/creator, fork ownership and resource balance
have native tests. The peer test now transfers only a writer in an actual sd-bus
reply after exact kernel sender capture; message borrowing/duplication is exercised.

The new code is used by the existing native execution tests, including a separate
enforcing Release probe. An initial Debug policy probe hit LeakSanitizer's denied
same-domain ptrace; its known drained failed test was retired before the Release
run, with no policy broadening. Production authorities, SQL/XML and incoming-FD
denials are unchanged. The new channel/transport test is not a live execution
endpoint, registered app, readiness or drain implementation. Next bind it to the
actual Root journal and selected starter, with authenticated current reply/runtime
and original held process/invocation. See the [error-channel contract](../../spec/runtime.md#issuer-owned-execution-error-channel).

Previous error-channel checkpoint: 34/34 Debug ASan/UBSan (199.11 s) and 34/34 Release (176.38 s).
Migration/wire passes 29/29 (39.113 s), orchestration 39/39 (0.612 s), runtime
vectors 14/14 and isolated constraints 374 assertions. The final Release enforcing
probe also passes all five cases and retires its owned units/module. Read-only
verification then confirms Enforcing, unchanged protected installation inputs,
both install/data gates closed, no pending/nonterminal live dispatch and no held
code lease. These are scoped test results/timings, not OS benchmarks or hardware
acceptance. No new authority binary/endpoint or resident service was installed.

An internal AETHEXE1 execution profile now binds the exact run/p0/none request,
Root/User runtimes, I/G/D/storage, operations, revisions, artifact, trusted starter
and independent allocation digests. The native owner APIs authorize the accepted
User data fence and journal Root REQUEST, BIND, PREPARE and ARM. ARM freezes the
supervisor invocation/data fence and changes dispatch/lease to dispatching/running;
it does not claim executed code or app readiness. All extra effects are journaled
before an external request, with a fresh-only effect flag that is zero on retries,
including after a lost commit acknowledgement. Root restart retains uncertainty
and the lease. A different journal target prevents old no-exec withdrawal from
settling this operation, even if given its profile hash.

The new catalogue.execution test covers canonical bytes against independent
Python, statement/commit faults, byte-identical database rollback, actual owner
exits before/after REQUEST/ARM commit, all-stage Root restart, User replacement,
changed evidence, closed gates and overflow. Previous journal checkpoint: 33/33 Debug
ASan/UBSan (193.91 s) and 33/33 Release (177.66 s); the new focused test passed
in 65.70 s. Host migration/wire 29/29 (34.240 s), orchestration 39/39 (0.320 s),
runtime vectors 14/14 and isolated constraints 374 assertions also pass.
These are test-run times, not performance benchmarks. The new cases are isolated
native owner-store tests, with no live starter or endpoint/policy installation.
Read-only controller verification passes with Enforcing, both install/data gates
closed, no pending request and no unreleased lease/nonterminal live dispatch.
Existing SQL/XML and dependency families are unchanged. Next bind authenticated error
transport and actual target-image registration, readiness and independent drain.
See the [execution journal contract](../../spec/runtime.md#durable-execution-intent-and-arm).

The new execution fixture now has a separate authenticated user installation
binding, StorageRef, DataEpoch and immutable allocation record. Private
Development1 1.4 adds only RefreshExecFixtureInstall; its original methods keep
their selection/signatures and the public capability list remains empty.
The existing owner-scoped native transaction enrolls a second installation's
data/attachment/closed gate without changing unrelated rows or allocation objects.
Shared enrolled parents are reused; every private leaf/config/receipt is distinct.

The fixed exec data profile is c35,c36, separate from original c33,c34. Native
enrollment checks full raw label, UID/GID, 0700, selected account/install/publisher
and independent allocation identity before publication or SQL. An actual confined
cap-zero negative probe rejected c37,c38 with EACCES before any allocation record
or catalogue change; the same known object was restored after verified drain.
Normal enrollment and exact retries then passed for both installations. All old
system/user business rows, both sealed packages and original private objects
matched the independently captured before/after snapshot.

Previous projection/data checkpoint: 32/32 Debug ASan/UBSan (136.26 s), 32/32 Release (120.78 s),
migration/wire 29/29 (28.172 s), orchestration 39/39 (0.235 s), runtime vectors
14/14 and isolated constraints 374 assertions. Actual new projection retries,
same-UID/same-domain/app caller denials, broker departure FD cleanup, user
replacement and Root-only restart pass. These are test timings, not benchmarks.
No SQL version, dependency, policy grant or resident process was added. A controller
field-name error was corrected before its planned negative-test mutation and is
covered by owner-field/alias regression cases.

Both data gates and install gates remain closed. No real app was dispatched.
Next connect this exact new I/G/D/storage/action tuple to the executing starter's
durable dispatch, process/image registration, readiness and independent drain.
The original no-exec release proof must not be used for executable code. See the
[scoped projection/data contract](../../spec/runtime.md#scoped-execution-fixture-projection-and-data).
After orderly QEMU exit 0 and verified PID absence, an offline boot also passes
new projection/data and original data checks with cross_boot_reopen=true. Exact
allocation records, closed gates and old objects/history remain preserved; no
app dispatch was made and owned authority/probe services are stopped.

The compatible aetheri.p0.exec fixture now has its own installation, generation,
artifact, preallocated slot and native installation operation in the existing
system catalogue. It passed staging, sealed no-replace publication and atomic
first activation with exact retries. Its gate stays closed. Two compiled protected
registry selections preserve the original fixture/configuration. The root controller
tracks both and rejects reused immutable identities. All original package/config
objects and old install/generation/operation/step/dispatch/lease rows matched the
independent before/after snapshot. No private data was rebound or SQL patched.

The core action resolver now copies an exact manifest-declared executable,
literal argv and expected file size/hash from verified immutable metadata. It
does not turn main into run or supply runtime admission. The execution boundary
accepts argv[0] plus all eight manifest arguments, with actual maximum/overflow
tests. Both full native suites pass 32/32 Debug ASan/UBSan (137.62 s) and Release
(125.85 s); final expanded execution cases pass Debug/Release focused repeats
(1.52/0.31 s). Host migration/wire tests pass 29/29 (28.552 s), orchestration 36/36
(0.254 s), runtime vectors 14/14 and isolated constraints 374 assertions.
Normal enforcing endpoint/app-denial checks also pass after the authority update.
These timings are test evidence, not OS performance measurements.

No new schema/wire version, dependency, policy permission or resident process was
added. The installed authorities were updated only after verifying no unresolved
setup/lease/dispatch; the no-exec helper still cannot run app code. The new fixture
now has the projected binding and private DataEpoch/allocation described above;
the executing starter's durable dispatch/registration/readiness/drain remains next.
The existing live broker profile remains original-only. See the
[fixture/action contract](../../spec/runtime.md#separate-execution-fixture-and-declared-actions).
After verified old-process absence and an offline VM boot, publication and
activation checks also report cross_boot_reopen=true with unchanged objects/
completed receipts and gate closed. Normal enforcing endpoint/app-denial checks
pass on that boot; owned services are stopped. No app run was dispatched.

The internal [execution boundary](../../src/linux/execute.h) now verifies an exact
held P0 ELF image, seals all internal descriptors, drops groups/IDs/capability sets,
selects the mandatory SELinux exec label and calls execveat without a fallback.
Failure exits immediately, including after an irreversible change. A bounded
error record preserves the actual phase/errno; an empty channel remains unknown.
The [image observer](../../src/linux/image.h) checks kernel-held process identity,
current credentials/label/cgroup and the actual executable backing object/hash.
This is a current observation, not app readiness or future liveness.

The separate temporary enforcing test passed a real setup-to-app transition at
c33,c34, UID/GID 1000, empty capabilities, descriptor/environment sealing, private
data writes and denial of executable data mappings. A same-UID observer with no
capabilities also verified the actual image. Pre-exec exit 125 and actual payload
exit 125 both leave the empty error channel unknown. Every owned test unit was
independently drained; its temporary policy and exact fixtures were removed.
This test never registered an app or changed the retained authorities/catalogues.
No public API, SQL version, dependency or resident service was added. The installed
no-exec helper and its narrow withdrawal proof remain unchanged. Next connect a
new compatible fixture to durable target dispatch/observation and real drain;
never reinterpret the old main/run or c31,c32 records. See the
[execution contract](../../spec/runtime.md#native-one-way-execution-and-image-observation).

Current execution baseline: 32/32 Debug ASan/UBSan (237.03 s), 32/32 Release
(223.30 s), host migration/wire 29/29 (53.841 s), orchestration 33/33 (0.214 s),
runtime vectors 14/14 and isolated constraints 374 assertions. Native suites ran
concurrently in the same guest; durations are not performance measurements. The
five earlier failed test-unit records were reset only after independent drain,
preserving their journals. Detailed boundaries/results are in the
[journal](journal/2026-10-26-first-prototype.md#native-execution-boundary--27-october-2026).

The short-lived [setup helper](../../src/launcher/main.c) now connects private
code/data views to the original durable launch operation. It cannot execute app
code. Root journals setup before dispatch, pins the actual bus ProcessFD and
systemd invocation, and records preparation separately from target readiness.
Live withdrawal is denied. After held process death and independent same-invocation
unit/cgroup/job drain, Root may release this no-exec attempt; User completes from
the authenticated terminal receipt. Broker replacement and exact terminal retries
are supported. Root restart uncertainty still holds the obligation.

An early failed helper has a separate supervisor-only recovery proof, with no
invented peer identity. Its fixed protected no-exec image, exact unit/argv/context,
exit evidence and independent drain are checked. A confined offline owner action
also supports this proof without beginning a new catalogue runtime. It was used
to recover the first failed attempt before updating the old authority. No SQL
patching or repeat of a failed setup dispatch was used.

The actual enforcing VM setup-check passed preparation, independent reconstruction
of the native context digest, live withdrawal denial, broker replacement, observed
drain, byte-identical terminal retries and removal of only the known empty scaffold.
Private data, owner records and operation histories remain. Setup-only READY is
not app readiness. The helper has a separate setup_t domain and two setup
capabilities; no app can inherit them because this helper has no exec path.
Private Setup1 1.0 and Development1 1.3 add no public capability or SQL version.
See [runtime contract](../../spec/runtime.md#supervised-no-exec-setup).

Complete native suites pass 31/31 Debug ASan/UBSan (154.94 s) and Release
(118.88 s). Final bounds changes pass focused Debug/Release repeats (0.07/0.02 s).
Host migration/wire tests pass 29/29 (50.747 s), orchestration 30/30 (0.235 s),
runtime vectors 14/14 and isolated constraints 374 assertions. A final enforcing
setup-check and endpoint/app-denial check pass after the updated authority install.
These durations are test evidence, not OS performance measurements.
Copied-identity/same-domain wrong-unit broker checks, departure FD cleanup,
User replacement and Root-only restart pass with the new private methods. Normal
restart preserves terminal launch history; this final check was not a guest reboot.
Owned authority/probe/setup units are stopped, with SELinux still Enforcing.

Native [linux/view.h](../../src/linux/view.h) now creates private mount namespaces,
clones exact admitted code/data directory handles and attaches them to checked
empty setup slots. Code is readonly; data is writable/noexec; both are nodev,
nosuid and nosymfollow. Existing restrictions are never removed. Move attempts
with uncertain results cannot be repeated. Process/namespace identity and owned
CLOEXEC handles are checked; closing them is not a claim of mount/app drain.

The original standalone view tests use administrative unconfined_t in the existing
enforcing guest. The new setup integration above separately exercises setup_t.
Those standalone tests prove kernel mount boundaries,
including readonly writes, actual exec and executable mmap refusal, device-open
denial, omitted nested mounts, source/target replacement checks and no propagation
to an independent parent while views are live. The earlier full native baseline was 30/30
Debug ASan/UBSan (113.06 s) and Release (101.14 s); final expanded mount cases
also pass focused Debug/Release repeats. Host orchestration 28/28 and isolated
constraints 374 assertions pass. Existing installed authority launch-history and
endpoint/app-denial checks still pass; their service/probe units are stopped.
See [runtime details](../../spec/runtime.md#native-private-codedata-views).

The user writer now commits canonical launch intent, local operation, requested
run/proposed code lease and immutable request fingerprint before system RPC.
After independently rechecking the exact private allocation/MAC and current root
snapshot it may open the metadata data gate. This is admission, not prepared
storage/code views or readiness. Acknowledgement binds the actual root plan/op/
intent returned by the authenticated system receipt API; readiness stays unknown.

User runtime replacement marks unsettled runs recovery_required, closes open data
gates and retains original remote obligations. A current eligible replacement
broker may explicitly withdraw its authority/account's old still-reserved tuple;
the original request is neither resent nor rebound. Proven root withdrawal with
the exact local operation as cause completes the user release journal cancelled,
with start_failed/unavailable run and completed remote link. Root uncertainty,
readiness, invocation, grants or additional setup effects block this narrow path.
Historical retries preserve IDs, revisions and completion timestamps.

The earlier metadata-only slice added Catalogue1 1.1 (actual reservation receipts)
and Development1 1.2 (protected fixed-fixture prepare/cancel). Original Runtime1 XML and all SQL
versions remain unchanged. Public capabilities stay empty; public launch and
DispatchLaunch remain unsupported. No dependency, policy permission or resident
process was added by that slice. See [runtime contract](../../spec/runtime.md#durable-user-launch-acceptance-and-withdrawal).

Actual cap-zero/NNP enforcing prepare, user-service replacement, held original
root lease, explicit withdrawal and byte-identical terminal retry pass. Independent
Python reconstruction agrees with both native intent digests. Normal endpoint,
same-UID/same-domain wrong-unit denials and historical data enrollment checks pass.
Data/code view preparation, readiness, target dispatch/exec and real drain remain
next; no app unit was created by these tests. Detailed executed results follow in
the [session journal](journal/2026-10-26-first-prototype.md#durable-user-launch-journal--27-october-2026).

Launch-journal baseline evidence: 29/29 Debug ASan/UBSan (134.29 s), then expanded admission
faults in focused Debug (16.45 s); complete Release 29/29 (99.29 s) includes those
cases. Final pre-RPC withdrawal eligibility and synthetic readiness/invocation/setup
guards pass focused Debug/Release repeats (20.05/19.16 s) and a distinct enforcing
launch-new-check, preserving prior attempt history. Host migration/wire 28/28
(48.137 s), orchestration 28/28, cache guards 7/7,
runtime vectors 14/14 and isolated constraints 374 assertions pass. After graceful
QEMU exit 0 and verified PID absence, offline boot of the same disk reports
cross_boot_reopen=true with unchanged terminal launch business history. Normal
endpoints/app denials, stale-profile rejection, original sealed publication/
activation, app data and retained storage pass after that boot. Owned units are
stopped; the dedicated VM and all owned fixtures remain available for further work.

The original P0 app now has a fresh private DataEpoch/StorageRef, separate from
the retained storage test fixture. Root-preallocated carrier infrastructure is
bound to an independently observed immutable allocation witness before native
publication. The native user writer inserts data, exact attachment and a closed
gate in one SQLite transaction. Failed/uncertain commits preserve one whole result
or no result. Exact live retry changes nothing, including a later gate state;
detached/retained/purged/conflicting rows fail rather than being restored.
The protected original catalogue baseline also prevents recreation after pruning
and revision advancement. This is explicit trusted P0 enrollment, not a dynamic
allocator, allocation operation journal or live system admission. See
[runtime contract](../../spec/runtime.md#initial-private-data-enrollment).

Current data evidence: 26/26 native cases pass Debug ASan/UBSan (83.90 s) and
Release (76.33 s), then final expanded data cases pass focused repeats
(4.13/3.02 s). Host migration/wire tests 28/28 (36.901 s), orchestration 26/26,
cache guards 7/7, frozen vectors 14/14 and isolated constraints 374 assertions pass.
Actual enforcing cap-zero/NNP native enrollment and retry pass. The same-UID app
cannot open the new owner, payload, allocation record or configuration. QEMU exits
0 after orderly shutdown and its PID is verified absent before offline boot of
the same disk. data-check then reports cross_boot_reopen=true, preserving the
data identity, closed gate and old storage fixture. No app dispatched; general
admission, allocation recovery and removal remain work.

The user service now obtains one bounded, typed observation of an eligible active
app from the authenticated system service. Its own SQLite writer stores an exact
installation binding; it never reads the system database. Repeats change no rows,
and changed identities/revisions or revoked bindings require reconciliation.
The protected development trigger selects only the original trusted fixture.
Root chooses a short-lived cap-zero user-domain helper; held process identity,
UID, SELinux context, active unit, invocation, cgroup and boot are checked before
the operation and rechecked before the local write. No new resident service.
The binding is a last authenticated observation, never launch or file authority.
No data epoch, attachment, grant, lease or gate is created/opened by this step.
See [runtime contract](../../spec/runtime.md#authenticated-installation-projection)
for implementation, test scope and the remaining admission boundary.

Projection evidence: complete native suites pass 25/25 Debug ASan/UBSan (99.02 s)
and Release (72.47 s). After the final protected-controller integration, the six
projection/codec/config/peer/broker/process cases pass Debug again (6.21 s).
Host migration/wire tests pass 28/28 (40.352 s), orchestration/cache guards 32/32,
runtime vectors 14/14 and isolated constraints 374 assertions. Actual offline
enforcing checks pass with real separate writers, unchanged retry hashes, no
data/gate effects and denied ordinary/same-domain wrong-unit callers. Broker
replacement and root-only restart pass; projection observes the changed system
runtime. Graceful QEMU exit 0 and verified PID absence precede an offline boot of
the same disk. Projection then reports cross_boot_reopen=true, followed by normal
endpoint/app denials, sealed-store/activation and retained-allocation checks.
Owned service/probe units are stopped. No schema or dependency pin changed.

[Payload staging](../../src/core/import.h) now validates an independently expected
ArtifactId and complete trusted synthetic source inventory, then creates exclusive
copies in an empty private root. It rejects unexpected entries, links/nonregular
objects, mount transitions, wrong owner/mode and visible unsupported metadata.
Copied content is hashed after writer FDs close; files/child directories become
read-only while the root stays 0700. Errors preserve partial state. A separate
recheck repeats every required file/directory flush after uncertain results.
Native tests cover actual copies, corruption/IO faults, retained state, maximum
file/byte/depth bounds and real same-inode file/directory binds/device rejection.
The core tests run unconfined; privileged mount probes use a private namespace.
The protected system-domain administrative path now also calls the stager with
NoNewPrivileges and empty capabilities. Root-provisioned carrier infrastructure
supplies an immutable expected manifest/configuration and preallocated empty slot;
this is not a production dynamic allocator or untrusted package enrollment.

The native catalogue writes the fixed canonical P0 install intent, scope read,
three effects, operation/step and preparing component/artifact/install/generation
metadata atomically. It commits the configuration/object-witness binding before
payload IO. Exact retry retains IDs/revisions; failed/uncertain work remains live.
READY means an applied stage with operation running/publish, artifact staged,
install preparing and generation staged. Nothing is published, active or completed.

The separate [publication path](../../src/core/publish.h) now moves the original
payload into a preallocated manifest/payload container and publishes that container
under ArtifactId using same-mount RENAME_NOREPLACE. Immutable AETHSTO1 configuration
binds the original stage hash, store anchor and container witnesses to journal
step 1 before effects. Retries revalidate identity/content and repeat every flush;
unknown targets and invalid namespace combinations are preserved and rejected.
Final files/directories receive separate read-only MAC types. Actual enforcing
cap-zero system units publish and replay without changing original root inodes,
file metadata or operation revision. Owner chmod and executable mmap are denied.

Publication commits artifact verified/generation committed atomically, while
install stays preparing, activated_once=0 and operation running/reconcile with
activation_pending and no result/outcome/lease. Public capabilities are unchanged.
General dynamic allocation, volume/publisher trust, updates/admission, launch,
document transport and complete recovery coordination remain pending.

The separate first-activation path now verifies the already published object and
protected AETHACT1 (publication hash, install/generation/closed-gate revisions and
administrative stamp). One native SQLite transaction adds applied step 2, activates
the original generation and completes the original install operation with InstallId.
It requires current boot/runtime, exact scope and revisions, a cause-free closed
gate and no conflicting leases/dependencies/dispatches. The gate stays closed;
no app, data, grant or launch obligation is created. No new policy permission,
dependency, schema/wire version or public capability.

Completed retry returns the original history without changing current generation,
gates or removal state. installation_active separately describes the current
catalogue pointer. Receipt lookup is not code validation or launch authorization;
first activation verifies the actual store, and fixture-store-check remains its
current-object check. General update/rollback, receipt expiry and admission are pending.

Actual native ELF stage/retry and cross-boot reopen pass. A same-filesystem copied
source with identical bytes fails native ESTALE without changing the catalogue;
its isolated probe is retired while the real source/stage remain. Enforcing AVCs
confirm executable-map denial for both source and stage types, including the
system role. Existing endpoint/app denial checks and private allocation reopening
pass after the boot. Owned services/probes are stopped; both tracked fixtures remain.
The staged payload has since moved into the sealed store through its original
operation. After graceful shutdown/QEMU exit 0, verified PID absence and offline
boot of the same disk, fixture-store-check reports cross_boot_reopen=true. Exact
labels, original root objects, denied final mutations and unchanged publication
revision pass again, followed by normal endpoint/app-denial and private storage
checks. Use fixture-store-check now; the old stage path no longer exists.

Native suites pass 23/23 Debug ASan/UBSan (72.63 s) and 23/23 Release (65.20 s),
including publication rename/fsync faults, process exits, separate-process replay,
unknown/copied roots, metadata conflicts and actual same-inode bind rejection.
The extended journal tests inject before/after COMMIT failure at every publication
phase and retain immutable evidence/IDs without activating. Internal configuration
decoders cover byte bounds and missing/malformed object witnesses. Actual final
owner-chmod denial probes were then built and exercised in the installed service.
The strengthened copied-final-container case passes focused publication/mount
repeats in Debug (12.00 s) and Release (10.77 s).
Subsequent activation tests add before/after statement/COMMIT IO faults and process
exit, stale revisions, open/closing/owned gates, missing runtime, live holders and
completed receipts after a newer generation/removal/reclamation or later gate
opening. Focused journal/config Debug/Release repeats pass (18.80/15.57 s).
The independent Python encoder supplies the exact AETHACT1 digest. Config decoding
covers lengths/ranges and unchanged output. Actual enforcing cap-zero
first activation/retry preserves the published tree, retains the closed gate and
retains the same completion/catalogue bytes on retry. The real fixture remains
active with its gate closed; future lifecycle states are exercised only in tests.
After graceful VM shutdown (QEMU exit 0, verified PID absence) and offline boot of
the same disk, fixture-activation-check reports cross_boot_reopen=true with its
original receipt and no dispatch. Store verification, normal endpoint/app-denial
and retained private-allocation checks pass again. All owned service/probe units
are stopped; the activated fixture and its closed gate remain.
Thirty-one host guards pass (24 orchestration and seven cache/input checks);
27 migration/wire tests pass (31.065 s); 14 runtime-vector tests pass. The isolated host validator passes
374 assertions on SQLite 3.53.1. Native guest SQLite remains pinned at 3.51.2.
SQL/XML and frozen byte contracts and SDK
pins are unchanged. New internal AETHFIC1 configuration, policy types and owned
administrative binaries are documented in [runtime contracts](../../spec/runtime.md).
Public methods/capabilities remain discovery-only. Updates, general recovery
coordination and app/run admission remain next.

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

Next: admission, app/run principal
mapping, protected read refs, launcher/target-exec and remaining recovery/policy.
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
