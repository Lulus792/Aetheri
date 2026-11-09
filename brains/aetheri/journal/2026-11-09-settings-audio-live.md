# Connected Sound GUI and original-resource retirement

9 November 2026. `tools/test_settings_audio_live.py` now joins the existing
confined user graph/policy fixture, native embedded GUI owner, local PAM session
and headless Wayland input/capture. The final v10 fixed-lab workflow passes.
Earlier failures below remain historical. No normal app/session provisioning, hardware or lifecycle acceptance
follows. Preserve the original P0 guest, USB documents and foreign UI notes.

The private native-login prerequisite passes on boot
`a9e7518f-4ff8-4bb5-892d-0ec21b4e6541`: native `sd_pid_get_session(0)` returns
the same c1 session as logind; native cgroup lookup returns its original scope.
UID/GID1000, cap-zero/NNP, original service/scope end and profile retirement pass.
The exact extended public login probe also passes on boot
`72b8b870-f79f-4f52-84b3-8c21157e629a`, token
`ab51be4276284736b6903c7894a21e63`. Normalized source SHA256 is
`2cbe18470a2efe502f8f10898d4dc6173ef4e586553ad4ea3f746ddf64482ebc`.
Its actual native session/cgroup agree with original logind/PID1 observations;
both original groups end with missing names and event-reader ENODEV. PAM removal,
tty1 restoration and the original host controller's exit0 are observed. This
temporary pam_permit test is not production authentication.

Evidence is retained under `local/settings-audio-live-2026-11-09`:

- v1 staging token `5b8d65d5d6354e47a9b759440e74400f` incorrectly selects two
  helper binaries from Release rather than native in the old export. No role or
  policy starts. A new preflight detects the actual missing helper before any
  fixture creation. Independent inspection verifies the complete owned prefixes,
  partial gateway-only code, no actors/policy and the original paused keeper.
  That exact keeper is terminated through a new administrative pidfd, then its
  scratch mount is retired. Historical failure remains unchanged.
- v2 token `bdf0ebcceeac4105be5ee66d575d62fa` starts both confined sources and
  the headless compositor. The test reads ordinary credentials before setpriv
  finishes exec. A controlled root-to-UID1000 barrier reproduces that distinction;
  checks now follow READY and the actual selected ELF. Existing guards remain.
  The separate ten-second PCM actor-observer processes are unsuitable for an
  interactive test. This fixture uses the real GUI owner's native actor checks
  instead, retaining independent root source/end observations. Existing PCM tests
  and their observer deadlines are unchanged.
- Original QEMU62376/controller83974 exits 0 before separate boot retirement of
  v2's exact unit files/modules. The failure/controller bytes remain historical.
- v3 token `64f94ebe321f441381e8631fd2a430d3` reaches protected GUI input
  preparation. The system-bus broker is denied file ioctl on the original
  systemd image handle and disconnects the administrative call. GUI start stays
  pending; a missing unit never proves no dispatch. No retry occurs.
- Original QEMU68016/controller89116 exits 0. A later exact administrative
  retirement removes v3's original PAM/target files and policy, preserving the
  pending dispatch record. A cleanup-script syntax error executes no guest code;
  the corrected script is parsed locally before use. No production recovery is
  claimed. The next fixed test uses the already available Root-only PID1 private
  connection for StartTransientUnit FD delivery and the ordinary bus for RefUnit.
  No broker permission or installed method table changes.

- v4 token `3d6a898b35d34799a48c03de4b3cb42d` still fails FD delivery on the
  private PID1 bus: SELinux denies receiving an O_PATH image descriptor. Linux's
  file_receive mapping selects FILE__IOCTL when the descriptor has neither read
  nor write mode; the denial does not establish an ioctl syscall. A separately
  tracked readonly-FD control passes with the exact same three image objects,
  actual child fstat checks, original process/unit/cgroup end and unit GC.
  Carry O_RDONLY image views instead; native object binding remains unchanged.
  Neither broker permission nor global policy is broadened.
- QEMU70680/controller43276 exits0 before v4's separate exact administrative
  retirement. Its uncertain GUI dispatch remains historical, not replayed.
- v5 token `7da1e996bd99461c9573021c8b7537de` positively joins the real GUI
  owner to the graph/policy in an actual local session. The native Sound page
  displays the actual synthetic output at 100%. Its original 1920x1080 capture
  is viewed after lossless PPM-to-PNG encoding. No physical playback or final
  design acceptance follows. The driver then fails because its output() method
  shadows the inherited evidence Path; rename it observed_output(). Control
  state is a flag set: only bit0 is disabled, not selected/focused flags.
