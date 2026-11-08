# Per-user audio integration

The private session/playback/logout fixture now passes, extending `95cb01c`.
Normal session/device admission and desktop audio remain work. See the
[contract](../../../spec/audio-session.md) for responsibilities and reproduction.

## Direction and implemented work

Use the existing systemd user manager for the two long-lived audio processes,
not a third Aetheri supervisor daemon. The opt-in generated user units have
explicit dependencies, `Type=exec`, no automatic restart, cap-zero/NNP execution
and a mode-0700 runtime directory. A session target coordinates start/stop; its
existence is not authorization. There is no normal unit enablement or installation.

The new RAM image adds a finite root/PAM/user test around the preceding native
HDA fixture. Standard udev/logind uaccess replaces the old fixed UID ownership
rule. Device ownership stays root:audio; the ordinary test user has no audio
group. Check actual device opens before login, during an active local seat0
Wayland user session and after logout. Native library controls and independent
full host PCM remain mandatory. The final user-manager exit/runtime removal and
original server/policy pidfds have separate checks; none releases a production
lease or proves general external-holder drain.

`tools/audio_session_runtime.py` selects pinned existing login/D-Bus/PAM inputs
and their ELF dependencies from the reviewed desktop inventory. Explicit dynamic
ACL/seccomp seeds are necessary because DT_NEEDED does not enumerate dlopen.
Copies repeat file hashes and preserve package notices. No package scripts,
downloads, host installs or original carrier services are changed.

## Current evidence and corrections

Evidence is in `local/audio-session-2026-11-08`; preserve every failed attempt.

- Build v1 fails the strict C compiler on two misleading one-line conditions.
  Its 2,190-member export is verified before exact helper retirement.
- Build v2 compiles, exports 381 members and retires. Its cold boot rejects the
  device-permission precondition. The reused fixture replayed change events and
  disabled name resolution; standard group rules require add events and the
  existing named system groups. The session-only variant now replays add events
  with early name resolution. Original manual/device variants keep their profile.
- Build v3 compiles and retires. Its guest reaches real PAM/logind and the user
  manager. ACL application fails with EOPNOTSUPP: the ELF selection lacks the
  dynamically loaded ACL library. Add the already reviewed pinned libacl and
  libseccomp objects and notices; do not bypass access failures.
- Build v4 compiles and retires. Its actual session context reports UID/GID1000,
  cap-zero, seat0, active/local, type wayland and class user. The supplementary
  group count is one: PID1 retains the account's primary group. The old test-only
  empty-list precondition rejects this valid context before access/playback.
  Accept only empty groups or that exact primary GID1000, still rejecting every
  other group. The finite native probe reports the actual count. Add real
  privileged-group rejection and repeat private probe tests in both build modes.
- Sixty host-side HDA/session contract cases pass. They reject inactive/remote
  contexts, foreign unit images, equal actor PIDs, missing process retirement,
  direct user-owned devices and missing/out-of-order access withdrawal.

- Build v5 passes six private native probe graphs in Release/ASan/UBSan, both
  twelve-control parser tests, sixty host contract cases and two actual
  supplementary-audio-group rejections before connection. Its guest now proves
  denied-before/allowed-active device access, without an audio-group membership.
  The first user-manager mutation fails while its bus name is still appearing.
- v6 waits read-only for the manager's actual unique owner before any mutation;
  subsequent calls retain that original destination, without replay/replacement.
  The guest observes owner `:1.1`, then fails later at user-service startup.
- v7 adds read-only failure diagnostics. The session target is active, PipeWire
  is failed with exit-code, WirePlumber stays inactive and the audio target is
  inactive. Start acknowledgement was not readiness. The initial diagnostic
  incorrectly requested unsigned ExecMainStatus; correct it to its signed `i`
  wire type. Do not infer an exit status from the failed property read.

- v8's correctly typed property proves exit status 218, the capabilities setup
  failure. A user manager cannot impose the requested empty bounding set this
  way without a user namespace. Remove that unsupported own-unit setting, keep
  NNP and actual effective/permitted/inheritable-zero checks. Vendor sandboxes
  are not weakened.
- v9 proves both services active/running with successful start status. The
  independent process-context gate still rejects them. v10's new diagnostic
  has a shadowed local caught by Werror; no runtime result is accepted. Add an
  upfront syntax check before subsequent expensive image builds.
