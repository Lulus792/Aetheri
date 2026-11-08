# Private audio policy

WirePlumber 0.5.19 now selects a default output, creates stereo links and applies
volume/mute controls in an actual private PipeWire graph. Independent captured
samples verify those effects. This is the policy layer above the existing
[transport and HDA tests](audio.md), not normal desktop audio or Settings yet.

Use the maintained policy manager for device profiles, routes and stream linking;
keep Aetheri's presentation and eventual management adapter original. Rebuilding
those backend rules would duplicate substantial hardware-specific behavior.
The exact [input inventory](audio-policy-inputs.json) records the selection.

## Inputs and ownership

The two WirePlumber 0.5.19-1.fc44 RPMs are signed with the already reviewed
Fedora 44 key. Package license metadata is MIT. The profile also selects two
additional modules and ACP configuration from the existing PipeWire 1.6.11 RPM,
and ALSA configuration from the existing 1.2.16.1 RPM. Keep their separate notices;
ACP data includes LGPL material and is not Aetheri MIT code.

The prepared prefix has 232 package payloads, totaling 2,060,405 bytes:

| Role | Members | Bytes | Purpose |
| --- | ---: | ---: | --- |
| Runtime | 72 | 1,602,528 | Manager, library, modules and trusted policy/configuration scripts |
| Device data | 156 | 338,649 | ALSA/ACP configuration for the next device-monitor integration |
| Test tool | 1 | 90,600 | `wpctl` as an independent control/readback oracle |
| Notices | 3 | 28,628 | WirePlumber and ALSA license files |

Eight separately pinned existing library/notice objects add 2,855,590 bytes.
These include GIO/GObject/GModule, libffi and Lua from the carrier baseline.
Installed-object hashes are not fresh RPM-signature verification. This inventory
excludes the original PipeWire SDK and other base libraries; it is not the final
desktop footprint. An unused file-monitor module was removed after observing
the actual loaded libraries. No GTK/GNOME desktop service is introduced.

The existing Lua 5.4.8-5 baseline has Fedora fixes but is older than upstream
5.4.9. Its known limitations remain in the manifest. This profile executes only
trusted pinned policy scripts; it is not permission to run app Lua/bytecode or
production security acceptance. Review/update that runtime before normal rollout.
Aetheri's original application language remains C17.

`prepare_audio_policy.py` checks signature, payload digest and extraction against
one sealed memory object per RPM. Only named regular members reach a fresh 0700
prefix; package scripts and archive links never execute. Fixed aliases are added
after extraction. Existing runtime files are separately checked and copied as
data. `check` refuses changed/missing files, aliases, profile or unexplained members.
Failures preserve the new partial prefix. Nothing is installed to the build host.

## Execute the tests

Use an ordinary Linux build user and the existing exact PipeWire SDK. Cache the
four original PipeWire RPMs, both new WirePlumber RPMs and the existing ALSA RPM.
The protected carrier's root filesystem has little free space; the executed
tests use new private tmpfs workspaces instead of installing packages.

```sh
python3 tools/prepare_audio_policy.py prepare /absolute/fresh/policy-prefix \
  --cache /absolute/audio-rpm-cache
python3 tools/test_audio_policy_inputs.py /absolute/policy-prefix \
  /absolute/fresh/test-parent
python3 tools/test_audio_policy.py /absolute/audio-build/aetheri-audio-check \
  /absolute/audio-prefix /absolute/policy-prefix /absolute/fresh/test-parent
```

The last parent must already exist; each test run creates its own new child.
The normal audio CMake option remains off by default. The playback diagnostic's
additive `--autoconnect` asks the session manager to choose/link an output on its
explicit remote. Without the flag, the existing manual fixture behavior remains.
`--wait-for-start` still keeps the producer inactive until both original channel
links are observed. `DONT_RECONNECT`, frame bounds and the deadline remain in force.

Each graph has a private socket, home, state directory and FIFO. The manager uses
a copied upstream configuration plus a named stateless test profile. Audio/video/
Bluetooth monitors, session/system D-Bus and notifications are disabled. There
is no hardware scan, original user-bus connection or installed service. Module
aliases compose only the two checked prefixes in another private directory;
neither prefix is changed. Metadata support loads after the native protocol on
both server and control clients.

The original server, manager and player have retained pidfds, checked images,
UID/GID, zero effective capabilities and NoNewPrivileges. Connection snapshots
check Unix sockets and absence of audio/video device FDs. Links must join the
actual current FL/FR ports and belong to the retained manager according to the
pinned server's peer credentials. Roles, ports and links are joined from the
same graph snapshot: policy adapter configuration can recreate ports.
These are controlled test observations, not app/run or production admission.

Three managed graphs verify unity, 50% volume and mute. The current `wpctl`
cubic scale gives gain 0.125 at 50%; independent samples must match that gain,
not merely a printed percentage. Muted output must actually be zero. A fourth
graph omits the manager: no links or samples may appear, and the original player
must time out without a drain-success result. Teardown reaps only original
children; policy stops before the server after successful capture.

Release and ASan/UBSan runs both pass these four cases and the original four
audio CTest groups, now including seven PCM-verifier cases and 16 invalid
requests. Six actual prefix corruption controls also fail as required. See the
[executed record](../brains/aetheri/journal/2026-11-08-audio-policy.md).
The sanitizer variant instruments Aetheri's producer, while upstream binaries
retain their pinned package builds; it is not an upstream sanitizer clearance.

## Remaining integration

This suite proves policy on a FIFO node. The separate
[discovered-device test](audio-device.md) now joins actual udev enumeration,
ACP profile selection and automatic playback through virtual HDA. Its two cold
boots pass full sample verification. The FIFO suite and original manual HDA
image remain independent controls; neither alone proves the joined behavior.

Normal login/session supervision, mediated device access, a C17 management
adapter, Settings controls, HP headphone playback and Bluetooth remain work.
`wpctl` is a test oracle, not the future UI's subprocess protocol. PipeWire object
IDs/default selections are live backend observations, never durable component
identity, document grants, launch readiness or lease-release proof. The current
catalogue/wire capabilities and protected carrier stay unchanged.

References: [WirePlumber releases](https://pipewire.pages.freedesktop.org/wireplumber/resources/releases.html),
[features and policy boundaries](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/features.html),
[ALSA monitor](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/alsa.html),
[wpctl](https://pipewire.pages.freedesktop.org/wireplumber/man/wpctl.html),
[Fedora package](https://packages.fedoraproject.org/pkgs/wireplumber/wireplumber/fedora-44-updates.html)
and [Lua's known bugs](https://www.lua.org/bugs.html).