- QEMU4812/controller26723 exits0 before exact v5 administrative retirement.
- v6 token `6986486e36744024a18eea4a60596aa5` reaches real pointer volume
  readback at 50%, but cannot focus the still-pending slider. Do not accept
  keyboard/mute/loss/retirement from that partial result. Its original GUI,
  source pins and failed controller remain retained until QEMU71256/controller1359
  exits0. Host test controller72562 then ends after the VM connection closes.
  The independent 12-file export has checked hashes; historical failure remains.

## Input-free slider resubmission regression

The live failure motivates an independent native counterexample. nx_slider
snaps incoming values to its one-percent grid even without an event. Comparing
that with an unrounded backend observation generates an unsolicited volume
intent. The view now compares grid values while preserving the raw observation.
Near-50% floating-point values, off-grid values and repeated frames produce no
intent; existing pointer, keyboard, cancellation, mute and held-row cases pass.
This proves a real defect, not yet the complete cause or resolution of v6.

The isolated UID1000 build uses the preserved enforcing carrier, unchanged
reviewed SDKs and a fresh private mount namespace. All nine native groups pass
in Release and ASan/UBSan; all four unprivileged driver controls pass. Rebuilding
the previous view against the new test fails precisely at !m.sound.queued;
restoring the corrected source passes. No permission or installed source changes.
Original helper1363539/token `2ccdf59283c24bc6a44344284f1d7ff7` exports210 checked
members and exits0 after independent acknowledgement and scratch retirement.
Export SHA256: `d73033559bb8551a00ada7b5e5a683b7f923df2e082bbbf6cc01395099ff5146`.
The next lab profile selects its two original programs; third-party inputs and
notices remain unchanged. A fresh joined test is still required.

## Corrected view in the actual graph

After observed QEMU71256 exit0, exact v6 administrative retirement succeeds on
boot `ca541ddb-cfd9-444e-ad18-e5c4199db5eb`. Two earlier cleanup transports
fail before guest program execution (unpublished host identity, then SSH banner
timeout); only the independently checked later dispatch performs retirement.
Historical controller bytes remain unchanged.

- v7 token `29396da6dcb1497986932ade802e3e7a` fails during input preparation:
  the driver applies an output-mode check to the original read-only input
  libraries. No fixture/controller, PAM, policy or actor starts. Preserve the
  partial fresh tree. Validate original input ownership/object/bytes separately
  and assign the manifest mode to the fresh copy; do not chmod the old snapshot.
- v8 token `1526621b674846e6b98e2a05f12b45f8` uses that fresh projection and the
  corrected native view. Actual pointer and keyboard readback pass; final
  volume is 0.489999995 with feedback=OBSERVED, revision4. This is the same
  non-grid precision the view must retain without resubmitting. Independent
  pw-dump exits0 but prints ENOENT for optional ProcessLatency enumeration. The
  initial overly strict empty-stderr assertion stops before property comparison;
  no mute/loss/retirement acceptance is inferred. Export12 files with checked
  hashes before original QEMU71464/controller61763 exits0. Controller74431 ends
  after the connection closes; exact infrastructure retirement remains separate.

The diagnostic reader now preserves stderr and accepts only that exact pinned
ProcessLatency ENOENT spelling. Other errors, parameters and result codes reject;
exit0, one exact sink and actual Props/value comparisons are still mandatory.
The updated five driver controls and nine projection tests pass as UID1000 in
the enforcing original carrier's separate private namespace. Helper1365585/token
`6f68ee23b48c48fe8645e2a6c07bb909` exports4 checked members and exits0 after
retirement; export SHA256
`631febd7bb40a8f2325c27baeb5ea702d085c8b44232fc1cc3cf7c7c6aad9423`.
No claim about latency support follows.

## Independent graph readback and retained loss history

V8 infrastructure is exactly administratively retired only after original
QEMU71464 exit0, on boot `31ac2ef8-3dc3-459f-a730-bb3f1e905363`.
V9 token `93920a726bef41feb2e4f45ae56f1f51` independently confirms UI volume
0.489999995 against both raw channels0.117649, then mute and unmute against
actual Props. The original two sources retire. The native model latches fenced1,
feedback UNKNOWN and removes all dynamic Sound controls. Its old copied audio
observation intentionally remains for history. The test incorrectly expects
state DISCONNECTED/count0 in that retained copy, and times out. This is not a
failure to fence or permission to submit again. Full successful close is still
unaccepted. Preserve24 exported files and the failed controller.

