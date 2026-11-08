# Discovered-device audio test

The separate diskless guest now joins udev device discovery, WirePlumber's ALSA
monitor and ACP profile selection with automatic stereo playback. Two cold boots
deliver every 96,000-frame signal to the independently captured virtual HDA
output. This extends the [transport/HDA](audio.md) and [policy](audio-policy.md)
tests; it does not install audio in the USB desktop.

## What runs

The guest uses the same Linux kernel, HDA modules, PipeWire 1.6.11, ALSA
1.2.16.1 and WirePlumber 0.5.19 already reviewed for the preceding tests.
The [additional inventory](audio-device-inputs.json) selects the existing
systemd-udev 259.10 executable, kmod library and its notice by installed-object
hash. Those hashes are not new package signatures. Third-party notices remain
in the image; Aetheri's MIT license does not replace them.

The original C17 test supervisor loads the fixed QEMU HDA modules, starts udev
and checks its actual device database. A test-only rule initializes the known
PCI card and grants its control, playback and timer nodes to the fixed UID/GID
1000 account. The supervisor verifies the resulting ownership and mode; this
variant does not manually grant access or write mixer controls.

PipeWire, WirePlumber and the finite Aetheri playback client run as that ordinary
user with zero capabilities and NoNewPrivileges. WirePlumber's ALSA monitor
creates the card and output from udev information. The server configuration has
no static ALSA node. ACP selects the available Analog Stereo Output profile;
the test requests full volume/unmute through `wpctl`, then reads the actual
kernel mixer controls independently. Both automatic channel links must exist
before the client starts generating samples.

This guest has no login session, production SELinux policy, network, physical
disk, shared directory or host-speaker backend. Its root supervisor retains
the narrowly enumerated capabilities needed by this disposable fixture,
including NET_ADMIN for udev netlink and FOWNER for mode changes after chown.
None passes to the three ordinary audio processes. The source build runs as an
ordinary user in a private workspace on the enforcing carrier.

## Reproduce

Prepare the exact SDK, HDA and policy prefixes described in the preceding
specifications. Build as an ordinary x86-64 Linux user with the pinned kernel
and existing runtime objects available:

```sh
python3 tools/build_audio_device.py /absolute/fresh/output 0123456789abcdef0123456789abcdef \
  /absolute/audio-prefix /absolute/hda-prefix /absolute/policy-prefix
python3 tools/test_audio_hda_contract.py
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
  tests/test_audio_device.c -o /absolute/fresh/parser-test
/absolute/fresh/parser-test
```

The builder retains a separate manual-control image under `output/manual/image`.
On the prepared Windows host, use the pinned QEMU and a different new output
directory for each cold boot:

```sh
python tools/test_audio_hda.py --image /absolute/output/image --output local/device-boot
python tools/test_audio_hda.py --image /absolute/output/image --output local/device-reboot
python tools/test_audio_hda.py --image /absolute/output/manual/image --output local/manual-control
python tools/test_audio_hda.py --image /absolute/output/image --output local/no-card --missing-device
```

The controller checks the actual archive and source hashes before starting.
Each VM has new NVRAM and one retained original process/AF_UNIX QMP connection.
It captures the virtual controller before guest execution, observes actual guest
poweroff, explicitly finalizes the capture, then quits the powered-off original
VM. QEMU can close its transport before delivering the quit reply on Windows.
Only EOF/reset after proven poweroff permits waiting for the original zero exit;
the uncertain acknowledgement remains recorded and quit is never replayed.
Timeouts, unexpected protocol errors and nonzero exits still fail.

## Acceptance and limits

Both final policy cold boots and the manual regression contain the complete
48 kHz stereo signal with channel order intact and maximum error below 0.5 LSB.
The no-card guest fails at the device gate before starting udev or audio actors.
Thirty-one Python controls reject incomplete samples, inappropriate evidence,
failed udev permission updates and invalid shutdown handling. Twelve pure C
parser controls pass in Release and ASan/UBSan with leak detection. The included
privileged fixture entry is never called by those parser tests. See the
[executed record](../brains/aetheri/journal/2026-11-08-audio-device.md).

The fixed rule's direct non-system UID/GID ownership is deprecated by systemd
259.10. That diagnostic remains visible; the rule is not a proposed production
access model. Normal access needs session/device mediation and revocation.
ACP is used with UCM disabled for this QEMU codec, not as a global hardware
choice. The finite sink stays active through a bounded settling interval so the
full tail reaches capture; this is not a production device-drain proof.

`wpctl` and `pw-link` are bounded test oracles, not the future Settings protocol.
Next implement the original C17 management adapter, per-user supervision and
device mediation, then Settings and HP headphone output. Bluetooth, recording,
hotplug and suspend/resume remain separate work. Review/update the legacy Lua
baseline before normal rollout. No public capability, catalogue contract,
protected carrier service or USB image changes in this milestone.
