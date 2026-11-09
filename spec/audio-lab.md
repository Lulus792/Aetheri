# Separate audio test carrier

The audio lab is a disposable Fedora development VM. It allows new policy and
service experiments without changing the original P0 guest or its quarantined
runtime. It is not the complete Aetheri desktop experiment environment and is
not a release image.

`tools/audio_lab_vm.py` owns only `vm/audio-lab`. Its command accepts no disk,
USB, source guest or backing-image argument. The guest has two vCPUs, 4 GiB RAM,
its own UEFI variables and a 32 GiB virtual disk with serial `AETH_AUDIO_LAB`.
The kickstart refuses a different DMI product, disk serial, size, disk count or
non-UEFI boot before partitioning. There is no physical disk passthrough.

The controller reuses an isolated instance of the existing VM helpers. It does
not retarget the original module. Config, process and command records use stable
`image_state` guards. Independent file-object witnesses reject replacement of
registered disk, NVRAM, keys and kickstart; they are not volume authentication.
Incomplete preparation, unknown starts, live processes and a remaining QMP
socket require inspection. Installation is never automatically repeated.

QMP uses a host-local AF_UNIX socket under the protected VM directory. SSH is
forwarded only on `127.0.0.1:19223`, using a separate key and known-hosts file.
Installation uses the already reviewed Fedora 44 netinst input and its official
release repository. A first-use SSH host key is accepted into that isolated
known-hosts file. Normal boots restrict guest networking; there are no host
folder shares. Package inventory is evidence of the installed base, not approval
to bypass exact dependency checks in the audio tools.

## Commands

Run from the checkout with the existing reviewed host tools available:

```text
python tools/audio_lab_vm.py status
python tools/audio_lab_vm.py install
python tools/audio_lab_vm.py boot
python tools/audio_lab_vm.py boot-hda
python tools/audio_lab_vm.py ssh "uname -r"
python tools/audio_lab_vm.py poweroff
python tools/test_audio_lab_vm.py
```

Use `install` only for a new empty registration. It and `boot` remain attached
until the original QEMU process exits. Observe status before another start;
neither a timeout nor a missing reply authorizes replay. The installation receipt
joins the original successful installer exit, command, registration and complete
console transcript. It permits a first boot. The private kickstart post log and
carrier marker still need checking over SSH before running privileged fixtures.

Do not modify the controller or kickstart while their original process is live.
The kickstart hash also prevents silently reconfiguring an existing registration.
Preserve process records, guard files and unresolved fixture evidence.

The explicit `boot-hda` profile adds virtual playback only. It records a fresh
private capture file and uses two protected local AF_UNIX QMP endpoints: one for
ordinary control, one retained for capture. A paused start and explicit capture
stop after guest shutdown provide finalized WAV evidence. Plain boot and install
are unchanged. See the [device diagnostic](audio-pcm-device.md) for permissions,
full-signal verification and the limits of the silent guard interval.

## Executed boundary

On 9 November 2026 the installer and first UEFI disk boot both exit 0. SSH checks
the private post marker, enforcing SELinux, own disk serial and geometry. Six
host tests initially cover isolated helper globals, registration/geometry rejection,
command confinement, kickstart guards, object replacement and installer evidence.

A separate administrative fixture passes real transient-unit
creation, a system-bus reference and exact FD3 process-handle transfer. It checks
UID/GID 1000, cap-zero/NNP, original invocation, held process/cgroup handles,
cgroup event-reader deactivation and unit collection. A final PID1 FD scan finds
no copy of that original extra object. This is a fixed `/usr/bin/sleep` test in
`unconfined_service_t`, not confined audio, app authorization or production drain.

The earlier failed trials and pending records remain archived. They are not
replayed or rewritten as successful runs. See the
[execution record](../brains/aetheri/journal/2026-11-09-audio-lab.md).

At that preflight checkpoint the new guest's glibc, libgcc and libselinux differed
from the accepted audio pins. The later [role record](../brains/aetheri/journal/2026-11-09-audio-mac.md)
documents the reviewed eight-package alignment, cold boot, actual enforcing
graph/policy/control/player tests and exact temporary-policy retirement. See the
[role fixture guide](audio-mac.md). The original guest receives only copied-store
offline compilation; its loaded policy and normal files remain unchanged.