The corrected driver checks the latest frame's latched fence and absent dynamic
controls, rather than fabricating cleared history. Old CONTROL lines cannot
enable a removed current control. Six driver controls and nine projection tests
pass in an isolated UID1000 enforcing-carrier namespace. Original helper1367189,
token `bf65552ac0a3497b997afa03ced8a20a`, exports4 checked members and exits0;
export SHA256 `d7dfb4c38bdc1941eb70b92f15f33fc2519a89c072e718dae881acc84e8603d8`.
The actual native muted1920x1080 capture is viewed after hash-checked lossless
encoding: 49%, inactive volume track, active mute switch and readable content.
No physical sound or final product design approval follows.

QEMU46600/controller10082 exits0; controller38287 then ends on transport loss.
V9's first administrative cleanup preflight finds its session target already
retired by the original successful source-stop sequence. It executes no cleanup
effects. The separate inspected continuation checks the historical retired
target, absent file and both retired source records, then removes only the
remaining exact PAM/modules after original VM exit. The retirement receipt on
boot `4e8aa142-c296-46c5-999d-91e976be9a0e` completes without changing historical
controller bytes. Fresh v10 staging token `6ad2e35e03a74af4ad4ca793fe494635`
then executes on QEMU68616/controller94160; host test controller23783.

## Final positive boundary

V10 fixture `95a68c8b36294be289fadc4f3e531146` passes the complete fixed workflow
on boot `4e8aa142-c296-46c5-999d-91e976be9a0e`. Actual pointer/keyboard input
observes volume0.489999995; independent channel values are both0.117649.
Actual mute/unmute readback passes. Ending the original graph/policy permanently
fences this GUI and removes current controls while retaining diagnostic history.
CtrlQ closes its native owner; the original service exits0, inactive/no-job/no-PID
with unchanged invocation `74777b10f24f4200ae9bc2bffd21d64d`.
Both original service/scope groups have missing names AND original reader ENODEV.
The original session disappears, references/extra inputs retire and PID1 retains
none of the checked input objects. Original compositor exits0. Source units and
their target retire; scratch root unmounts; exact PAM/module removal restores the
baseline module list and enforcing state. Host controller23783 ends0.

Independent post-retirement inspection verifies all staged source hashes,
no remaining test units/PAM/root, and27 exported files. Each of12 captures has
both compressed and original-byte SHA256 checked. The actual loss-state image
is viewed: clear connection-loss text and no output/volume/mute controls.
The export SHA256 is
`d2c4226c691a864cb7508c13701b9d71ebefb346ec40916bfd3cfbb8582350ac`;
controller SHA256 is
`c740306701c76ef367d5915dc5920252a7d4ef628dec5bcb2c064d9fcf6d0ecc`.

This is trusted unconfined GUI/headless/local PAM test infrastructure joined to
the existing confined user sources and native original observers. It is not
normal login/app/broker/device policy, microphone/mixer, complete delegated
holder/restart handling, physical HP/Bluetooth, release review or an updated USB.
Next establish that protected normal provisioning and its policy/device evidence.

The final native check matches346 current C/header/CMake inputs to the accepted
build. Read-only preservation checks retain445 original guest inputs,19 protected
hashes,31 frozen SQL/XML,52 normal files, the previous SDK, original authority,
loaded enforcing policy and190 CLOSED history receipts. Four foreign planning/UI
files are byte-identical at that audit. A later post-commit check observes a
concurrent change to the untracked UI design note; this task neither edits nor
stages it. The three tracked foreign notes still match. The original carrier and user USB documents remain
unchanged. NexaUI handoff remains delivered; no new chat message is pending.

After the checked export, normal audio-lab shutdown also ends original
QEMU68616/controller94160 with exit0. No failed controller or original P0 guest
is replayed/stopped to obtain this result.

The source-level explanation for O_PATH receive permission is corroborated in
[Linux6.19 SELinux hooks](https://github.com/torvalds/linux/blob/v6.19/security/selinux/hooks.c),
file_to_av/selinux_file_receive. Actual denied/readonly-positive carrier controls
remain the runtime evidence; this is not a claim to have audited every Fedora
kernel patch.
