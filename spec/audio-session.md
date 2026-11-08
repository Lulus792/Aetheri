# Audio in a user session

This opt-in diskless fixture joins the native audio adapter to PAM, logind,
standard device ACLs and systemd's user manager. Two independent cold boots now
pass the complete private session/playback/stop/logout flow and full PCM checks.
The [executed record](../brains/aetheri/journal/2026-11-08-audio-session.md) keeps
actual results and failures. It does not install normal audio in the USB desktop.

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
