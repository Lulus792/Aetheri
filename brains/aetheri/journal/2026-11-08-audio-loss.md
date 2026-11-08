# Audio session loss

Work extends `c03484b`. Evidence stays in `local/audio-loss-2026-11-08`.
The full OS goal stays active; no normal desktop/USB audio is promoted here.

## Implementation

`audio/session_guard` is a read-only calling-thread observer. It subscribes to
logind session notifications before capturing the calling local Wayland session,
then duplicates the independently admitted original server/policy pidfds. A
notification prompts a fresh original-session check; a pidfd indicates original
process exit. Loss and observation errors latch. Reactivation never restores
LIVE, and close does not signal a process or revoke an exported descriptor.

No extra permanent daemon, thread, callback or recovery database is introduced.
The caller still owes current image/unit/endpoint admission and authenticated
manager ownership. This observer cannot settle a lease or prove general holder
drain. Logind/manager restart identity and complete app MAC remain open.

The private PAM client now watches while waiting for playback and fences new
work on loss. It submits one stop to its original manager destination, observes
both original pidfds, waits for the audio target/services and private runtime
directory to retire, and verifies no automatic restart. Stop acknowledgement,
process exit and runtime removal are separate observations. Unknown calls are
not replayed. There is no silent adoption of a replacement manager.

Three fixed additional RAM profiles inject loss after the complete reference
signal: root switches VT8 to VT1 and back, or the ordinary user signals one
original server/policy pidfd. VT loss additionally requires actual new device
opens denied while inactive. Two user-written fixed markers coordinate only
that disposable root test; they select no PID, unit, path or command and are
never authorization. The initial marker wait has the same 90-second budget as
the PAM fixture. These profiles do not establish mid-stream interruption or
normal background-music/lock product policy.

## Evidence so far

- Build v1 helper 1267482, token `fdedf6dca6424cda861fae3e715ad04c`, exports
  449 verified members and retires with exit 0. Bundle SHA256
  `979ff66dfd05ba0d2c37322946d97f37b0cde39e7469b635c55d7d9453d54847`.
- The new native CTest passes in Release and ASan/UBSan. Context notifications
  are controlled; real processes/pidfds/poll, exit, duplicated-FD ownership,
  close and immutable loss are exercised. Real logind integration remains a
  separate VM obligation. Seventy-three host contract cases pass.
- Two default-clock playback boots panic before init at the IO-APIC/PIT timer
  check. The newly built original native HDA comparison has the same early
  failure. Kernel SHA256 and all host-tool hashes match the preceding accepted
  session run. One explicit single-TCG-thread countercheck also fails there.
  Preserve all four original attempts; none is audio or session acceptance.
- An explicit instruction-clock diagnostic with the unchanged v1 image passes
  actual PAM/login, native controls, the guard during playback, all 96,000 PCM
  frames, original process stop and logout. It captures 474,784 frames, signal
  start 330,528, maximum error 0.49795693 LSB, original QEMU exit 0. This is not
  cycle-accurate or real-time/performance evidence. Its initial experimental
  auto/sleep-off command is preserved, not the final reproduction profile.
- Final review adds monotonic-addition headroom, exact unreaped-child cleanup
  in the test, and the longer initial VT barrier wait. Build v2 runs these
  changes. The final explicit clock profile uses QEMU's documented auto,
  align-off, sleep-on mode plus `-rtc clock=vm`; the default remains unchanged,
  with no automatic fallback. Guest reset now fails the test immediately rather
  than waiting for the poweroff deadline.
- Build v2 exports 449 members and retires with exit 0; native guard tests pass
  in both modes. Its VT test detects and latches actual loss, then fails the
  coarse root login-test gate. A separate server-loss comparison passes actual
  serial lifecycle/end/logout checks, but PCM rejects with maximum error 1024
  LSB. Its very first nonzero samples already have the wrong phase. Keep that
  capture rejected; drain text does not prove delivery.
- Build v3 adds read-only step diagnostics, exports 449 members and retires with
  exit 0. Its VT attempt fails at loss-ready after DRAINED, before injection.
  That early failure is inconsistent with exhausting the 90-second marker wait.
- Source review finds a concrete create-before-write marker race. A reader can
  successfully read an empty newly created file and reject it before its writer
  writes two bytes. The new native regression reproduces that old observation
  deliberately. A marker now writes/closes a private pending file and atomically
  renames without replacement; its final name is absent throughout the write.
  These remain ephemeral test barriers, not a durable operation journal. The
  exact marker bytes in the old failed guests were not captured.
- Build v4 catches a new test-entry macro naming conflict, exports 27 verified
  diagnostic members and retires with exit 1. Correct the test-only entry alias;
  do not weaken warnings. Build v5 repeats both native groups and images.
- To separate PCM from the adaptive clock's rate changes, the next explicit
  test clock uses fixed shift 3, align off, sleep off and a VM RTC. QEMU's selected
  implementation uses shift 3 as its initial adaptive rate; fixed mode has no
  adaptive adjustment. This is a test profile, not performance or production
  timing policy. Final loss-profile execution is pending.
