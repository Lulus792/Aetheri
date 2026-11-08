# Discovered-device audio

The preceding `4b6274c` milestone proves policy against FIFO capture. This work
joins actual udev enumeration and ALSA/ACP discovery with virtual HDA playback.
It advances the OS goal without installing audio in the desktop or changing USB.
The [contract](../../../spec/audio-device.md) describes reproduction and limits.

## Boundary

Reuse the reviewed PipeWire, ALSA, WirePlumber and same-kernel HDA inputs. Add
only three pinned existing udev/kmod library/notice objects and fixed aliases.
The builder preserves the manual image and builds a separate policy owner.
Its root has no static ALSA node; a named trusted WirePlumber profile requires
the ALSA monitor, uses ACP without UCM for QEMU and disables other hardware
monitors/D-Bus. The fixed account exists only inside this RAM test image.

The root supervisor checks udev's database and node access. PipeWire,
WirePlumber and the finite C17 client are UID1000, cap-zero and NNP. The policy
selects Analog Stereo Output, applies volume/unmute and creates both channel
links before the retained client receives its start signal. Root reads the
kernel mixer controls without writing them. Full host PCM remains mandatory;
local DRAINED alone never proves device drain or audibility.

## Observed failures

Evidence remains under ignored `local/audio-device-2026-11-08`.

- Build/boot v1 delivers the full signal, but udev cannot resolve numeric UID1000
  because the minimal base lacks that account. Its old manual grants mask that
  error. This is historical playback evidence, not accepted udev ownership.
- V2 adds the account and removes all manual grants from the policy path.
  udev changes ownership/mode but reports EPERM in its later mode update.
  FOWNER is absent after chown transfers ownership. The stronger final gate
  rejects that diagnostic even when the observed node tuple happens to match.
- V2 also observes QEMU closing the original Windows QMP transport after guest
  poweroff, capture finalization and a sent quit, before its response arrives.
  The failed attempt remains preserved. A bounded EOF/reset terminal path now
  retains that unknown acknowledgement, never resends quit and requires the
  original zero exit. Timeout/protocol/nonzero cases remain failures.
- V3 adds FOWNER only to the disposable root supervisor. udev permission updates
  then succeed without those errors. Ordinary actors retain zero capabilities.
  systemd's direct non-system UID/GID deprecation stays visible; this is not the
  production permission scheme.

The readiness poll now lists outputs quietly before inspecting the default;
expected startup absence no longer emits repeated default-ID errors. Pure
bounded parsers reject malformed/duplicate properties and wrong stereo links.

## Final execution

Build-v3 runs as ordinary UID1000 in a fresh private tmpfs/mount namespace on
the existing enforcing carrier, boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`.
Original helper 1226346, token `4aaca863307142ef8591e4ea91578e1c`, exits 0 after
all 25 export members and the bundle hash are independently checked:
`2f39996ec35b8d4c81640daf559c99642166b20c84ce18ed0c48843a8505be8c`.
No package installation or normal source/build/service update occurs.

| Actual cold boot | Result | Captured frames | Signal start |
| --- | --- | ---: | ---: |
| Policy v3 | All 96,000 ordered stereo frames | 190,704 | 71,680 |
| Policy v3 second boot | All 96,000 ordered stereo frames | 191,551 | 72,212 |
| Manual control v3 | All 96,000 ordered stereo frames | 135,677 | 17,408 |
| No-card v3 | Device failure before any actor starts | — | — |

All three complete signals have maximum error 0.499658 LSB. Captures have valid
unmodified finalized RIFF headers and silent prefix/suffix. Each VM has a unique
UUID/new NVRAM, no NIC/disks/shared directories/host audio and one original
AF_UNIX QMP connection. All four final quits are acknowledged and original
processes exit 0 after guest shutdown. This does not relabel v2's unknown reply.

Thirty-one Python contract cases pass on native Linux. Twelve C parser controls
pass in strict C17 Release and ASan/UBSan with leak detection; their included
privileged entry point is never called. These supplement actual boots and
sample comparison rather than substituting for them.

The independent audit checks 541 current code/pin/RPM inputs, all 25 exported
members, ten repeated signature/payload receipts and 243 policy/extra objects
inside the actual newc root. It independently recomputes each full stereo
pattern, rejects manual evidence as joined policy and rejects missing card
evidence. All three build helper workspace/control paths have retired.
Read-only preservation retains 445 original source inputs, 19 protected hashes,
31 frozen SQL/XML and 52 normal P0 files, the original authority and CLOSED
history. Foreign project/UI work and the user's USB documents stay untouched.

## Next

Implement the original C17 audio-management adapter, per-user supervision and
mediated device access. Then connect Settings and test HP headphone output.
Review/update the legacy Lua baseline before rollout. Bluetooth, recording,
hotplug, suspend/resume and physical audibility remain separate work. No normal
service/profile, installed authority, catalogue/wire capability or full OS
acceptance follows from this fixed RAM test. The full goal remains active.
