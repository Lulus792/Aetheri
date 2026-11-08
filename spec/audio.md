# Audio development foundation

The first audio diagnostic sends original C17-generated stereo samples through
a real private PipeWire graph. An independent FIFO reader checks the resulting
float samples, channel order and complete signal. It runs without a sound card,
microphone, normal session service or network audio backend.

This is the transport foundation for Aetheri audio. It does not yet provide sound
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
do not relabel the whole package as Aetheri MIT code. A later ALSA/WirePlumber/
Bluetooth integration needs its own exact dependency and license review.

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
Release/ASan/UBSan results and failed controls. Next review and pin the ALSA
backend, connect a dedicated virtual sound card and verify its host-file output. Then
integrate a per-user service, device/profile policy and Aetheri Settings controls,
followed by HP jack output and Bluetooth tests. WirePlumber is the recommended
policy manager; its version/profile and those integrations are not implemented.

References: [upstream 1.6.11 NEWS](https://raw.githubusercontent.com/PipeWire/pipewire/1.6.11/NEWS),
[Fedora package](https://packages.fedoraproject.org/pkgs/pipewire/pipewire/fedora-44-updates.html),
[stream API](https://docs.pipewire.org/group__pw__stream.html),
[pipe-tunnel fixture](https://docs.pipewire.org/page_module_pipe_tunnel.html).
