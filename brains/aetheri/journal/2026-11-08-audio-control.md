# Native audio management

The preceding `7bbe704` milestone joins actual udev/ACP discovery and virtual
HDA playback. This work adds the original C17 management adapter behind the
future Settings controls. It does not install normal audio or finish the OS.
The [contract](../../../spec/audio-control.md) describes ownership and reproduction.

## Implementation and inputs

The opt-in static backend calls libwireplumber's maintained default-node and
mixer API modules directly. No control CLI is launched by the library. It owns
bounded copied snapshots, connection-local output references, revision checks,
one pending command, a server barrier and effective-state readback. Timeout or
disconnect never proves cancellation or permits automatic replay. Close retires
observation without undoing an already submitted change.

Use the reviewed WirePlumber 0.5.19, PipeWire 1.6.11 and GLib 2.88.0 runtime
families. Two signed matching development RPMs add 351 headers / 2,825,653 bytes.
One existing GLib object / 1,469,656 bytes is copied for explicit linking/loading.
The header prefix requires the policy prefix and its existing third-party
notices; it is not a standalone redistributable SDK. The full Fedora44 key
fingerprint and payload digests are checked against the same sealed RPM objects
used for named regular-member extraction. No installer/package script runs.

The explicit Unix connection helper now also serves the finite producer. It
passes the original connected nonblocking CLOEXEC FD to the backend rather than
allowing ambient configuration to choose the endpoint. Same-UID credentials
are only a sanity check; normal server/process/session admission remains work.

## Observed failures and boundaries

Evidence remains under ignored `local/audio-control-2026-11-08`.

- Header inventory-v1 mistakenly requires directory entries to be regular files.
  V2 skips directories and checks the actual named header members. Both failed
  and successful exports remain retained.
- Build-v1 compiles the strict C17 sources but fails linking the private
  WirePlumber library's PipeWire dependency. Link the exact existing private
  PipeWire DSO explicitly; do not fall back to an unreviewed system library.
- Runtime-v1/v2 reaches a five-second startup deadline. The core has its private
  context, but nested GTask operations capture the thread-default context.
  Require that context to remain pushed for the complete lifetime. The native
  driver proves rejection without it and success with it; do not iterate an
  unrelated global context or remove the deadline.
- Runtime-v3 performs real default/volume/mute operations, then exposes a routing
  violation: a missing explicit target still connects to the valid server in
  PIPEWIRE_REMOTE. Pass an explicitly connected AF_UNIX FD in both adapter and
  producer. The final missing-target controls cannot fall back.
- Runtime-v4 completes those controls and safe close cases, but changing defaults
  while the inactive DONT_RECONNECT diagnostic already exists leaves extra
  links. Keep exact stereo-link acceptance. Test selection before creating that
  deliberately retained-route stream; existing-stream migration remains open.
- Runtime-v5 observes the output collection before policy chooses a default.
  Wait for the actual expected default; installed collection is not selection
  readiness. Runtime-v6 then passes both variants and all original audio tests.
- Runtime-v7 adds actual paused-server timeouts, explicit new observers, SDK
  corruption controls and rebuilt HDA images. Runtime-v8 additionally passes the
  cleanup-exception tests, but its sanitized maximum transport capture contains
  one inserted silent 1024-frame quantum at source frame 80,896. Later samples
  resume at the original phase with that offset. The strict verifier rejects it
  despite the producer's local DRAINED result. Preserve that rejected capture.
- The producer now prepares its complete bounded signal before streaming; each
  process callback only copies its next slice. This reduces callback work
  without changing the 1024-frame clock, sample tolerance or channel/frame-order
  requirements. Runtime-v9 passes the full suites. This does not establish the
  cause of every TCG scheduling stall or guarantee glitch-free normal audio.

Source review also finds unescaped node-name interpolation in the pinned default
module. The adapter displays such names but refuses to submit them as defaults;
an actual quote-containing third output exercises the guard. It also refuses
duplicate default names. Library callbacks retain the owner through cancellation;
the caller keeps the private context alive until final data retirement.

