# Audio in a user session

This opt-in diskless fixture joins the native audio adapter to PAM, logind,
standard device ACLs and systemd's user manager. Two independent cold boots now
pass the complete private session/playback/stop/logout flow and full PCM checks.
The [executed record](../brains/aetheri/journal/2026-11-08-audio-session.md) keeps
actual results and failures. It does not install normal audio in the USB desktop.

## Shared session control handle

[`audio/session_control.h`](../src/audio/session_control.h) now joins the existing
manager, graph/policy actor, local Wayland-session and original-listener checks
in one C17 handle. This is an opt-in owner-side library, not an installed service
or public app capability. The caller supplies independently admitted buses,
protected images, literal units/contexts, original process pins, seat and socket.
The bridge does not select these from an application request or socket claims.

Open establishes all observers and rechecks them before the strict connection.
Snapshot and every volume/mute/default request check the original observations
again. The backend still enforces its latest revision/output reference, command
serialization and readback rules. The bridge forwards those results without
turning submission into acknowledgement. Checks are observations, not locks
against later execution, delegation or configuration changes.

The first observed loss is immutable. It clears selectable outputs, rejects all
later commands and starts local backend retirement. A pending command in the
last available snapshot becomes `UNKNOWN`; losing an observation never proves
that a submitted effect did not occur. A returning session or replacement
manager/service cannot restore the handle. Its caller must retain and reconcile
the lost attempt, rather than reopen to bypass this fence.

