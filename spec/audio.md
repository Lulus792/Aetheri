# Audio development foundation

The first audio diagnostic sends original C17-generated stereo samples through
a real private PipeWire graph. An independent FIFO reader checks the resulting
float samples, channel order and complete signal. It runs without a sound card,
microphone, normal session service or network audio backend.

The separate diskless HDA fixture below also exercises an actual virtual sound
card with independent host-file sample verification. This does not yet provide sound
in the USB desktop, Settings controls, headphone switching, Bluetooth, recording
permissions or normal admitted app playback.

## Selected components

Use PipeWire 1.6.11 from the signed Fedora 44 `1.6.11-1.fc44` packages for this
development profile. Its C API fits the existing language/toolchain, and its
graph can later join separate application streams to maintained device backends.
The upstream 29 October release includes fixes to draining and Bluetooth security;
this review is not a claim that all vulnerabilities or deployment policies are
resolved. Exact packages, hashes, selected members and existing development
dependencies are in [audio-inputs.json](audio-inputs.json).

The chosen SDK has 252 regular members: 235 headers, 10 runtime members, 3 test
members and 4 notices. Their total is3,712,345 bytes; runtime members account for
2,471,008 bytes. This excludes existing glibc/libgcc/GLib/PCRE2/SELinux libraries.
Their exact carrier versions are checked separately. It is not a complete future
image footprint. Most of the Fedora audio/video/network/codec plugins are not
selected. `libpipewire-0.3` and `spa-0.2` are API/directory names; the package and
checked library/header release is 1.6.11.

Original Aetheri code is MIT. Preserve all third-party notices and package
license metadata. The aggregate `pipewire-libs` package includes other licenses;
do not relabel the whole package as Aetheri MIT code. The separate ALSA review is
recorded below; WirePlumber/Bluetooth still need their own exact input reviews.

| Piece | Purpose in this graph |
| --- | --- |
| `pipewire` and `libpipewire` | The private server and C client/graph implementation. |
| `protocol-native` | Local Unix-socket communication. |
| `client-node` | Export application streams and share their buffers. |
| `adapter` and `libspa-audioconvert` | Adapt raw samples to channel ports and negotiated formats. |
| `link-factory` | Connect the original output and input ports. |
| `spa-node-factory` and `libspa-support` | Create the test clock driver and provide event-loop support. |
| `access` | Classify access on the private socket; this fixture deliberately permits its own trusted user. |
| Test-only pipe-tunnel, pw-link and pw-dump | Capture samples and inspect/connect this disposable graph. |

## Prepare and build

Use a fresh, ordinary-user Linux build workspace. The existing protected carrier
has little root-filesystem space; the executed tests use private tmpfs mounts.
Neither the RPM database nor normal source/build/service trees are modified.
Cache the four exact signed RPMs from the profile before preparing the SDK.

```sh
python3 tools/prepare_audio.py prepare /absolute/fresh/audio-prefix \
  --cache /absolute/audio-rpm-cache
cmake -S . -B /absolute/fresh/audio-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAETHERI_AUDIO=ON \
  -DAETHERI_AUDIO_PREFIX=/absolute/fresh/audio-prefix
cmake --build /absolute/fresh/audio-build --target aetheri-audio-check
ctest --test-dir /absolute/fresh/audio-build -R '^audio[.]' --output-on-failure
```

`prepare_audio.py` hashes each RPM into one sealed 0600 memory
file. Signature verification and `rpm2cpio` read that same immutable object,
including if the original cache path changes. Package scripts never execute.
The bounded newc reader selects only pinned regular data; archive symlinks never
become extraction paths. The two library aliases are created from fixed literals.
A failure preserves the new partial prefix. A later `check` validates every
member, alias, profile and development baseline, rejecting unexplained files.

The CMake option defaults off. Missing or changed prefixes are refused. Ordinary
builds do not acquire this library, a new service, or a change to SDL's existing
audio-disabled profile. A development RPM check is not a runtime requirement for
the future Aetheri image or a general package manager.

