# Confined audio through the virtual device

This explicit audio-lab test connects the existing PipeWire graph, WirePlumber
routing policy and original C control/player clients to the virtual HDA output.
It extends the [per-user fixture](audio-user.md). It is development evidence for
normal audio integration, not an installed desktop service or physical HP test.

## Device access

ALSA's reviewed PCM adapter opens its configured device path itself. The earlier
[preopened-FD diagnostic](audio-pcm-device.md) therefore remains a separate test.
Here root selects the fixed virtual PCI card and its playback/control nodes,
checks their original file objects, root:audio ownership, mode 0660 and contexts,
and refuses a preexisting ACL. It sets the virtual mixer's checked unity baseline.

The test adds a temporary named-user ACL for UID 1000 and two temporary device
types. It changes neither node ownership nor the user's groups. The graph role
alone may open/read/write/ioctl/map the playback node and open/read/write/ioctl
the control node. Compiled prohibitions exclude the policy, control, player and
foreign roles from raw device I/O. Ordinary apps are intended to use mediated
audio requests rather than receive the device itself.

A same-UID, cap-zero, NoNewPrivileges control proves direct opens fail before
the grant, succeed while it is active and fail after restoration. A separate
confined foreign service must still fail both opens during that active grant.
Its actual enforcing AVCs must identify the original actor and both selected
device types. On this carrier those records arrive through the journal's audit
transport; a kernel-only journal query excludes them. EACCES alone is not used
as proof of the permission layer.

This ACL is fixed administrative test provisioning. It does not authenticate a
login, implement logind/uaccess integration, fence future dispatch or provide
production device revocation. The unconfined lab manager/tools retain the trust
boundary described in the per-user fixture. Do not grant this profile to normal
unconfined apps or install its temporary transition rule as production policy.

## Code and configuration

The graph loads the already reviewed ALSA 1.2.16.1 and PipeWire 1.6.11 plugin
from a separate checked prefix. A second small prefix projects the existing
pinned libudev DSO and its license from the accepted HDA runtime manifest. Its
full inventory, bytes, modes and exact alias are checked. The fresh carrier's
same-named libudev has different bytes, so it is not silently substituted.
No system library is replaced and no new package version is adopted.

The projected objects have installed-file hash evidence; this does not create
new RPM signature or publisher-admission receipts. ELF metadata shows the ALSA
plugin needs libasound, libudev, libgcc, libm and libc. The remaining runtime uses
the existing reviewed SDKs and aligned carrier libraries. Actual graph mappings
must include the selected ALSA/plugin/libudev objects. Complete production
configuration/dependency provenance still requires its own admission boundary.

Root-owned configuration creates one explicit ALSA sink with stereo S16_LE at
48 kHz. Hardware discovery/ACP remains disabled in the routing policy; the fixed
card selection is not normal device enumeration. WirePlumber still owns the
observed stereo links and the C controller uses the original graph process pin.
The producer sends exactly 96,000 F32 reference frames. The real adapter converts
them for the device. No synthetic FIFO payload may substitute for host audio.

The synthetic fixture now exposes small configuration/environment/frame/result
hooks. Their defaults retain the existing FIFO graph, 48,000-frame signal and
PCM validation. The user fixture defaults to all three existing session cases;
the hardware subclass selects one case per cold-boot capture. This keeps the
shared actor, session-target, client, buffer and retirement checks in one flow.

## Acceptance and lifetime

Use a fresh HDA boot and protected inputs for each unity, half or muted case.
Both source services run as UID/GID 1000, cap-zero and NoNewPrivileges, and have
native original-manager/image/context observers. Control/player verify their
actual original graph peer. Foreign graph/secret/TCP denials remain required.

After the finite player finishes, a bounded test settling interval precedes
session-target stop. Both original sources must end through that relationship
before independent unit/cgroup retirement. This interval is not a production
device-drain proof. Original holders, worker and supervisor FD copies retire;
then the exact original device contexts and absence of ACLs are restored and
all temporary modules/targets/mounts are removed.

Guest success records `host_capture_required`; it does not accept audio from a
DRAIN marker. Export evidence before shutdown. After explicit capture stop and
original QEMU exit, the existing unmodified WAV verifier must accept all 96,000
tone frames at unity or cubic half gain, or sufficient completely silent output
for mute. Keep its two-LSB bound and all failed raw files. Never trim or repair
a capture to obtain a pass.

## Run

Only the separate root-only enforcing audio lab is supported. BUILD supplies
`probe_audio_mac`, `probe_audio_actor`, `probe_audio_pcm`,
`probe_audio_device_access`, `aetheri-audio-check` and the native control probe.
RUNTIME contains only the checked libudev projection described above.

```text
python tools/audio_lab_vm.py boot-hda
python tools/test_audio_hardware.py SDK POLICY CONTROL BUILD SOURCE HARDWARE RUNTIME FRESH_OUTPUT --case unity
python tools/audio_lab_vm.py poweroff
```

The fixture command runs in the guest. Use a fresh result for every attempt.
Inspect unknown effects before any continuation; the tooling is not production
recovery. Normal protected services, session/device admission, discovery/ACP,
Settings, suspend/lock/revocation/delegation, and HP jack/Bluetooth acceptance
remain work. No USB update follows from this test.

Primary implementations: PipeWire 1.6.11
[ALSA adapter](https://raw.githubusercontent.com/PipeWire/pipewire/1.6.11/spa/plugins/alsa/alsa-pcm.c),
ALSA 1.2.16.1
[hardware plugin](https://raw.githubusercontent.com/alsa-project/alsa-lib/v1.2.16.1/src/pcm/pcm_hw.c)
and Linux [ACL xattr ABI](https://raw.githubusercontent.com/torvalds/linux/master/include/uapi/linux/posix_acl_xattr.h).
The actual carrier UAPI headers and kernel grant/restore checks separately bind
the fixed ACL encoding used here; upstream master alone is not version evidence.
