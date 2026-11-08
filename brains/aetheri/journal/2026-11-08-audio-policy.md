# Private audio policy

The previous goal turn publishes actual virtual HDA playback in `088f538`.
This turn advances the full OS goal with maintained automatic linking and
volume/mute policy. It does not complete normal audio, Settings or the OS.

## Selection and scope

Select signed Fedora 44 WirePlumber 0.5.19-1.fc44. Actual metadata identifies MIT
and the matching source RPM. Full key fingerprint
`36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6` and payload SHA256 are checked on
the same sealed input used for data-only extraction. Reuse the already selected
PipeWire/ALSA RPMs. Preserve all third-party notices.

The [manifest](../../../spec/audio-policy-inputs.json) contains 232 package
members / 2,060,405 bytes: 72 runtime, 156 explicitly staged device-data, one
test utility and three notices. Eight separately hashed existing library/notice
objects add 2,855,590 bytes. This is not the future desktop's total footprint.
The unused file-monitor module is removed after actual loaded-library review.
ALSA/ACP data is prepared for subsequent hardware work and unused by this suite.

The installed Lua baseline already exists in the development image inventory.
Its version review and known limitations are recorded in the manifest; the test
does not clear it for production. Only trusted pinned policy scripts run here.
The original C17 language, existing SDK and normal session remain unchanged.

## Executed failures and fixes

All evidence remains under ignored `local/audio-policy-2026-11-08`.

- Inventory-v1 fails on calling `startswith` on a borrowed memoryview. Compare
  its bounded leading bytes instead. V2/V3 verify actual signatures, inventories,
  licenses and ELF dependencies without installing packages.
- Runtime-v1 cannot resolve the extra absolute module name. Compose a new private
  directory of aliases to only the checked prefixes; keep both prefixes immutable.
- Runtime-v2 loads metadata before the native protocol and fails with Protocol
  error. Load protocol first. Runtime-v3 actually auto-links, but the control
  client cannot bind metadata because its minimal client configuration lacks
  that module. Add it after native protocol on that client too.
- Runtime-v4 passes all Release policy cases and both existing transport suites,
  but its sanitized policy gate times out despite the final graph containing
  valid stereo links. The verifier holds early port IDs across policy adapter
  reconfiguration. Rejoin current roles/ports/links from the same snapshot.
- Runtime-v5 passes both variants and records port reconfiguration. In final
  Runtime-v6, five of six managed graphs change port IDs; the old cached-pair
  predicate falsely rejects two of those actual successful graphs. The final
  variant also checks the pinned server's peer PID/UID against the original
  retained policy child. ID changes do not always change the complete pair set.

No failed attempt is replayed or relabeled as successful. Each original helper
exports immutable member hashes; the host verifies all members and bundle before
acknowledging retirement. Unknown results are never treated as a pass.

## Final evidence

Runtime-v6 uses the final trimmed manifest and current source. Original helper
1221397, token `cc9de9cf870344d982798364a2c55347`, exits 0 after all 142 export
members are independently checked. Bundle SHA256:
`0cf686b67dbfacb49e0fddf21bd84672dad6736598c178fcdb035327078a7965`.
The source build uses fresh private tmpfs/mount namespaces on the existing
enforcing carrier, boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`, as UID 1000.
Normal source/build/services, packages and policy are not replaced.

Both Release and ASan/UBSan pass the four original audio CTest groups: seven
input cases, seven independent PCM-verifier cases, 16 invalid CLI requests and
seven real transport graphs per variant. The new producer flag is opt-in;
default manual graphs still pass. Both variants additionally pass four policy
graphs each:

| Case | Expected result |
| --- | --- |
| Unity | Complete 48,000-frame stereo signal, maximum error below 4e-9 |
| 50% | Actual cubic gain 0.125, maximum error below 5e-10 |
| Muted | Producer drains 257 non-silent source frames; captured output is zero |
| Without manager | No automatic links or signal; original player times out without success |

The two links belong to the original policy child, as observed through the
trusted private server's peer fields. Its original pidfd/image/UID/capability
checks bracket the test. These are controlled fixture joins, not a new general
caller-authority mechanism. Nodes are virtual FIFO objects; all hardware
monitors, session/system D-Bus and notifications remain disabled.
The sanitized variant instruments Aetheri's producer; upstream executables and
libraries remain their pinned package builds. This does not clear upstream leaks
or the separately recorded Lua limitations.

Six actual clone corruptions are rejected: changed library, missing script,
changed alias, unexplained file, changed profile and aliased prefix. Original
and restored-clone checks pass. Clone mutation never changes the real prefix.
All actual processes stop through original handles; policy stops before server.

The local acceptance audit rejoins current code/pins, exported members, actual
captures, actor/library and automatic-link evidence, prefix controls and the
old cached-port negative. Read-only preservation retains the original 445
source inputs, 19 protected hashes, 31 frozen SQL/XML and 52 normal P0 files.
The protected authority/CLOSED history and foreign project/UI work stay intact.
No USB or user test document is read, changed or replaced.
The independent audit checks 531 current code/pin inputs, all 142 export members,
18 managed actor snapshots and all 17 selected ELF members in actual maps. All
nine native helper workspace/control paths have retired.

## Next integration

Join actual udev enumeration and ACP profile/route selection with the separate
HDA image and this policy profile. A FIFO policy pass and a manually configured
HDA pass are not joined device-policy acceptance. Review/update the Lua runtime
before normal rollout. Then implement the C17 management adapter, mediated
per-user service/device boundary, Settings and HP output, followed by Bluetooth.
Keep `wpctl` as a test oracle rather than a UI subprocess protocol. No public
capability, catalogue schema, normal installed service or production audio
readiness follows from this milestone. Full OS scope remains active.
