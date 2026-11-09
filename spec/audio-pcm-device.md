# Confined virtual sound-device diagnostic

This explicit test crosses the ALSA device boundary in the separate enforcing
[audio lab](audio-lab.md). A root-selected playback FD reaches one finite
UID 1000 backend under the real user manager. It is a diagnostic for subsequent
device integration, not a replacement audio engine or a normal app API.

## Device and actor ownership

`tools/test_audio_pcm_device.py` refuses the original carrier, a different DMI
product, a non-enforcing policy or a different virtual PCI controller. It checks
the original root:audio, mode 0660 character nodes and holds the exact playback
object. Major116 is checked; the minor is discovered rather than assumed.
Only the selected playback node receives a temporary test type. Its original
raw SELinux xattr and file metadata must survive retirement exactly.

The root-only native baseline checks both virtual mixer controls before setting
the fixed test unity/unmuted state. This does not define physical HP mixer
initialization. The playback process has UID/GID 1000, no extra groups, no
effective/permitted/inheritable capabilities and NoNewPrivileges. The service
also requests an empty capability bounding/ambient set. No node owner, mode or
ACL is broadened. An ordinary direct open must fail before and after the test;
EACCES alone is not attributed to a particular permission layer.

The temporary PCM rule grants the graph test domain FD read/write/getattr/ioctl,
without open or mmap. Existing routing/control/player/foreign domains have
compile-time prohibitions on device I/O. The finite bus worker and supervisor
can transport the selected FD. They are trusted test infrastructure, not a
production device authority. The lab's unconfined user manager and its temporary
transition rule retain the limitations in [the user fixture](audio-user.md).

The original service is pinned before releasing its gateway. A second barrier
lets the native actor observer join the actual PCM image, context, invocation
and original process to the retained user manager before samples are written.
After the finite process ends, the controller separately observes the original
unit, held cgroup deactivation, unit collection, worker exit and absence of
supervisor FD copies. Closing a device FD alone is not revocation or drain.

## Native diagnostic

`probe_audio_pcm` accepts only fixed baseline, playback and ordinary-open-denial
modes. It receives an independently selected FD3, closes higher descriptors and
checks the card/stream through kernel ioctls. The profile writes 96,000
stereo S16_LE frames at 48 kHz, left 440 Hz/right 880 Hz, amplitude 4096, followed
by 8,192 explicitly counted silent guard frames. The unpadded diagnostic lost
its last audible samples in the host observation despite kernel DRAIN success.
The guard keeps the complete reference ahead of that virtual-device tail; it
does not prove complete physical or production drain. Only an
acknowledged positive frame count advances the input; unknown writes are never
replayed. A successful kernel DRAIN or process exit is not proof that the host
received every sample.

The existing `tools/test_audio_pcm.py` remains the independent synthetic PCM
verifier suite. The separate `tools/test_audio_pcm_native.py` exercises ordinary
bad-input paths against a regular FD and verifies its original bytes. It opens
no sound device. Release and ASan/UBSan builds are separate from actual confined
execution; test policy is not weakened for instrumentation.

## Host capture

`audio_lab_vm.py boot-hda` adds only a virtual HDA output and a `none` backend.
It passes no host speakers, microphone or physical device. A fresh private WAV
file has an independently recorded file/directory object before dispatch.
The guest starts paused. A second host-local AF_UNIX QMP endpoint lets the
controller retain its original capture connection while the ordinary endpoint
continues to serve status and poweroff. Neither is a loopback TCP listener.

The pinned QEMU 11.1 monitor capture is test-only and deprecated upstream. Its
exact deprecation messages are retained; other diagnostics fail the check.
The controller checks an empty capture list, starts the fixed-format capture,
checks its exact listed path, then resumes the guest. After actual guest
shutdown and stopped-VM observation, it explicitly stops the capture, checks
its removal, requests quit once and observes original QEMU exit. Unknown effects
preserve the process and journal rather than redispatching them.

Final file identity, size and whole-file SHA256 are recorded separately from
audio acceptance. The existing `test_audio_hda.validate_wav` must accept the
complete unmodified RIFF and all 96,000 reference frames with its unchanged
two-LSB bound. No trimming, header repair or shortened reference is allowed.
The first WAV-backend attempt left zero RIFF lengths and only 94,747 payload
frames despite guest DRAIN success; that failed raw file remains evidence.

Relevant primary implementations: QEMU 11.1
[WAV backend](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/audio/wavaudio.c),
[monitor capture](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/audio/wavcapture.c)
and [monitor commands](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/audio/audio-hmp-cmds.c).

## Run and remaining integration

Only the explicitly disposable enforcing lab and protected root-selected inputs
are supported:

```text
python tools/audio_lab_vm.py boot-hda
python tools/test_audio_pcm_device.py BUILD SOURCE FRESH_OUTPUT
python tools/audio_lab_vm.py poweroff
```

The first command remains attached. Run the fixture in the guest; inspect its
original state before attempting any continuation. Export evidence before
poweroff and wait for the original boot controller to exit before WAV acceptance.

Normal protected device/session admission, PipeWire/WirePlumber hardware
integration under the final service policy, delegated-holder/revocation and
restart handling, Settings, physical HP/3.5mm/Bluetooth acceptance and desktop
release integration remain work. This test does not update the USB image.