- v11 captures the actual server context: correct image, UID/GID1000, primary
  group only, effective/permitted zero and NNP=1, but inheritable mask
  `0000000800000000` (CAP_WAKE_ALARM). The selected pam_systemd source deliberately
  seeds this into ordinary local/user-manager sessions. Do not loosen the gate.

v12 applies the capability boundary at the privileged system-managed user@
startup, like the already cap-zero PAM test leader. The ordinary child manager
does not need to lower a privileged bounding mask. This fixed RAM-fixture parent
profile passes the complete flow; no normal carrier/PAM policy is changed.

## Final evidence

All twelve original build helpers retire after complete export verification,
including the two rejected compiler builds. Builds run as ordinary UID1000 in
new private mounts of the enforcing original carrier, boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. The initial v12 audit joins 559 then-current
code/pin inputs, 940 members across the final native/image evidence bundles,
six private probe graphs and four actual privileged-group counterchecks.
V5 exports 557 members, helper 1257635, token `9de7629a56a64e09ae7f2ee26bfb2529`,
SHA256 `97b4c5cedfa3325e4937047687ad06cc25b4b03980d7aa492cac3ad044a198bb`.
V12 exports 383 members, helper 1265182, token `6ce8a192fc3b49a6a1eff30d33414382`,
SHA256 `16067cc1ada35773ea799dbaf86c6620f3e6c56a205da737bf327d8709e45632`.

Release/ASan/UBSan probe cases retain all 48,000 frames at unity/cubic 50% gain
or actual zero mute, including callback-time observer retirement. Twelve pure
device-parser controls pass in both variants. Final review tightens two host-only
verifier inputs: a missing completion must return false rather than raise, and
a no-card rejection cannot also contain success or bus-failure evidence. The
host suite now has 64 cases. Its negative controls also reject incomplete/reordered
device/session/end evidence, foreign process images and privileged groups.

All six cold boots are repeated as v13 against the unchanged v12 images with
the new verifier. The final audit separately verifies 557 unchanged code/pin
inputs and the two explicit host revisions. Their prior bytes remain bound to
the sealed v12 source archive; current hashes join the 64-test receipt and each
new VM result. Keep `acceptance-v12.json`, `acceptance-v13.json` and both sets of
original results; no v12 receipt is rewritten to claim the newer verifier.

Every final VM has a new original process, fresh NVRAM and host-local AF_UNIX
QMP. All six original processes exit 0 after observed guest poweroff and capture
retirement. The image has no disk, NIC, shared directory or physical audio.

| Cold boot | Captured frames | Signal start | Result |
| --- | ---: | ---: | --- |
| User session v13 | 200,070 | 79,264 | All 96,000 frames at cubic gain 0.125 |
| Second user-session boot | 190,956 | 71,141 | Same complete signal |
| Original native HDA control | 202,047 | 82,944 | Same complete signal |
| Discovered-device control | 214,923 | 94,423 | Complete unity signal |
| Manual HDA control | 135,918 | 17,465 | Complete unity signal |
| No card | — | — | Device gate rejects before actors |

All five captures stay below 0.5 LSB error, within unchanged two-LSB acceptance.
Both session boots prove denied-before/allowed-active/denied-after actual opens,
root:audio/0660 node ownership before/after, active local Wayland context,
ordinary original graph/policy processes, native control readback, independent
complete PCM, session-target stop, original pidfd exits, PAM logout and complete
user runtime removal. No rejected capture is repaired or accepted retroactively.

Read-only preservation still verifies 445 original carrier source inputs,
19 protected hashes, 31 frozen SQL/XML and 52 normal P0 files. Original authority,
quarantined executing history, CLOSED DRM and foreign UI/project work are unchanged.

## Remaining boundaries

Standard uaccess is per-user DAC, not Aetheri app MAC or brokered authorization.
Removing an ACL does not revoke an already opened device FD. Active-VT loss,
multiple simultaneous sessions/users, endpoint admission/manager replacement,
unexpected server loss, complete holder fencing, restart/recovery, normal login
and Settings remain work. `Type=exec` and a started target alone are not graph
readiness; current acceptance requires the separate client observations and PCM.
Next address active-session/server-loss coordination before normal desktop
promotion. The RAM guest has no production SELinux policy. Do not
promote these units merely because an isolated test succeeds. Review/update the
legacy Lua runtime before normal rollout. No USB replacement is performed.

The original carrier/quarantine, normal v2 inputs, frozen SQL/XML, foreign project
and UI files remain protected. NexaUI handoff stays closed. The full OS goal stays
active.
