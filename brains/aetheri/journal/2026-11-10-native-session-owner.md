# Native desktop owner and session boundary

10 November 2026. The existing C17 desktop owner now runs the complete current
protected desktop workflow and observes its original active local session.
This advances normal-session integration; it is not normal login, app admission,
trusted lock/suspend or first-version acceptance. No USB image build or flash.

## Implementation and choice

Reuse `src/boot/desktop.c` and add the small libsystemd-only
`src/linux/session.{h,c}` observer, rather than maintain a second product owner.
A one-shot startup check would miss later loss. Reusing the audio guard would
import its distinct two-process/backend ownership contract. The shared Linux
self-observer instead owns only the original main-thread process/session facts;
audio behavior stays unchanged. No new third-party family, process or thread.

The observer retains its self pidfd, mandatory kernel credentials/cgroup ID and
original session/scope/cgroup strings, with a monitor subscribed before capture.
Each check reobserves even without an event; the first loss is irreversible.
Wrong process/thread use preserves outputs and pointers. It requires trusted
host views and is neither authentication nor an atomic future-context lock.
The public close enforces ownership; unpublished constructor failure uses private
cleanup so it cannot leak through a rejected public close.

The C owner checks before fresh effects and integrates monitor events/deadlines
with its existing poll loop. It requires four observed zero capability sets and
NNP, creates a fresh private runtime/cache and retains original child barriers,
pidfds, FD-transfer rejection and Files-first shutdown. Uncertain/dirty document
choices are not automatically discarded. Build constants bind separate private
test ELFs to the protected runtime without changing image defaults or admitting
caller-selected code. Files carries the selected regular and bold Terminal fonts.

## Native build and behavior evidence

Lab VM UUID `033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 5548, boot
`8b915c0c-ab36-4cce-a8a3-e84e1c12627c`, SELinux enforcing 1. Original vm/p0
remains stopped/quarantined. Revalidate live state before another dispatch.

Final standalone source `/var/tmp/a-session-081ce4b2da82`, work
`/home/aetheri/session-081ce4b2da82`: all ten Release/memory compile/run commands
pass, including both native test binaries and both default owner builds.
Strict warnings and GCC analyzer remain enabled; memory runs retain ASan/UBSan,
leak detection and halt-on-error without suppression. The model uses controlled
logind metadata with real kernel pidfd/eventfd/poll/thread/fork behavior. It tests
no-event refresh, immutable losses, query/monitor failures and FD balance.

Pinned CMake Release source `/var/tmp/a-session-cmake-f0b9f455aa35` and memory
source `/var/tmp/a-session-cmake-62894b4d7531` each configure, build the separate
owner/session-test targets and pass `linux.session` as UID1000. Compile databases
confirm the three relevant compilation commands and memory instrumentation.
The old configure/environment receipt is independently hash-pinned before use.
The deliberate no-event-snapshot mutant compiles but fails `snapshots>previous`,
exit 134. It is test-only, never installed; core dumps are disabled.

## Actual KMS workflows and loss

The explicit wrapper stops before effects; Root retains its original pidfd,
scope/service/group observations. Same-process exec changes only the observed
image to the independently hashed native owner. No Python owner remains.
Both successful full workflows use bochs-drm 1280×800/74.994 Hz, ordinary UID1000,
zero checked capabilities, NNP and a separately checked private 1 GiB tmpfs.
The compositor reports `keyboard_layout=backend`; ASCII input success is not
explicit US-layout acceptance or external Compose/IME evidence.

Final full workflow source `/run/a-ns-ef00c554367c`, output
`/root/aetheri-native-desktop-ef00c554367c4153a1c5951bbde8ac9f`;
host controller 45646 and native input driver 69655 both exit 0. Its build-bound
documents are `/run/a-native-doc-a189b0498cbc`. Actual input opens Files/editor,
saves exact `hello alpha\n`, writes `desktop` to `owner-proof` through the own
shell and opens Settings. Viewed captures show the saved editor and four-window
desktop with actual output information. They are development UI evidence.
The original saved editor is closed through input during Files-first shutdown;
all five direct children exit 0. Nine original actors and nine code-map records
bind to selected runtime bodies plus the explicit native owner. The original
service/session/groups end; controller reaches `retired` and tty1 returns.
This final run also exercises protected-parent population/bottom-up handover and
ordinary-user document verification in the latest test helper.

An earlier successful native full workflow is retained at
`/root/aetheri-native-desktop-d4dd9c62948d4217b9ace0b9f4736cd6`.
The separate active-VT-loss case is
`/root/aetheri-native-desktop-ea63d4ac5ee34e47b5f059878b896e46`:
Root changes to original tty1, without sending SIGTERM. Native output records
`AETHERI_SESSION_LOST result=0 loss=2`; no app-role spawn occurs. The expected
native/service exit 1 is checked separately from the successful controller exit 0.
All three original actors/session/groups end and the fixture reaches `retired`.
This does not implement lock, resume, device revocation or production drain.

## Failures, audit and continuation

Retain early compiler/header/harness/analyzer failures and their exact inputs.
The first native full workflow passed app behavior but the host driver mistook
the acknowledged finish operation's pending window for a failure. Its original
controller timed out while retaining observations. After closing the existing
saved document, explicit manual retirement checked the original end and removed
only its exact PAM fixture; it is not a passing automatic controller or recovery.
The corrected driver polls that same pending operation, never replays dispatch.

Independent streaming audit verifies all 2,718 exported files and 2,510 submitted
inputs, four separately host-hashed bound owner ELFs, final C sources, CMake
instrumentation, complete selected code maps and native failure evidence.
Archive `local/full-desktop-build-2026-11-09/native-session-evidence-v1.tar.gz`:
10,889,413 bytes, SHA-256
`38e51374416dd431a14713480d1301fe97f11cef8e59ad325557d5284d6de92f`.
Its protected guest origin is
`/root/aetheri-native-session-evidence-5c534095bbf94799bae5452bd8bced6c`.
User-produced selected artifacts are exported using ordinary-user file rights.
The existing independently audited 200-body runtime is rechecked, not duplicated.
Final native controls pass 55 without skips; host controls pass 71 with 16 platform
skips (87 cases). Packages and original udev 693/invocation remain unchanged,
enforcing 1, no test PAM/local seat session or selected code mapping remains.

Next connect normal session/service provisioning and device integration, complete
the system runtime closure and retain the broader first-version requirements.
Normal installed authorities/public capabilities and old quarantine stay closed.
NexaUI handoff remains delivered; no catalogue or new chat message was needed.
