# Running the development desktop

4 November 2026. This starts the native Aetheri compositor and Settings in the
dedicated Fedora carrier. It is a development workflow, not the finished desktop,
an Aetheri system image, normal app admission or the resettable experiment product.

## Use it

Run commands from the Aetheri checkout with the configured Python 3 interpreter.
To boot an already prepared, stopped carrier with its native QEMU window:

```sh
python tools/run_vm.py --offline --console boot
```

That command remains attached to the VM. Use another terminal for the session:

```sh
python tools/development_session.py start
python tools/development_session.py status
python tools/development_session.py stop
```

Use `start --profile debug` for the existing ASan/UBSan build. An already running
headless VM must be stopped gracefully before selecting a different frontend;
do not start another QEMU process against its disk. `run_vm.py status` checks
QMP identity and the recorded host process. `run_vm.py poweroff` requests guest
shutdown. The native viewer is the existing QEMU SDL2 frontend; Aetheri's own
Settings UI still uses the reviewed SDL3 profile. No new viewer package is needed.

The viewer explicitly disables OpenGL and window-close shutdown. QMP remains
host-local AF_UNIX, networking remains restricted in the example, and no VNC,
host directory share, clipboard bridge or new network listener is added. Actual
QMP display options confirm `type=sdl`, `gl=off`, `window-close=false`. Native
Windows window metadata identifies the running QEMU window. Host-window pixels,
mouse grabbing and Windows keyboard forwarding still need separate acceptance:
both available native-UI inspection paths timed out. Guest pixels/input are
tested through QMP; do not call that a completed Windows interaction test.

Settings can close with Ctrl+Q. The development supervisor then ends the compositor
and retains the original service result. Run `stop` to retire the inspected unit
and return to tty1. `stop --token TOKEN` selects a specific original development
attempt. Repeating a completed stop returns history and cannot stop a newer one.

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

The ordinary-user Python supervisor is a development tool. It retains two actual
Popen child handles, starts the compositor, then Settings. It notifies PID1 only
after the selected Settings has submitted its first frame, not merely printed
READY. This is a development startup signal, not the Aetheri app READY protocol
or proof of admitted code, data, grants or visible host presentation.

The supervisor sleeps on child pipes and signal wakeups. Closing Settings stops
the compositor; an explicit stop ends Settings first, then the compositor. Each
child is waited/reaped. Pending pipe output is drained, and requested termination
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
| `cache` beside that runtime | UID1000, Settings shader cache with the existing soft budget |
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

## Evidence

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
shared WSL setup or other project was changed. Next is the actual desktop shell
and app launcher; normal session/app policy and hardware acceptance remain open.
