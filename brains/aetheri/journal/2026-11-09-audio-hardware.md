# Confined graph reaches virtual HDA

9 November 2026. PipeWire, WirePlumber routing and the original C controller and
player now pass unity, reduced-volume and mute tests through the audio lab's
actual virtual PCM device. Three independent cold-boot WAV captures pass the
unchanged verifier. The default synthetic per-user fixture also passes after
the shared-flow refactor. Normal protected services, Settings and physical
audio remain separate work. See the [contract](../../../spec/audio-hardware.md).

## Changes

The new hardware subclass reuses the existing actor, original-peer, stereo-link,
session-target and retirement flow. Small configuration/environment/frame/result
hooks retain the synthetic defaults. Its fixed ALSA adapter uses the reviewed
HDA prefix; WirePlumber still creates the actual observed stereo links.
The native producer sends exactly 96,000 frames, without the earlier raw-PCM
diagnostic's silent guard. The graph continues during the bounded settling
interval; full host comparison remains mandatory rather than inferring device
drain from that delay or a client marker.

The root-only fixture adds temporary UID 1000 ACLs and playback/control test
types. Node owner/group/mode and supplementary user groups are unchanged. Only
the graph backend may open those nodes. Same-UID ordinary opens distinguish
before/active/after DAC behavior; a confined foreign actor still fails both
active opens, corroborated by its actual enforcing AVCs. Those AVCs reach the
journal's audit transport, rather than `journalctl -k` output on this carrier.
All original ACL absence and raw contexts are restored after owned actors end.

The fixed 44-byte ACL encoding follows the observed UAPI headers and actual
kernel round trip. Original header SHA256 values are
`79a2903236a9ffd32702e6879e48e219556b860486119145561f64c591f87e96`
and `6d8268f585f020db01fa3ff8172cde7e75e65ae56adbd9a664971be9692f55b3`.
This is temporary provisioning for the fixed virtual account, not a general
ACL manager, session authority or production app sandbox.

## Runtime inputs and controls

The fresh guest's libudev has the expected filename but SHA256
`4b312790c988d0c0af174cde5de4253aed332412ccc1f7904100736b2ca5f47d`,
which differs from the accepted HDA object's
`22513c89165308495bb0d4e93c4885b52b038920d78856c50d709d908952a9a4`.
Do not infer compatibility/admission from that name. A separate private prefix
projects the already pinned original object and its license, read-only from
the original guest. It changes no system library or package version. These are
installed-object receipts, not newly acquired package signatures.

ELF metadata confirms the ALSA plugin needs libasound, libudev, libgcc, libm and
libc; libudev adds only the already aligned libgcc/libc. Actual graph mappings
include the selected plugin, ALSA and private libudev. Prefix checks reject
changed bytes, an extra file and a changed alias in every actual hardware setup.
The untouched original runtime remains a prerequisite for the known library
closure; this does not establish general configuration/dependency provenance.

The producer/controller source closure and three input manifests match the
earlier checked audio SDK artifact. The actor/gateway/PCM/device-access probes
are freshly built. No manager object from the older producer build is adopted.
Evidence lives under `local/audio-hardware-2026-11-09`:

- Release build v1: helper 1332178/token `06f253100bd14b898becacd8b2b625df`,
  original exit 0, 41 checked export members, SHA256
  `d02a1d74a3813638ad7a2856110e852b6733b2308606877b8db474fe6c82b788`.
- ASan/UBSan build v2: helper 1332699/token `73f292e0996646aab818102884afb5a9`,
  original exit 0, 41 members, SHA256
  `0fbc063e89d6cb8a625570878833fe0f02282a77688a5e880e9c4d33b3c48b7f`.

Both builds pass four malformed access-probe argument controls, five existing
PCM bad-input controls, signed/sealed HDA extraction and copied-store compilation
of base+user+device CIL. The original policy store and loaded policy remain
unchanged. Compiled device prohibitions cover every client role; actual new raw
device-open denial probes use the foreign role. Positive confined runs use
Release; no instrumentation ptrace permission is added.

## Retained setup failures

The first staged HDA prefix has raw-transfer file modes instead of its declared
modes. It fails before fixture creation or device effects. The next staging
sets exact declared modes before validation; the old staging is preserved.

