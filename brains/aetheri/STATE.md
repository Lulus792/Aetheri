# State

7 November 2026 — first diskless UEFI/systemd boot proof; laptop image remains work.

## Coordination

**NexaUI: handoff delivered; catalogue maintenance only; no pending chat message.**
New needs go into the [existing catalogue](D:/NexaUI/NEXT_AETHERI_UI.md).
Carry this status forward after compaction; follow the
[maintenance rule](knowledge/nexaui.md#maintenance-rule--confirmed-5-november-2026).
NexaUI SDK implementation has its own owner; resume the current Aetheri task below.

Preserve the quarantined old runtime. The UI milestones below do not
resolve its provenance or lifecycle obligations.

## Own initramfs boot proof — 7 November 2026

The first original initramfs now boots with the independently pinned existing
Linux kernel under UEFI in a separate diskless QEMU. systemd is PID1 and starts
the native C17 diagnostic, which verifies its original nonce/profile, kernel,
UEFI, RAM root, API mounts and cgroup-v2 service context. It emits PASS and the
image-only unit powers the VM off. This is direct firmware-assisted kernel/
initramfs boot, not a standalone USB/disk artifact or a desktop/session test.
Read the [contract](../../spec/boot-probe.md) and
[record](journal/2026-11-07-boot-probe.md).

The ordinary-user builder verifies 38 pinned payload/licence files and produces
byte-identical boot inputs in two new directories on the same toolchain. Actual
UEFI cold boot passes twice with distinct kernel boot IDs; an actual mismatched
nonce rejects. Required dlopen mount closure and systemd executor are explicit;
initial failed attempts remain retained. Five independent host archive/
no-overwrite tests pass. Original QEMU/firmware/DLL hashes are verified, each run
has private NVRAM, no system/data disks, networking or QMP. No carrier disk,
normal authority/session, policy, old quarantine or foreign work is changed.
Final LF-normalized source binds five inputs and 106 verified evidence members;
both builds reproduce the exact already cold-booted kernel/initramfs/cmdline bytes.
The final read-only audit preserves 445 normal source inputs, 31 frozen SQL/XML,
19 quarantine hashes, 52 normal P0 files and the original closed DRM history.

Next produce a standalone removable-media UEFI artifact and prove its normal
firmware boot path, then system root and session integration. Production security,
updates/rollback, full apps, installer, laptop hardware/suspend and the complete
resettable experiment desktop remain required. The full goal stays active.

## Native Editor original background saving — 7 November 2026

Named and first saves now run in one original on-demand C17 worker. It owns the
copied text and file adapter while the UI keeps its live document editable.
Actual success acknowledges only the captured history state; newer edits stay
dirty and adjacent typing is not split by completion timing. Undo can reach the
saved state. Original close/review correlations survive tab and input changes;
Cancel never pretends to cancel a started disk write. UNKNOWN retains its file/
name/close fence even with clean text. Read the [contract](../../spec/graphical-editor.md)
and [executed record](journal/2026-11-07-editor-save-work.md).

Both profiles pass all ten editor groups, native window workflows, fourteen
clipboard cases and nine controlled native save attempts. The save fixture
pauses real fsync on the worker while actual UI input continues, then verifies
saved bytes, newer dirty/Undo state, close cancellation, conflicts and uncertainty.
The full-matrix 375 evidence files are independently verified before original private
namespace retirement. Final snapshot-body mutation rejection also passes all ten
groups and nine native saves in both profiles, with 279 separately verified files.
Native saving-state images are reviewed. ThreadSanitizer
could not link the missing runtime; no TSan result is claimed. The UNKNOWN test
actor is separately administratively retired after join, not normally closed.

Next prioritize the own-image boot path toward the requested laptop alpha.
File loading and folder enumeration still need moving off the UI thread; those
paths remain synchronous. Full accessibility/IME, search/chrome/reflow/final design,
Save As/export/abnormal recovery and normal app/session admission remain work.
Normal sources/builds/authority/quarantine and foreign work are preserved. No new
NexaUI requirement/handoff or dependency family/version is introduced. The full
OS/apps/image/installer/hardware/experiment objective remains active/incomplete.

## Prior native Editor asynchronous clipboard checkpoint — 7 November 2026

Ctrl+V now uses the existing SDK's bounded nonblocking Wayland read. Original
document/input/caret/selection/focus correlations are checked after queued native
events. Editing, focus/tab/file/review changes cancel the exact request; empty or
failed transfers never delete a selection. Repeated explicit Paste owns a new
reader. Cut keeps the document open on a failed model deletion. No new thread,
service, dependency version or installed capability is added. Read the
[contract](../../spec/graphical-editor.md) and
[executed record](journal/2026-11-07-editor-clipboard.md).

All 14 independent native clipboard cases pass Release/ASan, including actual
Unicode bytes, undo/redo, native copy receiver/cut/self-paste, no-op EOF, repeated
request, cursor/F6/Escape/tab/picker/type/focus cancellation, live partial timeout
and UTF-8/NUL/line-limit failures. Timeout is observed at about 5 seconds and idle
uses zero CPU ticks/no redraw. Both older graphical workflows and all nine editor
groups still pass (3.010 s/18.319 s). Native receiving-status captures are reviewed;
missing emoji glyphs remain a font-coverage limitation, not lost source bytes.
All 331 main, 197 initial visual and 259 final-reviewed evidence files are
host-verified before exact private namespace retirement. Final native window/
clipboard suites pass both profiles after an explicit progress/redraw loop guard;
no tiny-window runtime acceptance is inferred from that inspection correction.
Normal sources/builds/authority/quarantine remain unchanged.

Next implement responsive original file I/O; it remains synchronous. Full native
editable accessibility/IME, search, chrome/reflow/final design, Save As/export and
abnormal/UNKNOWN recovery remain work, followed by normal app/session integration.
The full OS/apps/image/installer/hardware/experiment objective remains incomplete.
NexaUI handoff stays closed; no new requirement catalogue delta is needed here.

## Prior native graphical Editor window checkpoint — 7 November 2026

The opt-in `aetheri-text-editor` now renders real direct input, document tabs,
workspace/gutter/selection, held-file picker and document-first close review.
Native Save/Discard/Cancel, unnamed close-save, empty last-tab view and external
change conflicts are tested against actual files. The modal Terminal editor and
normal dock/admission paths are unchanged. Read the
[contract](../../spec/graphical-editor.md) and
[executed record](journal/2026-11-07-editor-window.md).

Both native workflows pass Release/ASan (8.493 s/22.157 s), as do all nine editor
groups without skips (3.053 s/18.832 s). Selected dark/light/contrast, DE200%, F6
and actual2x virtual-output cases have native captures; idle uses zero CPU ticks
and no redraw over1.2 seconds. All297 evidence files are host-verified before
exact original namespace retirement. Normal guest sources/builds/protected state
remain unchanged; no reboot or foreign work retirement occurs.

This is a functional opaque development view, not final design or installed
admission. Next integrate responsive file I/O and asynchronous clipboard/focus
cancellation; current synchronous paste is provisional. Full native editable
accessibility/IME, search, chrome/resize/reflow, Save As/export/UNKNOWN and abnormal
display-loss recovery remain open. SDK native close is sticky; latch prevents
reopened Cancel but repeated native-close/management acceptance needs W01 work.
Keep the full OS/apps/image/installer/hardware/experiment objective active.

## Prior graphical-editor document workspace checkpoint — 7 November 2026

`aetheri_editor_workspace` now joins eight original document tabs, direct input,
local file saving and correlated close review. Existing unchanged sources reuse
their original tab; first saves are create-only. Original document/input/review
tokens reject delayed picker results before writes. Cancel preserves content,
Discard never deletes a file and Save closes only after actual success. UNKNOWN
keeps its exact file/name context and blocks completion even when text is clean.
Read the [contract](../../spec/graphical-editor.md) and
[execution record](journal/2026-11-07-editor-workspace.md).

Both isolated native builds and all nine editor groups pass Release/ASan without
skips (3.565 s/18.531 s). All 186 evidence files are host-verified before original
private-namespace retirement. No normal source/build/app/authority is replaced.
There is still no graphical Editor window: next render actual tabs, picker and
approved close review, with responsive original I/O and accessibility. Full Save
As/export/unknown recovery and normal grants remain work. The full OS/apps/image/
installer/hardware/experiment goal remains active and incomplete.

## Prior direct graphical-editor input checkpoint — 7 November 2026

The new C17 `aetheri_direct` library provides non-modal input over our existing
document/history kernel: whole-grapheme selection/movement/deletion, direct text,
line-ending-aware newline, undo/redo and original-input paste tokens. Cancellation
preserves selection while invalidating a delayed receive; identities do not reuse
object addresses. No new dependency or process is added. The separate modal
Terminal editor is unchanged. Read the [contract](../../spec/graphical-editor.md)
and [executed record](journal/2026-11-07-direct-editor.md).

All eight editor groups pass in isolated Release and ASan/UBSan (3.023 s/19.728 s),
with actual UID1000/Enforcing execution and 160 hash-verified evidence files.
Normal sources/builds/history are untouched. There is no graphical Editor window
yet: next connect original document-tab/file lifetimes, approved close review and
native presentation/accessibility. The work disk below remains unattached while
the foreign NexaUI helper is live. Other OS work can progress independently; full
OS/apps/image/installer/hardware/experiment scope remains active and incomplete.

## Developer storage prepared; live attachment deferred — 7 November 2026

The separate 8 GiB developer image and original-object registration now exist on
the host. Future normal VM boots include that exact image; installer boots refuse
it. A separate stopped-CPU QEMU probe verifies actual cold attachment and block/
serial observations. Fourteen storage host tests, 52 VM-tools regressions and two
explicit enforcing guest loop cases pass. The loop workflow formats/mounts once,
builds/runs as UID1000 with private TMPDIR, preserves bytes on remount and rejects
wrong identities, unknown format state and the actual carrier root. Read the
[executed record](journal/2026-11-07-development-storage.md) and
[commands/boundary](../../spec/development-storage.md).

The actual fixed image is still unattached and unformatted. The current carrier
has a live foreign NexaUI helper, and q35 has no usable hotplug connection; do not
reboot/terminate that work. No current root capacity has been added yet. Next
storage step is a genuinely coordinated idle boundary, ordinary cold boot,
explicit guest enrollment and actual virtio/storage acceptance. Do not repeat
image creation, initialize an unknown format attempt or move old build/history
paths. Other OS work can proceed independently. Production lifecycle, bootable
image/installer, apps, hardware and experiment environment remain incomplete.

## Normal development promotion — 7 November 2026

Both fixed development builds and the shared source now select NexaUI 0.1.23.
All six owner groups pass in each profile (Release 32.020 s / ASan 61.608 s),
plus all 32 Linux controller tests. Three real PID1 temporary-profile probes and
foreign-carrier rejection pass. A new original virtual-DRM session exercises
Files/editor/save/reopen, Settings, Terminal split/tab commands and individual
retirement. All five roles exit 0; Root closes the exact session and historical
retries preserve both the older and current completed records.

Fresh starts journal `tmpfs-v1`: a separate bounded 1 GiB `/tmp`, actual backing
preflight and original-unit property checking. A pending source/build marker
blocks new starts during promotion. The final capture-heavy tests use a separate
private temporary artifact root and verified host export; normal binary/cache/
SDK checks are unchanged. Read the [executed record](journal/2026-11-07-development-promotion.md).
The first source/cache and Debug disk-full failures remain attributed; no failed
input was adopted by relaxing an owner guard. The normal desktop now uses the
new SDK; the private consumer checkpoint below remains historical evidence.

No development actor/session remains; tty1 and the free-seat baseline are restored.
The audit preserves 445 source inputs, 31 frozen SQL/XML, 19 quarantine hashes,
52 normal-P0 files/controller, old SDK/SDL inputs and the original closed history.
Rebuildable inactive compiler caches were reclaimed with an explicit source
allowlist and per-object hashes. Root space remains limited: provision dedicated
build/capture capacity before more broad carrier runs, preserving existing volume/
object identities and protected state. Production admission/recovery/removal,
bootable image/installer, hardware acceptance and the full experiment product
remain work. The full goal remains active; no new NexaUI handoff is pending.

## Prior private consumer checkpoint

NexaUI now selects the exact 0.1.23/97a4264 source pin. Both private SDK/app builds
pass; all 73 app groups have successful Release evidence (67 plus six corrected
owner checks), and the complete 73-group ASan/UBSan run passes without skips.
The final two peer-header adapters also pass actual native AT-SPI in both profiles;
all 27 Linux development-owner tests and two real CMake pin negatives pass.

The export contains 2,530 files / 49,513,350 bytes. Selected native images were reviewed;
final design/hardware/human assistive acceptance remain open. A quota-exhaustion
SIGBUS path is recorded under the catalogue's existing P04; private test tmpfs
avoids the exhausted shared quota, without claiming to fix SDK failure handling.
The final audit verifies 444 source inputs, 31 frozen SQL/XML, 19 old quarantine hashes,
52 normal files/controller, original CLOSED/old SDK/SDL04, local links and no live
upgrade actor. Foreign planning/design edits remain excluded.

Read the [executed record](journal/2026-11-07-nexaui-upgrade.md). Shared normal
source/builds were not promoted: private u23r/u23d evidence does not switch the
fixed development controller or an existing session. Retain old 0.1.18/CLOSED
inputs and the old executing quarantine. Next UI work is controlled source/build
promotion and native development-session acceptance with enough private temporary
space; avoid overwriting concurrent SDK-owner work. The full OS goal stays active
and incomplete. No new NexaUI handoff/chat message is pending.

## Current update

Root v11 now binds the original connection to its actual issuing main process,
systemd invocation/cgroup, pidfs identity and independently selected original
Root ELF. Enrollment precedes REQUEST; fresh effects and original cleanup
repeat actual admission. AETHUIS1 and historical getters cannot replace those
held/current observations. The unchanged AETHIMG1 codec now lives in core.
See the [contract](../../spec/unit-profiles.md#root-v11-original-issuer-identity)
and [execution record](journal/2026-11-07-issuer-identity-work.md).

Final source-v9 has 444 inputs. Both complete native builds and all five groups
pass Release and ASan/UBSan: 438.085s/469.514s, including all 32 supervised
drivers per profile. All eleven affected regression groups pass without skips:
394.448s/446.280s. Host contracts/runtime/issuer tests pass 55/25/3. The checked
export contains 541 files / 2,907,189 bytes. Earlier compile/STDOUT/cgroup/barrier
fixture failures remain attributed; a deterministic regression rejects the old
named-file publisher and passes anonymous, fully flushed create-only publication.

All 64 final driver configurations/images are retired. Three earlier terminal
administrative configurations received separate inspected operator cleanup;
their logs, original observations and both owner histories remain retained.
Resetting a failed test-unit marker is not native recovery or lease settlement.
No new test actor/unit/image remains. The audit preserves all 30 published SQL/XML,
19 old-quarantine hashes, 52 normal files/controller and unchanged CLOSED DRM,
SDK0.1.18/SDL04/vendor inputs. Parallel planning/UI edits are preserved/excluded.

Explicit quiesced Root10->11 migration backfills no process evidence and changes
no old rows/gates/runtime/revisions. Normal creation/installed owners remain v2;
open validates exact Root 2–11/User 2–4,7–8 without migrating. No wire/capability,
normal admission or production confined Root provisioning is added. Bus-loss/
owner recovery, volume/clone continuity, external holders and durable issuer/
custody retirement remain work; preserve the old executing quarantine.

UI maintenance: the user asked why the SDK remains 0.1.18. It is an explicit
tested consumer pin, not a decision to stay on an old toolkit. Review a controlled
upgrade using the newer standalone consumer evidence and Aetheri's original
Settings/Files/Terminal/desktop tests. No pin/default change is part of this
issuer checkpoint. The full OS/apps/image/installer/update/hardware/experiment
desktop goal stays active and incomplete; no repeated NexaUI handoff is needed.

## Prior connection-custody checkpoint

Root v10 now binds the original exclusive system-bus connection and ordered
per-node Subscribe/Ref/Start/Unref/Unsubscribe intent/acknowledgement history to
the witnessed allocation. Compiled Linux hooks require confirmed fresh intents
and repeat admission before sends. First quarantine propagates under callback
busy guards to the Root execution fence, including asynchronous Start/journal
failures; a different coordinator cannot bypass that local exclusion.
Start acknowledgement is only the authenticated job reply. AETHUBS1 origins are
connection metadata, never issuer process/supervisor/image/volume proof.
Complete sequences remain visible and immutable, with no unit/run/lease release.
See [the contract](../../spec/unit-profiles.md#root-v10-connection-custody-journal)
and [executed record](journal/2026-11-07-unit-custody-journal.md).

Final source-v5 has 430 inputs; both complete isolated core/IPC builds and the
32 native/11 catalogue cases pass Release and ASan/UBSan (330.573s/367.794s).
All nine affected regression groups also pass, without skips: 350.932s
Release and 396.772s ASan/UBSan. The checked export has 250 files /
907,882 bytes. The final audit verifies 430 source inputs, 29 unchanged
published SQL/XML, 19 protected quarantine hashes, all 52 normal files/controller,
exact CLOSED/SDK0.1.18/SDL04 and vendor drop-in. No own actor or unit/image from
190 recorded administrative fixtures remains; both owner histories/leases stay
retained. Three foreign planning files match the initial checkpoint; the parallel
UI-note update is preserved and excluded.
Host contracts pass55/55 and frozen runtime tests25/25. Prior candidate failures
and v3/v4 results remain attributed; v5 separately fixes quarantine propagation.
Unprivileged snapshot space ran out; 144,928,096 bytes of own inactive compiler
objects were reclaimed without removing sources/programs/libraries/histories.
V5 uses the existing administrative build boundary and explicit free-space guard;
disk size/reservation settings and foreign active work remain unchanged.

Explicit quiesced Root9->10 migration adds no old origins/history, gate/runtime/
revision changes or admission. Normal create/installed owners stayv2, User9/10
unsupported, published v1-v9 SQL/XML frozen. Keep the old executing quarantine.
Next: independently bound original issuer process/unit/image, connection-loss
reconciliation and durable custody retirement; then owner/volume continuity and
external/delegated holders. The full OS/apps/image/installer/update/hardware/
experiment-desktop goal stays active and incomplete; no new NexaUI handoff.

## Prior durable unit-allocation checkpoint

Root v9 now journals the separate unit allocation before filesystem effects.
Fragment recipe and original directory/parent placement are immutable. Fresh
confirmed preparation permits one native O_TMPFILE/create-only-link publication;
complete selection is witnessed once before executing REQUEST/ARM. The Root
adapter matches that record and original placement instead of relying on a
process-local selection alone. Canonical digests exclude FDs/padding and require
source/parent birth observations. See [unit profiles](../../spec/unit-profiles.md)
and the [executed record](journal/2026-11-07-unit-registry.md).

Preparing and witnessed unit obligations remain visible in recovery inventory.
Explicit quiesced Root v8->v9 migration adds no old rows, runtime/gate/revision
changes or admission; ordinary creation and installed owners stay v2. The old
executing quarantine is not opened/migrated/adopted. User v9 is unsupported and
frozen v1-v8 SQL/XML remain unchanged. Real production policy/volume/owner
admission, custody retirement and recovery remain separate work.

Both complete isolated core/IPC builds pass. The final 26 native cases, eight
catalogue cases and independent codec pass Release/ASan without skips (259.92s /
227.81s). Eight affected regressions also pass (286.79s / 328.02s), including
Python/native populated v9 validation. Host contracts pass 52/52; frozen runtime
tests 25/25. The export has 161 files / 443,115 bytes. Final audit checks 426
sources, 28 unchanged frozen SQL/XML, 19 quarantine hashes and all 52 normal files/
controller, exact CLOSED/SDK 0.1.18/SDL04 and vendor drop-in. No own actor or unit/
image from 102 exercised fixtures remains; histories/leases stay retained.
Three foreign files match the prior checkpoint; the current parallel UI-note
revision is preserved and excluded from this work.

Next: record and reconcile the original per-unit reference/subscription lifetime
without replay, then original owner/volume continuity and external/delegated
holder settlement. Never infer unit or lease retirement from a stored selection,
completed PID1 job, closed connection or an empty metadata projection. The full
OS/apps/image/installer/update/hardware/experiment-desktop goal stays active and
incomplete; no new NexaUI handoff.

## Prior Root REQUEST and full-custody checkpoint

The internal Root adapter now binds an unused exclusive unit owner, independently
selected source/graph/service profile and original held/named starter ELF to the
original AETHEXE1 before REQUEST. Native read-only REQUEST-state checking does
not grant replay; the coordinator still requires a confirmed fresh commit. Its
retained catalogue/runtime hold survives actual JobRemoved and unknown reference
releases. Original SYSTEM cleanup context plus independent end/holder admission
governs retirement even after source fencing. Abandon changes only local ownership.
The last compiled admission callback is followed by a new original tuple/gate
check, preventing a closed gate from leaving a usable stale decision. See the
[executed Root-custody record](journal/2026-11-07-root-unit.md).

Both complete isolated core/IPC builds pass; the final 20 native cases pass
Release and ASan/UBSan without skips. Four deliberately defective Root producers
are detected with positive controls and suppressed target submission. Host
contract/migration tests pass 50/50 and frozen runtime/codec tests 24/24. The new
source/build directories leave concurrent NexaUI sources/builds alone. This is
actual PID1 plus synthetic native owner journals and inert UID0 administrative
helpers, not a normal UID1000/private-view app launch or production end proof.
Original unresolved fixture journals remain retained. No installed input, frozen
schema/wire, SDK pin or UI default is changed.

All six affected native regression groups also pass without skips: Release
282.68s, ASan/UBSan 342.13s. The checked export contains 180 evidence files /
446,878 bytes. Final audit verifies 421 source inputs, 28 unchanged frozen SQL/XML
contracts, 19 protected quarantine hashes, all 52 normal-P0 files/controller and
four untouched foreign files. The exact CLOSED DRM record, SDK 0.1.18/SDL04 and
vendor drop-in remain unchanged. All own native actors, 94 recorded administrative
units and their image files have retired; their owner journals remain.

Next: durable independent unit/profile allocation and provisioning, original
owner/volume continuity and custody recovery, followed by remaining external/
delegated-holder settlement. Never replace the old quarantined executing intent
with a new app fixture or infer SQL/lease release from completed jobs/disconnect.
The full OS/apps/image/installer/update/hardware/experiment-desktop goal remains
active/incomplete; no new NexaUI handoff.

## Prior unit-input and connection-custody checkpoint

The new internal unit owner now moves an exclusive PID1 client connection,
retains its original pin and duplicates independently witnessed fragment/drop-in
inputs. Loaded objects, graph and bounded service/ExecStartEx behavior must match.
Subscribe/Ref/Unref/Unsubscribe are one-shot; unknown replies preserve original
obligations. Job completion keeps reference custody alive. Admitted original end/
holder evidence is required for retirement; abandon closes without queue flush,
process control or SQL/lease settlement. See the
[executed unit-custody record](journal/2026-11-07-unit-custody.md).

Both complete builds pass. All 24 actual-PID1 cases plus the existing inert-target
case pass without skips in Release (6.96s) and ASan/UBSan (8.79s). Administrative
true starts, exact source/loaded graph checks and real references are exercised;
lost returns are controlled. Four separately compiled defective producers fail
the intended unchanged checks with original positive controls. Their deliberate
target submission is suppressed; owned administrative files are retired. Earlier
variant-framing and implicit journald-edge failures remain attributed and retained.
The vendor drop-in is pinned and unchanged; no dependency check is relaxed.

Final audit verifies 417 source inputs, unchanged 28 frozen SQL/XML contracts,
19 protected old-runtime hashes, all 52 normal-P0 files/controller and four foreign
files. Exact CLOSED DRM/SDK 0.1.18/SDL04 and the vendor drop-in are preserved.
No owned unit files, private broker or native test actor remains. Normal services
stay stopped with the previous authority binary; no update/restart is claimed.

277 verified exports are in `local/unit-custody-2026-11-07`. No installed profile,
policy, capability, schema/wire, SDK pin or UI default is changed. The old lost-
handle execution quarantine remains separate. This bounded P0 family is not a
normal app launch or production end/holder/volume admission.

Next: register independently protected unit profiles against the original
AETHEXE1 and join the Root coordinator to this entire custody lifetime, rather
than its older job-only hold. Then continue remaining owner/volume and external/
delegated-holder recovery. Do not lend this connection to independent legacy
Ref/Subscribe/Start owners or create an executing fixture to bypass old quarantine.
The complete OS/apps/image/installer/update/hardware/experiment-desktop goal stays
active/incomplete; no NexaUI handoff.

## Prior supervisor request checkpoint

The internal Linux start observer and Root request adapter now submit exactly
one StartUnit/fail after the existing coordinator's fresh REQUEST commit. Original
PID1 reply/signal credentials and exact job path join bounded early JobRemoved
events; a same-unit job never recovers a lost reply. A cleanup hold keeps the
original catalogue/runtime alive while request/job state is unsettled, including
after source fencing. Known job completion retires watches/local hold only;
quarantine remains pending, and explicit abandon has no cancellation/SQL/lease
effect. A final local latch blocks Manager loss dispatched by the last compiled
admission callback. See the [executed supervisor record](journal/2026-11-07-supervisor-start.md).

Both complete builds pass. All 19 final private-bus/native-journal cases plus an
actual PID1 process-free target case pass without skips in Release (74.60s) and
ASan/UBSan (74.83s). The private cases control PID1 role/unit admission; the
separate target case verifies actual credentials, no dependencies, original job
result and exact owned cleanup. No new executing app fixture is created.
Four deliberately defective producers are rejected without changing originals.
Three existing peer/coordinator/fence regressions pass 92.89s/102.12s on unchanged
inputs. Earlier positive revisions and one strict-compiler indentation failure
remain separately attributed; no weakened deadlines, flags or policy.

Final audit preserves all 52 normal-P0 files/controller, 19 protected old-runtime
hashes, 28 frozen SQL/XML contracts, four foreign files, exact CLOSED DRM and
SDK 0.1.18/SDL04. All own actors and test target files are gone; the VM is running.
Normal services stay stopped with their previous authority binary; no installed
profile/API/policy/capability or UI default changes. Sixty-five evidence exports
are in `local/supervisor-start-2026-11-07`; Git records publication.

Next: independently admitted immutable unit/dependency configuration and
reference/subscription custody for the live supervisor integration; then remaining
old/intervening owner, external/delegated holder and owner/volume continuity with
durable per-run recovery. A queued or completed PID1 job is never target exec,
REGISTER/READY or executing drain/lease release. Preserve old v6/User-v4 quarantine.
The complete OS/apps/image/installer/update/hardware/experiment-desktop goal stays
active/incomplete; no NexaUI handoff.

## Prior no-exec reply checkpoint

Installed Setup1 methods now hold a current retained SYSTEM cleanup frame,
rejecting reentrant use/clear and catalogue close/runtime replacement. Prepare,
Register and Prepared success replies source-check after native work and again
after construction before sending. Entered sends are handled even when unknown,
preventing an automatic replacement error. Original journals/helper/unit pins
remain; admitted observation and independently proven no-exec withdrawal still
work while fenced. No SQL/XML/policy/dependency/public capability changes. See the
[executed setup-fence record](journal/2026-11-07-setup-fence.md).

Both complete builds pass. All 22 final private-bus/process-pin/native journal
cases pass without skips in Release (94.95s) and ASan/UBSan (102.78s). Actor labels,
broker/PID1 context and drain admission remain controlled, not live MAC/view
acceptance. Three final deliberately defective producers are rejected without
changing original artifacts. Four affected regressions pass 93.47s/98.97s.
Initial 19-case results and the selector/timeout-expectation fixture failures are
retained; final checks use exact Error.Timeout/-ETIMEDOUT without widening timeouts.
The existing private broker replaces an unavailable dbus-run-session wrapper.

Normal P0's reviewed seven-stage update/check/stop sequence passes enforcing,
including actual connection negatives, User replacement, Root restart with User
retained and idle. Both services are stopped. All 52 files are compared, preserving
owner/epochs, lock inodes, policy and every non-runtime owner table. Authority SHA:
`dd111cfb9fdd632fed3b86c22df22ec31e946fcf22236ea283171e85a94c2ae9`.

Final audit: 407 source inputs, unchanged 28 frozen contracts/19 protected old-
runtime hashes/four foreign files/exact CLOSED DRM/SDK 0.1.18/SDL04; no own actor,
Aetheri service, protected sidecar or seat0 session. Sixty verified exports are in
`local/setup-fence-2026-11-07`; Git records publication. Unchanged migration/codec
campaigns are not represented as rerun by this transport-only step.

Next: actual supervisor activation and its original pending request/job duties,
then old/intervening owners, external/delegated holders and admitted owner/volume
continuity with durable per-run recovery. The local latch and handled reply are
not global drain, general replay/recovery or executing-start/removal permission.
Preserve old v6/User-v4 quarantine. The full OS/apps/image/installer/update/
hardware/experiment-desktop goal remains active/incomplete; no NexaUI handoff.

## Prior fresh-admission checkpoint

The retained SYSTEM source latch now rejects fresh opening of install
reservation admission, new canonical reservation/lease/dispatch and the first
no-exec setup intent. Checks run after source observations and before mutation.
Exact historical acknowledgments, admitted existing-helper observations and
proven no-exec withdrawal remain available, preserving original IDs/times/state.
They confer no new supervisor/start/helper-reply permission. No schema/XML,
policy/dependency or public capability changes. See the
[executed admission record](journal/2026-11-07-admission-fence.md).

Both complete builds pass. Twelve new native metadata cases pass without skips
in Release (37.37s) and ASan/UBSan (35.10s), including late-observation and lost-
commit source fencing. Synthetic caller evidence is not live supervisor/drain
admission. Three isolated producers missing gate/late-reservation/setup checks
fail the intended unchanged tests; original artifacts stay unchanged. Six
affected regressions pass respectively 258.94s/248.42s, with complete case output.
All 50 contract/migration and 24 runtime-vector tests pass.

The normal P0 reviewed seven-stage update/check/stop sequence passes enforcing,
including connection negatives, User replacement, Root restart with User retained
and idle. Both services are stopped. All 52 files are compared, preserving
owner/epochs, writer-lock inodes, policy and all non-runtime owner tables.
Installed authority SHA-256:
`2b2ff6e6d6d7b1e65d2930db0c8eb79244e58c2f4b49e79c791813faa25a3c25`.

Final audit: 406 source inputs, unchanged 28 frozen contracts/19 protected old-
runtime hashes/four foreign files/exact CLOSED DRM/SDK 0.1.18/SDL04; no own actor,
Aetheri service, protected sidecar or seat0 session. Fifty verified exports are
in `local/admission-fence-2026-11-07`. Git records publication.

Next: no-exec helper reply transport and supervisor activation, preserving
original helper pins and admitted observation/drain cleanup. Current historical
metadata alone authorizes neither. Admitted owner/volume continuity, remaining
effect/old-owner/external/delegated sources and durable per-run recovery remain
open. Preserve the old v6/User-v4 quarantine. The whole OS/apps/image/installer/
update/hardware/experiment-desktop goal remains active/incomplete. No NexaUI handoff.

## Prior custodian-export checkpoint

The native original-observer handoff bridge now shares the retained SYSTEM
catalogue's local source latch. Current Root-custodian permits bind epoch/boot/
runtime without inventing a current User delegation. Raw export checks after
fresh PREPARE; fixed-slot transport checks before every notification. Partial
or unknown sets remain pending and cannot be filled/replayed. Existing admitted
receive/retirement use separate cleanup brackets allowed while fenced, preserving
all original exact-object/terminal/holder conditions. Local frames prevent
catalogue close/runtime replacement, never prove external drain or lease release.
No SQL/XML, policy/dependency or public capability change. See the
[executed handoff-fence record](journal/2026-11-07-handoff-fence.md).

Both complete builds pass. Fourteen new cases pass without skips in Release
(101.92s) and ASan/UBSan (183.38s); ten affected regression groups pass respectively
373.52s/413.08s. These new handoff cases use real SQLite/local descriptors with
controlled lifetime/PID1 adapters, not joined live app/supervisor acceptance.
Three deliberately defective producers are rejected at the intended post-commit,
per-send and cleanup boundaries, leaving original artifacts unchanged. All 50
contract/migration and 24 runtime-vector tests pass; full case output is retained.

Normal P0 is updated and checked enforcing through the reviewed seven-stage
controller sequence, including actual connection negatives, User replacement,
Root restart, idle and stop. All 52 files are compared; owner/epochs, writer-lock
inodes, policy and every non-runtime table remain unchanged. Services are stopped.
Installed authority SHA-256:
`bf69732c3af586858bf7fccea6732be66e21dfa7da3c454200ea966ff0509194`.

Final audit: 406 source inputs, unchanged 28 frozen contracts/19 protected old-
runtime hashes/four foreign files/exact CLOSED DRM/SDK 0.1.18/SDL04; no own actor,
Aetheri service, protected sidecar or seat0 session. Fifty verified evidence
exports are in `local/handoff-fence-2026-11-07`. Git records publication.

Next: admitted owner/volume continuity, remaining effect and old/intervening-owner
sources, supervisor activation/exported/delegated holders and durable per-run
recovery. The old v6/User-v4 lost-handle quarantine stays unchanged. No normal
executing launch, full recovery or removal is claimed; the complete OS/apps/
image/installer/update/hardware/experiment-desktop goal remains active/incomplete.
No repeated NexaUI handoff.

## Prior local execution-source checkpoint

The retained SYSTEM catalogue now owns an explicit one-way local execution latch
shared by borrowing coordinators and writer/release transport. Fresh native
stages and new post-commit permits reject once fenced. Entered brackets remain
separate obligations; close/runtime replacement reject while one is active.
Historical receipts/diagnostics stay available without replay. Root fences before
normal event-loop shutdown. No SQL/XML, policy/dependency or public capability
change. See the [source-fence record](journal/2026-11-07-execution-fence.md).

Both complete builds pass; all twelve affected groups pass without skips in
Release (390.50s) and ASan/UBSan (357.01s). Nine source and six private-bus fence
scenarios cover shared prior coordinators, fresh-stage/late-commit boundaries,
writer publication, entered sends and retained lease/intent/output state.
Explicit full-output repeats pass R 56.20s/SAN 60.48s and verify every new case;
the earlier CTest output truncation is retained. A separate pre-commit-permit
producer is rejected at the intended actual post-commit callback. Its original
production archive/test ELF remain unchanged. All 50 contract/migration and 24
runtime-vector tests pass. Initial compile/harness failures are documented.

Normal P0 is updated and checked enforcing, including connection negatives,
User replacement, Root restart, idle and stop. Root's actual shutdown logs a
local fenced source with zero brackets. All 52 files are compared; owner/epochs,
writer locks, non-runtime tables and policy are preserved. Both one-task services
have zero CPU/context-switch deltas in the 5.032s idle sample and are stopped.
Authority SHA-256:
`0cc2e74e6bec4c41e24bdb3d34c549a619219848f2d079a52a9c1f2a44c2072e`.

Final evidence exports: 53 files, 406 source inputs, unchanged 28 frozen contracts/
19 protected old-runtime hashes/four foreign files/exact CLOSED DRM/SDK 0.1.18/
SDL04. No own native/build actor, Aetheri service, protected sidecar or seat0
session; Enforcing/tty1. Evidence: `local/execution-fence-2026-11-07`.
Git history records publication.

Next: connect this current source barrier to admitted owner/volume continuity,
other effect families, old/intervening owners, supervisor/transport/exported and
delegated holders, plus durable per-run recovery. Local active==0 is never global
drain or permission to release. Preserve the old v6/User v4 quarantine; no new
live execution fixture bypasses it. Whole OS/apps/image/installer/update/hardware/
experiment-desktop work remains active/incomplete. No repeated NexaUI handoff.

## Prior reciprocal-connection checkpoint

Normal User now retains and repeatedly authenticates the selected current Root
connection, joining correlated sent-request replies, original ProcessFD/current
MainPID/unit/invocation/cgroup/boot and protected Root epoch/unit selection. Its
private Root RPCs use that unique destination. Discovery event bytes are not
interpreted as the Root runtime. Root also checks its own selected MainPID/unit
on protected context use. Live drift quarantines; actual departure retires only
identity. No automatic sent-call replay, app adoption, drain or lease release.
See the [connection record](journal/2026-11-07-system-connection.md).

An initial native release/reacquire case exposed queued ownership loss despite
final unique-name equality. Bounded on-demand dispatch closes that demonstrated
gap; the final test rejects a deliberately disabled-dispatch producer. Five
relevant groups pass without skips in Release (3.62s) and ASan/UBSan (3.88s);
both complete native builds pass. All 50 contract/migration and 24 runtime-vector
tests pass. Existing SQL/XML and normal v2 schemas stay unchanged.

The stopped normal bootstrap was updated through the reviewed controller, adding
one protected selected-Root slot and minimal reciprocal proc-metadata rights.
Root ELF access and ptrace have explicit denials. Actual enforcing discovery/
denials, changed-selection rejection, retained live quarantine, fresh User,
User replacement, Root restart with User retained and idle checks pass. All 52
normal files are compared with explicit program/policy/slot/runtime exceptions;
owner/epochs, stable lock inodes and every non-runtime table are preserved.
Services are stopped. Authority SHA-256:
`df418f839cfd3414b0643fc1602550ee2f5cc488050b7e52531de13aa8b1d495`.

Audit: 405 source inputs, 40 verified evidence exports, unchanged 28 frozen
contracts/19 protected old-runtime hashes/four foreign files, exact CLOSED DRM
history, SDK 0.1.18 and SDL04. No native/build actor, Aetheri service, protected
sidecar or seat0 session; Enforcing/tty1. Evidence:
`local/counterpart-2026-11-07`; Git history records publication.

Next: independently admitted volume/clone continuity, prior-owner/future-effect
fencing and complete holder/recovery coordination. The conservative private
connection is not a normal SDK or complete recovery-owner admission. Preserve
the old quarantined v6/User v4 runtime and all unresolved leases/operations.
Whole OS/apps/image/installer/update/hardware/experiment-desktop work remains
active and incomplete. No NexaUI pin change or repeated handoff is required.

## Prior authority-image checkpoint

Normal Root/User now select their independently provisioned role ELF before
catalogue access and recheck their current own program at context queries and
User delegation. Source replacement, including identical bytes in a different
object, cannot adopt a new program. Failures latch until process restart. The
self-only API also works for capability-empty UID/GID0; the remote peer UID0
rejection is unchanged. No new library, permission, capability or SQL/XML.
See the [image record](journal/2026-11-07-authority-image.md).

Complete Release/Debug builds pass; five affected groups each pass without skips
in Release (1.61s) and ASan/UBSan (3.32s). Ten native self-image cases/profile
exercise real UID/GID0 and 1000, repeated calls, invalid credentials/digests,
identical-content foreign objects, FD cleanup and actual EMFILE. An initial
private-home fixture failure is retained. Actual-domain shadow tests distinguish
the previous missing guard from the candidate's pre-catalogue ESTALE rejection;
their empty read-only registries cannot adopt normal catalogue state.

The stopped normal v2 bootstrap was updated through update-owned. Enforcing
discovery/denials, User replacement and Root restart with User retained passed.
The preceding inventory scan is now observed at actual Root/User startup.
Normal owner/epochs, stable lock inodes, non-runtime tables and policy hash stay
unchanged; all 51 files were compared with explicit update/runtime exceptions.
Both one-task services have zero CPU/context-switch deltas in the five-second
idle sample and are stopped. Selected authority SHA-256:
`329c9ff42b127b43c9b71564a953db7ecd874716d301264af13a54f28afa81a3`.
Final audit verifies 401 source inputs and 90 exported evidence files, unchanged
28 frozen contracts/19 protected old-runtime hashes/four foreign files, exact
CLOSED DRM history and SDK/SDL pins. No native actors, Aetheri services, protected
sidecars or seat0 session; Enforcing/tty1. All 24 runtime-vector tests also pass.
Evidence: `local/authority-image-2026-11-07`; Git history records publication.
No old isolated execution-fixture recovery or release follows.

Next: complete current-counterpart/continuity and holder/future-effect recovery
coordination. Own ELF checks are prerequisites, not remote actor admission or
global recovery. Preserve the old quarantine. The full OS/apps/image/installer/
update/hardware/experiment-desktop goal remains active and incomplete.

## Prior recovery-inventory checkpoint

Native `catalogue/recovery.h` now copies a bounded, typed owner snapshot in one
read transaction, including unresolved operations/steps, cause-bearing gates,
held leases/launches/grants/access and later lifecycle journals. Complete start
results do not hide a live code lease. Errors publish no partial result; the API
never migrates, replays, settles or adopts anything. Source startup scans after
begin_runtime and before protocol publication; no installed bootstrap or public
capability changes. See the
[inventory record](journal/2026-11-07-recovery-inventory.md).

All ten selected catalogue groups pass without skips in Release (617.43s) and
ASan/UBSan (723.70s); both complete native builds pass. The strengthened inventory
test separately passes Release (6.97s) and ASan/UBSan (11.62s), comparing 40
owner/schema/failure cases per profile with actual populated metadata. The 50
host contract/migration tests and 24 Linux runtime-vector tests also pass.
Expanded native lifecycle checks exposed a historical-retry test assumption;
the query correctly omitted terminal interruption metadata. A parallel process
group also reached its unchanged 240s limit. Originals are retained; the failed
campaign was explicitly stopped, then the corrected affected groups passed
serially under their original limits. This does not establish the cause of the
earlier timeout or a production recovery fix. Audit: 398 source inputs, 135
verified evidence exports, unchanged 19 protected hashes/28 frozen contracts/
SDK 0.1.18/SDL 04/exact CLOSED DRM history, no protected sidecars, build actors,
Aetheri services or seat0 session. The four foreign files are preserved. Evidence:
`local/recovery-inventory-2026-11-07`.

Next: connect independently admitted current-owner/continuity and holder/future-
effect recovery coordination; this inventory only identifies metadata to inspect.
Source startup integration is built, not installed/accepted in the normal
protected v2 bootstrap. Git history records publication. The old quarantine
must not be replayed/adopted/settled. The full OS/apps/image/installer/update/
hardware/experiment-desktop goal remains active and incomplete.

## Prior Terminal reading-selection checkpoint

Reading Shift moves now select whole original rows across viewport/history
boundaries, retaining/reversing/contracting a valid anchor. Ctrl+Shift+C prefers
explicit selection and otherwise copies the checked original current/pending
reading row without changing selection, cursor or viewport. Model export shares
the existing UTF-8/wide/combining/soft-wrap rules. Selection gesture history
survives clear: a range that disappears before presentation cannot silently
become another row copy. New requests require current original content; already
transferred clipboard bytes remain independent copies. No new SDK/dependency,
thread/process or installed capability.

All 32 affected groups pass each Release and ASan/UBSan without skips:
8 focused R 21.85s/SAN 31.06s plus 24 other R 148.47s/SAN 213.55s. Independent
libatspi/libwayland peers verify exact focus and seven actual clipboard transfers
per profile, including range contraction, a fresh history-edge anchor and exact
Unicode bytes. The actual preceding producer fails the bounded no-selection row
copy task. Its first peer-idle harness interruption and the corrected eviction
fixture are retained separately. See the
[executed record](journal/2026-11-07-terminal-reading-selection.md). This does not
claim complete screenreader range integration or a new full 83-group app suite.

Audit: 395 source inputs, 502 exports/216 verified native images, unchanged
nineteen protected hashes/28 frozen contracts/SDK 0.1.18/SDL 04/exact CLOSED DRM
history and no sidecars. Both eight-shell idle samples have zero frames/ticks
over 2s. Enforcing tty1; no own build/native/private-registry actor, Aetheri
service or seat0 session. Quarantined old runtime unchanged. Inspected captures
retain the selected Mono font's CJK missing-glyph limit despite exact text/copy;
broader font coverage remains work. Evidence:
`local/terminal-reading-selection-2026-11-07`; Git history records publication.

Next: inspect and continue the P0 executing recovery/holder-coordination boundary
in the current runtime contracts before normal session/image admission. Preserve
the old quarantine; do not replay/recover/adopt its lost-handle fixture. Remaining
accepted desktop/apps, font coverage, human/physical/final design and persistence
remain open. The whole OS/apps/image/installer/update/hardware/full experiment-
desktop goal stays active and incomplete. No repeated NexaUI handoff is needed.

## Prior keyboard reading checkpoint

The original Terminal now reads rows, pages, visible ends and complete retained
history by keyboard without sending navigation bytes to its shell. Escape leaves
reading; normal accepted input and new composition return to their original
owners. One deferred content-bound target composes batched keys and checks the
original pane/model, source/target, dimensions/screen/current focus and viewport
before same-frame focus/outline publication. Live output can move original rows
at the current offset or preserve their content as the history offset grows.
Changed/evicted content and unrelated/disabled/hidden projections cancel.

Final 32 affected groups pass each Release and ASan/UBSan without skips:
7 focused R 22.24s/SAN 30.60s plus 25 other R 150.07s/SAN 211.55s. Independent native
clients verify numbered original-shell output, both original split panes, exact
focus and unchanged PTY input; the same shell writes its proof afterward. A core
case exposed offset-only focus loss; the first content-top-only correction then
failed live row movement. The joined implementation passes both, preserving
unrelated-scroll refusal. Actual old-producer and live-offset negatives, earlier
positive profiles and the initial stopped archive-hash harness are retained.
See the [executed record](journal/2026-11-07-terminal-reading.md). This is not a
claim of a complete new 83-group app/UI suite.

Audit: 395 source inputs, 617 exports/228 verified native images, unchanged nineteen
protected hashes/28 frozen contracts/SDK 0.1.18/SDL 04/exact CLOSED DRM history and
no sidecars. Eight-shell idle samples have zero frames and 0/1 CPU ticks over 2s.
Enforcing tty1; no own build/native/private-registry actors, Aetheri service or
seat0 session. Quarantined old runtime unchanged. Local evidence is under
`local/terminal-reading-2026-11-07`; Git history records publication.

Next: selection/copy tasks tied to the reading row, then remaining accepted
desktop/apps and production lifecycle. Human/full screenreader, physical/final
design and persistence remain open. The full OS/apps/image/installer/update/
hardware/experiment-desktop goal stays active and incomplete. No SDK pin upgrade
or repeated NexaUI handoff is implied.

## Prior native pane checkpoint

The original multipane Terminal now supplies localized selected/other-pane
descriptions through the existing NexaUI 0.1.18 semantics. Its bounded optional
view description does not own a model or change process/window ownership.
Independent native clients read both panes, select their original shells, verify
same-name proof files in separate working directories and distinguish background
selection from refused inactive-window reading focus. Menu refusal, old-row
retirement across hide/restore, fresh changed text, DE/EN metadata, larger text
and high contrast are checked. An independently retained original D-Bus text
path must still return Defunct after the fresh view is published.

Thirty distinct affected groups pass both profiles across the recorded runs:
four focused groups R 15.58s/SAN 21.39s plus other 26 R 151.26s and SAN 25/26 in 243.68s;
the remaining multiplexer case then passes R 7.51s/SAN 12.60s after a proven test
readiness correction. Its original failure sampled the clipboard callback before
the queued SDL focus event. A real-log-prefix counterexample and native trace
support waiting for both observations from before peer mapping, preserving the
original no-extra-focus/closed-transfer assertions and deadlines. This is not a
claim that a complete new 81-group suite was run. A compiled no-description
producer is rejected by the independent native task; its inputs/ELF are retained.

Audit: 394 source inputs, 530 exports/198 verified native images, unchanged nineteen
protected hashes/28 frozen contracts/NexaUI 0.1.18/SDL 04/CLOSED DRM history and no
sidecars. Both eight-shell samples have zero idle frames and at most one CPU tick
over 2s. The Enforcing carrier is on tty1 with no own build/native/private-registry
actor, Aetheri service or seat0 session. The old quarantined runtime remains
unchanged. See the [executed record](journal/2026-11-07-terminal-pane-access.md)
and `local/terminal-pane-access-2026-11-07`; Git history records publication.

Next: keyboard navigation within explicit Terminal reading focus, then the
remaining accepted desktop/app work. Human/full screenreader, physical/final
design, persistent sessions and production lifecycle remain open. The complete
OS/apps/image/installer/update/hardware/experiment-desktop goal stays active and
incomplete. No new SDK requirement or repeated NexaUI handoff is implied.

## Prior native SDL wakeup checkpoint

The original Terminal query workload exposed a concrete SDL 3.4.20 queue/arm
race: a worker successfully queued an event before native wakeup was armed,
leaving the UI asleep until another event. A private one-shot native probe proves
the original failure and the corrected second queue check. This does not prove
that every historical Text/GetState/Close failure had the same cause.

The first full 03 UI profile then exposed a real 96-byte pending Wayland wake
callback leak in the ASan/UBSan Files/editor close path (79/80 groups). It is
preserved, not accepted or suppressed. The separate Linux-only backend candidate
uses one owned nonblocking, close-on-exec eventfd; device teardown closes it.
The complete 04 profile built with exact source/artifact checks. The permanent
controlled runner passes both queue variants, 16 lifetime cycles/512 events and
independently witnessed eventfd-creation failure with unchanged FD snapshots.
An initial fixture expectation of the private error text was corrected after
observing SDL's public `wayland not available`; no SDK source changed for that.

The ordinary uninstrumented 04 profile passes all80 app/UI groups in Release
(326.82s) and ASan/UBSan (544.24s), without skips, at
`terminal-wakeup-eventfd-final-3qienfs2`. The original Terminal's separate sixteen-
focus/four-client query workload passes both profiles (8.43/10.74s). All twelve
ordinary executable hashes remain unchanged after a late diagnostic-only guard
and runner docstring clarification; explicit trace targets compile separately.
The development controller and carrier builds select 04; original 02/03 profiles
remain intact. GNU patch is a signed/pinned private build tool, not installed or
linked into Aetheri. NexaUI
0.1.18/e71ee30 and all production lifecycle/admission boundaries remain unchanged.
See the [investigation record](journal/2026-11-06-sdl-wakeup.md).

Audit: 394 source inputs, 727 exports/216 verified native images, unchanged nineteen
protected hashes/28 frozen contracts/SDK/CLOSED DRM history and no sidecars. The
eight-shell samples have zero idle frames and one CPU tick over 2s. Enforcing tty1,
no remaining build/native/private-registry actor, Aetheri service or seat0 session.
The old quarantined runtime remains unchanged. Local evidence is under
`local/terminal-native-query-2026-11-06`. Git history records publication.

Next: complete original-session multipane assistive tasks, then continue the
accepted desktop/app work. General attribution of older untraced native failures,
human/physical/final design acceptance and production lifecycle remain open.

The full OS/apps/image/installer/update/hardware/experiment-desktop goal remains
active and incomplete. No repeated NexaUI list or chat handoff is needed.

## Prior native Terminal tab checkpoint

The NexaUI Terminal now publishes an actual required single-selection tab group
through the pinned 0.1.18 SDK. Independent standard AT-SPI selection switches the
original shell before its view is projected. Menus/closing reject pending requests;
disabled tabs retain required membership. An old native tab reference is refused
after a fresh process is running in its retired slot. Native clipboard producers
also prove pane/tab transfer cancellation inside the same focused window, then
literal paste into the selected pane without automatic execution.

Three focused groups and the other 25 affected Terminal/desktop-owner groups pass
each Release and ASan/UBSan without skips. The strengthened retained-tab task
passes both profiles after a test-only client/driver extension. An additional
existing read-only native check nevertheless fails at group acquisition in
`terminal-atspi-oxk6md_z`; its helper-free path does not enter the new tab code.
The cause remains open, and prior positives are not a fix. See the
[native tab record](journal/2026-11-06-terminal-native-tabs.md) for exact runs,
initial membership/flag corrections, the old-producer negative and retained failure.

Audit: 387 source inputs, 355 exports/156 native images, sixteen selected ELF
hashes/C17 commands, unchanged nineteen protected hashes/28 frozen contracts,
SDK 0.1.18 and exact CLOSED DRM history. Both eight-shell idle samples record
zero frames/ticks over two seconds. Enforcing tty1, no remaining build/native/
private-registry actors, Aetheri unit or seat0 session. The old quarantined runtime
remains unchanged. Local evidence: `local/terminal-native-tabs-2026-11-06`.

Next: instrument/reproduce the open native discovery/query failure without
unmodified retries, then complete multipane assistive tasks on original sessions.
Human/physical/final design acceptance and the full OS/apps/image/installer/update/
hardware/experiment-desktop goal remain incomplete. No new SDK/default/public
capability or handoff is implied.

## Prior original-session multiplex checkpoint

The own NexaUI Terminal now hosts up to eight independent original shells in
one existing window: tabs, split trees, keyboard/pointer focus, real PTY resize
and readable small-window fallback. Stable sessions retain their own models,
pins, history and input; one worker and a rotating shared16KiB read turn replace
no original owner. Named projections do not alias equal content tokens across
models. Focus transfer cancels old paste/preedit and rejects queued old commits.
Owner v2 additively binds the same-process PTY bootstrap/sources while preserving
historical v1 and Files' separate single-editor save/close rules. Both PTY openers
now share the original canonical baseline, including UTF-8 erase for actual
external cooked-mode programs. A real single-flag negative catches that defect.

Final `terminal-multiplex-final-canonical-6girjfpb` passes all79 app/UI groups
in Release318.29s and ASan/UBSan547.56s, without skips. The audit joins387 source
inputs and601 retained exports, including252 native images and16 selected
executable hashes. Ordinary and large-text/high-contrast/owner images were
inspected. Both eight-session idle samples have zero frames/ticks over 2s.
Protected19/frozen28, unchanged SDK0.1.18, no sidecars, historical closed DRM and
no remaining build/native/private-registry actors are checked. The Enforcing
carrier is idle on tty1 with no Aetheri unit/seat0 session; the old quarantined
runtime remains unchanged. See the [multiplex record](journal/2026-11-06-terminal-multiplex.md)
for original failures, canonical correction, fixture synchronization and actual
scope. The old untraced fresh-Close and native query/Close observations remain
open; later positives are not root-cause fixes. Git history records publication;
local/terminal-multiplex-2026-11-06 retains the exact audit/evidence inventory.

Next: native per-pane clipboard cancellation and complete tab semantics/assistive
tasks on original sessions, then the accepted desktop/app work. Persistent/
restorable sessions, final glass, normal admitted launch and physical acceptance
remain work. The full OS/apps/image/installer/update/hardware/experiment-desktop
goal remains active and incomplete. NexaUI handoff stays delivered; maintenance
only, no new chat message. Preserve the four independent project/design edits.

## Prior original-shell startup checkpoint

The original shell now emits the accepted ASCII startup mark and actual kernel
information in the NexaUI Terminal's normal profile. The report is ordinary PTY
output, with explicit unknowns, checked arithmetic and defanged kernel strings;
no helper process/network or fake release data. Six focused groups pass Release/
ASan/UBSan and three compiled counterexamples reject their intended faults.
The first whole-source run passes 75/75 Release groups, but its instrumented
run ends 74/75 on a native Text-interface query. Further original-producer
GetState/Close failures remain unresolved; diagnostic positives are not fixes.
The final suite with added failure diagnostics passes all 75 groups in Release
(316.68s) and ASan/UBSan (559.35s), without skips, at
`/home/aetheri/build/terminal-startup-final-diagnostic-hm7pr6dg`. This does not
resolve the intermittent original-producer native query/Close failures. See the
[startup record](journal/2026-11-06-terminal-startup.md). SDK/defaults/authority
boundaries and old quarantined runtime are unchanged; tabs/panes remain next.
The audit joins 378 source inputs and 246 exports, including 96 native images,
unchanged nineteen protected hashes/28 frozen SQL/XML contracts and no sidecars.
Both independent native samples record zero idle CPU ticks/frames over two
seconds. The enforcing carrier is idle on tty1 with no test actors; the historical
closed DRM record and old quarantined runtime remain unchanged. Ninja's stored
link dependencies are valid; transient consumed link.d files need no recreation.
The external UI design note gained account/recovery/Files visual approvals; preserve its
reviewed new bytes separately from the original foreign-file hash manifest.
An isolated post-thread PTY creation probe now runs two real original shells,
checks distinct controlling sessions/current images and continues one after
the other exits. It supports the next tabs/panes work, not a shipped launcher,
tab UI or production admission. See the startup record for exact scope.

## Prior compact Terminal controls checkpoint

Compact NexaUI Terminal chrome now provides left Close/Zoom controls, observed
maximize/restore, Ctrl+Shift+M and original-press cancellation. Minimize is
explicitly unavailable until the compositor implements hide/restore. All 74
app/UI groups pass Release (325.80s) and ASan/UBSan (543.66s), without skips, at
`/home/aetheri/build/terminal-chrome-final-_rtszmuz`. Separate label-cache and
press-owner counterexamples detect their intended regressions. See the
[chrome record](journal/2026-11-06-terminal-chrome.md), including the initial
fixture error, asynchronous negative detection and scoped visual acceptance.

The final audit joins 375 host/snapshot/guest inputs and 253 exported records,
including 96 native images, unchanged 28 frozen contracts/nineteen protected
hashes and no sidecars. Sixteen native peers per profile include observed Zoom/
Restore, unavailable Minimize and the two intended negatives. Native assistive
samples have zero idle CPU ticks/frames across two seconds; the full terminal
workflow records zero idle frames with 0.01s/0s CPU in Release/SAN respectively.
The counterexample's linker metadata was regenerated through the normal link;
the tested product stays byte-identical with unchanged object/library hashes.
The enforcing carrier is idle on tty1 without test actors, running Aetheri unit
or seat0 session. Closed DRM history, foreign files and old quarantined runtime
remain unchanged. No new DRM session, SDK/pin/default or authority change.

Next: implement the truthful accepted startup mark/system information, then
actual tabs/panes on original sessions. Final glass/geometry, minimize/fullscreen
integration, human/physical/normal-session acceptance and the earlier untraced
native-state query remain open. The full OS/apps/image/installer/update/hardware/
experiment-desktop goal remains active and incomplete.

## Prior original-content reading focus checkpoint

Original-content reading focus now works in the NexaUI Terminal: native row
focus and Ctrl+Shift+R, preserved identity when content moves and retired focus
when content changes. Model-size changes advance the checked ID range; returning
to an earlier size cannot revive a target. The visible outline leaves the original
model and PTY
input unchanged; preedit/review/closing disable reading targets. Native focus
requires an already active window. View/native probes and separately compiled
positional-ID and resize negatives supply scoped evidence. All 74 app/UI groups
pass Release (324.26s) and ASan/UBSan (530.46s), without skips, at
`/home/aetheri/build/terminal-focus-resize-final-fgjjjb29`.

Audit joins 375 host/snapshot/guest inputs and 292 evidence exports, including
90 native images, unchanged 28 frozen contracts/nineteen protected hashes and
no sidecars. Both native samples record zero CPU ticks/new frames across two
idle seconds. The enforcing carrier is idle on tty1 without build/private-
registry actors, running Aetheri service or seat0 session; exact closed DRM
history and foreign edits are unchanged. No new DRM run or SDK/default change.
Preserve original native getter/fixture failures in the
[focus record](journal/2026-11-06-terminal-focus.md). Direct wire diagnostics
and a bounded 128-query stream improve observation, but do not establish the
cause of older untraced replies. The old quarantined runtime remains untouched.

Next: advance the accepted Terminal chrome/startup/tab/pane design on the same
original session. Complete screenreader/native activation/text geometry/events,
IME/review semantics, font coverage and physical/final material acceptance remain
open. The full OS/apps/image/installer/update/hardware/experiment-desktop goal
remains active and incomplete.

## Prior native Terminal transport checkpoint

Native Terminal accessibility now uses the pinned SDK's public Linux host seam.
The original SDL window/input/PTY/worker remains; only duplicated readiness
handles enter that worker. Read-only visible rows, refused mutations, actual
focus loss/return, same-process private bus replacement and localized Close have
independent libatspi/private-registry evidence. Live wrong-focus and disabled-
bridge negatives are rejected. Native objects retire before view/window teardown.
The full app/UI suite passes all 74 groups in Release (321.31s) and ASan/UBSan
(507.07s), without skips, at
`/home/aetheri/build/terminal-access-adoption-y3qo6o3l`.

The earlier 73/74 clipboard failure, native reader/registry fixture failures,
untraced reconnect/ACTIVE failure and actual cleanup leak remain in the
[accessibility record](journal/2026-11-06-terminal-access.md). The driver now waits
for the matching native paste-review frame, closes its compositor control only
after retiring children, and permits reader rediscovery only for a defunct
frame within the original deadline. This is not a repair claim for the exact
untraced earlier wire reply. A final retired-object diagnostic no longer reads
optional object metadata; its independent native client is rechecked separately.

The final audit joins 375 host/snapshot/guest inputs and 270 evidence exports,
including 90 native images, with unchanged 28 frozen contracts/nineteen protected
hashes and no sidecars. Both new native samples show zero CPU ticks and new
frames across two idle seconds. The final native-client cases pass Release
(10.97s) and ASan/UBSan (12.01s) against unchanged application bytes. The enforcing
guest is idle on tty1 without build/private-registry actors, running Aetheri unit
or seat0 session; closed development history is unchanged. The existing NexaUI
catalogue received scoped transport evidence only. SDK 0.1.18/default SDL choices
and public capabilities remain unchanged.
Human/Orca reading, complete selection/caret/event/geometry, paste-review/preedit
semantics, final design/font coverage and normal/physical session acceptance
remain work. The full OS/apps/image/installer/update/hardware/experiment-desktop
goal stays active; preserve the old quarantined runtime.

## Prior Terminal development-owner checkpoint

The development owner now accepts `--terminal-ui nexaui-owned` for both the
normal shell window and Files' direct-editor window. Its separate frozen
Terminal profile binds original Root/User selection, source/executable/SDK/cache
and font/license inputs. Fresh Terminal or Files role starts reject drift;
live original windows are reused. Missing fields retain historical SDL meaning.
SDK 0.1.18, the default SDL choice and installed capabilities are unchanged.
See the [owner record](journal/2026-11-06-terminal-owner.md) and
[guide](../../spec/development-session.md).

All ten affected supervisor/owner groups pass Release (52.47s) and ASan/UBSan
(82.95s), without skips, at `/home/aetheri/build/terminal-owner-mztov0ft`.
The original supervisor executes all 24 tests on Linux. This is the affected
integration matrix; app C/SDK bytes are unchanged and the full app suite was
not rerun for this Python/controller change.

The separate actual virtual DRM session `a2d8e87a70a14e32a832c1cc4aa0dcdc`
creates a folder/file, edits and saves exact `hello` bytes, reopens Files,
uses German 200-percent grid/information and runs a normal selected Terminal
command producing exact `owner-files` bytes. Original held process/image/library
observations bind both terminal entry paths. The original session then ends
all five roles with status zero; Root stop and its historical retry are closed.
Sixteen stable DRM captures and 108 native images supplement those observations.

Audit joins 373 host/snapshot/guest inputs and 163 native evidence exports,
unchanged 28 frozen contracts and nineteen protected hashes without sidecars.
The previous closed record is archived byte-exact; older history and foreign
edits are unchanged. The enforcing carrier is idle on tty1 without build/private
registry actors, running Aetheri service or seat0 session. The old quarantined
runtime remains unresolved. These developer hashes are not protected loader or
per-document admission, and this is not a normal login/session acceptance.

Next: continue the accepted UI design and native assistive integration on the
working app models. Final compact/glass presentation, terminal tabs/panes,
optimized/physical presentation and the complete OS/apps/image/installer/update/
hardware/experiment-desktop objective remain incomplete; keep the full goal active.

## Prior Terminal presenter checkpoint

An explicit `aetheri-terminal-nexaui` now paints our actual terminal/editor
through NexaUI cell-grid/text/raster APIs. The original SDL Wayland window,
composition/clipboard/review, shared session, shell/editor identity and child/
stream cleanup remain. NexaUI opens no second window/connection/parser. The
current path uploads one bounded RGBA canvas to the original renderer. Same-family
regular/bold fonts are pinned; SDK 0.1.18/default owner selection remain unchanged.
See the [view record](journal/2026-11-06-terminal-view.md) and
[contract](../../spec/terminal.md#opt-in-nexaui-presenter).

Initial five native groups pass Release 53.48s and ASan/UBSan 71.62s. Later
complete-row semantics and explicit native-presenter/SDK/font checks now pass
all 70 final groups in Release (279.58s) and ASan/UBSan (468.75s), without skips,
at `/home/aetheri/build/terminal-view-final-pdn6_7my`. The dense
240x100 pixel/semantic test retains over 65,536 aggregate UTF-8 bytes without
truncation or model mutation. Native assistive host transport is not connected.

Audit joins 373 host/snapshot/guest inputs and 668 exports/456 native images,
unchanged 28 frozen contracts and nineteen protected hashes without sidecars.
Both native terminal samples record zero new idle frames/CPU ticks over two
seconds. Shell/preedit, German 200-percent review and the actual dirty editor
Save/Drop/Back were inspected. The enforcing carrier is idle on tty1 without
build/private-registry actors, Aetheri service or seat0 session. Closed DRM
history/foreign files are unchanged; no new DRM run. The existing NexaUI
catalogue received scoped adoption evidence only, retaining all 103 IDs.

Next: continue actual presenter/owner/design/assistive integration, beginning
with explicit development-owner selection. Accepted compact chrome/glass,
truthful startup mark, tabs/panes,
optimized/physical presentation and the complete OS/apps/image/installer/update/
hardware/experiment-desktop objective remain incomplete. The old quarantined
runtime remains unresolved; keep the full goal active.

## Prior shared Terminal session checkpoint

The actual SDL Terminal now uses the internal toolkit-free session for bounded
input admission/flush, original-master output feeding, PTY/grid resize and close
intake. Model/PTY identity, shell/editor startup, original child/stream waits,
composition/paste review and clipboard ownership remain with their existing
owners. No NexaUI Terminal executable, new SDK pin/default or installed launch
capability is implied. See the [session record](journal/2026-11-06-terminal-session.md)
and [contract](../../spec/terminal.md#model-and-ownership).

Focused Release/ASan/UBSan session tests pass. Four separately compiled Release
counterexamples detect capacity, unsent-suffix, close-fence and read-budget
regressions at their exact expected checks. The first fixture's FIONREAD
assumption was independently disproved; actual remaining bytes are checked by
MSG_PEEK without changing product limits. All 65 app/UI groups pass in Release
(244.75s) and ASan/UBSan (416.26s), without skips, on final source bytes at
`/home/aetheri/build/terminal-session-full-4sx3exx_`.

Audit joins 369 host/snapshot/guest inputs and 392 evidence exports, including
264 native images. Twenty-eight frozen SQL/XML contracts, nineteen protected
hashes, closed development history and foreign edits are unchanged. The enforcing
carrier is idle on tty1 without build/private-registry actors, an Aetheri service
or seat0 session. No new DRM session. Native shell output, composition, German
200-percent paste review and the original Files/editor desktop were inspected.

Next: connect NexaUI presentation to this real session. Remaining terminal
presentation/input/assistive acceptance and the complete OS/apps/image/installer/
updates/hardware/experiment-desktop scope are incomplete. The old quarantined
runtime stays unresolved; keep the full goal active.

## Prior Files native text checkpoint

Own Files name/filter inputs now use actual NexaUI text fields with history
disabled, replacing custom editing/painting. Aetheri still admits the complete
candidate into its 255-byte operation/query state. Invalid/over-limit edits
restore prior bytes and become invalid/read-only until clear. Changes rebuild
before publication and discard simultaneous old confirmations. Enter follows the
same frame path; a new regression prevents direct dispatch after focus loss.
The pinned descriptor has no description member, so localized rejection context
is on the immediate named group. SDK 0.1.18 and defaults remain unchanged.

An independent native C17/libatspi client verifies Unicode text/character counts,
caret and selection mutations, range insertion/deletion, retired targets and a
256-byte replacement retaining the old draft. Native Search changes actual
filtered-entry counts. All seven affected view/owner groups pass final Release
51.49s and ASan/UBSan 103.25s, without skips. Controller byte/confirmation/lifetime
checks and the full original create/rename/move/editor/fault/owner workflow remain.
See the [native text record](journal/2026-11-06-files-text.md) and
[contract](../../spec/files.md#opt-in-nexaui-view).

Audit joins 366 source inputs, 263 exports/180 native images, unchanged 28 frozen
contracts/nineteen protected hashes and closed development records. The enforcing
carrier is idle on tty1 without build/private-registry actors, an Aetheri service
or seat0 session. No new DRM session; foreign files remain unchanged. Unicode
semantics are tested; the font's missing emoji glyph remains a coverage gap.

Next: continue Terminal/editor presentation migration on these established SDK
boundaries, keeping its original PTY, composition, clipboard and app state. Full
native geometry/events/human assistive acceptance, field styling/IME/clipboard,
final materials/physical hardware, normal session/admission/grant/recovery/removal
and the full remaining OS/apps/image/installer/updates/suspend/multimonitor/
experiment-desktop goal stay open. Preserve the old quarantined runtime and keep
the full goal active.

## Prior Files chrome checkpoint

Own NexaUI Files now supports native title double-click maximize/restore,
including the maximized title, and the tested Settings-style original-press
move/resize rules. Pending gestures recheck modal admission; opening a dialog
cannot turn an old caption press into a move or dialog/Close activation. Escape
owns repeats/release, and device/focus/mode changes retire pending click state.
The compositor retains its original cancellation/geometry rules. No SDK/default,
compositor, file-operation or lifecycle contract changes.

All seven affected view/owner groups pass Release 42.32s and ASan/UBSan 81.91s,
without skips. A test-only pixel refinement captures actual movement before
Escape and checks restored position/bounds. Both final native groups pass again
(31.33s/57.05s); product and C test bytes are unchanged. Tests cover all eight
resize directions, actual double-click dimensions, pending/active Escape,
modal drift, title/body/Close ownership and native pointer replacement, followed
by the existing file/editor/AT-SPI workflow. See the
[chrome record](journal/2026-11-06-files-chrome.md) and
[Files contract](../../spec/files.md#opt-in-nexaui-view).

Audit joins 366 source inputs, 244 exports/168 native images, unchanged 28 frozen
contracts/nineteen protected hashes and closed development history. The enforcing
carrier is idle on tty1 without build/private-registry actors, an Aetheri service
or seat0 session. Foreign edits are unchanged; no new DRM session was started.

Next: integrate full accessible field text, then continue Terminal/editor and
desktop presentation migration. Final material/human/hardware acceptance and
normal session/admission/grant/recovery/removal remain open, alongside the full
OS/apps/services/image/installer/updates/suspend/multimonitor/experiment-desktop
goal. Preserve the old quarantined runtime; keep the full goal active.

## Prior Files editing checkpoint

Own NexaUI Files now has visible-caret name/filter editing: arrows, Home/End,
Shift selection, Ctrl+A/U, whole-grapheme Backspace/Delete and mouse click/
Shift-click/drag. Typing atomically replaces the selected bytes after complete
length/validation checks. Rejection retains the original draft until explicit
clear. NexaUI supplies text geometry and painting; app drafts remain authoritative.
The caret stays visible through a clipped horizontal viewport, without idle
repainting or a second text/undo model. Filter changes retire visible action
scopes even when the selected ordinal is unchanged. SDK/defaults are unchanged.

All seven affected view/owner groups pass final Release (32.51s) and ASan/UBSan
(65.57s), without skips. Native captures wait for an actual focus round trip
after long input. Visual review also exposed a retained rejection notice after
the field was cleared; successful recovery now clears that exact old notice.
The original view fails the new middle-editing assertion. Native input corrects
the exact created folder name, replaces a long file draft and a mouse-selected
query, and retains original save/rename/move/unknown-operation/owner behavior.
See the [editing record](journal/2026-11-06-files-editing.md) and
[contract](../../spec/files.md#opt-in-nexaui-view).

Audit checks 366 source inputs, 183 exports/114 native images, unchanged 28 frozen
contracts/nineteen protected hashes and closed development records. The enforcing
carrier is idle on tty1 with no build/private-registry actors, Aetheri service or
seat0 session. No new DRM session or SDK adoption is implied. Full native
accessible text/IME, final material/human/physical acceptance and normal
session/admission/grant/recovery/removal remain open, alongside the remaining
OS/apps/image/installer/updates/suspend/multimonitor/experiment-desktop objective.
Next: finish remaining Files chrome interactions and accessible text integration,
then continue the Terminal/editor and desktop view migrations. The old runtime
stays quarantined; keep the full goal active.

## Prior Files accessibility checkpoint

Own NexaUI Files now supplies a localized accessible Information name and native
invalid-field state/explanation. Unknown file-operation outcomes have read-only
captured metadata and one focused Close inspection action; there is no editable
name or confirmation. Closing/reopening retains uncertainty and never repeats
or rolls back the operation. SDK 0.1.18 and SDL defaults remain unchanged.

A separate C17/libatspi peer on the actual private compositor/registry proves
selection/actions, inline naming, stale/disabled rejection, invalid state and
localized dialog focus/parent/lifetime; a disabled bridge fails registration.
Actual held/lost mkdir/rename acknowledgements separately prove one attempt,
retained effects and original-move-worker close waiting. Ordinary Files has no
fault wrappers. See the [executed record](journal/2026-11-06-files-access.md)
and [contract](../../spec/files.md#opt-in-nexaui-view).

The full 64 app/desktop/compositor groups pass Release (208.25s) and ASan/UBSan
(367.11s), without skips. The later read-only inspection refinement and stricter
dialog-parent assertion change exactly three recorded inputs; all seven affected
groups pass again on final bytes (29.27s/54.62s). This is full earlier coverage
plus targeted final verification, not a repeated final full matrix. Audit joins
366 source inputs, 116 exports/54 native images, unchanged 28 frozen contracts
and nineteen protected hashes/no sidecars. The enforcing carrier is idle on
tty1; no build/private-registry actors, Aetheri service or seat0 session remain.
Closed development history and foreign files are unchanged; no new DRM session.

Next: complete Files name/filter text editing and remaining chrome interactions,
then continue per-view migration without waiting for global NexaUI completion.
Complete text/IME, human/physical/material acceptance, normal session/admission/
grant/recovery/removal, remaining apps/services/image/installer/updates/suspend/
multimonitor and persistent experiment desktop remain required. The old
quarantined runtime is unresolved. Keep the full goal active.

## Prior Files owner checkpoint

The new explicit `start --desktop --files-ui nexaui-owned` profile now selects
our own Files through the fixed development owner. Settings selection remains
independent; SDL stays the default. Separate `aetheri-files-v1` metadata binds
the executable, nineteen model/dependency and five presentation/build/notice
inputs, cache and reviewed SDK. Original root/user binding and fresh-spawn checks
refuse drift without fallback. Existing live windows and document-close
obligations keep their original handles. Old records retain their historical SDL
meaning. See the [owner record](journal/2026-11-06-files-owner.md) and
[guide](../../spec/development-session.md).

Release coverage is 62 groups: the unchanged other sixty original groups plus
the two final owner groups (9.62s) after a test-only Settings-ready expectation
was corrected. The full final ASan/UBSan run passes all 62 (347.20s), without
skips. Four native owner cases across the two builds/Settings choices retain
36 images and prove unsaved-editor close waiting, exact save, independent
pin/view/controller drift refusal, fresh Files and ordered app retirement.

Fresh actual virtual DRM token `2cf05a61afe548108f8a5248f1168cc3` proves root-bound
Files/SDK identity, native create/editor save, German 200-percent grid/information,
close/reopen with retained workspace and coexistence with own Settings/Terminal.
All five roles and Root end zero; the closed binding/history retry is unchanged.
Sixteen stable PNGs remain. Audit verifies 364 source inputs, unchanged 28 frozen
SQL/XML and nineteen protected hashes/no sidecars. Its 57 exports include the
36 native images. The enforcing guest is idle on tty1 with no build actors,
running Aetheri service or seat0 session. Foreign edits and the old quarantined
runtime remain unchanged.

Next: independent Files AT-SPI and fault-state view tests, retaining current
SDK/defaults and the original lifecycle contracts. Full text/IME, public
fullscreen entry, remaining chrome/material/human/hardware acceptance and all
normal session/admission/grant/recovery/removal, apps/services/image/installer/
updates/suspend/multimonitor/experiment-desktop requirements remain work. Keep
the full goal active; never settle the old runtime from UI evidence.

## Prior own Files view checkpoint

`aetheri-files-nexaui` now supplies our own C17 list/grid, held locations, filter,
inline naming/rename, move review, menus and information on the shared Files
controller. Native Wayland/software presentation retains the original worker and
Terminal/editor obligations. Snapshot, selection, dialog/draft and sidebar action IDs
cannot silently retarget after refresh or hidden-folder changes. Minimum-size
modal keyboard and 200-percent information/name layouts are exercised. See the
[executed record](journal/2026-11-06-files-view.md) and
[contract](../../spec/files.md#opt-in-nexaui-view).

All 60 affected app/desktop/compositor groups pass Release (184.55s) and ASan/UBSan
(358.79s), without skips. After final draft-confirmation hardening, all three
directly affected view groups pass again (8.26s/19.17s); the collector verifies
only view/test sources changed and legacy tested program hashes are unchanged.
Native Files view tests retain 21 images per profile,
exact original editor save and real create/rename/move effects, plus native
640×420 resize/200-percent naming/cancellation/information. Both bounded idle
samples record zero CPU ticks and no new frame. Audit verifies 363 source inputs,
unchanged 28 frozen SQL/XML and nineteen protected hashes/no sidecars; 70 final exports
include 42 verified native images. Fourteen output-0 previews remain. Foreign
project edits are unchanged. The enforcing guest is idle on tty1 with no build
actors, running Aetheri service or seat0 session. Prior developer history and the
quarantined old runtime are unchanged.

The reviewed SDK stays 0.1.18/e71ee30. SDL Files remains selected by the development
owner; this new executable has private-seat evidence, not a new DRM/owner result.
Next: add a distinct bound development selection and actual virtual DRM lifetime
tests, then independent Files AT-SPI and fault-state view checks. Complete text/
IME, public fullscreen entry, full glass and human/physical acceptance remain
open. Normal sessions/admission/recovery/removal, all apps/services, image/installer,
updates/hardware/suspend/multimonitor and the persistent resettable experiment
desktop remain required. Keep the full goal active and preserve the old runtime.

## Prior shared-controller checkpoint

Files now uses a shared C17 per-window controller in `src/files/session.*` for
navigation feedback, naming/rename/move operations, worker completion, unknown
outcomes and original document-child close obligations. The SDL view owns only
its rendering/input/host resources around that state. The same controller can
serve the next NexaUI presentation without copying the operation logic. There is
no NexaUI Files executable or toolkit/default change yet. See the
[controller record](journal/2026-11-06-files-session.md) and
[contract](../../spec/files.md#application-controller-and-presentation).

All 57 affected app/desktop/compositor groups pass Release (191.35s) and
ASan/UBSan (318.76s), without skips. The new headless controller case runs without
UI libraries. The real native Files suite retains 61 cases/48 images per profile;
108 verified exports include 96 native images. Actual DRM token
4003d80b2a464ae3af7ce2dcbcec7a3d proves inline naming/rename, exact document save,
grid/list, localized 200-percent/information/fullscreen and fresh Files lifetime.
Files/desktop/compositor and Root end0; sixteen stable PNGs remain. Audit joins
358 source inputs, unchanged 28 frozen SQL/XML and nineteen protected hashes/no
sidecars. The enforcing carrier is idle on tty1 without build actors, an Aetheri
service or seat0 session; prior closed developer history is unchanged.

Next: build the own NexaUI Files view on this controller and the accepted design,
retaining the existing naming/menu/document/unknown-state behavior. Respect the
recorded public fullscreen-entry gap rather than silently changing F11 semantics.
Broader app functionality, full glass and human/physical acceptance, normal
session/admission/recovery/removal, image/installer/hardware and the persistent
resettable experiment desktop remain required. Keep the full goal active and
preserve the old quarantined runtime.

## Prior Settings chrome checkpoint

Own Settings now uses compact semantic window controls, a shared painting/hit-test
geometry, original-press native title dragging and all eight border resize
directions. Double-click maximizes/restores; Escape cancels pending/native gestures
without leaking repeats or continuing the held drag. Below 320x240, a clipped
hint/Close replaces stale page actions while retaining the model. The pure fallback
is checked down to 64x64. No SDK pin/default/production authority changes.

All 56 affected app/desktop/compositor groups pass Release (186.46s) and
ASan/UBSan (323.22s), without skips. A final test-only refinement waits for the
replacement **pointer** enter rather than any device enter; its two native
Settings groups also pass separately (45.10s/119.29s). Audit joins 355 source
inputs, unchanged 28 frozen SQL/XML and nineteen protected hashes/no sidecars.
The 142 verified exports include 114 native images. The enforcing carrier is
idle on tty1, without build actors, an Aetheri service or a seat0 session.
Actual DRM token
84a8ed394f664ab19cdc67b88f0eeaca proves the current own image, reuse/fresh lifetime,
Terminal/Files coexistence and localized 200-percent maximize/restore. All five
roles and Root end0; historical retry is unchanged. See the
[chrome record](journal/2026-11-06-settings-chrome.md) and
[interaction contract](../../spec/nexaui.md#window-chrome-and-gestures).

Next: continue per-view adoption from the accepted studio and finish the remaining
window/design behaviors. Full glass, public fullscreen entry, snapping/drag-out of
maximized windows, human/physical acceptance, real services, other apps, normal
session/lifecycle/recovery/removal, image/installer/hardware and the persistent
resettable experiment desktop remain required. Preserve the quarantined old runtime;
the full project goal stays active.

## Prior development-owner checkpoint

The explicit `start --desktop --settings-ui nexaui-owned` profile now selects
our own Settings executable. Original Root/user/fresh-spawn checks bind its
image, own model, five presentation inputs, bounded build configuration and
reviewed SDK library. The frozen `aetheri-settings-v1` shape is distinct from
the existing upstream option and historical SDL meaning. Own model changes do
not require an SDK rebuild; the retained original owner refuses changed source/
model/pin inputs before a new process. No fallback or production admission follows.

Fourteen affected Settings/desktop groups pass Release (74.12s) and ASan/UBSan
(139.15s). The sole final app-model independence refinement is rechecked through
three owner/supervisor groups/profile (6.07s/10.89s); current image/library/profile
bindings remain identical. Actual DRM token825441a3cab84098b53369cec3bf5f54 proves
own-image launch/reuse, retained Light choice, close/fresh default model, exact
Terminal-saved bytes, Files coexistence and localized 200-percent maximize/restore.
All five original roles end0 and Root closes0; the historical retry is unchanged.
See [record](journal/2026-11-06-owned-settings-owner.md) and
[guide](../../spec/development-session.md).

Audit joins 355 source inputs, unchanged 28 frozen SQL/XML and nineteen protected
hashes/no sidecars. Twenty-six verified exports include twelve native images;
24 stable DRM PNGs and both actor snapshots remain. The enforcing carrier is
idle on tty1 without build actors, running Aetheri unit or seat0; the old
quarantined recovery stays unresolved. No normal accessibility service or default
toolkit replacement is implied. SDL and the original upstream option remain.

The subsequent chrome checkpoint above adds ordinary move/resize and compact
controls. Full glass/design/human accessibility, normal session services, real
backends and the wider OS/app scope remain open.

## Own Settings presentation — completed checkpoint

An opt-in `aetheri-settings-nexaui` now renders the unchanged model through our
own C17 presentation: display observations, appearance/text size and language.
The sidebar, muted surfaces and grouped controls follow the accepted local
studio's direction with an opaque fallback. Keyboard focus/reading, local
DE/EN/200-percent choices, actual maximize/restore and independent native AT-SPI
actions are exercised. This is neither the complete glass design nor real system
settings. See [contract](../../spec/nexaui.md#aetheri-owned-settings-presentation)
and [record](journal/2026-11-06-settings-view.md).

All 55 affected app/desktop/compositor groups pass Release (169.42s) and ASan/UBSan
(288.74s), including all four new view groups. Audit binds 355 source inputs,
unchanged 28 frozen SQL/XML and nineteen protected hashes/no sidecars. The 148
verified exports include 108 native images. The enforcing carrier is idle on tty1
with no build actors, running Aetheri unit or seat0 session; old recovery stays
unresolved. No new owned-view DRM/session admission is implied.

The selected SDK remains the reviewed 0.1.18/e71ee30, without a pin/source change.
The native host adapts its existing event/observation scaffold and retains the
MIT notice. The two raster-density scene groups retain all 540 matrix cases;
native input and separate private-registry/independent-client checks retain
actual model, semantic, pixel and original-process evidence. Normal bus, human
screenreader/design, full input/window chrome, physical and performance
acceptance remain open.

At that checkpoint, own-image binding and actual DRM lifetime were next; the
current update above records their implemented opt-in path and executed result. The existing
`--settings-ui nexaui` still selects its original upstream consumer; SDL stays
the default. Preserve the original owner/process/pipe obligations. Other apps,
real backends, lifecycle/recovery/removal, image/installer/hardware and the complete
persistent resettable experiment desktop remain required. Keep the full goal active.

## Bound development owner — completed checkpoint

The existing development owner now offers `start --desktop --settings-ui nexaui`
for the fixed reviewed SDK/consumer paths; SDL remains the default. Root records
the original model/image/metadata binding before dispatch, the original ordinary
supervisor rechecks it, and each fresh Settings spawn must still match. A changed
candidate produces the existing error reply without a new process or fallback.
Live reuse and original process/pipe retirement remain unchanged; closed retries
are historical and never select current SDK inputs. This is developer consistency,
not sealed package or production authority admission.

All nine affected Settings/desktop groups pass Release (45.17s) and ASan/UBSan
(67.15s). A sole final raw-JSON read cap is rechecked through two owner/supervisor
groups/profile (3.74s/5.05s), preserving the same bindings and runtime binaries.
Actual native owner input proves search/dock reuse, local model change,
closed/fresh same-profile lifetime, private-copy drift refusal and coexistence
with original Terminal/Files. Actual DRM token d1a3fba307d14e6494f99611f6bdc55b
proves two selected Settings actors, exact Terminal-saved document bytes,
localized 200% maximize/restore and original five-role end/controller close0.
Twenty-four stable DRM PNGs and an unchanged historical stop retry remain.
See [record](journal/2026-11-05-nexaui-owner.md), [running it](../../spec/development-session.md)
and `local/nexaui-owner-2026-11-05`.

Audit binds 348 source inputs, frozen 28 SQL/XML and nineteen protected old hashes
unchanged/no sidecars. The enforcing carrier is idle on tty1 without candidate
apps, running Aetheri unit or seat0. Old recovery remains unresolved. No normal
accessibility bus/service, default toolkit replacement or final UI is implied.
Next: an Aetheri-owned NexaUI Settings presentation following the accepted design,
retaining model/owner and SDL until behavior/design/assistive gates pass. Normal
bus/human screenreader/physical/performance, real backends, further apps,
lifecycle/recovery/removal, OS image/installer/hardware and complete resettable
experiment desktop remain required. Keep the full project goal active.

## Native AT-SPI consumer — completed checkpoint

The pinned NexaUI Settings view now passes a real private AT-SPI registry and
independent C17/libatspi peer on the Fedora/glibc Aetheri compositor. Native
roles/checked states, German/high contrast/200%, semantic resize to 320x320,
150%/actual display selection, 2560x1440 restore and native close are checked
against the original app's independent model logs/pixels/process exit. The
same client correctly rejects a visible disabled-bridge window. No normal SDL
selection, SDK source, package installation or installed service changed.

Four Settings groups pass Release and ASan/UBSan; the final strict-diagnostic
native test passes again in 7.71s/9.58s with nine images/profile. Audit binds
347 sources, unchanged frozen 28 SQL/XML and nineteen old-runtime hashes with
no sidecars. The enforcing carrier is idle on tty1 without candidate/test
processes or seat0. See [native record](journal/2026-11-05-nexaui-atspi.md),
[guide](../../spec/nexaui.md) and `local/nexaui-atspi-2026-11-05`.
Old recovery remains unresolved. The existing NexaUI catalogue records this
scoped evidence; no new chat handoff is pending.

At this checkpoint the bound development-owner/DRM profile was next; the
current update above records its implemented opt-in path and executed result.
Retain the original owner, model and SDL path before any default replacement. Human screenreader/focus-event quality, complete semantics,
normal session bus, accepted design/physical/performance and the remaining
full OS/apps/backends/lifecycle/image/installer/experiment desktop stay open.
Keep the full project goal active.

## NexaUI Settings first probe — completed checkpoint

The first original-model NexaUI Settings consumer now builds privately and runs
on the actual Aetheri compositor in the Fedora/glibc carrier. The pinned 0.1.18
candidate uses Wayland/software presentation, with native AT-SPI compiled from
signed header-only inputs; no SDK source or normal app selection is changed.
Actual pointer/keyboard, three output observations, German/high contrast/200%,
320x320 resize/restore, focus/close and idle checks pass in Release and ASan/UBSan.
The probe exposed and corrected a standalone-compositor output reconfiguration
path that previously depended on a private desktop owner.

Five existing Settings/compositor groups pass per profile. The opt-in registered
`settings.nexaui` passes again with final native pointer checks (3.70s/11.51s),
four captured states/twelve images each. Audit binds 343 host/snapshot/guest
sources, unchanged 28 SQL/XML and nineteen old-runtime hashes without sidecars;
the enforcing carrier is idle on tty1 without candidate/native actors or seat0.
See [probe](../../spec/nexaui.md), [record](journal/2026-11-05-nexaui-settings.md)
and `local/nexaui-settings-2026-11-05`. Old recovery remains unresolved.

At this checkpoint real independent AT-SPI and development-owner/DRM integration
were next; the update above records the scoped native AT-SPI result. Retain the
behavior/model and implement the accepted design before replacement. The unavailable-bus probe is not accessible-session, physical, final
performance/design or normal admission acceptance. Other apps, device services,
grants/recovery/removal, OS image/installer/hardware and the complete resettable
experiment desktop remain required. Keep the full goal active.

## Fullscreen dock/input — completed checkpoint

The fullscreen dock overlap is corrected in the development compositor. Current
scene stack/geometry and committed coverage determine visibility per output;
normal foreground windows reveal the dock, and original fullscreen activation
hides it again. Explicit search remains available without changing the compact
surface's mapping/reservation. Stationary pointer hits/local coordinates are
rechecked, while original buttons/grabs retain their release and absent pointer
devices produce no notification. No new polling service or authority is introduced.

Two retained native negatives isolate the old visible dock and the initial
visibility-only stationary-input loss. All 46 affected groups pass Release
(126.56s) and ASan+UBSan (181.89s). A sole final pointer-presence guard is then
revalidated through 6/6 groups/profile (41.92s/58.54s); only compositor/test-server
binaries change. Final native desktop profiles have 25 cases/48 images each,
including three outputs, real drag geometry, search, original press/release,
unmap, selected-output resize and no configure/redraw loop.

Actual final-binary DRM token0641483bd71e4cbba5f5cdfc9e907a5b proves hidden dock,
responsive real SDL search, normal Settings activation, original fullscreen
return, close/restore and exact saved document bytes. Eighteen stable PNGs and
three actor hashes remain; original four-role end/controller close both succeed.
Audit binds 340 sources, unchanged 28 SQL/XML and nineteen protected old hashes,
no sidecars; the enforcing carrier is idle on tty1 with no known native actors/
Aetheri units/seat0 session. Old recovery remains unresolved. See
[record](journal/2026-11-05-fullscreen-dock.md) and `local/fullscreen-dock-2026-11-05`.

At this checkpoint the next step was the NexaUI Settings consumer evaluation;
the current update above records its scoped native result and remaining gates.
Full visual design/glass/autohide, input/accessibility, other apps, normal grants/
recovery/removal, installable OS/hardware and the resettable experiment desktop
remain required. Keep the full goal active.

## Native Files presentation — completed checkpoint

Files now uses the shared geometry for native drawing and hit testing: compact
list/card views, actual held locations, optional information and inline create/
rename fields. Ctrl+1/2, grid arrows, F4/F9 and toolbar controls are exercised.
At 640×420/200-percent text, whole entries and naming fields remain usable.
Valid field blur consumes its original pointer action; unchanged rename closes
without a worker. Unknown/stale inspection, moves and original document/worker
ownership retain their tested boundaries. This is the opaque fallback, not full glass.

All 46 affected groups pass Release (130.12s) and ASan+UBSan (186.07s), with
instrumentation checked. Native Files has 61 cases/48 images per profile; the
layout checks 113,344 combinations. Actual DRM session
c624e6ab6c6443948e05679ce7078572 proves create/edit/exact save, inline directory
rename, list/grid/details, German 200-percent text, information overlay and
window fullscreen/restore. Its sixteen stable PNGs and three actor hashes are
retained; the original owner closes successfully and preserves the workspace.
Audit binds 340 source inputs, unchanged 28 SQL/XML and nineteen protected old
hashes with no sidecars. The enforcing carrier is idle on tty1 without known
native actors/Aetheri units/seat0 session. Old recovery remains unresolved.
See [record](journal/2026-11-05-files-view.md) and `local/files-view-2026-11-05`.

**At this checkpoint**, the fullscreen screenshot still showed the dock above
Files. The current update above records its subsequent compositor/input correction
and activation/return/multiple-output evidence; native size alone was insufficient.
Full glass, complete input/accessibility, a per-view NexaUI adoption pilot,
copy/removal, normal grants/recovery, other apps, installable OS/hardware and the
complete resettable experiment desktop remain required. Keep the full goal active.

## Files layout foundation — completed checkpoint

The Files model now supports atomic navigation to held ancestors; Alt+Home is
wired into the native app and returns to its original root, clearing the query.
Tests preserve renamed ancestors, reject old-name replacement adoption and prove
failure atomicity and descriptor balance. Display escaping now includes Unicode
line/paragraph separators. Complete-snapshot access stays separate from filtering.

The SDL-independent layout foundation for the selected Files design now computes
compact toolbar/sidebar/content/optional-inspector geometry, list/card cells and
grid-aligned reveal. All 113,344 tested size/scale/pane/naming combinations retain
complete content cells and nonoverlapping controls, including 640×420/200 percent.
It is not yet connected to drawing/hit testing: the old renderer and minimum-size
zero-row behavior remain. Do not describe this as the implemented studio design.

All 46 affected groups pass Release (116.35s) and ASan+UBSan (184.19s), with compiler
instrumentation verified. Each native Files profile passes 53 cases/48 retained
images, including actual Wayland Alt+Home input. Audit binds 340 source inputs;
28 frozen SQL/XML and nineteen old-runtime hashes stay unchanged, no sidecars.
The enforcing carrier is idle on tty1 with no known native actors/Aetheri unit/
seat0 session. Protected old recovery remains unresolved. See
[record](journal/2026-11-05-files-layout.md) and `local/files-layout-2026-11-05`.

Next: connect layout to native drawing and input, then selected list/grid/sidebar/
inspector and inline naming with actual native text/focus/pointer/appearance proof.
Full glass, copy/removal, normal grants/recovery, other apps, installable OS and
the complete resettable experiment desktop remain required. Keep the full goal active.

## Held cross-folder moves — completed checkpoint

Files now retains a window-local move selection through navigation, creation and
cancelled previews. Ctrl+X/Ctrl+V, F10, … and right-click menus share the same
commands. The preview binds source/object and a separately held target; a source
replacement drops the stale selection. Menus own press/release through close.
Directory moves reject observed self/descendant targets through bounded repeated
ancestry checks and flush original directory metadata plus both parents. The
single worker never copies/deletes on EXDEV. Unknown state outlives UI-selection
clear, blocks new mutations/document starts and is not durable restart recovery.

All 45 affected groups pass Release (125.12s) and ASan+UBSan (221.55s). Five separate
copied-source controls reject missing replay/source/flush/overlap safeguards. The
sole final source change fixes empty-label/status overlap when no row fits; targeted
Files/both-owner 3/3 revalidation passes each profile (50.89s/59.60s). Final native
Files profiles have 52 cases/48 images. The final-binary DRM session proves selection,
menu/cancel, unchanged identity, file/directory moves, exact Editor save and moved
workspace retention after Files/document reopen and original session retirement.

Final audit: 337 matching source inputs; 28 frozen SQL/XML and nineteen protected
old-runtime hashes unchanged, no old SQLite sidecars; 1,171 retained native files,
342 images/twelve special records and twelve stable DRM captures. The enforcing
carrier is idle on tty1 without known native actors, an Aetheri unit or seat0
session. The quarantined old running/recover-handoff intent remains unresolved.
See [record](journal/2026-11-05-files-move.md) and `local/files-move-2026-11-05`.

The accepted studio design baseline was recorded concurrently and read. Follow it
for subsequent native presentation work; keep these tested operations beneath it.
The current view remains provisional. Next work includes that visual implementation,
copy/content publication and recoverable removal, followed by richer handlers.
Normal admission/grants, durable recovery, full app removal, other apps, OS image/
installer and the complete resettable experiment desktop remain required.
Keep the full objective active.

## Same-folder rename — completed checkpoint

Files now previews same-folder renames through F2 or its pointer control. The
original source witness, parent and both literal names remain bound. Source
replacement invalidates the preview; a Files-owned document must end first.
One worker attempts RENAME_NOREPLACE and the parent flush without UI blocking.
The destination guard is atomic; source matching is not compare-and-swap and does
not exclude hostile writers. Unknown outcomes retain the operation/names and
block creation/rename; unknown rename also blocks new document starts. No rollback,
replay, candidate cleanup, normal write grant or durable restart recovery follows.

All 44 affected groups pass Release (115.35s) and ASan+UBSan (224.08s). After the
sole final source change adds the original-folder label, Files and both owner
modes pass targeted 3/3 revalidation in each profile (48.23s/60.32s). Source delta
and unchanged other-binary hashes are checked. Each final Files native profile
has 41 cases/39 images; independent copied-source controls reject removing the
replay guard or original-source recheck. The actual final-binary DRM session
proves file/directory rename, inode/mode preservation, exact Editor save, conflict,
cancel, navigation and workspace retention across reopen/session retirement.

Final audit: 336 matching source inputs; 28 frozen SQL/XML and nineteen protected
old-runtime hashes unchanged, no old SQLite sidecars; 1,123 retained native files,
324 images/nine special records and fourteen stable DRM captures. The enforcing
carrier is idle on tty1 without known native actors, an Aetheri unit or seat0
session. The quarantined old running/recover-handoff intent remains unresolved.
See [record](journal/2026-11-05-files-rename.md) and `local/files-rename-2026-11-05`.

Next: copy/cross-folder move and removal previews with recoverable document
management, followed by richer handlers. Normal admission/grants, durable recovery,
full app removal, other apps, OS image/installer and the complete resettable
experiment desktop remain required. Keep the full objective active.

## Create-only Files workflow — completed checkpoint

Files now creates an empty file or folder through pointer controls or Ctrl+N /
Ctrl+Shift+N. A retained parent and immutable literal name bind each single-attempt
operation. Modal input cannot navigate the target; invalid/overflowing names cannot
submit a retained prefix. One SDL worker performs the staged file save or mkdir /
flush sequence without blocking the view. Existing entries remain unchanged.
Success refreshes/selects the new entry; unknown acknowledgements retain possible
objects/stages and block further creation in this window. Close waits for the
original worker. No durable restart recovery or brokered write grant is implied.

All 43 affected groups pass Release (121.05s) and ASan+UBSan (220.79s). Each native
Files profile covers 30 cases/30 images, including real lost-mkdir acknowledgement,
responsive UI, rejected-prefix fencing, 200-percent minimum viewport and exact
new-file Editor save. An unchanged copied-source test rejects removal of the
no-replay guard. The actual protocol3 DRM session creates a folder/file, saves,
rejects replacement, cancels an unsubmitted name and preserves contents across
Files reopen and session retirement. Its three direct roles end status0.

Final audit: 333 matching source inputs; 28 frozen SQL/XML and nineteen protected
old-runtime hashes unchanged, no old SQLite sidecars; 1,079 retained native files,
306 images/nine special records, and eighteen stable DRM captures. The enforcing
carrier is idle on tty1 without known native actors, an Aetheri unit or seat0
session. The quarantined old running/recover-handoff intent remains unresolved.
See [record](journal/2026-11-05-files-create.md) and `local/files-create-2026-11-05`.

Next: richer file/document management with explicit targets and conflict/removal
previews, then additional document handlers. Normal admission/grants, durable
recovery, full removal, other apps, OS image/installer and the complete resettable
experiment desktop remain required. Keep the full objective active.

## Files desktop ownership — completed checkpoint

Files now has an independent fixed development slot beside Settings/Terminal,
available through dock and global search. The tagged model and private version3
owner channel retain separate pending/process/error state. New format1 controller
records bind seven fixed binaries; historical protocol1/2 records are readable
without rewriting. The 0700 session `documents` workspace starts empty and remains
after window/session close. Direct document windows now identify/title as Editor,
separate from the normal Terminal slot.

The owner fences new starts and retires Files/document before Terminal, Settings,
desktop and compositor. Existing search/window activation remains available
during closing so the user can save/close the document. Original process and pipe
end still precede each next role. Full production admission/drain remains separate.

All 42 affected groups pass final Release (105.57s) and ASan+UBSan (168.65s).
The unchanged actual owner test rejects a copied owner that retires Terminal first;
the selected-inode negative control remains green/red as expected. The actual
protocol3 DRM session proves Files launch/reuse, document/normal Terminal/Settings
coexistence, exact save after the shutdown fence, all five direct roles ending
status0, original unit closure and retained workspace bytes.
Audit: all 329 host/snapshot/guest sources match; 28 frozen SQL/XML and nineteen
protected old-runtime hashes remain unchanged, with no old SQLite sidecars.
Retained native evidence has 1,035 files, 282 images and ten special-node records;
the final DRM session has twelve stable captures and six app-actor observations.
The enforcing carrier is idle on tty1 without an Aetheri unit or seat0 session.
The old running/recover-handoff intent remains quarantined and unresolved.
See [record](journal/2026-11-05-desktop-files.md) and `local/desktop-files-2026-11-05`.

Next useful workflow: file/directory creation and management with explicit targets,
conflict handling and previews, followed by richer document handlers. Normal
admission/grants, complete removal/recovery, other apps, the OS image/installer and
the full resettable experiment desktop remain required. Keep the full goal active.

## Files navigation/editor — completed checkpoint

The opt-in Files app now provides held directory/parent navigation, complete
bounded snapshots, visible selection, hidden entries and a whole-intent-rejected
filter. Its SDL3 view supports native move/resize, DE/EN, contrast and text scaling.
Selected regular files pass a literal parent/file witness through a separately
owned Terminal to the editor, which checks that witness before reading. The
Terminal close UI preserves dirty-content choices; Files waits for its original
document child and refreshes after end. Direct-editor Ctrl+Z is refused with a
notice because that window has no shell to resume it. Ordinary shell/editor jobs
retain their existing behavior. See [Files](../../spec/files.md).

All 42 affected groups pass final Release (104.06s) and ASan+UBSan (167.95s).
The unchanged copied-source test rejects omission of the selected-inode check.
The final ordinary DRM session proves navigation, selected document editing,
Ctrl+Z refusal, dirty-close cancel, exact saved bytes, original shell return and
ordered retirement; both original development attempts are closed.
Final audit: 329 host/snapshot/guest sources, 28 unchanged frozen SQL/XML and
19 unchanged protected hashes with no old SQLite sidecars. Both native Files
profiles have fourteen cases, eighteen images and zero CPU ticks in the bounded
one-second idle observation. Retained native evidence has 998 files/270 images
and ten special-node records; the final ordinary session has fourteen stable
captures. The enforcing carrier is idle on tty1 with no Aetheri service or seat0
session. The quarantined old run remains unresolved and untouched.
Evidence: [record](journal/2026-11-05-files.md) and `local/files-2026-11-05`.

The next step at this checkpoint was fixed desktop launch/focus/lifetime ownership,
now implemented above. Regular admission and brokered document access remain separate.
File mutation/removal, image/PDF viewers, richer editor/terminal panes/plugins,
other applications, original OS image/installer and the full resettable experiment
desktop are still required. Keep the full OS goal active.

## Model-owned selection and register — completed checkpoint

The modal owner now supports character/whole-line visual selection and one bounded
private editor register. v/V selects, y copies, d/Delete cuts, c changes, and p/P
pastes. Grapheme endpoints, original endings, empty/final lines and register/
document/index/history allocation failure are covered. Copy leaves text/history
intact; each cut/change/paste is one document delta. A linewise change records its
empty-line cursor atomically so redo restores the intended position. The view
paints selected content and ending markers, names the mode and keeps hints at
30x8. Register text is in-process only; platform clipboard/grants and UNKNOWN save
outcomes remain separate.

All 37 affected groups pass Release (90.34s) and ASan+UBSan (132.12s), including
27 editor PTY cases and nineteen actual provider cases per profile. The unchanged
copied-source test rejects omitted line-start cursor placement. A later explicit
visual-Delete test also passes both profiles with unchanged editor binaries.
The original ordinary desktop attempt proves character/line transfer, distinct
saved/undo/redo state, 100/200-percent highlighting including the ending marker,
exact cancel pixels, final file bytes, same-shell return and original retirement.
Audit: 321 host/snapshot/guest sources, 28 frozen SQL/XML, 19 protected hashes,
no old SQLite sidecars; 904 native files/210 raw images, six special-node records
and 34 stable console files. The enforcing carrier is idle on tty1. See the
[record](journal/2026-11-05-editor-selection.md) and `local/editor-selection-2026-11-05`.

The next workflow at this checkpoint was Files navigation and opening documents
with the editor, now implemented above as a development app. Block/mouse editor
selection, platform clipboard, panes/plugins, normal admitted apps/sessions,
other applications, original OS image/installer and the full resettable experiment
desktop remain required. Do not narrow the OS goal to the editor milestones.

## Search and native-input verification — completed checkpoint

Literal document search now has a read-only whole-grapheme scan and actual
Normal `/`/`?` query, inverse-match preview, Enter acceptance, Escape restoration
and n/N repeat. It uses bounded original C17 code without a new library, file
index or background process. Invalid/overflow/no-match queries cannot accept
an old result or replace the prior accepted query. Repeat searches the current
document after edits/undo. Save uncertainty and text/history remain independent.
All 36 affected groups pass Release (84.41s) and ASan+UBSan (123.95s), including
22 editor PTY cases and eighteen actual provider cases per profile. Strengthened
view/native follow-ups pass 2/2 in each profile (6.19s/6.69s); a later exact
repeat/clear/resize row case also passes independently in both. The unchanged
copied-source search test rejects omitted whole-grapheme endpoint admission.
The ordinary desktop proves 100-percent match/accept/repeat, exact-pixel cancel,
exact saved bytes and return to the same shell; both original attempts retire.

**The earlier 200-percent no-match counterexample is resolved.** Complete glyph
masks in the original image identify `zerr`, not the intended `zero`; no match
is correct for that input. Pixel comparison does not establish corrupt document
rendering. The old driver admitted stable pairs without checking actual text.
Do not reopen that interpretation after compaction: preserve its raw evidence
and read the [correction](journal/2026-11-05-native-key-input.md).

The host tool now explicitly makes/breaks a bounded key chord, verifies the live
carrier and never replays a lost dispatch acknowledgement. New controlled tests
pass (14/14 Linux; eight executed/six Linux-only skips on Windows). An actual
new PTY case distinguishes zerr/no-match from zero/match in both native profiles.
The unchanged Debug editor passes a fresh original desktop attempt with observed
printable commits, calibrated query-glyph/highlight gates, manual 200-percent
review, exact file bytes, same-shell return and original ordered retirement.
Evidence is in `local/editor-input-2026-11-05`: 26 stable console files, nine
native evidence files, source/frozen/protected hash matches (320/28/19), no old
SQLite sidecars and 1,072 valid local file targets. The existing C candidate stays
unchanged; no repeated whole-suite build is claimed for this Python-tool repair.
The original input mismatch's deeper timing/component cause is not established;
no general QEMU/SDL/compositor defect or timer fix is claimed.

At that checkpoint selection was next; it is implemented above. Broader editor/
pane/session behavior remains.
Physical/trusted/assistive/session acceptance, other apps, OS image/installer and
the full resettable experiment desktop remain required. The carrier is enforcing
and idle on tty1; the old quarantined runtime remains unchanged and unresolved.

## Document history — completed checkpoint

The document now owns bounded byte-delta undo/redo, adjacent same-line typing
groups and original cursor/logical content states. Limits are 1,024 complete
entries and 32MiB charged entry storage; oldest entries evict only after a new
successful edit. All text/index/history growth is preflighted; failures preserve
redo. Traversal reuses grow-only capacities. A separate monotonic revision keeps
stale save replies invalid while history can return to the clean saved state.
No file/save/grant effect follows from undo or redo.

Normal `u` / Ctrl+R and existing terminal Ctrl+Shift+Z/Y chords use that owner.
Changed history returns to Normal; unknown save/export fences remain separate.
The signal reader now drains further queued managed signals after its first exit
decision. Actual old-code tests found duplicate emergency copies and suspension
after a stop arrived during a controlled copy. The new guard passes both tests.
Visual/PTY review also found and corrected a swallowed key after fast Escape;
non-CSI/SS3 next input now receives ordinary processing after cancellation.

After the Escape correction, all 35 affected groups pass Release (77.86s) and
ASan+UBSan (107.45s), including eighteen actual editor PTY cases and seventeen
provider cases per profile. Both unchanged-test omission controls reject the
broken copies. The ordinary DRM desktop proves grouped undo/redo, saved-state
return, new-branch rejection and exact final file bytes at 100/200-percent scaling.
The final original attempt closes successfully and returns the same shell.
The read-only audit matches 319 host/snapshot/guest sources, 28 frozen SQL/XML
and 19 protected hashes without sidecars. Retained evidence: 888 native files,
204 raw images, six special-node records and eighteen final stable console files
(eighteen earlier console files retained separately). The enforcing carrier is
idle on tty1. See the [record](journal/2026-11-05-editor-history.md) and
`local/editor-history-2026-11-05`.
Next app work is document search/selection
and broader editor/pane/session behavior. General signal/crash recovery, normal
app/document admission, accessible text, other apps, OS image/installer, hardware
and the complete resettable experiment desktop remain required. Preserve the
quarantined runtime and its independent lifetime/provenance obligations.

## Executable modal editor — historical checkpoint

The opt-in [own modal editor](../../spec/editor.md) now joins the original
document/modal/file libraries to a pure terminal-cell view and foreground app.
It opens or explicitly creates bounded files, edits graphemes/endings, saves and
handles close/cancel/create-only save-as/export. UNKNOWN stays separate from clean
content; export never settles the original write. Discard requires literal
DISCARD, and extra uncertain exports remain held. Untitled save-and-close asks
for a new path before exiting. Invalid/overflow commands cannot submit a prefix.

The view tracks whole-grapheme panning/caret and explicit unsupported-cluster
fallbacks. It uses the original terminal's native Compose/text scaling and moves
focus to close choices, restoring the document on cancel. Below 30x8 it pauses
edits/commands; the help wraps and scrolls at the minimum. Separate nonblocking
TTY ownership, alternate screen/mode restoration and stop/bg/fg preserve the
original shell and unread next input. Signal-exit recovery copies are best effort.

All 34 affected groups pass Release 97.40s / ASan+UBSan 126.33s, with twelve editor
PTY cases and sixteen actual provider cases per profile. The copied-source
clean/UNKNOWN-close omission fails the unchanged test. Visual review then moved
the basename before long parent paths; all three affected view/native/provider
groups pass again in both profiles. See the [record](journal/2026-11-05-editor-app.md)
and `local/editor-app-2026-11-05`. Two original ordinary DRM attempts prove exact
close-cancel pixels, real save bytes, 200-percent contrast and return to the same
shell. The final header candidate and its original attempt close successfully.
The audit matches 318 sources, 28 frozen SQL/XML and 19 protected hashes without
sidecars; 899 native files/204 raw images, six special-node records, 44 console
files (34 final stable files) and 1,040 local file targets. The enforcing guest
is idle on tty1. Earlier failed fixture and clipped-help evidence is retained.
No normal app/document admission, accessible
document API, full undo/search/plugins/panes or final UI follows from this app.

Next app work after verification is document undo/search/selection and broader
editor/pane/session behavior, with explicit simultaneous-signal/shutdown fault
coverage before treating best-effort emergency copies as a broader guarantee.
Full ordinary sessions/apps, remaining applications,
the original OS image/installer, updates, hardware and the complete resettable
experiment desktop remain required. Preserve the quarantined runtime and its
separate provenance/lifecycle obligations.

## Local file adapter — historical checkpoint

The [editor file adapter](../../spec/editor.md#local-development-file-adapter)
now loads exact bounded text and saves through an exclusive same-directory stage.
It preserves ordinary permissions/GID/readable xattrs, checks original bytes and
held/named metadata, and uses no-replace for new files. Failures before rename
leave the target untouched; uncertain publication/flush/close fences later saves.
UNKNOWN is separate from document dirty state. Existing replacement is not CAS
against other apps; this is a local development profile, not a document grant.

Four affected text/editor groups pass Release 2.92s / ASan+UBSan 6.55s with no skips.
Actual 8MiB load/edit/save bytes, attributes, conflicts, allocation/FD cleanup,
short/interrupted/failed writes and injected flush/rename/close failures pass.
An unchanged test rejects the copied-source omitted post-rename flush result.
See the [record](journal/2026-11-05-editor-file.md), including the earlier xattr
fixture limitation. No executable editor or new GUI acceptance is claimed.

Final audit matches 312 host/snapshot/guest sources, 28 frozen SQL/XML and 19
protected old hashes without sidecars. The 120 regular evidence files/eight
special-node records are retained; 1,023 local Markdown targets resolve. The
enforcing carrier is idle on tty1; the old intent's bytes/state remain unchanged.

Next wire the document/modal/file libraries into the actual terminal view and
key/command adapter, with independent uncertain-save and unsaved-close decisions.
Document undo, normal brokered access/recovery, complete app/session provisioning,
the other apps, original image/installer, hardware and full experiment desktop
remain required. The old quarantined runtime remains untouched.

## History search and modal core — historical checkpoint

The actual shell now has bounded Ctrl+R history search with independent query/
preview, original draft/cursor/undo preservation, older/newer traversal and
separate match acceptance/submission. Ctrl+G/Escape cancels; no-match cannot accept
an old candidate, and overflow retains whole-input rejection. No index/file,
allocation, new process or service is added by search.

The [own file-editor core](../../spec/editor.md) now has original copied text/gap/
line metadata, byte-exact UTF-8/LF/CRLF, grapheme-safe edits and Normal/Insert
semantic controllers. Bounds/OOM checks precede mutation; actual 8MiB/line/count
limits, both allocation paths, 4,000 independent reference edits and modal actions
pass. This is a tested core, not an editor executable: file I/O/conflicts/unknown
saves, document undo, terminal view/key mapping and unsaved-close remain work.

All 31 affected groups pass Release 67.19s / ASan+UBSan 91.85s, including sixteen
own-editor PTY and thirteen graphical provider cases per profile. Copied-source
first-Enter execution and stale-line-offset mutants fail unchanged tests. An
ordinary DRM session proves actual query/selection, no first-Enter file effect,
exact cancel pixels/cursor, subsequent file bytes and 200-percent resizing,
then closes its original token. See the [record](journal/2026-11-05-search-and-editor-core.md).

Final read-only audit: 309 host/snapshot/guest sources, 28 frozen SQL/XML and 19
protected hashes without sidecars; 688 copied native files/186 raw images, 14
console files and 1,010 local Markdown targets. Native/console binary identities
agree. The enforcing carrier is idle on tty1 after original-session retirement.

Next connect the modal core to explicit file I/O and an actual terminal view,
with conflict/unknown-write and unsaved-data behavior. Normal admitted sessions/
apps, other apps, image/installer, physical hardware, accessible text and the
early complete experiment desktop remain required. Preserve the quarantined
old intent; this app/model work cannot settle its lifecycle/recovery obligations.

## Undo and literal completion — historical checkpoint

The [own editor](../../spec/line-editor.md) now has bounded undo/redo and literal
command/path Tab completion. Consecutive typing is grouped; successful new edits
discard redo. History traversal/reset/acceptance define separate undo lifetimes,
and blocked input cannot be revived. The graphical terminal sends provisional
Ctrl+Shift+Z/Y chords while preserving Ctrl+Z job control and existing preedit/
clipboard routing. History/undo stay in memory under independent byte/count limits.

The original completion module only reads advisory directory/PATH metadata and
returns sorted/deduplicated, safely quoted literal replacements. It never runs a
candidate, opens file contents, changes cwd/environment or treats a hint as
authority. Query limits and cooperative cancellation preserve the whole draft;
remote filesystem calls can still block. No daemon/thread/runtime library is added.

All 29 affected groups pass Release 67.21s / ASan+UBSan 93.22s, with thirteen actual
editor PTY cases and twelve graphical provider cases per profile. Unchanged
tests reject copied-source quote and stale-redo mutants. The final ordinary DRM
session proves actual undo/redo pixels, quoted-space Tab file effects and original
ordered shutdown. See the [record](journal/2026-11-05-line-assist.md).

The final read-only audit matches 303 host/snapshot/guest sources, 28 frozen
SQL/XML and 19 protected old hashes without sidecars. It verifies 673 copied
native files, 186 raw native images, 22 console files and 983 local Markdown targets.
Final native/console binary hashes agree; the enforcing carrier is idle on tty1
after original-session retirement. NexaUI's existing case was updated in place.

Next app work is history search and the own modal editor/panes/session model.
Advanced completion, fully cancellable remote lookup, accessible text/external
IME, normal admitted sessions/apps, remaining apps, image/installer, hardware and
the early complete experiment desktop remain required. Final bindings/design/
durable history policy remain product work. Preserve the old quarantined intent.

## Own line editor — historical checkpoint

The graphical terminal now starts `aetheri-shell -i --edit`, using our
[toolkit-free line editor](../../spec/line-editor.md). It supplies whole-grapheme
movement/deletion, Home/End, cut/yank, horizontal viewports and copied bounded
in-memory history with live-draft restoration. Invalid or oversized input blocks
the whole draft until reset. No history file, new daemon or runtime library is
added. The Linux adapter retains a separate nonblocking controlling-terminal OFD
and restores original modes before dispatch without consuming subsequent program
stdin. Plain canonical `-i` and existing job control remain available.

Original Unicode 18 segmentation uses exact reviewed standard data and a retained
Unicode-3.0 license. All 853 official vectors pass. All 28 affected groups pass
Release 77.02s and ASan/UBSan 93.16s, including nine native PTY cases and ten actual
Compose/provider cases per profile. An actual wide-cluster cursor counterexample
was retained and fixed. A scratch GB11 omission fails the unchanged Unicode test.
An ordinary virtual DRM session verifies real edit/history/grapheme file effects
and exact preedit cancellation at 100/200 percent, then closes the original token.
See the [dated record](journal/2026-11-05-line-editor.md) for exact identities,
original fixture errors, generated-data provenance and remaining limits.

The final audit matches 299 host/snapshot/guest sources, 28 frozen SQL/XML and 19
protected old hashes without SQLite sidecars. All 967 local Markdown targets
resolve. Final native/console binary hashes agree; the enforcing guest is idle
on tty1 after original-session retirement.

Next app work: undo/completion/search, the own modal editor and panes/session
behavior. Normal admitted sessions/apps, external IME/accessible text, remaining
apps, OS image/installer, hardware and complete experiment desktop remain work.
Final UI, bindings and durable history/session policy remain open. Preserve the
quarantined old runtime and its independent lifecycle/recovery obligations.

## Built-in Compose provider — historical checkpoint

The optional [built-in Compose provider](../../spec/compose.md) now connects real
compositor key events to native v3, SDL preedit/commit, the original terminal PTY
and shell. It uses the existing libxkbcommon plus exact embedded standard data
and retained MIT/X11 notices. Build/runtime opt-ins add no daemon or external
input-method/virtual-keyboard global. `development_session.py start --desktop
--compose` records the explicit profile and preserves the existing app owner.

Admission retains the original seat/client/surface/field. Repeated enable resets
composition; cursor-only updates retain it. Secret fields are excluded and a
competing input cannot steal or automatically inherit admission. A new native
case found that an uncommitted old enable could cross leave/re-enter. The fix
discards pending admission at focus change; the original counterexample and the
expanded positive test are retained. Keyboard/keymap changes and context loss
also retire drafts. Exact shell effects cover `é`, `ä` and `j` + combining acute.

All 21 affected groups pass final Release (62.39 s) and ASan/UBSan (90.85 s).
Provider integration has eight cases/six images per profile; the independent
native lifecycle group has seven cases. A separate lost-enable-boundary mutant
fails the unchanged assertion; feature-off native input/windows and invalid CLI
cases pass. Fifty host contract/migration, 24 runtime-vector and seven VM-input
tests pass. Native evidence has 330 verified files/186 raw images; final virtual
DRM testing uses the same terminal and ends its original processes with status0.
See the [handoff](journal/2026-11-05-compose-provider.md) for candidate/fixture limits.

Final preservation audit matches 285 source files, 28 frozen SQL/XML and nineteen
protected old hashes without sidecars. All 925 local Markdown file targets
resolve. The enforcing guest is idle on tty1 after original-session retirement.

The next app work is a toolkit-free rich shell/editor line model and its native
input/job-control integration, followed by panes/editor/session behavior. External
IME/font/accessible-text work, normal admitted session/apps, remaining apps, the
OS image/installer and complete experiment desktop remain required. This input
milestone does not finish those scopes or select final UI/persistence policies.

## Terminal preedit checkpoint — historical

The [terminal](../../spec/terminal.md) now copies and draws bounded SDL preedit
separately from PTY cells/history and committed input. Its C17 model retains
<=1024 UTF-8 bytes, scalar/byte offsets, cursor/selection and a horizontal viewport.
Native candidate placement follows actual logical geometry through text-scale
changes. Editing/confirmation keys stay out of the PTY while composing; Escape,
focus/context loss, close and invalid input clear it. Committed text validates
completely before one queue admission; control/multiline commits are rejected.

The pinned SDL Wayland backend has a successful no-op ClearComposition path, so
the adapter uses actual disable/enable transitions. Text input is inactive while
unfocused, closing, shell/stream-ended or in paste review. A timestamp fence
rejects already queued old events; this is not future-provider/serial admission.
The normal compositor still has no installed text-input/IME route. Its existing
Compose/Dead-Key fallback does not provide these preedit events. A bounded
controlled v3 producer exists only in the private test server after explicit
inherited-socket I on; normal compositor globals, provisioning and policy are unchanged.

All fourteen affected groups pass Release (52.85 s) and ASan/UBSan (66.15 s).
The native IME group has seven cases/nine images per profile, using actual SDL,
native v3 messages and the original shell/PTY: exact committed Unicode file bytes,
no speculative Enter/editing effects, Escape/empty draft, a native Escape+old
commit batch, long 200% DE/contrast drafts, malformed/control rejection, real
other-client focus loss and original close. A separate mutation removing the
editing-key guard fails the unchanged real-input assertion. Initial fixture
activation failed because it observed only wlroots commit, not its distinct
enable/disable signals; the corrected listener observes all three. No production
reset or freshness guard was weakened to pass that fixture.

All 162 raw native image hashes and 274 copied evidence files verify under
local/terminal-composition-2026-11-05. Native short/selected and long 200% draft
images were visually inspected. Existing clipboard/desktop-owner regressions
remain green. The final ordinary Debug DRM/QMP session is a separate regression,
not a production-IME, hardware or human assistive acceptance claim. Exact context
and the preservation audit are in the [handoff](journal/2026-11-05-terminal-composition.md).

Final audit joins 276 host/snapshot/guest source hashes, all 28 frozen SQL/XML,
nineteen protected old hashes without sidecars and 886 local Markdown file targets.
The normal DRM session is closed; the guest is enforcing on tty1 without a known
native app/test, running Aetheri service or seat0 session. The old running/
recover-handoff attempt remains unchanged and quarantined.

Full compositor/provider admission and routing, own rich line/editor/pane/session
models, accessible text/shaping, normal admitted apps/session, the OS image/
installer, remaining apps and full experiment desktop remain active work.

## Native clipboard checkpoint — historical

The [terminal](../../spec/terminal.md) now copies immutable selected UTF-8 and
receives native Wayland clipboard text without synchronous SDL clipboard loops.
It borrows the actual SDL connection/surface/queue; all protocol/IO remains on
the owner thread. The existing readiness worker adds only a stable epoll FD and
an atomically published absolute deadline. There is no new thread, daemon, host
bridge, dependency family, installed capability, policy or SQL/wire change.

The adapter bounds sources/offers, FD transfers, per-pump IO and text allocations.
Five-second complete-transfer deadlines do not renew on progress; in-flight
exports retain the original snapshot after replacement. One receive sync barrier
bounds uncertain queued FD requests. Old offers are dropped on native focus loss;
closing a local FD is never remote revocation or process/lease drain.

Paste admits at most 4096 raw bytes after complete strict UTF-8/control validation.
CRLF/CR normalize to LF. Multiline/tab content requires a scrollable escaped
review; Cancel is initial, and confirmation requires a fresh non-repeat action.
The app rechecks original focus/shell/foreground observation/screen/input state.
Rejection/cancel sends no partial bytes, and accepted input is queued only once.
The profile retains TERM=dumb and has no bracketed-paste/complete VT or IME claim.

Twelve affected groups pass Release (54.84 s) and Debug ASan/UBSan (62.06 s),
including desktop/native ownership and compositor input/clipboard. The separate
clipboard group has eleven cases/profile: actual independent sources/receivers,
real shell file effects, default-cancel/fresh confirmation, plain MIME, full long
preview at 200% DE/contrast, malformed/oversize rejection, trickled receive timeout,
isolated other-client focus loss, eight blocked exports/ninth refusal, source
replacement and close during transfer. Its independent adapter client checks
wrong-thread/wake reentry refusal, borrowed lifetimes and sixteen FD-stable
open/close cycles. Final clipboard repeats after a file-readiness fixture fix
pass in 12.91/15.86 s; final pure-paste field-comparison repeats pass both profiles.

A build-local mutation renewing receive deadlines on every byte fails the
unchanged native deadline assertion. An initial negative fixture exceeded the
AF_UNIX path bound before compositor startup; another exposed that file creation
precedes its contents. Neither counts as the deadline counterexample. The test
now waits for expected file bytes. Original attempts are retained. A separate
earlier failure proved the input driver's event counter was not a real timestamp;
the fixture now uses CLOCK_MONOTONIC milliseconds without weakening UI freshness.

All 144 native raw-image hashes verify. Actual final Debug DRM/QMP testing also
copies and reviews text at 100/200%, restores both original pixel baselines after
cancel, and ends its original compositor/desktop/terminal with status0. Twelve
console PNGs form six stable pairs. Token e8c394ed63c54e9c81dc4c6e2bbdea74 is
closed, with original stdout stream d0dea03917bf4135914207433a9300c3 under
session-827.scope. The linked terminal matches the independent native result.
The native DE/high-contrast and DRM 200% images were visually inspected. These
are virtual software/PS2/bochs-drm results, not physical, host-window or human
assistive acceptance. Third-party SDL/text/Mesa remain Release libraries.

Final audit joins all 270 host/snapshot/guest source hashes, all 28 frozen SQL/XML,
nineteen protected old hashes without sidecars and 872 local Markdown file targets.
The carrier remains enforcing on tty1 with no known native app/test process,
running Aetheri service or seat0 session after original-unit retirement.

Evidence and original failures are under local/terminal-clipboard-2026-11-05;
see the [handoff](journal/2026-11-05-terminal-clipboard.md). The separate old
running/recover-handoff intent remains quarantined and its nineteen protected
hashes unchanged without SQLite sidecars. Normal app/session admission, editor/
panes/plugins/persistence, accessibility, the image/installer, complete apps and
full experiment desktop still remain part of the active objective.

## Selection-only checkpoint — historical

The [terminal](../../spec/terminal.md) now has toolkit-independent text selection
with mouse drag, Ctrl+Shift+arrows/A and Escape-to-clear. Per-row content tokens
move with history; edits, changed wrap boundaries, eviction and screen/size changes
cannot retarget the selected text. Every selected row is checked, including the
interior. Select-all excludes unused trailing screen rows. UTF-8 export handles
wide/combining cells and actual soft versus hard wraps with atomic output bounds.
It is an in-memory export API, not native clipboard copy/paste or a sandbox.

Native keyboard/pointer tests measure unchanged PTY input counters, selected
background pixels, drag cancellation after actual device/focus loss, Escape and
200% text/contrast/locale behavior. Clipboard chords are reserved and report
unavailable; they must not become Ctrl+C/Ctrl+V. A build-local model omitting the
row-token validation is rejected by the unchanged test. A first fixture sent a
button release after removing its pointer; the driver correctly rejected that
invalid command. It now recreates devices without inventing an old-button release.

The final five affected groups pass Release (18.44 s) and Debug ASan/UBSan
(23.84 s), including actual desktop-owner integration. Terminal native has sixteen
cases/thirty images per profile; owner adds seven cases/six images. All 72 raw
image hashes verify. The model mutation without selected-row validation fails
at the unchanged overwrite assertion. Actual final Debug DRM/QMP tests also
select at 100/200% text, reserve clipboard chords without interrupt/literal-next,
restore the exact baseline after Escape and close original processes/status0.
Ten console PNGs form five stable pairs; the same binaries are in the native
owner result. Earlier Release/pre-shortcut Debug runs remain historical.
The device-removal fixture removes pointer and keyboard together; isolated
keyboard-focus loss with a still-held pointer is not separately accepted.
Evidence and scope are indexed under local/terminal-selection-2026-11-05.
Third-party SDL/text/Mesa remain Release libraries; physical, host-window,
production session and assistive acceptance are still separate.

Final audit joins all 263 host/snapshot/guest source hashes, verifies all 28
frozen SQL/XML and nineteen protected old catalogue/static/profile hashes without
sidecars, and resolves all 857 local Markdown file targets (not URLs/anchors).
The guest remains enforcing on tty1 without a known native test/app, running
Aetheri unit or seat0 session. The unresolved old intent is unchanged.

The existing SDL3 Wayland clipboard receive loop is synchronous, grows its buffer
while reading and has no complete-transfer byte limit. Do not call its documented
main-thread-only API from the worker. Next implement bounded async native transfer,
complete-transfer deadlines/cancellation and reviewed multiline paste; existing
native clipboard transport in the compositor alone does not supply app integration.
No new package, service, policy, SQL/wire capability or persistent user data is added.
Normal admitted sessions/apps, full IME, editor/panes/plugins/persistence, native
accessibility, the image/installer, complete apps and experiment desktop remain
required. The old running/recover-handoff intent remains separate and quarantined.

## Two-app ownership checkpoint — 5 November 2026

The [development desktop](../../spec/desktop-ui.md) now starts and reuses Terminal
or Settings from dock/search, activates their observed windows, and permits either
to close/reopen while the other stays alive. App choices have separate tags and
pending/process/error state; they never alias compositor window IDs. The private
protocol2 has only finite S/T selectors and carries no paths, argv or FDs. New
format1 controller records bind five binaries and the existing Mono font/license;
closed legacy records remain historical and readable.

The actual development owner keeps one slot/128 fresh attempts per app. It waits
for each original process AND output pipe before retiring Terminal -> Settings ->
desktop -> compositor, also after failure. A later pipe-setup error retains its
spawned child rather than replying “no effect.” The native integration holds a
real hangup-ignoring shell job and then a real stdout writer after Terminal exits;
later roles stay alive until those obligations end. A build-local pipe-wait
mutation is rejected. The test's process inspection/force cleanup is private
fixture infrastructure, not a normal app authorization or holder-drain protocol.

All four registered desktop groups pass Release (16.55 s) and ASan/UBSan
(20.96 s). The added desktop.supervisor group runs all eleven existing controller/
file/synthetic-process checks without skips, including Settings-only failures.
Review found its old desktop client still sending protocol1; the fixture now
uses protocol2 and tests strict version types plus unchanged historical records.
The production binaries are hash-identical to the reviewed DRM runs. The
existing UI test includes 27 images/profile; the actual-owner fixture adds six,
all across three virtual outputs. Both actual DRM/logind sessions also start
both apps, run an own-shell file effect, reuse Terminal and close all four direct
native children with status0 before original-unit retirement. Two rounds per
profile retain sixteen QMP
PNGs and stdout stream records are under local/desktop-terminal-2026-11-05.
Final four-group logs are retained there under final-regressions; earlier
three-group results (16.94/21.65 s) remain at their original checkpoint.
The original process-bound, current-boot PAM stdout stream was used: unit-only
or punctuated-UUID journal filters initially missed a live app and did not cause
a second session start. The optional preexisting PAM warning remains unchanged.
All 262 host/snapshot/guest source hashes agree, and all 28 frozen SQL/XML plus
nineteen protected old catalogue/static/profile hashes are unchanged without
sidecars. All 850 local Markdown file targets resolve; URLs/anchors are excluded.
The eight console image pairs match exactly. The guest remains booted/enforcing
on tty1 without a known native test/app, running Aetheri unit or seat0 session.

Initial owner-fixture failures came from sending the close chord before actual
terminal focus and a missing underscore key in its input driver. The final test
waits for native focus and verifies Ctrl+Shift+Q does not alias desktop Ctrl+Q.
A later actual compositor startup failure exposed unused parent socket copies
keeping the owner alive before desktop creation. The five-second failure is
preserved; stopping now closes those copies and fences late first-app creation.
The final owner fixture covers that early failure as well as the full UI flow.
There is no new library, installed service, policy or normal lifecycle capability.
Normal admitted apps/sessions, clipboard/IME, rich shell editing, editor/panes/
plugins/persistence, physical/accessibility acceptance, image/installer and the
full experiment desktop remain required. The old running/recover-handoff intent
remains separate and quarantined.

## Graphical terminal checkpoint — 5 November 2026

An opt-in original C17 [graphical terminal](../../spec/terminal.md) now connects
the own interactive shell to a held PTY and SDL3 Wayland/GLES view. Toolkit-free
bounded UTF-8/cell/history state supports colors, cursors and an alternate screen.
Native DE/US and AltGr input has exact output-byte checks; real Ctrl+Z/fg/Ctrl+C,
scrollback, maximize, DE/EN chrome, contrast and 100–200% text are exercised.
Fork precedes SDL/Mesa threads. One readiness worker sleeps on IO and hands work
to the UI owner, with one pending acknowledgement and bounded queues/caches.

Process exit and PTY end are independent obligations: tests retain a writer after
shell exit and keep a trusted child alive after stream EOF. Close refuses early
release. A failed SDL event post now retains ownership and delivers a failure for
normal shell/stream retirement; a build-local abrupt-exit mutation is rejected by
the unchanged fault assertions. No descendant/cgroup/lease drain is inferred.

Final terminal groups pass 3/3 in Release (10.16 s) and ASan/UBSan (13.70 s),
including a persistent event-wait failure that must enter synchronous cleanup.
The six shell/Settings-model/desktop-model regression groups also pass in both
profiles (2.87/3.59 s, before the terminal-only final adjustments). Each native
profile captures 21 hashed originals across three virtual outputs, with actual
compositor frame observations. Original typing-map/focus-handoff harness failures
and the independent model delayed-wrap regression remain documented. These are
software/virtual tests; third-party SDL/text/Mesa remain Release libraries.
Exact result/artifact/binary records are in local/terminal-2026-11-05.

The first two-second llvmpipe idle samples have no new frames and no process CPU
ticks at the recorded clock resolution. RSS/threads are observations, not accepted
budgets; profile their sources and peak/long-run behavior. Font evidence is an
existing carrier Mono package and exact font/OFL-license hashes, not a fresh RPM
signature verification or completed distribution font package.
All 260 host/snapshot/guest source hashes agree; all 28 frozen SQL/XML and nineteen
protected old catalogue/static/profile hashes remain unchanged, without sidecars.
All 839 local Markdown file targets resolve (URLs/anchors excluded). The VM stays
booted/enforcing on tty1 with no known Aetheri native program, running Aetheri unit
or seat0 session. The preserved running/recover-handoff intent remains unresolved.

Next connect the terminal to the fixed development owner/dock. Selection/clipboard,
IME, rich shell editing, panes/editor/plugins, persistence, accessibility, physical
drivers and normal app/session admission remain required. The complete OS/apps/
image/installer and persistent experiment desktop remain the goal. The old
running/recover-handoff intent is separate and must stay quarantined.

## Interactive shell checkpoint — 5 November 2026

The original shell now accepts `-i` on an independently foreground canonical
N_TTY controlling terminal. Separate original pipeline groups receive real
Ctrl+C/Ctrl+Z; jobs/fg/bg retain direct children, saved job terminal modes and
nonreused IDs. Terminal handoff precedes the existing start-barrier release.
Foreign ownership fails without takeover; exit/EOF refuses unresolved jobs.
Hangup/termination signals original waitable direct children and waits without
force-kill; an ignoring child keeps its shell owner waiting. This is ephemeral
development job control, not durable app/descendant/cgroup or lease settlement.

All four shell groups pass in Release (3.24 s) and Debug ASan/UBSan (3.91 s),
including 16 batch execution and 13 real private PTY cases plus parser/failure
checks. Final fixture synchronization/cleanup checks pass separately (2.64/3.17 s).
The 28 final PTY sessions have inspected transcripts and exact exit results.
The unchanged foreign-owner test rejects a weakened reclaim guard. Earlier tests
reproduced blocked FIFO-open Ctrl+C and canonical input prefix execution; both
are fixed. Actual EOF/stop/continue, TTIN, mode restoration, slot limits, stale IDs,
idle events and handoff denial are covered. Evidence is retained under
local/shell-interactive-2026-11-05 and its referenced guest directories. See the
[shell guide](../../spec/shell.md) for exact boundaries and input limitations.
All 251 host/snapshot/guest source hashes agree; all 28 frozen SQL/XML and nineteen
protected old catalogue/static/profile hashes remain unchanged, without sidecars.
The Release shell still needs only libc. All 819 local Markdown file targets
resolve (external URLs/anchors excluded). The VM stays booted/enforcing on tty1
with no active Aetheri program/unit or seat0 session; the old running/recover-handoff
intent remains unresolved and unchanged.

Own raw input editing/history/completion, terminal emulator/model/GUI, persistent
sessions and final language/product choices remain open. The Linux kernel's
canonical editor still owns input editing and can discard bytes before userspace;
saturated records are refused, not recovered. No normal shell installation,
launcher/capability/policy or new library is added. Full OS/apps/image/installer,
experiment desktop, physical and accessibility acceptance remain required.

## Noninteractive shell checkpoint — 5 November 2026

An opt-in original C17 [shell profile0](../../spec/shell.md) now parses quoted
arguments, scalar environment/status expansion, foreground pipes and file
redirections. Its CLI executes actual Linux programs through explicit execve/PATH
lookup without an implicit /bin/sh fallback. Parent builtins manage cwd/environment/
exit; a bounded start barrier prevents command/file effects after incomplete
pipeline creation. Direct children are retained/waited and extra child FDs close.
The language spelling remains an experimental recommendation for later user review.

Release and Debug ASan/UBSan pass all three shell groups (0.42 s / 0.94 s), including
16 actual process/file/signal cases, parser grammar/ownership/budgets and 20,000
deterministic hostile inputs. The unchanged fault test rejects a build-local
barrier-EOF mutation at an unexpected output file. The GCC analyzer finds no
diagnostic. A native export/printf/tr/cat/false/exit demonstration produces the
expected saved text and last-status result in both profiles. Evidence is retained
under local/shell-2026-11-05 and its referenced guest directories.
All 248 host/snapshot/guest source hashes agree. The 28 frozen SQL/XML contracts,
nineteen protected old catalogue/static/profile hashes and dependency input pins
remain unchanged; no SQLite sidecars are created. Root shell execution is refused.
The VM remains booted/enforcing on tty1 without active Aetheri native programs,
units or seat0 sessions. All 811 local Markdown file targets resolve; anchors and
external URL checks are outside that file-target check.

The Release executable needs only libc; no new library, installed service, app
admission, authority capability or graphical launcher is added. Interactive
prompt/editing, terminal ownership/signals/jobs, persistent sessions, terminal
emulation/panes/plugins and GUI integration remain required next work. This is
not a completed terminal, POSIX compatibility layer, sandbox or lifetime authority.
The full OS/apps/image/installer/experiment/hardware scope remains open.
Next: original interactive foreground terminal ownership/signals/job behavior,
then the graphical terminal transport/model/view; retain this parser and keep
business/session state independent of SDL/NexaUI.

## Work-area checkpoint — 5 November 2026

The development compositor now sends advisory v4+ normal-window bounds excluding
the dock and provisional placement margins. Actual Settings opens fully above
the dock on the 1280×800 carrier: 1100×636 normally, 1280×716 maximized, then back
to 1100×636. Super+Up/Down consumes both arrow events and acts only on actual app
focus outside search, pointer interaction and fullscreen. Desired compositor modes
remain separate from client requests/committed state. Reconfiguration follows the
current selected output, including fullscreen away from the original placement.

The native test reproduces the original overflow with the old build. All ten
Release checks pass across focused runs; Debug ASan/UBSan passes 10/10 in 36.88 s.
The extended selected-output test then passes in both profiles. All 108 image
hashes per profile match and all 216 gzip originals verify. Real DRM sessions in
both profiles repeat normal/maximized/restored geometry and ordered shutdown with
original status0. Their three guest captures match, including exact normal/restore
identity. Evidence is retained under local/workarea-2026-11-05; the [consumer
guide](../../spec/desktop-ui.md) distinguishes these checks from the prior checkpoint.
All 240 host/snapshot/guest source hashes agree; all 28 frozen SQL/XML and nineteen
protected old catalogue/static/profile hashes remain unchanged, without SQLite
sidecars. The VM remains booted/enforcing on tty1 with no active Aetheri unit,
native compositor/UI process or seat0 session. All 795 local Markdown file targets
resolve; external links and anchors were not part of that file-target check.

Bounds are recommendations, not arbitrary-client clipping. General off-screen/
minimum-size handling, fullscreen dock policy, assistive/IME/physical acceptance,
normal session/app admission and the remaining full OS/apps/image/experiment work
remain open. The protected old execution intent is separate and unresolved.

## SDL desktop checkpoint — 5 November 2026

The original [desktop client](../../spec/desktop-ui.md) now supplies a compact
dock, DE/EN keyboard search and observed window selection on up to three outputs.
Its bounded C17 model is separate from SDL/NexaUI. Actual SDL custom roles use
the existing private compositor connection, initial configure before renderer
allocation and SDL-owned viewports. Correct premultiplied clear colors and a
bounded resize follow-up repaint fix the observed transparent/missing-dock frames.
The compositor adds the standard viewporter from its already pinned library;
no new dependency family, installed policy or normal authority capability changes.

The explicit development controller's `--desktop` mode now starts the shell and
launches fixed Settings on request. It owns one original Popen slot, reuses a live
child and recycles a finished child only after its output pipe is drained. Two-byte
inherited messages carry no paths/argv/FDs. Pending starts are not repeated; channel
loss/invalid packets fail without reconnect. User Ctrl+Q asks the owner to fence
new effects and stop Settings, desktop and compositor in order. This is development
supervision, not normal app admission, readiness, lifetime recovery or lease release.

All ten native Settings/compositor/desktop checks pass in Release (22.34 s) and
Debug ASan/UBSan (39.59 s). They cover model limits, native pixels/input, multiple
outputs, small-layout reflow, app start/activation/close, pending/error/empty
states, transparency, idle, owner close and malformed/lost-channel cases. All
eleven Linux controller/file/generated-process tests pass (0.763 s); the unchanged
ordering test rejects a deliberate desktop-before-app shutdown mutation. Actual
DRM controller sessions exercise fixed Settings start, close/reopen/reuse and
original result retention. Records/journals/guest PNGs remain under
local/desktop-ui-2026-11-05. See the guide for exact evidence and remaining gates.
All 99 image hashes per profile agree and all 198 compressed originals verify.
Final native Release/Debug DRM captures also match after actual Settings launch;
controller stop and user-requested ordered shutdown retain original status0.
All 240 host/snapshot/guest source hashes match; the 28 frozen SQL/XML contracts
and nineteen protected old catalogue/static/profile hashes remain unchanged.
The VM stays booted/enforcing on tty1 with no active Aetheri unit/native program
or seat0 session. No old execution intent is settled by these development tests.

This is a provisional development UI. Native assistive semantics, complete
editing/IME, 200% text and physical mixed DPI, final visuals, ordinary admitted
sessions/apps, trusted lock/resume and real settings backends remain work. The
full terminal/editor/shell, browser, Files, device/software/VM applications,
original image/installer and persistent experiment desktop remain required.
The protected old running/recover-handoff intent is separate and remains unresolved.

## Private connection checkpoint — 5 November 2026

The compositor now accepts one original inherited AF_UNIX stream connection for
private development desktop control and layer-shell v5. A global filter denies
ordinary same-UID discovery/binding, including known-ID forgery. No-shell defaults
remain unchanged. A separate window scene and bounded per-output dock/search
profile provide work-area reservation, exclusive keyboard focus and restoration,
existing-window activation, never-reused session IDs and bounded descriptive UTF-8.
Unexpected control/peer loss fails the development session; acknowledged retirement
permits an orderly departure. It is a connection capability, not process/account/
app admission, secure lock UI, launch authority or lease settlement.

See the [desktop contract/evidence](../../spec/desktop.md). Actual Release and
Debug ASan/UBSan each pass all eight selected Settings/compositor checks, including
the new independent native protocol/pixel/input/foreign-client test. All 78 image
hashes per profile agree and all 156 gzip originals verify. Existing native input,
clipboard, nested and Settings behavior remains green. The unchanged test rejects
a deliberately weakened global filter at the precise foreign-discovery assertion.
The upstream layer XML is copied from the already pinned wlroots archive with its
license intact and a CMake hash check. No new library family/policy is installed.

At this earlier checkpoint the desktop consumer painted diagnostic color blocks
only; the separate consumer integration is now described above. No normal shell/
session capabilities are advertised. The nineteen
protected old catalogue/static/profile hashes remain unchanged and all 28 frozen
SQL/XML contracts are raw-byte identical. The old running/recover-handoff intent
stays unresolved. The VM remains booted/enforcing on tty1 with no active Aetheri
unit or seat0 login session. Full OS/app/image/experiment/hardware work remains open.

## Previous update — 4 November 2026

The [development controller](../../spec/development-session.md) now provides
start/status/stop through the verified dedicated-carrier connection. Its separate
root-owned version1 journal keeps original boot/unit/invocation/result and refuses
pending-effect replay. Closed-token stop retries are historical even while a newer
session is live. An ordinary UID1000 helper retains both native children, notifies
PID1 after the first submitted Settings frame, sleeps on pipes/signal wakeups,
stops/reaps children and drains final output. Planned shutdown cannot conceal a
cleanup/sanitizer failure. RemainAfterExit keeps the exact original process result
until root records it and retires the unit; no permanent privileged daemon is added.

The optional `run_vm.py --offline --console boot` reuses QEMU's existing native
SDL2 viewer with GL/window-close shutdown off. QMP confirms those actual options;
the running Windows process has its own QEMU window. No new viewer dependency,
VNC/listener, host folder/clipboard bridge, policy or host/WSL setting was added.
Aetheri Settings still uses SDL3. The dedicated guest shut down with QEMU exit 0
and PID absence before the console boot; its new BootId is
124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056. All nineteen protected old catalogue/static/
profile hashes match before/after. The old intent stays unresolved and unchanged.

Actual final Release and Debug controller sessions display Settings and close
with retained original supervisor status0. Debug journals have no sanitizer
finding. Their guest PNGs are byte-identical and were visually checked through
the original QMP capture. Actual duplicate start, foreign token and old completed
stop during a new live session are rejected or historical without altering the
current record/process. Native Ctrl+Q becomes original_exit with the original
status retained, then explicit stop closes the attempt. One idle second has
unchanged ticks across supervisor, two native programs and the sd-pam helper.

Ten Linux contract/file/child-process tests pass without skips, covering stale
owner/boot rejection, interrupted writes, acknowledgement loss, missing first
submitted frame and child/cleanup failure with final pipe drain. The unchanged
test rejects a deliberate cleanup-outcome guard mutation. Five common contracts
pass on Windows; five Linux-only cases are skipped there and executed in Linux.
Five selected host VM identity/console/network guards pass. Existing native SDK/
compositor/Settings sources and frozen SQL/XML are unchanged by this tool slice.

Windows native app/window inspection timed out in both available automation paths.
Window metadata, QMP display options and guest pixels are narrower evidence;
Windows keyboard/mouse grabbing and host-window visual acceptance remain open.
Controller tests retain dedicated records/caches for inspection; no production
app-state purge or general power-loss reconciliation is implemented. Both final
attempts are closed and their native programs/supervisors are gone. The VM itself
remains booted with its native console; normal authority capabilities stay closed.
All 229 host/snapshot/guest source hashes agree. The 28 frozen SQL/XML files remain
raw-byte unchanged and 746 local Markdown file targets resolve. The development
control history is root-owned, bounded and separate from the preserved old intent.

Next: original desktop shell/dock/search and app launching, then the remaining
normal admitted session, trusted lock/resume, app backends and full experiment
desktop. Original image/installer and physical hardware acceptance remain required.

Previous DRM checkpoint (bebd4da):

The optional compositor now drives an actual KMS connector and evdev devices
through a selected DRM/libinput/logind profile. The existing virtual VGA/bochs-drm
and PS/2 devices work without reboot or virtual GPU replacement. An explicit
ordinary UID1000 PAM/logind session on an inspected free tty2 displays Settings
at 1280x800/74.994 Hz. Both processes have zero capabilities and NoNewPrivileges;
Fedora remains enforcing. No device chmod/group, new policy grant, root compositor,
seatd daemon or host/WSL change was needed. See [scope/build/operations](../../spec/session.md).

`AETHERI_DRM_SESSION` is separately opt-in and requires the reviewed session-capable
wlroots profile; ordinary builds stay unchanged. A missing/wrong libseat backend
or malformed card never falls back or publishes a socket. Root is refused, and
the actual headless-only library candidate fails configuration at the missing
DRM feature check. The normal executable exposes no input-test global/driver.
Initial activation waits on the same session with a five-second monotonic bound;
unexpected session destruction invalidates its pointer. Deactivation/output loss
currently exits rather than pretending lock/resume policy exists.

Eleven supplementary exact signed RPMs are pinned in session-inputs.json. Ten
packages were added and hwdata updated; the offline no-weak-deps transaction adds
about 3 MiB net. All signatures match the full Fedora44 key. libinput 1.31.3,
libseat 0.9.3, libdisplay-info 0.3.0, hwdata 0.412 and existing libudev259 build the
same pinned wlroots0.20.2 with DRM/libinput/session, Pixman and DRM dumb allocation;
optional GL/Vulkan/GBM/libliftoff remain disabled in that library. Existing SDL
Settings still uses the separately reviewed GLES/llvmpipe path.

Release passes 7/7 Settings/compositor tests (10.96 s); Debug ASan/UBSan with a
separately instrumented wlroots passes 7/7 (16.09 s). This includes five safe
backend/card negative cases in the new session argument test. The four image
suites produce 54 equal hashes per profile; all 108 compressed originals were
decoded and reverified. The ordinary default Release build and five selected
existing core/broker/boot tests pass (0.20 s). Seven host cache/input guards pass.

Actual QMP key/mouse events traverse PS/2 -> Linux/libinput -> Wayland -> SDL.
Tab/Enter, a native pointer Light-theme click, German and 200% text work in the
Release DRM session. Original PNGs were visually inspected. One idle second has
unchanged CPU ticks for both processes and 28 compositor commits before/after.
Ctrl+Q closes Settings with status0; VT deactivation ends the provisional server
with status1. A fresh Debug DRM session then reacquires that same card, displays
Settings and native keyboard navigation, and closes both processes with status0
without a sanitizer finding. Two initial DRM PNGs are byte-identical across
profiles. Actual Debug process/unit/context/mapped-library evidence is retained.
After the final input-enrollment failure flag correction, both final native DRM
profiles were exercised again on inspected free tty8, outside the carrier's
checked automatic-VT range. Exact live binary/unit/context/library hashes and
original PID1 success events are retained for both; final initial/Appearance PNGs
are byte-equal. Collected transient-unit defaults are never used as exit evidence.

The initial inactive-VT/getty race, direct init_t home-exec denial, wrong test font
prefix and nonpersistent configure environment are retained as original setup
failures. Native mouse transitions need separate QMP batches; a combined PS/2
down/up can coalesce into no click. The optional carrier pam_lastlog2 warning is
recorded, with no PAM weakening or claim of login-manager acceptance.

All 227 host/snapshot/guest source hashes agree, eleven transferred RPM hashes
agree and the 28 frozen SQL/XML files are raw-byte unchanged. The old Root/User
catalogue hashes and sixteen static files plus protected profile remain unchanged;
running/recover-handoff is still historical unresolved bookkeeping. No selected
new compositor/Settings process or PAM session remains; tty1 is active. These
test exits are never old-intent drain/release evidence.

Next: reusable supervised development start/stop, a visible VM console and the
desktop shell/apps, with normal admitted session/app confinement, trusted lock/
resume and actual settings backends. Original boot image/installer, full persistent
resettable experiment desktop and physical hardware acceptance remain required.
No normal authority capability, catalogue migration or final UI design changed.

Previous clipboard checkpoint (5c68764):

The private compositor now supports real Wayland clipboard selection and FD
transfer between independently connected clients. Publications/clears require
the actual keyboard-focus client/current mapped surface and a serial from that
focus interval. Replacement/clear/producer exit preserve the expected source
lifetime. Retained offers cannot begin a transfer in background or empty focus.
The compositor keeps metadata only, with no history or payload cache. See the
[native clipboard boundary](../../spec/graphics.md#native-clipboard-selection).

Release passes all six Settings/compositor tests (10.64 s); Debug ASan/UBSan with
the separately instrumented wlroots passes 6/6 (17.80 s). The clipboard test has
11 native workflows, including UTF-8 bytes, old/unknown serials, empty selection,
empty focus, keyboard replacement, producer departure and rejected source-less/
source-backed drags. A replacement keyboard now retains the user's current
window rather than selecting an unrelated fallback, with a fresh focus interval.
An initial empty-read test acknowledgement mistake and the genuine replacement-
focus failure are retained. All 54 image hashes match both profiles and the
previous published Settings/input checkpoint; no final UI design was changed.

The wlroots 0.20.2 private integration patch adds original requester provenance,
receive-time offer focus checking and safe unstarted-drag rejection. Required
private API symbols reject an old library before a socket/READY is published.
Archive/patch and all five affected file preimage/result hashes are pinned and
checked by the fresh source builder. Both prior pointer-only and new library
profiles remain separate. With the identical final native binaries, a deliberate
receive-guard mutation exposes bytes during empty focus and fails the exact
test assertion. That negative library is test-only and never selected normally.
An old-ABI negative control also fails as expected before opening a socket.

No package/library family, daemon, policy grant, SQL/XML contract, authority
capability or installed app lifecycle was added. This same-UID private test is
not clipboard consent, secure lock/sandbox/holder admission, parent bridging,
FD revocation or completed drag-and-drop. Already transferred descriptors/bytes
remain outside offer cancellation. Native accessible app controls, actual settings
backends, normal session/app provisioning, the full experiment desktop, original
image/installer and physical hardware acceptance remain work.

The ordinary Release build and five selected existing core/broker/boot tests
pass (0.22 s). The protected old Root/User catalogue hashes and 17 static files/
profile remain unchanged, with running/recover-handoff historical bookkeeping.
The old execution intent is still unresolved; clipboard/test process exit is
not its drain or release evidence.
All 225 host/snapshot/guest source hashes agree. Both actually mapped private
wlroots library hashes match their original build reports; all five patched
source files match their pinned results. All 108 compressed native captures
reproduce their original hashes. The 28 frozen SQL/XML files are unchanged and
722 local Markdown file targets resolve. No selected UI/compositor test remains
live. The retained controls/builds/logs stay separate from normal dependencies.

Previous Settings checkpoint (0d2ef8f):

The optional C17 Settings view now reads actual display observations from the
private Aetheri Wayland session. Display selection/refresh, native keyboard and
pointer controls, per-window appearance/language/text size, focus scrolling and
native border resize work. App observations/selection stay independent of SDL.
See [build, dependency and acceptance boundary](../../spec/settings.md).

Release passes settings.model/settings.native 2/2 (4.43 s); Debug ASan/UBSan
passes 2/2 (5.59 s). Aetheri code and the selected Debug wlroots are instrumented;
SDL/text/Mesa remain reviewed Release libraries. The four composed Settings
workflows produce 12 matching image hashes in both profiles. Native evidence
covers dark/light, DE/EN, 100/200% text and 760x480/1100x760 logical windows at
the current 1x output density. All four Release captures were visually inspected.
High-contrast/150% controls exist; their complete native/accessibility acceptance
and physical mixed-DPI checks remain open. Screenreader/AT-SPI is not implemented.

The existing compositor rendering/input/nested suites pass 3/3 in each new build
(Release 5.81 s, Debug 9.57 s). Their 42 additional image hashes match across
profiles: 54 native images per profile in total. Original lossless captures and
logs remain in the private build artifacts. The measured one-second Settings
idle interval has zero new frames and zero CPU ticks in both profiles; this is
a bounded observation, not a whole-session performance budget.

The accepted SDL direction now has reviewed exact SDL3 3.4.20, SDL_ttf 3.2.2,
FreeType 2.15.0, HarfBuzz 15.0.0 and Liberation Sans inputs. Actual execution
rejected the initial pure-software SDL window profile. The working path uses
OpenGL ES with explicitly selected Mesa llvmpipe in the VM. Three matching GCC
toolchain RPMs and 17 signed EGL/Mesa runtime packages were installed only in the
dedicated guest. The latter add about 205 MiB; Fedora Mesa brings X11 client
libraries, while SDL's X11 backend is disabled and no X server is used. The broad
GL development package is not installed: only pinned EGL/GLES2/KHR headers are
read as data into a private build SDK. Exact dependencies/licenses are recorded.

Native close exposed SDL's null dereference when the optional data device is
absent. One pinned null guard retains the existing normal cleanup. The identical
final app binary still SIGSEGVs with unpatched SDL in the independent negative
control and closes normally with the selected patch. Original failures/core
dumps remain. No clipboard capability was fabricated to avoid the bug.
An explicit owned cache directory contains Mesa shader data; both successful
test runs record 17 files / 2,115,023 bytes there. This is development cache
ownership, not StorageRef admission, a hard disk quota or production removal.

The ordinary Release build completes and the five selected existing core/broker/
boot checks pass (0.19 s). The old Root/User catalogue hashes and 17 static files/
profile are unchanged; its running/recover-handoff controller remains unresolved
historical bookkeeping. No lease, authority gate, SQL/XML contract, policy grant,
normal installed API or foreign project was changed. The NexaUI handoff now
records 103 requirements/13 surface groups, including installer/recovery UI.
All 223 host/snapshot/guest source hashes and 200 private dependency artifacts
match. The 25 cached UI inputs/patch and 28 frozen SQL/XML files are verified;
713 local Markdown file targets resolve. No selected UI test executable remains
live. The dedicated VM is still verified running with host-local AF_UNIX QMP.

This is the first native development app view, not a complete/admitted Settings
app or desktop. Actual system-settings backends, accessible controls, managed
cache/app/session provisioning, recovery coordination, the original system image/
installer, full experiment desktop and physical hardware acceptance remain work.
Next connect real session/application ownership and accessibility; keep final
visual choices with the user and the retained old intent protected.

Previous compositor input/nested checkpoint (af90a50):

The private compositor now has native pointer/keyboard seat delivery, click and
provisional Alt+Tab focus, serial-bound move/resize, implicit-grab retention and
device replacement. US fallback and backend-provided German keymaps are tested.
Explicit nested mode creates three native windows in a named parent Wayland
display; actual parent-composed pixels and two-output input forwarding pass.
Parent loss ends the child without fallback. See [current boundary](../../spec/graphics.md).

The final Release suite passes 3/3 (5.93 s); Debug ASan/UBSan with separately
instrumented patched wlroots passes 3/3 (9.83 s). The 11 rendering, 8 input and 3
nested workflow checks include 42 matching lossless image hashes per profile.
The same final binaries fail the intended nested input control with unpatched
wlroots. No test executable remains live. The input driver is a separate compiled
test program with an inherited socket; production has no driver symbols or FD
argument. No virtual-input global, extra installed library family, policy grant,
public capability, permanent process or lifecycle/catalogue mutation was added.

A narrow wlroots 0.20.2 patch forwards the actual enter coordinates before an
immediate click; archive/patch/preimage/result hashes are pinned. A fresh-only
unprivileged builder reproduces both dependency profiles and preserves licensing.
Earlier unpatched builds/evidence remain separate. This is private native input
and presentation acceptance, not physical devices/displays, a trusted session,
ordinary app readiness or the full experiment desktop. Next integrate a real app
view and session behavior while continuing the separate old-intent recovery work.
Final product/UI choices remain with the user.

All 215 host/snapshot/guest source hashes agree. The default Release build and
five existing core/broker/boot checks pass (0.24 s); 28 frozen SQL/XML files and
the retained old Root/User catalogues plus 17 static files/profile are unchanged.
The old controller remains running/recover-handoff bookkeeping, not a live app
or a settled operation. 698 local Markdown file targets resolve. No new package
was installed for this input work; existing xkeyboard-config 2.47 is used.

Previous first-render checkpoint (ec32e0f):

The optional C17 wlroots compositor now renders actual xdg-shell windows into
three private software outputs (1080p/1440p/4K), with maximize/fullscreen/restore,
independent teardown, negotiated v5/v3 consumers and frame pacing. Release passes
the native composed-pixel workflow in 4.68 s; Debug ASan/UBSan, including separate
instrumented wlroots, passes in 6.69 s. Twenty-four losslessly compressed captures
per profile and original logs are retained. See [scope/build/evidence](../../spec/graphics.md).
All 24 Release/sanitizer image hashes agree, including mapped-window and empty
idle checks; lossless original bytes were reverified. No test executable remains
live. All 206 host/snapshot/guest source hashes agree, 28 frozen SQL/XML files
remain raw/normalized unchanged and 689 local Markdown file targets resolve.

The minimal wlroots 0.20.2 dependency profile is separate from normal carrier
inputs; its exact archive and 18 signed RPMs are pinned. It adds 16 packages and
updates Expat/libdrm in the dedicated guest, with no X11/Mesa/Vulkan/session
backend adoption. The compositor option defaults OFF. The normal Release build
and five existing core/broker/boot checks pass (0.32 s). Preserved old Root/User
catalogue hashes and inspected static files remain unchanged. No installed
authority/capability, SQL/XML, policy permission, app dispatch, recovery settlement
or permanent graphics process was added. This is not a usable desktop or hardware/
security acceptance. Next add real input/focus and a visible private output before
app UI/session integration. Final design stays with the user.

Previous service-main checkpoint:

Root discovery and active retained-broker verification now require the actual
PID1 MainPID, no ControlPID and no pending Unit.Job, alongside the held authenticated
ProcessFD/current UID/label/unit/invocation/cgroup proof. Generic process and
activating-registration APIs stay unchanged. A self-only strict observer does
not substitute for a missing remote caller pin. See [boundary](../../spec/runtime.md#current-service-main-process-boundary).

Release passes six targeted process/identity/lifetime/boot/broker/admission checks
(3.94 s); Debug ASan/UBSan passes process/broker/admission 3/3 (1.24 s). Actual
same-unit children pass generic observation and fail strict remote/self main
checks; typed PID1 faults/drift/short reads preserve outputs/FD counts. Complete
Release/Debug builds pass. No library, schema, wire or policy permission changed.
All 202 host/snapshot/guest source hashes agree. All 28 SQL/XML files match the
retained v8 working-file hashes and Git-normalized committed blobs; 678 local
Markdown file targets resolve.

The verified stopped normal bootstrap was updated through update-owned, then
actual enforcing/capability-empty Root/User discovery/check/broker-check passed.
User replacement and Root-only restart with retained User passed; Root FDs changed
12 -> 11 on User departure. Final authority image SHA-256 is
67c564f3304fe76b58684d1b4c989fd1106f21e68a2a60f26bd91b4516743324.
Normal v2 epochs/account, 41 static file hashes and writer-lock inodes remain;
both owners' gates are closed and services stopped. The dynamic broker-unit slot
matches the selected new User exactly. The initial snapshot assertion included
that intentionally mutable slot; its original evidence remains and the corrected
check validates the controller's exact expected bytes, not a broad exemption.

The old failed fixture's distinct Root v6/User v4 catalogue hashes/controller/
17 static inputs remain unchanged and unsettled. No target dispatch, original
identity replacement, migration, release or purge occurred. Old host process
records contain no independent original disk-object witness; this is a provenance
gap, not permission to infer continuity from UUID/path/content. Next bind reviewed
continuity and current image/unit/epoch admission into the separate v8 coordinator.
Normal-service proof alone cannot authorize that old isolated authority's release.
Independent compositor/early UI bring-up may proceed on a private development
surface while that legacy continuity boundary is reviewed. Review exact wlroots/
SDL dependencies first, preserve the retained intent, and keep final visual
choices with the user; a graphical test is not app-lifecycle acceptance.

Previous native interruption checkpoint:

The explicit v8 maintenance contract now records cross-boot interruption separately
from a successful v3 run close. Both owners enroll immutable AETHINT1 current-
actor/provenance/fencing metadata, then Root BEGIN / User QUIET / Root RELEASE /
User EXIT append distinct immutable receipts with atomic local launch/lease effects.
Original completed start results, process records and old closure receipts remain
unchanged. Another owner/boot replacement enrolls a fresh binding and continues
the actual historical chain without adopting a run or rewriting earlier steps.
See [native boundary](../../spec/runtime.md#cross-boot-interruption-transactions).

Normal creation stays v2; v7 -> v8 is explicit under the retained owner lock.
Open validates exact supported versions without migrating. SQL v1-v7/XML remain
unchanged. The native path requires original successful completion and original
app evidence; early/unfinished starts and general grant/access settlement remain
separate. Both current actors, original machine/catalogue/volume and anti-clone
continuity, all prior actors/holders and future-effect fencing must be independently
verified by the caller. Stored hashes/boot IDs/receipts are not those proofs.

Final Release catalogue.interruptions passes twelve controlled-evidence scenarios
(109.86 s); Debug ASan/UBSan passes the same twelve (136.34 s). They use real SQLite,
every mutation/commit fault, actual child exits before/after commit, synthetic
boot/runtime replacement, terminal historical retries, old v3 closing/quiet,
wrong predecessor, current-context drift, inconsistent redundant binding metadata,
gate drift, overflow and missing start evidence. Original process records and old
close/quiet receipt bytes are included in the final preservation assertions. Host contracts/
migrations pass 50/50 (200.260 s), including both v8 owner migrations at every
boundary, process exit, full disk and source drift. Byte contracts pass 24/24
(0.062 s). Full Release/Debug builds pass. The shared-history/refactor checkpoint
passed original lifetime, strict Root recovery, paired recovery and interop 4/4
(357.54 s); its exact source snapshot/log are retained under ignored local/.
Final changes only strengthen new interruption binding-row validation/tests;
final populated v2-v8 Python/native interop also passes (3.74 s). All 202 final
host/snapshot/guest source hashes agree, 25 existing SQL/XML files are byte-for-byte
unchanged and 668 local Markdown file targets resolve. No installed acceptance follows.

The retained real fixture is NOT upgraded, settled or retired. No new live app
test, carrier reboot, installed service/wire, public capability, policy permission
or dependency adoption is part of this native contract work. Next integrate
independently admitted current actors and actual cross-boot continuity/fencing;
different-boot metadata alone cannot authorize the quarantined v6 release.

Previous boot-observer checkpoint:

Native linux/boot now joins retained/named procfs boot objects, bounded raw/cached
UUID checks and held PID/cgroup/time namespaces, including child bindings. It
rejects drift/fork reuse without acquiring privilege or changing lifecycle state.
AETHBOT1 is a canonical 48-byte observation, not machine/volume/clone provenance
or original-process end proof. Host scope still requires independent admission.
See [runtime boundary](../../spec/runtime.md#read-only-current-boot-observation).

The build-local administrative inspect-boot path now checks the original protected
profile/catalogue/handoff/starter/app/User metadata and sealed code against the
current boot in a private mount namespace. Release and Debug ASan/UBSan both pass:
original boot 751f2367-b57d-4344-ba57-5474d04e090e differs from the current
7e6602b8-721b-4aa1-8b31-33cbb81d5c8c; original catalogue revision stays 17.
This does not admit replacement actors or prove safe closure. Normal loader
boot checks, installed fixture files, SQL/XML, policy and capabilities stay fixed.

Native boot suites pass Release 2/2 (0.09 s) and Debug ASan/UBSan 2/2 (0.18 s),
including actual child-local namespace changes and identical-content boot-file
substitution. The first PID-for-children test exposed a real missing-binding
classification error; ENOENT now latches drift, and the expected outcome passes.
Full Release/Debug builds pass, all 196 host/snapshot/guest source hashes agree,
647 local Markdown file targets resolve, and host byte contracts pass 23/23
(0.039 s at final check). No new confined
service acceptance is claimed. The existing VM is running and enforcing; this
boot-observer checkpoint did not reboot it or redispatch the failed app test.
The administrative inspection now refuses missing/symlinked locks and any
journal/WAL/SHM before ordinary SQLite open. Its positive case and five private
negative shadow cases pass in Release and Debug with unchanged inputs. It still
requires independent quiescence/fencing; it is not a read-only SQLite API or
permission to inspect a live authority.

After the probes, the original controller/pending intent, Root v6 catalogue hash
and all 17 persistent static-input hashes still agree. Dispatch is uncertain,
lease running, handoff observed/revision 2, with no closure/recovery-owner rows or
hot journal. At that checkpoint the separate cross-boot transaction contract and
independent provenance/current-actor/future-effect/holder admission were pending;
do not migrate or settle the retained fixture from different-boot metadata alone.

The user prefers working foundations such as SDL3 for early app UI without
waiting for NexaUI completion, with per-app migration when suitable. wlroots
remains the compositor. Exact SDL/text/widget dependencies are not yet installed
or pinned by this direction. App models/commands/persistence must remain toolkit-
independent; SDL3 itself supplies no complete widget/accessibility stack.

The published NexaUI main was inspected at 05e28bb16c1e60ac339cd9428ff2d3afbafc21f6
through a read-only remote query and local committed objects. Public widget/text/
Wayland families exist, while the Linux build unconditionally requires X11 and
documented numeric-input/nested-overlay scope and native acceptance remain work.
No fresh NexaUI build or performance comparison is claimed. At the user's
request, NEXT_AETHERI_UI.md in its local project now holds 102 unique requirements
and 12 app/shell surface groups, each with priority/source status/acceptance.
It includes delivery order, responsibility boundaries and a maintainer return
format. Existing NexaUI HEAD and all 104 prior local status entries/file hashes
are unchanged; only the authorized handoff file was added. No message was sent
to another chat and no NexaUI code or consumer pin was modified/published.

Before this boot-observer checkpoint the carrier was again found stopped. Dead-process and
failed AF_UNIX connection checks preceded preservation of host metadata/socket
and an offline boot. Current observed boot is
7e6602b8-721b-4aa1-8b31-33cbb81d5c8c, enforcing. The old failed Root v6 catalogue
hash below still agrees with its original pending intent/running lease/no
closure. Cross-boot settlement remains unfinished; the read-only observer above
cannot settle that attempt. Current UI steering was handled before this work.

The user requires an early experiment environment and general VM support. They
selected a complete Aetheri desktop first, persistent changes until explicit reset
and individually permitted personal files/network/devices. The [plan](knowledge/experiment-environments.md)
and milestone 4a record isolation, resource pressure, reset/interruption and AI
acceptance. This feature is planned; there is no complete Aetheri desktop yet.

At this continuation the recorded QEMU process was dead and AF_UNIX QMP refused
connection. The stale socket and old host process/command/source metadata were
preserved under ignored local/carrier-interruption-2026-11-04 before booting the
existing disk offline. The user confirmed that the Codex usage limit interrupted
agent work. Cause/time of VM-process termination were not separately established.
That first observed guest reboot was 525715ac-42d5-4059-8658-df208f0e501b, enforcing. Prior PID1
handles and /run retention units are gone; they cannot be reconstructed from IDs.

The old failed intent remains quarantined at its original path and pending
recover-handoff. Root v6 catalogue SHA-256 still equals
bdfca485ed5079d62afc2270946cfd9999a354b2240a51c9914b4c64e9291f2d:
uncertain dispatch, running lease, observed/revision-2 handoff and no closure.
User v4 also has no closure. No old catalogue, controller or protected fixture
was migrated/reset/retired. Same-boot retained-handle recovery can no longer
settle that attempt. A separate reviewed cross-boot/lost-handle protocol is needed;
do not fabricate old process pins or turn a fresh successful test into settlement.

Native paired-owner v7 recovery now exists only in separate temporary-catalogue
tests. Both owners independently enroll AETHRCP1 and can close a successful
original run after User replacement without changing start results or v3/v6
checks. Further replacement needs another admitted pair; Root can forward a
historical release to a newer User without restoring a lease. Actual actor/drain/
holder admission remains a caller obligation. No installed wire or live recovery
is implied. See [paired contract](../../spec/runtime.md#paired-owner-closure-transactions).

Release passes 4/4 targeted native suites (357.58 s): original lifetime, strict
Root recovery, paired-owner recovery and populated Python/native interoperability.
The pair suite has 12 controlled-evidence scenarios, with real SQLite mutation/
commit faults and child exits. Host contracts pass 48/48 (107.271 s); the two v7
migrations were repeated after the final SQL/frozen-v6 guard (24.655 s). Host byte
vectors pass 22/22; VM/tool guards pass 50/50 (3.047 s). Debug ASan/UBSan paired
recovery passes all 12 scenarios (135.00 s); complete Release/Debug builds pass.
All 191 host/snapshot/guest source hashes agree. Local Markdown file targets pass
(628 links; anchors/external URLs excluded). No old held catalogue was used for
these mutations. The experiment-desktop feature still has no runtime acceptance.

This does not restore lost guest-boot evidence or enable installed recovery.
Older statements below about retained live handles describe the 31 October
checkpoint, not the current boot. Normal Root/User v2 retain two closed gates
each and the controller is stopped with no pending operation.

## Confirmed

The user accepted the minimal extension/P0 plan and requested implementation of
versioned database/API contracts and migration tests.
The active user goal is to implement the full Aetheri project. The headless carrier
and core below are progress toward that scope, not a smaller completion target.
All earlier architecture, ownership, lifecycle and language decisions remain.
P0 carrier/resource selection is accepted as the development direction, not the
final Aetheri distribution or runtime acceptance.

## Implemented

The separate --recover-active test now selects Root v6/User v4, preserves the
original App/User across Root exit and transfers the actual four observation
handles. Original Root writes one protected immutable AETHUSR1 User expectation
before transfer; the controller independently compares it to its held original
User ProcessFD and actual unit/invocation/cgroup/label. Replacement code adds
explicit original-User reidentification, User-side current-image checks and the
native recovered-close sequence. These later RPCs are not runtime accepted yet.

The first actual attempt reached Root OBSERVE with the original App/User still
active, then failed at a combined source/process-image expression. The source
correction removes Root's direct cross-UID proc/exe check: current app-image
validation remains in the authenticated reidentified User before closing. No
capability or MAC grant was added. This correction is compiled, not deployed in
the held attempt. App and User subsequently expired under their test time limits.

The exact attempt remains at /var/lib/aetheri-p0-execution-runtime-test, pending
recover-handoff. Root/app/User are terminal; PID1 retains FOUR original handles
and the process-free retention target remains active. The native Root v6 dispatch
is uncertain, generation lease running and handoff observed/revision 2. No closure
or recovery-owner row was created. Its protected catalogue hash and all owned
static inputs still agree. No app/operation/lease was replaced or released.
Evidence is under the guest build's
active-recovery-failure-0f04cd9ae92d4500a42177e3e14de76b. Do not retire/reset it.

Next implement a separate replaced-User admission/quiescence protocol and original
ended-app restoration for this same intent. Do not weaken the v6 original-User
check, reconstruct a ProcessFD from a PID or discard the retained store to rerun.
The ordinary carrier remains verified/quiet with normal v2 gates closed. The
temporary active-test policy/units are intentionally retained for reconciliation.
The full desktop/apps/image/hardware objective remains active.

Host guards pass 50/50 (2.720 s). Release and Debug ASan/UBSan builds pass; the native
User-record reader agrees with the original handoff in both administrative private
mount-namespace checks. Those are metadata checks, not confined recovery acceptance.
No existing SQL/XML, dependency family, production service or policy rule changed.
All 187 host/snapshot/guest source hashes agree; 602 local Markdown file targets
exist, excluding anchors and external URLs. The held failed catalogue/static
inputs were rechecked after the metadata probes and remain unchanged.

Previous native-transaction checkpoint (31 October 2026):

Root-only v6 now adds immutable current-recovery-owner rows and separate native
BEGIN/RELEASE/GET functions for a successfully started original run after Root
replacement. They require original profile/process/start evidence, the current
handoff OBSERVE, same boot/authority/account and the still-original User. BEGIN
preserves an old uncertain closure/receipt; RELEASE still requires independently
verified original end, holder/future-effect fencing and authenticated User quiet.
A further Root replacement must OBSERVE and BEGIN again. Completed retries never
reopen gates, enroll a new owner or change original receipts/start operations.
Normal close/get APIs remain strict about their original runtime.
See [Root recovery](../../spec/runtime.md#root-replacement-closure-transactions).

The explicit v5-to-v6 migration changes no existing row and no frozen v1–v5 SQL.
It adds one table and replaces only the system closure-insert trigger with original
admission plus a narrow recovery alternative. Normal creation/installed catalogues
remain v2; User stays v4. No new dependency, wire capability or resident process.
Native tests use real owner SQLite and controlled already-admitted kernel/peer/end
inputs. This is not an installed active-run coordinator. The actual confined
--handoff fixture below still closes its run before Root replacement. Wiring the
new path to actual live Root/original-User communication and retained-handle end
proof, then replaced-User/early-start recovery, remains the next integration work.

New recovery transactions pass Release (116.60 s) and Debug ASan/UBSan (150.27 s),
with 15 controlled-evidence fixtures each. Host contract tests pass 46/46, with the
two v6 migrations repeated after their frozen-v5 guard; host VM/tool guards pass
49/49. Release regressions for original lifetime/handoff journals, Python-to-native
schema interoperability and the existing authority bridge pass 4/4 (278.75 s).
Interop now opens populated Root v2–v6/User v2–v4 without changing rows/file bytes.
These are native transaction/memory and host checks, not installed active-run or
physical hardware acceptance. Original User provenance must be independently
retained before a failure; a current unit/domain or broker UUID is insufficient.
Final read-only inspection confirms enforcing policy, normal Root/User v2/two
closed gates each, no pending normal work and no temporary execution infrastructure.
All 187 host/snapshot/guest source hashes agree; 599 local Markdown file targets
exist (anchors and external URLs excluded).

Previous confined-transfer checkpoint (31 October 2026):

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
