# Private Lua 5.5 policy runtime

This explicit test profile combines the original WirePlumber 0.5.19 RPM daemon,
core, scripts and other modules with the [rebuilt Lua module](audio-policy-lua.md)
and reviewed Lua 5.5.1 library. Normal audio/image profiles remain unchanged.

[audio-policy-candidate-inputs.json](audio-policy-candidate-inputs.json) pins the
original profile and rebuild-input profile using canonical Git LF hashes, and
selects the exact native module artifact. The preparer checks the complete old
prefix before copying into a fresh output. It replaces only the scripting module,
removes Lua 5.4 and its notice, and adds Lua 5.5 with its original copyright notice.
The original WirePlumber source archive retains all source-file notices alongside
the existing WirePlumber MIT notice. Build objects, test programs, headers and
build tools do not enter this runtime.

The result has 241 regular payload files and five aliases, totalling 5,718,952
payload bytes. Its separate `inputs.json` records candidate identity, input-profile
hashes and original package proofs. The checker independently derives every path,
hash, size and mode from the reviewed profiles; its own receipt cannot authorize
different bytes. It rejects extra files, changed aliases, missing notices and
the old Lua ABI. Preparation never overwrites/adopts an existing output or cleans
up a failed attempt. This is trusted development projection, not package admission,
a publisher signature, durable publication or an app ownership protocol.

As an ordinary native Linux build user, with independently verified inputs:

```sh
python3 tools/prepare_audio_policy_candidate.py prepare \
  ORIGINAL_POLICY_PREFIX REVIEWED_LUA55_SDK REBUILT_LUA_MODULE \
  ORIGINAL_WIREPLUMBER_SOURCE_ARCHIVE FRESH_CANDIDATE_PREFIX
python3 tools/test_audio_policy_candidate.py \
  ORIGINAL_POLICY_PREFIX FRESH_CANDIDATE_PREFIX FRESH_CONTROL_DIRECTORY
python3 tools/test_audio_policy.py \
  REVIEWED_PRODUCER PIPEWIRE_SDK FRESH_CANDIDATE_PREFIX WORK_DIRECTORY \
  --policy-profile lua55-candidate
python3 tools/test_audio_control.py \
  REVIEWED_NATIVE_TEST_DRIVER REVIEWED_PRODUCER PIPEWIRE_SDK \
  FRESH_CANDIDATE_PREFIX CONTROL_SDK WORK_DIRECTORY \
  --policy-profile lua55-candidate
```

The native test driver is `aetheri-audio-control-test`, built from
`tests/test_audio_control.c`; the separate demo probe has a different protocol.
The controls/work directories must exist and permit new, uniquely named outputs.
Keep independent original-process observations and all failed evidence.

The harness defaults remain `rpm`. Only the explicit `lua55-candidate` selection
uses the candidate checker; neither directory names nor a prefix's own metadata
select that mode. The normal checker rejects candidate receipts with a validation
error. The candidate checker also rejects a normal prefix. Successful candidate
graph cases additionally check the original live policy process's mapped Lua 5.5
and rebuilt module paths; actual audio samples still need to match independently.

The [execution record](../brains/aetheri/journal/2026-11-09-audio-policy-candidate.md)
contains actual graph, independent PCM and native-control comparisons against the
unchanged RPM baseline. The subsequent
[confined/GUI record](../brains/aetheri/journal/2026-11-09-audio-policy-confined.md)
also passes system/user-service roles and connected Sound/source-loss retirement.
All three explicit lab harnesses accept the same `--policy-profile` selection.
The normal defaults remain strict. Failed administrative system/user runs keep
their original observations pending for exact reconciliation.

The GUI client needs the C core, while the existing compositor's libinput still
requires Lua5.4. Its shared GUI runtime retains that separately attributed ABI;
the policy candidate uses Lua5.5 in its own prefix/process. Review libinput's
binding before claiming a maintained whole-desktop Lua closure or selecting new
normal manifests. Physical audio, normal session/device ownership and the PCRE2
release review remain separate work.
