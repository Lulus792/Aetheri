# Original audio user-service observations

9 November 2026. The previous goal turn made progress: commit `bab60b5` published
actual confined-role PCM evidence and the invalid-endpoint correction. This turn
adds the next native user-service observation boundary. It does not finish the
OS, normal audio admission, Settings or hardware support.

## Implementation

`src/audio/actor.h` exposes open/check/close in the existing session library.
Manager and actor share bounded kernel, executable and supervisor primitives
in `manager.c`; no extra daemon, database or source-level dependency family is
added. An actor retains the original manager through reference counting and
duplicates its own admitted process/cgroup handles. It rejects current main,
invocation, label/image, cgroup/subtree or ordinary capability/NNP drift. The
first failure remains immutable. Close never changes a service or lease.
Supplementary groups and complete protected code/configuration provenance remain
caller policy; these observations cannot exclude changes between checks.

The explicit native probe closes caller inputs and the caller's manager reference
before rechecking the actor, then observes actual original exit and repeated
latched failure. `tools/test_audio_actor.py` is root-only disposable-carrier
administrative tooling, not an installed authority. Its temporary UID1000 worker
holds unit references and directs effects at its original unique manager owner.
Pending effects are written before RPC/signals; unknown replies forbid replay.
Its independent original unit/cgroup/process end checks precede resource removal.

## Native evidence

All builds run as UID1000 in new private RAM/mount namespaces of the preserved
original carrier. No original source, package, loaded policy or unit is replaced.
Exports are checked by whole SHA256 and every member before acknowledging the
original helper's retirement. Successful helpers also compile the copied policy
store offline and compare its original files/kernel policy without loading it.
Local records: `local/audio-actor-2026-11-09`.

- V1 compiles the library, but the included-subject test rejects a name shadow
  between its old global `group` and the new local. Rename the fixture's global
  to `manager_group`; preserve the31-member failed export and original exit1.
- V2 reaches actor tests, then fails the valid actor with ESTALE. The test model
  temporarily makes the actor group equal init.scope; its path dispatcher then
  wrongly returns the actor's directory for the manager's own check and latches
  manager loss. Prioritize the manager's original path in that model and require
  it to remain live after every subtree rejection. Do not weaken production
  subtree or loss checks. Retain the38-member failed export/original exit8.
  Test assertion failures now also kill/wait only their own known child pins.
- V3 passes all four groups. V4 adds the standalone live probe and passes
  `audio.original_manager`, `audio.session_guard`, `audio.original_peer` and
  `audio.session_barrier` in Release. V5 passes the same groups with ASan/UBSan.
  Both final exports have41 independently verified members and original exit0.

Final export SHA256 values:

| Build | SHA256 |
| --- | --- |
| V4 Release | `5d7813356d60e1c713db87fa47386942beebd4dcd87cb11065ea998a00015fbc` |
| V5 Debug/sanitizers | `b5e9a006aa72759f101043d2289e04141803829656f6f1be3867d6920e3afcd3` |

The current C implementation/probe/test and CMake inputs match those manifests.
The two header-only differences clarify retained lifetime and the caller's
supplementary-group obligation; declarations and preprocessor tokens are unchanged.
Tests retain real pins, executable/context reads, process exits, FD accounting
and fork rejection. Bus/credential/unit/invocation/cgroup fault facts remain
controlled; do not relabel them real PID1/user-manager integration.

## Actual user-unit evidence

The separate enforcing audio lab boots as original QEMU71164, guest boot
`42eac9aa-ee11-47e6-9cad-52dbfac7abd9`. Its existing kickstart user@ drop-in
already sets an empty capability boundary and NNP=1; no service/policy workaround
is installed. SSH's real login starts the user manager without lingering.
An early short inspection and a later test can legitimately observe different
manager PIDs after logout. Each test captures its own original before effects.

Actual v1 fails before unit creation because bus availability precedes the
manager's NameHasNoOwner acquisition. Preserve that setup record. Independent
inspection finds no original worker/unit and no process/FD/map reference to the
four exact protected fixture files. A separate administrative receipt removes
only those files/tree; it neither rewrites the failed run nor claims recovery.

Initial discovery now waits at most five seconds for exactly that remote error,
before admission/effects. Other remote/transport errors never retry. Five host
controls pass, including the bounded deadline and ordinary-user guard. The first
Windows host run exposes an unavailable `os.geteuid` mock target; adding the
explicit test-only `create=True` makes the guard test portable without changing
the Linux guard. Original error objects/names remain available after cleanup.

Actual v2 and final v3 each pass two fresh real user units. The Release probe is
run as UID/GID1000 with cap-zero/NNP and the root-selected original process pin.
Actual final markers include:

```text
ACTOR_ADMITTED original=1 user_unit=1 image=1 context=1 caller_handles_closed=1
ACTOR_LOST error=-3 latched=1
ACTOR_RETIRED handles=closed no_service_mutation=1
```

The three separate actual negative probes reject another live same-UID unit
(EACCES), another root-owned executable (ESTALE) and another expected context
(EACCES), while their original manager stays live. These units execute the fixed
sleep program in the existing unconfined user context, not the audio role domain.
No physical device or new MAC permission is used.

The administrative controller separately terminates each exact original pidfd.
SIGKILL deliberately leaves a failed unit available for terminal inspection;
this is a test fault, not proposed production shutdown. Original invocations,
no main/control/job, dead process pins and held cgroup ENODEV plus absent names
precede reset/unref/GC. Both original units and all four native probes exit;
the exact worker exits0 before protected fixture files are removed.

Host verification binds the final exported controller/logs and source hashes:
`f00c7f3f7b62b5d6b52071692485a47dac762e1b3163a1c22bbaad1caa183dfa`.
Independent live inspection finds no fixture executable tree or process. The
separate VM then powers off; original QEMU71164 exits0 under its registered UUID.
The original P0 VM stays running unchanged.

The final preservation audit checks445 original source inputs,19 protected
hashes,31 frozen SQL/XML and52 normal P0 files, authority/CLOSED/quarantine and
all four foreign worktree files. No USB data, public capability or normal service
changes. Next join protected per-user audio role/configuration admission to this
observer, then Settings/device/session coordination. Existing default-clock
timing and physical HP/Bluetooth acceptance remain separate gates.

Primary review: systemd v259 [manager code](https://raw.githubusercontent.com/systemd/systemd/v259/src/core/dbus-manager.c)
and [interface](https://raw.githubusercontent.com/systemd/systemd/v259/man/org.freedesktop.systemd1.xml).
The process-handle lookup is a containing-unit observation; active main/current
context/original owner are separately verified, not inferred from that lookup.