## Diagnostic contract

`aetheri-audio-check --remote /absolute/private/socket` produces up to 96000
interleaved F32LE stereo frames at 48 kHz. Default duration is 48000 frames. Left
and right carry distinct 440/880 Hz tones at amplitude 0.125; `--silent` provides a
zero control. Frame/argument bounds are checked before opening a connection.
Only the exact 1.6.11 header/library pair is accepted.

The diagnostic uses ordinary matching user/group identities and NoNewPrivileges.
It connects to the explicit path; a failed properties allocation cannot choose
the default remote. This private path is fixture routing, not a production
broker-delegated connection or process admission protocol.

One main loop owns stream, callbacks and cleanup. Each process turn fills at
most 8192 frames in a checked single mapped buffer. No per-turn allocation or
logging is needed. Conversion/rate/channel drift fails rather than choosing a
different format. The callback does not request real-time scheduling.

For fixture setup, `--wait-for-start` creates an inactive stream. The controller
connects both original stereo ports before signaling that exact retained process
with SIGUSR1. This prevents the first connected channel from consuming samples
before the second channel exists. Repeated start signals do not restart the
producer. This signal is a development control, not an authenticated app API.

After the last queued frame, the diagnostic requests stream drain and waits for
its event. Server loss, interruption, unexpected format/buffer or a deadline of
15 seconds cannot report success. `CONNECTED` means an exported graph node;
`DRAINED` is local stream completion. Neither proves physical audibility, device
drain, Aetheri app readiness, run end or release of an authority lease.

## Tests and next integration

`audio.inputs` checks pins and malformed archive data. Its Linux case proves
immutable snapshots survive source replacement and refuse writes/truncation.
`audio.pcm_verifier` rejects missing/swapped channel samples, dropped frames,
truncation and nonfinite data. `audio.cli` checks 15 invalid requests.

`audio.playback` runs seven fresh graphs: tones of 48000 and 96000 frames, a
partial block of 257 frames, silence, server loss, interruption and an unstarted
timeout.
The signed pipe-tunnel module and pw-link/pw-dump utilities belong only to this
fixture. The FIFO is precreated with mode 0600 under a 0700 folder. The test checks
original held processes, expected images/library, observed UID/GID, cap-zero/NNP
state, Unix sockets and absence of audio/video device FDs at connection. It
compares actual captured samples rather than treating a drain log as PCM proof.
Known test processes are retired through their retained pidfds.

The [executed record](../brains/aetheri/journal/2026-11-08-audio.md) contains actual
Release/ASan/UBSan transport results and failed controls. The HDA extension has
its [own record](../brains/aetheri/journal/2026-11-08-audio-hda.md).

## Diskless HDA fixture

This is a separate RAM-only UEFI/systemd image, not the desktop or an installed
authority. Its original C17 test owner loads eleven modules of the already
selected kernel and identifies the fixed QEMU Intel HDA controller `8086:2668`.
It waits for both PCM and control device nodes before granting its trusted test
user playback access. The emulated codec's mixer starts muted at volume zero.
Only the exact independently identified fixture card's two master controls are
validated, set and read back. The emulated mixer remains enabled.

The server and existing diagnostic run as UID/GID1000 with zero capabilities
and NoNewPrivileges. Both channel links exist before producer activation. The
ALSA node stays active for this finite fixture and a 500ms settling interval;
otherwise idle transition can discard samples still queued at the device after
the producer exits. This interval is not a production device-drain protocol.
The independent capture must still contain the entire signal and silent padding.
Original child pidfds govern signaling/reaping. No permanent daemon is installed.

