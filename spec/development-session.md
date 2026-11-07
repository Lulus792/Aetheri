# Running the development desktop

6 November 2026. This starts the native Aetheri compositor and selected UI in the
dedicated Fedora carrier. It is a development workflow, not the finished desktop,
an Aetheri system image, normal app admission or the resettable experiment product.

## SDK upgrade checkpoint — 7 November 2026

The source manifest now selects NexaUI 0.1.23/97a4264. Its checked private app
builds are `/home/aetheri/build/u23r` and `/home/aetheri/build/u23d`; these do not
replace the controller's fixed `session-release`/`session-debug` builds or shared
`/home/aetheri/source`. SDK preparation, application rebuilding and controlled
shared-source promotion remain separate operations. Existing CLOSED records and
old 0.1.18 artifacts remain historical. See the
[consumer upgrade record](../brains/aetheri/journal/2026-11-07-nexaui-upgrade.md)
for exact executed tests and remaining promotion/resource boundaries.

## Use it

For automated native keyboard probes, `tools/run_vm.py` provides
`tap_keys(*qcodes)` on the independently verified disposable VM. It sends explicit
press/release events for one chord in a single `input-send-event` request, without
`send-key`'s delayed release timer. A returned QMP reply is dispatch acknowledgement,
not proof of application input, current focus or a rendered result. Do not replay
an uncertain press. Tests must observe the original target and verify the actual
text/result; stable screenshot pairs alone can preserve the wrong query.
See the [input investigation](../brains/aetheri/journal/2026-11-05-native-key-input.md).

Run commands from the Aetheri checkout with the configured Python 3 interpreter.
To boot an already prepared, stopped carrier with its native QEMU window:

```sh
python tools/run_vm.py --offline --console boot
```

That command remains attached to the VM. Use another terminal for the session:

```sh
python tools/development_session.py start
python tools/development_session.py start --desktop
python tools/development_session.py start --desktop --compose
python tools/development_session.py start --desktop --compose --settings-ui nexaui
python tools/development_session.py start --desktop --compose --settings-ui nexaui-owned
python tools/development_session.py start --desktop --compose --files-ui nexaui-owned
python tools/development_session.py start --desktop --compose --settings-ui nexaui-owned --files-ui nexaui-owned
python tools/development_session.py start --desktop --compose --terminal-ui nexaui-owned
python tools/development_session.py start --desktop --compose --settings-ui nexaui-owned --files-ui nexaui-owned --terminal-ui nexaui-owned
python tools/development_session.py status
python tools/development_session.py stop
```

Use `start --profile debug` for the existing ASan/UBSan build. Selecting a start
profile does not change an existing session. `--compose` requires
the matching optional [Compose build](compose.md), records that choice with the
original attempt and enables native Compose/dead-key input without a new daemon.
It is not a general external IME or trusted input/session implementation.
`--settings-ui nexaui` explicitly selects the pinned [NexaUI Settings development
view](nexaui.md) for a new desktop session; omit it to keep SDL. The reviewed
Release/Debug build paths are fixed in `spec/nexaui-inputs.json`. This option
does not switch an existing session, the Settings-only mode or the other apps.
Prepare the matching private SDK/consumer before starting. It remains a
provisional original-model view, not the accepted final Settings design or
persistent device configuration.
`--settings-ui nexaui-owned` selects Aetheri's own three-page Settings presentation
using the same reviewed SDK. Build `aetheri-settings-nexaui` first with
`AETHERI_NEXAUI_PREFIX` selecting that profile's private prefix. This also binds
the fixed executable, five presentation-source inputs and the bounded CMake cache,
including its SDK prefix/package path and sanitizer choice. Its own model is
independent of the unused upstream example's copied model. A fresh session can
bind an edited app model without rebuilding the SDK; an existing owner still
refuses changed inputs before a new process. Both NexaUI choices use eight runtime
images and retain the original launch/pipe/shutdown protocol. Neither changes the
SDL default or an already selected session.