## Final native evidence

Runtime-v9 uses the current source, as ordinary UID1000 inside new private
tmpfs/mount namespaces of the enforcing carrier, boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. Original helper 1235827, token
`8006a4cf878e429eaeb6657505d7c5e6`, exits 0 after all 1,121 export members are
independently checked. Bundle SHA256:
`eda0b6906ce094741385fdb3a3b47ebd639e5d3686fd48411eba7fa9ff7e8c84`.

Release and ASan/UBSan with leak detection each pass:

- Three actual policy/control graphs: unity, cubic 50% gain 0.125 and mute.
  Each has three discovered outputs, actual default switching/readback,
  stale/foreign/invalid/unsafe-name rejection and mute-preserving volume changes.
- Full 48,000-frame non-muted captures agree within 3.71e-9 at unity and 4.63e-10
  at half volume. Muted output is actually zero. Original server/policy/client
  and controller images, library paths, UID/capability/NNP and Unix-only socket
  observations remain checked. No audio/video device FD is present.
- Missing explicit endpoints fail even with a valid ambient remote. Immediate
  close, wrong-thread rejection and server disappearance pass. After a submitted
  command's original server is paused, the deadline leaves UNKNOWN, with no
  restored readiness or replay. An explicit replacement has a new connection
  identity and its own history. All six such timeout cases pass.
- Injected exercise and cleanup exceptions preserve the error while retiring
  both original server/manager handles. Two actual fault cases prove four
  original actor retirements. A controller-cleanup exception can no longer skip
  the surrounding fixture's cleanup.
- The four original audio CTest groups pass: seven input cases, seven PCM
  verifier cases, 16 invalid requests and seven transport graphs per variant.
  Six real SDK-clone corruptions are rejected; original/restored checks pass.

Only Aetheri's adapter, connection helper, producer and native driver are
instrumented. Pinned upstream binaries/libraries retain their package builds;
this is not an upstream sanitizer or security clearance.

## Hardware regression and next work

The final separately rebuilt RAM image exercises the corrected finite producer
through udev/WirePlumber and virtual HDA. Its cold-boot/capture evidence is joined
by image/source hashes in the local acceptance audit. Native management itself
is still tested on private virtual outputs, not installed as the HDA owner.

| Actual final image test | Result | Captured frames | Signal start |
| --- | --- | ---: | ---: |
| Discovered HDA v2 | All 96,000 stereo frames | 221,944 | 101,376 |
| Second cold boot v2 | All 96,000 stereo frames | 269,487 | 149,358 |
| Manual HDA control v2 | All 96,000 stereo frames | 143,611 | 24,576 |
| No-card v2 | Device failure before actor startup | — | — |

All three complete HDA signals have maximum error below 0.5 LSB. Each original
QEMU exits 0 after independently observed guest shutdown and capture retirement.
No WAV header or failed capture is repaired. These guests have no NIC, physical
disk, shared directory or host-speaker output.

The independent audit checks 552 current code/pin/RPM inputs, all 1,121 exported
members, both new header signature receipts, all 351 actual SDK headers, twelve
controller actor snapshots, six timeout cases, six SDK corruption controls and
the complete native/HDA captures. All twelve native build/inventory helpers
retire. Read-only preservation retains 445 original source inputs, 19 protected
hashes, 31 frozen SQL/XML and 52 normal P0 files, the original authority/CLOSED
history and foreign project/UI work. The user's USB documents stay unchanged.

Normal device mediation, per-user orchestration, native HDA management, Settings
and HP headphone/Bluetooth output remain work. Review/update the legacy Lua
runtime before normal rollout. Existing-stream migration, recording, hotplug,
suspend and production real-time scheduling have separate acceptance boundaries.
No normal service/profile, wire/catalogue capability or USB replacement follows.
The full OS goal remains active; NexaUI handoff remains closed.