The caller pushes and iterates one GLib thread-default context throughout the
handle's life. There is no new thread, process, periodic scan or automatic
restart. The owner must invoke snapshot on its session/process notifications
and before presenting controls. The [Settings binding](settings.md#native-sound-page)
now pumps these observations and one explicit intent; regular host event wiring
is still pending. Wrong-thread and post-fork API use is rejected. Close clears the
public handle, while original observer references survive until the backend's
asynchronous destruction. Keep the context alive until the data-destroy callback.
Closing neither stops the services nor revokes devices, settles delegated
holders or rolls back effects.

`audio.session_control` uses controlled observer/backend adapters and real GLib
context, thread and fork lifetimes. Release and ASan/UBSan runs cover partial
setup unwind, immediate/deferred retirement, all independent/combined losses,
cached pending uncertainty and direct commands without a preceding UI refresh.
A private counterexample with all three effect guards removed reaches the
expected forbidden-effect assertion; the restored source passes. See the
[execution record](../brains/aetheri/journal/2026-11-09-audio-session-control.md).
These tests do not establish actual joined Wayland/logind/MAC acceptance for
the new bridge. Protected normal provisioning, owner event-loop integration,
normal device/session admission and live joined Sound UI/backend acceptance remain work.

## Embedded native host event loop

`settings/audio_host` now connects that owner to the native NexaUI wait loop in
an explicit audio-linked host build. The ordinary executable still passes no
owner. There is no new CLI socket/PID option or normal installed capability.
The separate `probe_settings_audio_live` target is lab-only test infrastructure;
its positive joined UI/backend path has not been executed yet.
Its [reviewed runtime projection](settings-audio-lab.md) now has exact
component/notice coverage and actual lab loader compatibility checks.

The owner lends four wake hints: its original manager process, graph/policy
processes and login monitor, plus the monitor's absolute monotonic deadline.
These borrowed descriptors cannot be closed, consumed, modified or transferred.
Readiness is not admission or drain; the original observers recheck before
snapshot/intent handling. A latched loss removes permanently readable dead
process hints from subsequent waits, retaining the actual owner handles.

The adapter collects hints before GLib prepare/query, including sources added
by a fencing/close path. It acquires the creating thread's pushed context,
queries at most256 poll records and combines deadlines with the native window
budget. One fresh internal epoll FD wakes the native wait. Afterward, an actual
nonblocking GLib poll supplies revents for check/dispatch; the multiplexer is
only a hint. Close it and release the context every cycle. Abort retires the
wait without dispatching or submitting another intent. Callback-time close,
wrong-thread/post-fork use and overlapping waits are rejected.

This follows GLib's documented
[acquire/prepare contract](https://docs.gtk.org/glib/method.MainContext.prepare.html)
and [custom event-loop phases](https://gnome.pages.gitlab.gnome.org/gtk/glib/main-loop.html).
The online reference identifies2.90; actual compilation/runtime uses the already
reviewed2.88.0 library. This changes no dependency version. Aetheri adds no
watcher thread, resident helper or periodic audio scan; upstream libraries keep
their own thread/async behavior. The admitted owner and context still outlive
the adapter; local retirement is not service stop or holder settlement.

The embedded Settings entry binds its stack model before opening the window,
pumps original observations and UI intents in that same loop, then unbinds
before destroying the view. The lab client expects independently root-selected
FD3/4 process pins, a read-only root-owned `AETHAUI1` test profile in FD5 and
manager/graph/policy image FDs6/7/8. That codec, DMI check and trusted test delivery
are not production provenance/admission or an installed app transport.

Native adapter tests use real GLib sources/I/O/timers, epoll, threads, fork and
FD retirement with controlled owner observations. The separate enforcing lab
now also passes an actual local UID1000 PAM/logind session and original process,
service and session-scope end observation. This does not yet join the live C
Settings host to the audio graph or accept normal provisioning, broker policy,
holder/restart recovery or physical audio. Read the
[execution record](../brains/aetheri/journal/2026-11-09-audio-host.md).

`tools/test_audio_login.py` is an explicit root-only manual lab probe, never a
default host test. Its permissive temporary PAM profile is only for the
root-selected test process, not normal authentication. It refuses optimized
Python, foreign/pending profiles and a different carrier. Failure retains the
keeper and original references for independent inspection; do not retry or
infer teardown. Successful end requires the original dead pin, terminal unit,
unchanged invocation/no job and both original recursive cgroups empty/removed.
Missing names require original event-reader `ENODEV`, not path absence alone.
Only then remove the exact owned profile and restore the prior VT.

## Processes and units

The existing user manager supervises the two audio processes. There is no extra
permanent Aetheri watchdog. The finite integration client exists only in the VM.

| Unit | Responsibility |
| --- | --- |
| `aetheri-session.target` | Coordinate this test session's start/stop; not authenticate a login |
| `aetheri-audio.target` | Keep graph and policy together and stop them with that session target |
| `aetheri-pipewire.service` | Run the audio graph and own its private runtime directory |
| `aetheri-wireplumber.service` | Discover devices, select routes and link streams through the graph |

`tools/audio_session_units.py` generates the three audio units into the new image.
Nothing is enabled on the build host. `Requisite` plus ordering requires the
session target already starting/active; it does not silently create a session.
`BindsTo` propagates graph/policy loss, while `PartOf` propagates explicit stop.
Automatic restart is disabled. `Type=exec` confirms executable startup, not graph
or device readiness. Native output discovery/readback and full PCM are separate.
The fixture's privileged system-managed user@ parent sets its capability boundary
before user services start. Trying to lower that mask in the unprivileged child
manager fails with status 218. Live audio-role checks still require zero
effective, permitted and inheritable capabilities plus NNP; they reject the
ambient-default CAP_WAKE_ALARM inherited from an unmodified PAM manager.

The graph uses `%t/aetheri-audio`, below the user's existing runtime directory,
with mode 0700. Its socket, temporary policy state and locks stay together.
The server owns this scratch directory through `RuntimeDirectory`; the policy
service stops before the graph. This is not a production private-data allocation
or a component-purge journal. There is no new long-lived SQLite database.

PID1, udev, logind, the system/user buses and the existing user manager provide
the platform/session work. They already belong to the selected desktop baseline.
PipeWire and WirePlumber add the actual audio graph/policy processes. A future
component view must explain their roles rather than hide them.

## Device access

The old HDA fixture grants fixed ownership to UID1000. This image instead keeps
the sound nodes root:audio/0660 and uses the selected standard udev/logind uaccess
rules. The test account is not in `audio`. A normal initgroups result may contain
its own primary GID1000; that grants no new rights beyond its existing primary
group. Any other supplementary group is rejected by the finite test client.
Both an empty list and primary-only context have separate native probes.

## Observing session or process loss

The opt-in `audio/session_guard` library can run inside a session owner. It adds
no daemon or thread. Its caller supplies an independently admitted UID/seat and
original graph/policy pidfds. The observer subscribes before capturing its own
local Wayland session, duplicates those pins, and checks original context again.
Notifications are hints: flush the login monitor, then reread the original
session. A failed/mismatched observation fences work as well as an inactive
session. Neither a session identifier nor a monitor notification is authority.

The first loss mask is immutable. Returning to the VT does not reset it. The
caller must retain that lost attempt; closing and reopening an observer cannot
recover an old run or its admission. This is in-memory observation, not a durable
recovery journal. The observer only reads and polls; it sends no signal, stop RPC or restart. Closing
it closes its own monitor/pin copies, preserving the caller's original handles.
The caller must stop submitting work, coordinate one stop against the original
manager, and separately observe process/service/endpoint retirement. No stop
acknowledgement, missing socket or ACL change alone proves general holder drain.
The fixture retains its four unit objects through the original dedicated user
bus connection before actor acceptance. Otherwise systemd may garbage-collect
their inactive state before it is inspected. A GC reference is not invocation,
process or lease evidence; missing names are still rejected. Final bus close
releases only this connection's references, after the fixture's end checks.

The private fixture watches during playback. Additional fixed image variants
inject VT loss or original server/policy exit after the complete reference
signal. See the [loss record](../brains/aetheri/journal/2026-11-08-audio-loss.md)
for actual evidence and current acceptance. These tests do not choose final
background-music/lock behavior or prove in-flight interruption, manager restart
identity, endpoint authentication, app MAC or delegated-holder revocation.

Both native user-session clients now use the [original-listener connector](audio-control.md#original-listener-binding).
Their independently observed server pin crosses only the fixed fixture's
trusted fork/exec boundary in FD3. Every other nonstandard descriptor closes;
each child closes its inherited pin after its synchronous connection check.
The parent retains the lifetime observer. This is not a normal app transport or
permission grant, and third-party policy tools remain independent test oracles.

## Original user-manager observation

`audio/manager` now observes the original user manager before the private fixture
starts audio. Its caller supplies two independently trusted local bus connections
and the protected systemd executable. It requires non-augmented bus EUID/PID/
ProcessFD credentials, mandatory kernel PID/credential/cgroup information and the
original live pidfs object. PID 1 must identify that process as the active main
of `user@UID.service`, with no control process or pending job. Both invocation
observations must agree and be nonzero. The manager must inhabit that service's
exact `init.scope`; its current cgroup inode must match the kernel observation.
The current executable must match the independently selected root-owned,
non-writable file object and metadata.

The manager must also have zero effective, permitted, inheritable and ambient
capabilities and `NoNewPrivs=1`. Bounded trusted-proc status reads check this
before and after the executable observation, inside the existing repeated kernel
identity checks. Missing/duplicate/truncated fields and unreadable/oversized
status fail closed. The observer never changes the manager's credentials.
PID 1's selected user@ profile already applies the empty bounding mask/NNP;
this check observes that profile rather than replacing configuration admission.

An enforcing-carrier read failure is not automatically a SELinux denial.
The preserved carrier's ordinary upstream user manager has CAP_WAKE_ALARM
permitted/effective/inheritable and NNP disabled. Commoncap's proc inspection
rules independently reject a cap-zero caller inspecting that more privileged
process. The explicit [security probe](../tests/test_audio_manager_security.c)
keeps one own child, UID/GID, image, NNP, dumpability and SELinux context while
clearing only its CAP_WAKE_ALARM. The cap-zero reader's current-image open then
changes from EACCES to success under the unchanged enforcing policy. This is a
kernel privilege boundary test, not normal Aetheri MAC/service admission.

Initial discovery may wait only for the bus driver's NameHasNoOwner, before
capture and before any fixture unit effect. An available bus is not an already
registered manager. No admitted identity or mutation is retried this way.

The observer retains the original unique destination, both bus IDs, process pin,
invocation, cgroup and executable observations. Rechecks precede fixture effects
and run during playback. The first observed mismatch or read failure is latched;
returning to the old owner does not reset it. Closing drops the caller's reference;
the original pin/bus references remain while a retained actor still needs them.
It sends no service mutation, signal or reconnect, and adds
no permanent process or database. The caller keeps stop/drain coordination separate.

These are bounded observations on the calling thread, not exclusion of exec,
delegation, ptrace, configuration mutation or changes between checks. Trusted
host proc/cgroup views and protected unit, executable and library provenance
remain prerequisites. The executable comparison does not measure its dependency
closure or implement the accepted enforcing app policy. This separate audio
observer does not weaken `linux/peer` or `linux/process` MAC requirements and is
not an authority admission API. Normal desktop promotion remains separate.

The [executed manager record](../brains/aetheri/journal/2026-11-09-audio-manager.md)
includes both native build modes, intended counterexample failures, six full
cold boots and a separate actual enforcing-carrier read rejection. The native
`audio.original_manager` test uses existing read-only system-bus clients only
for real message objects/IDs; its subject's credential/PID1/cgroup facts are
controlled. Use the reviewed disposable Linux environment for that test; it
does not create a private bus, acquire a host service name or change host units.

`test_audio_manager_security` is built separately and has no automatic CTest
registration. Run it only as an administrator in the registered disposable
carrier. It changes only its own test children, never the original user manager,
policy or services. The [security record](../brains/aetheri/journal/2026-11-09-audio-manager-security.md)
retains the causal countercheck and ordinary-profile regression evidence.

## Original user-service actor observation

`audio/actor.h` adds a read-only observer for a specific original user-service
main. The caller supplies the already admitted manager, literal service name,
original live pidfd, independently selected root-owned non-writable executable
and exact expected MAC context. No PID is opened from a socket or app claim.
It reuses the manager library's bounded kernel/image checks without introducing
another daemon, database or installed management interface.

Calls go to the retained manager's original unique bus destination.
GetUnitByPIDFD must identify the selected service and a nonzero invocation;
ActiveState, MainPID, ControlPID, InvocationID, Job and ControlGroup must agree.
The main's current kernel credentials, cap-zero/NNP status, executable and MAC
context are checked repeatedly. Its exact cgroup must lie below this original
user@ delegation, outside init.scope, with the kernel cgroup ID matching the
held/named cgroup object. Another same-UID unit or same-unit non-main is refused.

The actor retains its own process/cgroup handles and a reference to the original
manager observer. Closing the caller's original FDs or manager reference does not
invalidate it. Close only retires owned observations; it sends no StopUnit,
UnrefUnit, signal or release. The first failed observation is latched, including
manager loss; returning to an earlier state never restores the attempt.

These remain snapshots, not exclusion of future exec, migration or delegation.
Protected unit/configuration/dependency provenance, supplementary-group policy,
mutation fences, device/listener admission and complete holder/recovery handling
remain separate. Comparing an expected MAC label does not install that policy
or prove its permissions. Normal audio is not enabled by this library.

The [actor record](../brains/aetheri/journal/2026-11-09-audio-actor.md) binds native
Release/sanitizer tests to an actual enforcing-carrier user-unit test. The
permanent explicit probe/driver uses root-selected `/usr/bin/sleep` units with
the existing cap-zero/NNP user@ profile, not graph playback or confined audio
roles. It rejects a different actual unit/image/context and observes original
process exit. The administrative controller independently retires unit/job/
cgroup resources; the observer's loss marker is never used as a drain proof.

Build `probe_audio_actor` with `AETHERI_LINUX_IPC` and `BUILD_TESTING`. In the
registered enforcing audio lab, with protected root-selected source/binary
inputs and its existing cap-zero/NNP user@ profile, run:

```text
python tools/test_audio_actor.py /absolute/build/probe_audio_actor /absolute/source /root/fresh-actor-evidence
```

The output directory must be new. An interrupted attempt requires inspection,
not redispatch. `tools/test_audio_actor_controls.py` runs five host-only cases
without opening a bus or changing a guest. The driver is not a normal installer
or a production recovery/removal command.

The executed private check is:

1. Before login, a cap-zero/NNP UID1000 process cannot open control or playback.
2. A root-selected PAM fixture creates a local active seat0 Wayland user session.
   The same user can now open those nodes through the actual dynamic ACL.
3. The user manager starts the graph/policy. The client joins their original
   pidfds, current images, UID/GID/group/capability/NNP state and service observations.
   It retains the original manager's unique bus destination; replacement is not
   adopted and unknown mutations are not replayed.
4. Native volume/mute, independently inspected output identity and complete host
   capture agree. The fixed QEMU software-mixer baseline remains required.
5. Stop the session target, observe the original two process exits, finish PAM
   logout and require runtime removal. Fresh UID1000 device opens must fail again;
   root ownership/group/mode must remain unchanged.

Standard uaccess grants per-user DAC. It is not Aetheri app authorization or MAC.
Removing an ACL does not revoke an already returned device FD. Current process
observations and a unique bus name also do not constitute complete authority/
image/cgroup admission. Normal use still needs protected service configuration,
active-session change handling, original process/holder fencing, endpoint policy
and the accepted broker/MAC integration. Multiple sessions, manager replacement,
unexpected exits, restart/recovery and suspend remain separate acceptance work.

This VM has only the selected output codec. It does not establish microphone
permissions. Normal apps must not inherit raw sound-device access or unrestricted
graph control merely because they share a UID with the backend. Capture and
per-app audio permissions require their own enforced transport/policy boundary.

## Runtime and reproduction

`tools/audio_session_runtime.py` selects existing exact desktop pins plus their
ELF dependency closure. ACL and seccomp are explicit dlopen seeds, not discovered
from DT_NEEDED. The current selection has 86 files, ten aliases and 10,302,437
bytes; four files are already present and 82 are copied. Actual copying repeats
size/hash/type checks and retains the selected package notices. These are
installed-object hashes from the reviewed desktop inventory, not new RPM
signature receipts. No package script, installer or binary is run to discover
dependencies; readelf reads their metadata as data.

Use the existing exact audio, HDA, policy and control prefixes as an ordinary
x86-64 Linux builder:

```sh
python3 tools/build_audio_session.py /absolute/fresh/output \
  /absolute/audio-prefix /absolute/hda-prefix /absolute/policy-prefix \
  /absolute/control-prefix --nonce 0123456789abcdef0123456789abcdef
```

On the prepared Windows host, use a new result directory for every attempt:

```sh
python tools/test_audio_hda_contract.py
python tools/test_audio_hda.py --image /absolute/output/image --output local/session-attempt
```

Additional fixed variants live under `output/vt-loss/image`,
`output/server-loss/image` and `output/policy-loss/image`. The final loss suite
uses the explicit `--icount` functional profile: fixed shift 0, align off, sleep
off and a VM-driven RTC, with the same two virtual CPUs and full PCM criterion.
There is no automatic fallback; default-clock failures stay failed. The observed
early APIC timer issue remains unresolved. Instruction counting is not physical
CPU speed or real-time/cycle-accurate acceptance. Preserve every original result
directory, including rejected clock, marker and retirement attempts.

The image preserves its original native/device/manual controls below
`output/native`. All source/archive hashes, original QEMU handles and the
host-local AF_UNIX QMP requirement remain enforced. No network, disk, shared
folder or physical audio is used. The test PAM permit stack is root-selected VM
infrastructure, not password authentication or a proposed login policy. The RAM
guest has no production SELinux policy; its missing cgroup-BPF backend is not
accepted as device-filter enforcement. Do not weaken a vendor sandbox to make
these tests pass. Review/update the legacy Lua runtime before normal rollout.

References: systemd 259 [unit dependencies](https://raw.githubusercontent.com/systemd/systemd/v259/man/systemd.unit.xml),
[uaccess rules](https://raw.githubusercontent.com/systemd/systemd/v259/rules.d/70-uaccess.rules.in),
[ACL loading](https://raw.githubusercontent.com/systemd/systemd/v259/src/shared/acl-util.c)
and [seccomp loading](https://raw.githubusercontent.com/systemd/systemd/v259/src/shared/seccomp-util.c).