`--files-ui nexaui-owned` independently selects our own [NexaUI Files](files.md#opt-in-nexaui-view).
Build `aetheri-files-nexaui` with the same profile's reviewed SDK prefix first.
Files can coexist with SDL Settings or with the separately bound own NexaUI
Settings view. Its `aetheri-files-v1` binding covers the executable, nineteen
model/controller/dependency source inputs, five presentation/build/notice inputs,
the bounded CMake cache and exact SDK metadata/library. It requires neither the
unused SDL Files executable nor the upstream Settings example. No new process is
started when those inputs drift; restoring them permits a fresh explicit request.
This is developer consistency, not sealed-code or atomic execution admission.
The optional UI choice applies only to a new desktop start. Existing-window reuse,
the original document-close fence and ordered app retirement keep the same protocol.
Both own views share one SDK library entry in the eight-image runtime inventory.

`--terminal-ui nexaui-owned` independently selects our own
[NexaUI Terminal presenter](terminal.md#opt-in-nexaui-presenter). Build
`aetheri-terminal-nexaui` with the matching reviewed SDK prefix first. This choice
applies to the normal Terminal slot and to direct-editor windows opened by either
Files view. The editor executable and original shell remain their own programs.
There is no fallback to the unused SDL terminal or in-place switch of a live
window. Existing original processes are reused until their process and pipe end;
a later explicit request creates a fresh selected process.

Historical `aetheri-terminal-v1` selections bind twelve model/I/O source inputs,
four presentation/build inputs, the executable, CMake cache, SDK metadata/library,
the raw UI-input pin and the regular/bold font plus license hashes. Settings,
Files and Terminal choices can be combined; the SDK library is recorded once.
The original SDL terminal is not required when the new presenter is selected.
Root checks selection before dispatch, and the user supervisor recomputes that
same binding before creating children. Every fresh Terminal or Files role start
rechecks its dependent Terminal selection; drift returns an error without a new
process. Reusing an already running original window does not replace it because
mutable source changed. Missing Terminal fields keep historical SDL meaning.

Fresh `nexaui-owned` selections now use additive `aetheri-terminal-v2`. The
original twelve source inputs remain, with four spawn/workspace inputs and the
separately named starter executable/source digest. The normal Terminal receives
that exact `--pty-starter` path for original-session tabs/splits; Files receives
only the selected Terminal executable, retaining its single-editor save/close
contract. Both dependent fresh app roles recheck the expanded selection. The
starter is part of the recorded binary inventory, not a new supervised daemon.
V1 validation/closed-history behavior remains intact; no old receipt or closed
DRM record is rewritten to match newly compiled bytes. See
[tabs and panes](terminal.md#original-session-tabs-and-split-panes).

These are development consistency checks on mutable build-user inputs. They do
not provide publisher trust, atomic image pins or loader admission. A live Files
process still starts its later document children through its existing internal
mutable program paths; per-document protected image admission remains separate.
The new terminal keeps its SDL native input/process host and now has scoped
[native read-only text transport](terminal.md#native-read-only-text-transport).
Complete accessibility,
the accepted final chrome/material design and normal app/session admission remain
work. See the [owner record](../brains/aetheri/journal/2026-11-06-terminal-owner.md).

An already running headless VM must be stopped gracefully before selecting a different frontend;
do not start another QEMU process against its disk. `run_vm.py status` checks
QMP identity and the recorded host process. `run_vm.py poweroff` requests guest
shutdown. The native viewer is the existing QEMU SDL2 frontend. The desktop and
default app views use the reviewed SDL3 profile; explicit NexaUI Settings/Files use
their own software presentation, and the NexaUI terminal paints into its original
SDL host. No new viewer package is needed.

The viewer explicitly disables OpenGL and window-close shutdown. QMP remains
host-local AF_UNIX, networking remains restricted in the example, and no VNC,
host directory share, clipboard bridge or new network listener is added. Actual
QMP display options confirm `type=sdl`, `gl=off`, `window-close=false`. Native
Windows window metadata identifies the running QEMU window. Host-window pixels,
mouse grabbing and Windows keyboard forwarding still need separate acceptance:
both available native-UI inspection paths timed out. Guest pixels/input are
tested through QMP; do not call that a completed Windows interaction test.

Choose one start command. `--desktop` opens the [dock/search client](desktop-ui.md)
with Settings, Terminal and Files launched on demand; closing one leaves the desktop
and other app running. Ctrl+Q while search has focus asks the owner to stop
Files (including its document window), Terminal, Settings, desktop and compositor
in order. Existing-window search remains available while closing; finish any
open document there before the display retires. In the original Settings-only mode, Settings can close with Ctrl+Q.
The development supervisor then ends the compositor
and retains the original service result. Run `stop` to retire the inspected unit
and return to tty1. `stop --token TOKEN` selects a specific original development
attempt. Repeating a completed stop returns history and cannot stop a newer one.

With `AETHERI_EDITOR=ON` in the selected build, the [own modal editor](editor.md)
can be started by its full build path inside Terminal. It is a foreground shell
command, not another fixed desktop app slot or an admitted normal application.
Close it through its save/close workflow before ending a development test session.

Files opens the session token’s `documents` workspace, initially empty. Windows
can close/reopen without deleting it; session retirement retains it too. This
is separate from normal home-directory provisioning and document grants. Enable
`AETHERI_FILES=ON` and `AETHERI_EDITOR=ON` in the selected UI build. The normal
Terminal slot and the Files-owned Editor window have separate window identities.

## What runs

The host command verifies the registered P0 VM over QMP and calls the root-only
guest controller over its existing SSH connection. The guest independently checks
the selected DMI UUID/product and enforcing SELinux. Only the fixed Release/Debug
build paths are selected; no user-supplied command, device or service name is run.
The dedicated development account, source checkout and builds are trusted; that
account already has administrative access to this disposable carrier. This tool
does not establish a new privilege boundary against a hostile build user and must
not be installed as a general root delegation endpoint.

The controller checks that seat0 and tty8 are free. tty8 must be outside the actual
automatic/reserved VT range; the carrier currently uses NAutoVTs=6/ReserveVT=6.
It creates one transient Type=notify unit as UID/GID1000 with PAMName=login,
NotifyAccess=main, RemainAfterExit=yes, no capabilities, NoNewPrivileges, no restart
and no forced SIGKILL. It changes no login-manager configuration or policy.

The ordinary-user Python supervisor is a development tool. It retains actual
Popen child handles, starting compositor then Settings or the selected desktop.
The desktop mode has separate on-demand Settings, Terminal and Files slots, a private
inherited Wayland connection and a bounded version3 fixed-command channel.
New format1 records add `launch_protocol: 3`, digests for the seven fixed binaries
and the existing Mono font/license. Explicit NexaUI selection replaces the selected
SDL app entries with their candidate apps and one shared library: eight runtime images
are recorded. Additive `settings_ui`, `settings_profile` and `settings_binding`
retain the exact selection, source revision/version, build path, model/image
hashes and raw metadata/input-pin hashes. Old records with no such fields keep
their historical SDL meaning. Historical protocol1/protocol2 records remain readable without
upgrading/rebinding them. It notifies PID1 only
after the selected first UI has submitted its first frame, not merely printed
READY. This is a development startup signal, not the Aetheri app READY protocol
or proof of admitted code, data, grants or visible host presentation.

The owned profile additionally carries `presentation.kind: aetheri-settings-v1`,
the exact own executable/digest, five source digests and raw cache digest. Its
field/source shape is frozen; a different source set requires a new kind with
explicit historical validation. Old upstream profiles have no presentation field
and cannot be reinterpreted as owned views. Historical validation uses recorded
values only; it does not read the current cache, source or SDK.

Files has separate additive `files_ui`, `files_profile` and `files_binding` fields.
Missing fields retain the old SDL meaning. Its presentation kind is
`aetheri-files-v1`, with nineteen model/controller/dependency inputs and five
presentation/build/notice inputs. The exact source sets and shape are frozen for
that kind. Settings and Files selections are validated independently and cannot
be reinterpreted as one another. Files uses the same canonical digest algorithm;
the existing Settings binding algorithm and recorded shapes are unchanged.

The NexaUI choice is checked before seat/history/start effects. The root controller
passes the canonical binding digest to its original unprivileged supervisor,
which repeats the check before creating children. Before every new Settings
spawn it compares the current selection with the original one. A changed or
unavailable candidate returns the existing launch-error reply, leaves the
desktop alive and never falls back to another toolkit. An already running child
is still reused through its original handle. Restoring exact inputs requires a
fresh explicit request; an unknown request is never replayed automatically.
Completed records remain historical: stop retries do not inspect or rebind a
newer SDK. The app uses the same `org.aetheri.Settings` identity and the existing
font/cache/diagnostic command shape; its FRAME/MODEL diagnostics are not a new
PID1 or production app-readiness contract.

The corresponding Files check runs before a fresh Files spawn and uses the same
original root-to-user binding and existing F request/reply channel. It does not
rehash or replace an already retained live child when focusing its window. A
Files-owned document remains a separate obligation: close waits for that original
Terminal/editor pair, and global search can return to the editor to save it.

These are developer consistency checks over mutable same-account inputs, not
publisher trust, a sealed package store, atomic execution pins or production
admission. The dedicated build account already controls the source/controller.
No registry, accessibility bus or other new normal service is started by this
option. Native assistive transport has separate private-test evidence; real
session-bus provisioning, human assistive acceptance and final UI remain work.

The supervisor sleeps on child pipes, its optional command channel and signal
wakeups. Settings-only close stops the compositor. Desktop mode keeps the shell
alive; an explicit or owner-requested stop ends Files and its document, Terminal, Settings, desktop, then
compositor. Each child is waited/reaped and its pipe drained before terminating
the next role, including failure cleanup. Requested termination
cannot hide a nonzero cleanup/sanitizer result. Stuck children retain their handles;
the tool does not silently escalate to force-kill. The existing sd-pam child is
PID1/PAM bookkeeping, not another Aetheri service. The root controller itself is
short-lived; no privileged Aetheri session daemon is installed.

The current programs are ordinary unconfined carrier development processes.
Production compositor/session/app domains, trusted login/lock, resume and normal
launch/data mediation remain separate work. The Python supervisor is not selected
as a dependency of the future production desktop.

## Files and retry behavior

| Location | Owner and purpose |
| --- | --- |
| `/var/lib/aetheri-development-session/writer.lock` | Root, stable exclusive controller lock |
| `active.json` in that directory | Root, version1 development intent and original unit/invocation/process/result |
| `history/TOKEN.json` | Root, closed original attempts; never changed by historical stop |
| `active.next` | Root, an interrupted/uncertain write; never removed automatically |
| `/home/aetheri/development-sessions/TOKEN/runtime` | UID1000, private development Wayland socket directory |
| `cache` beside that runtime | UID1000, development-view shader cache with the existing soft budget |
| `documents` beside that runtime | UID1000, initially empty workspace for Files; retained across window and session close |
| system journal | Native and supervisor diagnostics, filtered to avoid per-control layout spam |

These dedicated folders make the development footprint explicit. They are retained
for inspection; there is no complete-removal command yet. They are not production
DataEpochs/StorageRefs, and their token is not a grant or bearer capability.

The root writer uses bounded, owner-checked records, an exclusive next file,
file flush, atomic replace and directory flush. A lost flush acknowledgement
never triggers restoration of an old record. One record is active; each later
attempt archives a closed record without overwriting different history.

The stored stages are preparing, starting, running, stopping, exited and closed.
Starting and stopping are written before dispatching their effect. Pending stages,
an uncertain next file, a different boot or a replaced original unit require
manual reconciliation. The tool neither retries a possibly submitted effect nor
adopts a matching-name candidate. It is not production power-loss recovery.

The status response is version1 and separates the protected record from current
observation: absent, pending_unbound, live, original_exit, recovery_required or
historical_closed. A bound observation checks the original boot/invocation/main
process and selected unit configuration. UID/PID/start ticks and CLI observations
remain development checks, not normal app authorization or durable process proof.
`exit.code` is PID1's wait reason (1 means the program exited); `exit.status` is
the actual program exit status. A normal successful supervisor has status0.

Stop checks the original unit before signalling. RemainAfterExit preserves the
original main-process result until it is captured. The original held process pin
must become terminal; service population and logind seat state are inspected
before the unit is retired and tty1 restored. These fixed-program checks cannot
settle production holders, leases or the older quarantined execution attempt.

## Two-app checkpoint — 5 November

Both actual Release and ASan/UBSan DRM sessions now launch Settings/Terminal,
reuse a running Terminal, execute the own shell and close all four direct native
children successfully before original-unit retirement. The private-compositor
`desktop.owner` test additionally holds a real shell job and then an actual stdout
writer after Terminal exits; later roles stay alive until the corresponding
obligation ends. This is development process/pipe ownership, not app admission,
all-descendant/lease settlement or production restart recovery.

Native stdout journal entries are associated with the PAM login session scope,
not necessarily the root transient unit's `_SYSTEMD_UNIT`/invocation filter.
The original live controller-bound process was checked, its current-boot stdout
entries read and its exact `_STREAM_ID` retained for these results. Journal
`_BOOT_ID` uses 32 compact hexadecimal digits, unlike the punctuated UUID in the
record. Do not treat historical PID filtering or log text as current process
authorization. Initial unit-only and unnormalized-boot filters missed a live
terminal; the same original session continued, without a second start.
The previously recorded optional carrier PAM module warning remains unchanged;
there is no new login-manager/PAM policy configuration or finished login profile.
Evidence is in `local/desktop-terminal-2026-11-05` and [desktop](desktop-ui.md).

## Settings-only evidence checkpoint — 4 November

- First actual start/stop on the old carrier boot completed normally.
- The carrier shut down with QEMU status0/PID absence, then booted the same
  configured disk with the native viewer. All 19 protected old catalogue/static/
  profile hashes match before/after; the old intent remains running/recover-handoff.
- Both final Release and Debug supervisors start the actual DRM compositor and
  Settings, submit a frame and close with retained native status0. Their guest
  PNGs are byte-identical. Debug journals contain no sanitizer finding.
- Actual duplicate start, old completed stop during a newer live session and
  foreign token requests preserve the current attempt. Native Ctrl+Q produces
  original_exit while retaining the exact invocation/status, before explicit stop.
- One idle second records unchanged CPU ticks for supervisor, two native programs
  and the PAM helper. This is not a whole-system CPU/wakeup or performance budget.
- Ten contract/file/process checks pass on Linux. Five common contract cases also
  pass on Windows; five Linux-only cases are skipped there and executed in Linux.
  Controlled failures cover missing first-frame notification, unsolicited child
  error, cleanup error/output drain and lost file-flush acknowledgement. An
  otherwise identical test rejects a deliberate cleanup-outcome guard mutation.

Original results live in the private session-controller review directory and the
dedicated guest. No frozen SQL/XML, normal authority capability, physical disk,
shared WSL setup or other project was changed. The [desktop client/evidence](desktop-ui.md)
now adds the explicit dock/search/app-start workflow. Normal session/app policy,
the finished apps, experiment product and hardware acceptance remain open.
