# Confined audio development roles

The explicit audio-lab fixture runs PipeWire, WirePlumber and our C clients in
separate SELinux domains, with UID/GID1000, no effective/permitted/inheritable/
ambient capabilities and NoNewPrivileges=1. It uses a synthetic stereo graph.
It does not enable normal desktop audio or open a physical sound device.

| Role | Purpose | Private resources |
| --- | --- | --- |
| Graph | PipeWire owns the listener and processes samples | Socket/locks, own memory type, synthetic capture FIFO |
| Policy | WirePlumber selects routes and remembers settings | Own state directory and memory type |
| Control | Our C adapter observes and changes volume/mute | Original graph process handle and its own buffers |
| Player | Our finite C producer supplies 48,000 stereo frames | Original graph process handle and its own buffers |
| Foreign | Native negative control | No graph connection, secret read or TCP socket creation |

The root test controller creates fixed transient system-scope units and holds
their original process pins, invocation IDs and cgroup directories/event readers.
It is temporary administrative tooling. It is not an installed authority, app
launcher, normal user-manager integration or a production recovery service.
Trusted host PID1, system-bus, proc and cgroup views are prerequisites.

## Files and memory

Only the dedicated [audio lab](audio-lab.md) is accepted by the CLI. Its prepared
library versions must first pass the existing audio input checks. The separate
[runtime manifest](audio-lab-runtime.json) and
`tools/prepare_audio_lab_runtime.py check|apply DIRECTORY` provide the reviewed
eight-package Fedora library alignment. Inputs are signed and sealed before
metadata, scripts and transaction use. Existing transaction records forbid replay.
Reboot before testing. This is carrier preparation, not Aetheri's app updater.

The root-owned executable tmpfs contains:

- `code`, `sdk`, `policy`, `control`: selected gateway/programs and checked
  libraries. Ordinary actors cannot modify these files.
- Each case's `conf`: root-owned, readable/mappable configuration.
- `run`: UID1000 socket and lock files, managed by the graph role.
- `state`: UID1000 state, writable only by the policy role.
- `capture.fifo`: the synthetic graph writes; the administrative observer reads.
- `secret`: ordinary DAC permits reading; the test roles' MAC rules deny it.

Each creator also has its own anonymous-memory type. Both current carriers use
`memfd_class=0`; the fixture explicitly requires that reviewed legacy profile.
Role-to-tmpfs type transitions give new memfds their creator's memory label.
The fixture refuses a different class profile for review, rather than granting
generic tmpfs write/map. Foreign access to all four memory types is prohibited.
Graph/client buffer exchange is allowed only between the selected trusted roles.
This is not an unrestricted graph grant for ordinary applications.

The caller supplies trusted administrative source/native inputs. Checksums and
held runtime observations here do not replace package publisher admission,
volume authentication, per-app protocol permissions or a production sandbox.
The process roles do not by themselves exclude every interpreter/delegation path.

## Execution and retirement

Build with `AETHERI_LINUX_IPC`, `AETHERI_AUDIO`, `AETHERI_AUDIO_CONTROL` and
`BUILD_TESTING`. In the registered root-only guest, invoke:

```text
python tools/test_audio_mac.py SDK POLICY CONTROL BUILD SOURCE OUTPUT
```

SDK prefixes must already satisfy their reviewed preparation tools. `BUILD`
contains `probe_audio_mac`, `aetheri-audio-control-probe` and
`aetheri-audio-check`; `SOURCE` supplies `policy/audio-test.cil`. Keep these
administrative inputs protected while the test runs. OUTPUT is a fresh private
evidence directory, normally beneath `/run`.

The native gateway waits before target execution. The control probe's optional
`--server-pidfd 3 --wait-for-start` additionally holds after exec and before
backend submission, so the controller can inspect its actual image/domain.
These barriers are fixture-only. Process handles are selected from the original
supervised graph, never adopted from a socket's numeric peer PID.

Memory factories run first. Three graph runs then test unity, cubic 50% volume
and mute. Actual original-policy stereo links precede producer activation.
The complete captured PCM is checked independently on the host. No header,
sample, tolerance or failed capture is repaired to obtain a pass.

Finite clients finish first, then policy and graph. Each original unit must be
terminal, with no main/control process or job; its held cgroup event reader must
return ENODEV and its original name must be absent. Unit references are released
only afterward. PID1 may legitimately retain its supervisory graph pidfd while
the graph runs: matching pidfs inodes do not distinguish open descriptions.
The controller retains an independent identity pin and checks PID1 for extra
graph objects after **all source and client units** are collected. It closes
its own extra pins before removing the private mount and temporary module.

Unknown effects retain pending records and resources. A separate inspected
administrative disposal of a failed fixed fixture does not settle a production
operation or reconstruct missing original handles. Preserve those histories.

The [executed record](../brains/aetheri/journal/2026-11-09-audio-mac.md) binds the
successful 19-unit run, all four memory factories, actual negative controls and
full PCM, alongside every failed attempt and remaining timing limitation.
Next connect these roles to normal per-user admission, device/endpoint ownership,
revocation/holder coordination and Settings, then test the HP's actual jack.

The later [per-user fixture](audio-user.md) now exercises these roles under the
real user manager, with native actor observations and fixed session-target stop.
Its trusted-manager permission profile remains temporary administrative tooling;
normal desktop/device admission is still open. The default system-scope fixture
retains its original role contexts and namespace profile.