[audio-hda-inputs.json](audio-hda-inputs.json) selects signed Fedora44
`alsa-lib-1.2.16.1-1.fc44` and the ALSA SPA plugin from the already reviewed
PipeWire RPM. The three extra payloads total2,150,128 bytes, including ALSA's
LGPL-2.1-or-later notice. Preserve that notice and the existing PipeWire notices.
The profile separately pins kernel modules and existing runtime libraries as
data. Their installed-object hashes are not a fresh RPM-signature verification.
The kernel, libc and other base inputs remain those of the existing RAM boot
proof. This is a development inventory, not the future desktop footprint.

Preparation uses the same sealed-RPM signature/digest/extraction boundary as the
transport SDK. Use separate fresh prefixes and a new ordinary-user Linux output:

```sh
python3 tools/prepare_audio_hda.py prepare /absolute/fresh/hda-prefix \
  --cache /absolute/audio-rpm-cache
python3 tools/build_audio_hda.py /absolute/fresh/hda-build \
  c0ddaa2b3576450d857a01597eb9926e /absolute/audio-prefix /absolute/hda-prefix
```

The builder preserves its base proof and extends only a copy. It compiles the
test owner with strict C17 warnings and the existing player in Release. The
copied preflight retains its failure poweroff; only its success poweroff is
removed so the dependent audio test can run. No modules are loaded by the
builder, and no host package or policy is installed.

On the reviewed Windows QEMU host:

```sh
python tools/test_audio_hda_contract.py
python tools/test_audio_hda.py --image /absolute/hda-build/image \
  --output F:/Aetheri/local/fresh-hda-attempt
python tools/test_audio_hda.py --image /absolute/hda-build/image \
  --output F:/Aetheri/local/fresh-no-hda-attempt --missing-device
```

The controller rechecks the actual newc archive and original host-tool hashes.
Each attempt has new firmware variables, a UUID, the original child handle and
one protected host-local AF_UNIX QMP connection. There is no NIC, shared folder,
real disk or host speaker/microphone backend. It starts paused, attaches a file
capture to QEMU's `none` backend, then runs the guest. The guest must independently
power off; QMP must report `shutdown` before capture stop and explicit host quit.
The original QEMU process must then exit0. Unknown requests never reconnect or replay.

The pinned QEMU11.1 monitor `wavcapture`/`stopcapture` API is deprecated. It is a
test adapter, never an Aetheri audio/runtime API. Its exact warning is retained;
other diagnostics fail. Explicit stop finalizes RIFF. The first direct WAV-backend
attempt leaves zero length fields even after exit and is rejected; no header is
rewritten to make it pass. The final verifier checks complete48kHz stereo16-bit
PCM, ordered440/880Hz channels, all96000 frames, <=2LSB error and silent padding.
Fifteen local contract cases reject silence, swapped/missing samples, truncation,
underrun, extra signal, incorrect format and insufficient process/boot evidence.
The no-card countertest is expected to fail the guest device check; it is never
accepted as successful playback.

The native build workspace is on an enforcing carrier, but this small diskless
guest has no loaded production SELinux policy. UID/capability observations and
VM isolation do not establish production audio sandbox admission. Next integrate
a per-user service, device/profile policy and Settings, then HP jack output and
Bluetooth. WirePlumber is the recommended policy manager; its exact version and
profile are not selected here. Recording permissions, volume/device switching,
normal admitted app audio and physical audibility remain untested.

References: [upstream 1.6.11 NEWS](https://raw.githubusercontent.com/PipeWire/pipewire/1.6.11/NEWS),
[Fedora package](https://packages.fedoraproject.org/pkgs/pipewire/pipewire/fedora-44-updates.html),
[stream API](https://docs.pipewire.org/group__pw__stream.html),
[pipe-tunnel fixture](https://docs.pipewire.org/page_module_pipe_tunnel.html).
HDA references: [ALSA hardware PCM](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm_plugins.html),
[PipeWire properties](https://docs.pipewire.org/page_man_pipewire-props_7.html),
[QEMU11.1 capture implementation](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/audio/wavcapture.c)
and [monitor contract](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/hmp-commands.hx).