- Build v5 exports 453 members and retires with exit 0. Both new native groups
  pass in Release and ASan/UBSan. Fixed-shift-3 playback passes full PCM; its VT
  attempt times out in the inherited five-second module-load test budget before
  reaching login. A fixed-shift-0 countercheck provides one virtual nanosecond
  per instruction without adaptive rate changes; this is not physical CPU speed.
  It reaches actual VT loss and both original process exits, then rejects at
  loss-retirement. Do not relax that gate.
- Build v6 exports 453 members and retires with exit 0. Its read-only VT failure
  diagnostics prove `lstat=-1 errno=2` for the retired runtime and `missing=1` for
  all four original unit objects. systemd collected their inactive state before
  the client inspected it. A missing name is still not end/holder proof.
- Build v7 retains all four objects through the original dedicated user-bus
  connection before actor acceptance. The references are GC holds, not process,
  invocation or lease evidence; closing that connection releases only its own
  holds. Ref requests run once, unknown acknowledgements are never replayed.
  Its 453-member export retires with exit 0, SHA256
  `87286867a90ad06ef8b8c81a6c0f44e99d3a092d9f7afab6dccf3a1961566bc4`.
  Seventy-six host cases now require matching original-manager references before
  actor acceptance and reject missing, replaced or late holds.
- Current v7 execution uses explicit `shift=0,align=off,sleep=off` plus a VM RTC,
  retaining the ordinary default mode and no automatic fallback. The complete
  VT-loss/reacquisition case now passes all 96,000 PCM frames, actual inactive
  device denial, original process/service/runtime retirement and no restart.
  The server-loss case also passes full PCM and logout. Remaining final cases
  are still running; do not infer their acceptance from those first two results.

## Final acceptance — 9 November 2026

All six final v7 original QEMU processes exit 0 after observed guest poweroff.
The distinct runs use fresh NVRAM, two virtual CPUs, host-local AF_UNIX QMP,
explicit fixed shift 0/sleep-off and a VM RTC. No disk, NIC, shared directory,
physical audio or USB is used. The selected image runtime remains 86 files,
ten aliases and 10,302,437 bytes. Only the fixed selector differs between the
four independently inspected session images.

| Cold boot | Captured frames | Signal start | Result |
| --- | ---: | ---: | --- |
| VT loss and return | 134,464 | 14,688 | Full cubic 50% reference; no restart |
| Original graph exit | 134,944 | 16,736 | Full reference; both originals end |
| Original policy exit | 132,544 | 14,688 | Full reference; both originals end |
| Ordinary session playback | 134,464 | 15,712 | Full reference and logout |
| Legacy discovered-device control | 134,944 | 17,760 | Full unity reference |
| No sound card | — | — | Device gate rejects before actors |

Every non-muted capture contains all 96,000 ordered frames, with maximum error
below 0.5 LSB and unchanged two-LSB acceptance. The actual VT case additionally
proves inactive device-open denial, stopped original unit states, removed private
runtime, reactivation without resetting the guard, no automatic restart and
subsequent PAM/logout/runtime withdrawal. It retains the original manager's
four unit objects before accepting actors. This remains controlled fixture
observation, not complete normal authority or holder admission.

Both native groups pass in Release and ASan/UBSan. The executed host verifier
passes 76 contract cases. Final review reproduces another concrete false
acceptance: a setup timeout followed by device failure could satisfy the
no-card negative. The two host-only review changes reject that timeout in
negative or positive traces; 78 cases pass. All six complete original
serial/QMP/PCM records are revalidated with the strengthened predicate. The
original execution receipts, their 76-case verifier hashes and before-review
source copies remain unchanged; no capture is repaired or relabelled.

`acceptance-final.json` independently joins 561 unchanged code/pin inputs and
both explicit host review revisions to the sealed v7 source archive, 2,733
members across all seven exported builds, all seven exact helper retirements,
the two native groups in both modes, two actual privileged-group denials and
the six original cold boots. V7 uses helper 1275199, token
`b7891470f2a640c9b9863cb3d3170589`. Read-only preservation still verifies 445
original sources, 19 protected hashes, 31 frozen SQL/XML and 52 normal P0 files.
The original carrier remains running/QMP-verified; normal authority, quarantine,
CLOSED DRM, foreign UI/project work and USB remain unchanged.

Default-clock panics and the rejected adaptive/fixed-shift-3 captures/attempts
remain failed. The exact early timer cause is still open. Functional instruction
clock acceptance is not physical performance, real-time scheduling, normal
desktop admission, full FD-holder drain or suspend acceptance. Next join original
manager/endpoint lifetime and admission before desktop/Settings/HP promotion.

## References and limits

systemd 259's [monitor contract](https://raw.githubusercontent.com/systemd/systemd/v259/man/sd_login_monitor_new.xml)
requires flushing and rereading state after a notification; it is not authority.
Actual selected runtime objects remain pinned Fedora systemd 259.10.
QEMU 11.1.0 [invocation options](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/qemu-options.hx)
describe instruction counting and recommend a VM-driven RTC for that mode.
It provides no cycle-accurate performance claim. The exact cause of the newly
observed default-clock timer failure is not proven by the counterchecks.

Normal desktop/login integration, manager/endpoint replacement, delegated FD
holders, capture permissions, production MAC, hotplug, suspend and HP/Bluetooth
output still need their own admission and acceptance. Standard ACL removal
does not revoke an already opened device FD. Existing authority/quarantine,
normal P0 inputs, foreign UI work, frozen contracts and the USB stay protected.