The second attempt's device grant and foreign denials work, but its kernel-only
journal query misses the actual audit records. The root controller retains its
original handles for inspection. Administrative retirement first stops the exact
finite foreign unit. A target observation then exposes the absence of service-
only MainPID/ControlPID properties on a target; the first administrative action
stops there. A separate inspected action retires the untouched unused target,
restores the original nodes and removes the exact worker/mount/modules. No
sample was dispatched, and no unknown mutation is replayed.

This failed record remains failed-administratively-retired, with both action
sources and the original needs-inspection snapshot. Its 58-member independently
verified export SHA256 is
`48e73bb973fb77f9a435b2530375cd7d74c0f16b94fa694ce5d5423552a71351`.
The corrected collector requires both actual device-type AVCs and permissive=0;
it does not weaken the permission check or add a grant.

## Actual hardware-profile cases

All three fixed runs finish six role services, two native source observers and
one ordinary test-session target. Original graph/policy processes must still be
live before target stop, then independently end. Original cgroups, jobs, extra
FDs and worker retire before temporary resources disappear. Their guest records
require separate host capture acceptance.

| Case | Guest boot | Original graph PID | QEMU exit 0 |
| --- | --- | --- | --- |
| Unity | `b5d1f79b-f72e-42f9-b8e0-80e1179efdba` | 3625 | 48192 |
| Half | `9cef6e32-04e6-468a-9e8b-a9ab64b0b79c` | 2149 | 60016 |
| Mute | `b76bd04b-4b8d-47ea-b201-40bc140e6843` | 2157 | 61456 |

Unity captures 135,961 frames, reference start 14,737, all 96,000 tone frames and
max error 0.4996579601015583 LSB. WAV SHA256
`0b132bfa69ad9dbdac35dbf3aa8462f073013a036273478dd650aa74bce3f826`.
Half captures 136,915 frames, start 14,699, all 96,000 reference frames at cubic
gain 0.125 and max error 0.4979569299344817 LSB. WAV SHA256
`e5c13c92e6e2da73d581c8e5722d92f52a7f80551286a3b757a8a47a29617060`.
Mute captures 147,667 entirely zero frames while the normal tone producer runs;
WAV SHA256 `23a74da4d3fee1ba8a41edee8e1defff6c074919988529b01b5642fd62db1322`.
The two-LSB reference bound is unchanged. No WAV bytes are repaired or trimmed.

Each guest export has 83 checked members. Their SHA256 values, in that order:
`d98b1a1ce04268917a92697574b0f8630a41de88bfb483c15d3cef41e87c8271`,
`bf9ca2cc41a650b5b781e80f947f04bce14a3fe2960e2d384274f244d2ca6336`,
`b388f68b82d8106c50b01c201cf05266b59902e6020a8f87804ae89a874af9d2`.
File/directory witnesses, original process/UUID, actual shutdown, explicit
capture stop and whole-file hashes bind each raw capture.

## Default regression and preservation

A fourth ordinary boot has no HDA device and retains the original default VM
command. Actual UserAudio fixture `3184612278ad46178eed904303ed753e`, boot
`609c0375-166e-455d-b335-d4edd66a5d77`, passes 19 role services, six observers,
three session targets, all memory factories and full unity/half/muted F32 checks.
Each capture has 49,152 frames with 48,000 reference frames; non-silent start 1024,
maximum errors 3.7005065955941774e-09 and 4.6256332444927217e-10. The muted case is
entirely zero. All temporary resources retire. Its 89-member checked export
SHA256 is `d25df7319670ed80586f184b3375f6c23ddc3281b9b2ad4c3882d7994168ff26`.
Original QEMU 67656 exits0. In total the successful runs retire 37 role services,
12 native observers and six session targets. Historical default-clock losses
remain open; these successful observations do not rewrite those failed runs.

The final original-carrier audit preserves 445 source inputs, 19 protected hashes,
31 frozen SQL/XML contracts, 52 normal files, the authority/CLOSED/quarantine,
loaded/stored policy and all four foreign worktree files. Original P0 stays
running, the audio lab is off, and USB test documents remain untouched.

Next establish normal protected service/session/device ownership and connect the
native audio control path to Settings. Discovery/ACP, complete dependency and
environment provenance, app admission, device revocation/holders/restart,
physical HP jack/Bluetooth, suspend and desktop release acceptance remain work.
This fixed virtual profile does not complete the OS/apps/installer/update/
hardware/experiment-desktop goal.
