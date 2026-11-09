# Lua 5.5 policy runtime and actual PCM — 9 November 2026

## Result

The private Lua 5.5 candidate now passes real graph/default-sink/volume/mute
and native C control-adapter tests against the unchanged RPM baseline. The
normal runtime/image manifests are unchanged. Read the
[projection contract](../../../spec/audio-policy-candidate.md) and earlier
[module build](2026-11-09-audio-policy-lua.md).

The fresh projection contains 241 regular payload files and five aliases,
5,718,952 bytes plus its metadata. It retains exact original daemon/core/scripts/
other modules, replaces the scripting module, removes Lua 5.4 and adds the pinned
5.5 library/notice. The full original WirePlumber source archive retains source
copyrights alongside its existing MIT license. No compiler, header, Meson/Ninja,
new background service or system package is installed.

The candidate receipt distinguishes rebuilt origins from unchanged RPM inputs.
All expected bytes/modes/aliases derive from independently selected public
profiles, not from the receipt. Existing prefix checks/defaults stay strict;
the explicit test-only `--policy-profile lua55-candidate` selects the new checker.
The ordinary checker now reports a validation error for receipts without its
required profile field instead of exposing a KeyError. Valid original prefixes
still pass all six existing corruption controls.

## Actual graph and native adapter

Separate audio lab UUID `033ac49e-68bb-47a3-9c9c-26a812dc8300` remains under
original QEMU43164/controller10491, boot
`32baadd1-a444-4aa8-8c32-56eb3ce22c21`. DMI/boot/enforcing admission is repeated
for each fresh wrapper. Source inputs are independently selected verified native
exports. A private mount namespace/tmpfs runs UID/GID1000 with empty groups,
cap-zero/NNP observations. Guest networking remains restricted. No confined
test policy, transient service or login fixture is installed by these cases.

Graph v3 passes four cases for each of `rpm` and `lua55-candidate`: unity gain,
half user volume (cubic gain 0.125), mute and missing-manager deadline. All three
positive PCM captures have identical baseline/candidate hashes. Maximum observed
sample error is below 3.71e-9 at unity, well inside the independent 4e-6 bound.
Candidate live policy maps contain exactly its new Lua 5.5 and rebuilt scripting
module; original PID pins, executable and graph creator observations remain.

Native control v2 passes four PCM cases per profile plus one deliberate cleanup
fault per profile. The three ordinary cases cover eight invalid/stale requests,
default switch, unsafe names, mute preservation, missing remote, immediate close,
wrong thread, timeout/unknown and server loss. The fourth rejects a mismatched
original peer, including without SPA plugins and without protocol fallback.
Each injected cleanup failure preserves its error while observing both original
actors ended through their retained pidfds. No test treats EOF alone as drain.
Positive baseline/candidate PCM hashes again match exactly.

The ten candidate corruption/selection controls pass: each checker rejects the
other profile, changed module/Lua/source bytes, extra old Lua, missing Lua notice,
changed core alias, changed candidate record and wrong Lua mode. Source prefixes
and the actual candidate remain unchanged after checks in separate clones.

## Evidence and retained failures

Local inputs/intents/logs are in `local/audio-policy-candidate-2026-11-09`.
Every attempt has a new UUID, tmpfs and root-owned protected output. Parents
wait for the original test child; full evidence archives retain failed trees.
Namespace exit retires scratch mounts; this is not durable app recovery.

- Graph v1 rejects the candidate through the old checker with a KeyError for
  its absent RPM profile field. No audio actor starts. The checker now rejects
  that format with ValueError and retains its exact old positive boundary.
- Graph v2 reaches the deliberately removed Lua notice but exposes the new
  checker's FileNotFoundError. No audio actor starts. A missing/aliased regular
  member now has an explicit validation error; the missing-notice guard remains.
- Graph v3 exits0 (original host controller41972). Its protected output is
  `/root/aetheri-policy-candidate-c1703602ed054c1fa7936691e3a930a1`.
  Full export SHA256:
  `9b63bc8906f78c4bcf6f8e73c6f078ef78eedcaaf8e37568ba3208a53022455e`.
  All 1,489 file lengths/hashes are independently checked. The host recomputes
  all six positive PCM captures against the actual sine/gain/silence contract.
- Native control v1 selects the demo probe instead of the interactive test
  driver. It exits2 on the first request; the graph harness retires its original
  actors. Preserve its output and failure; it is not a Lua compatibility failure.
- Native control v2 uses the existing reviewed `aetheri-audio-control-test`.
  Original host controller99898 exits0. Protected output is
  `/root/aetheri-policy-candidate-fc528b659bb54a0380237574e4a30954`.
  Export SHA256:
  `27f0489e9401d7ee6b8d81202edccfb08d7db346eecf86cdf8d369df875628d7`.
  All 1,843 file lengths/hashes and submitted public source/profile snapshots
  are independently checked. The host recomputes all eight PCM captures and
  checks the two cleanup-fault receipts and native subcase evidence separately.

Failed inventories and driver logs are separately exported; their full original
archives remain protected in the guest. Nothing is adopted, cleaned or replayed.
The fourteen positive PCM captures across both suites therefore receive actual
host-side sine/gain/silence checks, in addition to the guest's recorded results.
After all original wrapper exits, an independent same-boot process scan finds no
executable or working directory under any of the five scratch roots. Enforcing
remains1. All four foreign planning/UI files match the initial hash snapshot.
These process observations are not a production descendant/cgroup/lease proof.

## Next boundary

Repeat the confined system/user-service roles and connected Sound GUI with
explicit candidate provenance. The current GUI test runtime still has its own
old Lua payload inventory; review that projection rather than silently altering
its receipt or weakening its checker. Preserve original source/process/cgroup/
policy-retirement obligations. Passing these ordinary-user tests is neither a
normal app/session/device admission nor physical HP/Bluetooth acceptance.

The original P0 carrier remains stopped and untouched. The audio lab stays
running for the next explicit tests. Foreign planning/UI files and USB user
documents remain untouched; NexaUI handoff remains closed. Full OS work continues.
