# Native Sound event host and isolated local login

9 November 2026. The embedded Settings path now dispatches the existing audio
owner's GLib context alongside native window input and original process/login
notifications. The separate local PAM/logind prerequisite passes on the enforcing
audio lab. The compiled GUI client has **not** yet been joined to a real graph.
See the [contract](../../../spec/audio-session.md#embedded-native-host-event-loop).

## Implemented boundary

The original manager/session observers expose borrowed polling hints and an
absolute monotonic deadline. The shared owner removes permanently readable hints
after latched loss. It still fences commands and retires its backend without
stopping services or treating descriptor closure as holder drain.

`settings/audio_host` retains the pushed calling-thread context, combines at most
256 GLib polling records with the original hints and native signal FD in a fresh
epoll object, then performs actual nonblocking GLib poll/check/dispatch. Collect
owner hints before prepare/query: fencing may attach backend-close sources.
Every wait finishes or aborts before another update. Wrong thread/process,
overlapping waits and callback-time destruction are rejected. No watcher thread,
periodic scan or permanent helper is introduced.

The normal executable still supplies no owner. A separate BUILD_TESTING client
embeds the same window entry and requires root-selected original descriptors,
protected profile and three image FDs. Its fixed lab checks/codec are not normal
admission or a public launch transport. It is compiled only. The exported ELF
closure candidate lacks the completed notices/provenance projection and must not
be promoted as an accepted runtime.

## Executed native checks

All builds run as UID1000 in isolated private tmpfs/mount namespaces of the
preserved carrier, boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. Original source,
services, policy and quarantined catalogues are not replaced. Exports and all
member hashes are checked before acknowledging each original helper's retirement.
Local evidence is under `local/settings-audio-host-2026-11-09`.

| Build | Original helper | Result | Export SHA-256 |
| --- | --- | --- | --- |
| v1 | 1344361 | Native groups pass; exit 0 | `0e42e4123d239baac409be2d1f4ed7ab15eebe8b2cf499e323e3ee07b8ce2906` |
| v2 | 1346092 | Native tests/client compile pass; closure export fails on missing reviewed prefix/lib search; exit 1 | `ead55e2d06491a37685a8c968ac5ad49585c413b24f4a94467b317509f76b4ae` |
| v3 | 1347833 | Reviewed path added; tests/closure export pass; exit 0 | `e6d48cc99d729fe259b11cdc5588a255ce44fbaa1cf98e1be2d2bc7a5205bcfe` |
| v4 | 1352298 | Final hint-before-query ordering and tests pass; exit 0 | `e3b196bdc66119421ea7b007561f44c24f75890d4bc86f362d266878a8e13716` |

Final v4 passes nine groups in Release (5.13 seconds) and ASan/UBSan (8.23):
`settings.view`, `settings.view_hidpi`, `settings.model`, `settings.sound`,
`settings.audio_binding`, `settings.audio_events`, `audio.original_manager`,
`audio.session_control`, `audio.session_guard`. The event test uses real GLib
I/O/timers, cross-thread wakeups, fork/thread rejection, callback-time close,
abort and descriptor retirement with controlled owner adapters. It injects a
source during hint collection to require its inclusion in the next query.

V2 also passes the ordinary no-owner native window/navigation test in Release
(14.00 seconds) and ASan/UBSan (19.50). Its normal window/view inputs remain
unchanged in v4; the later host-ordering correction affects only the audio-linked
path. These tests do not establish live GUI/audio or physical acceptance.

## Actual local session prerequisite

`tools/test_audio_login.py` is an explicit root-only disposable-lab test, never a
default CTest or production login manager. It creates one exact temporary
PAM profile with pam_permit and pam_systemd, registers one finite UID1000 Wayland
session on seat0/tty8, and retains the original process pin, service/scope
references and both cgroup-event readers. This tests registration, not user
authentication. It rejects optimization before effects and refuses pending PAM
fixtures. Failures preserve original handles and dispatch history for inspection.

Three prerequisite failures are retained independently:

- Boot `ac44a310-459b-48a4-9507-efc1f07ce59e`, token
  `9ba7a5c9ab364ead86b08fff2fc56bf7`: PID1 denied receiving the root output-log
  FD under enforcing policy; dispatch reply was uncertain. Use journal output,
  without weakening that permission or inferring no dispatch from a missing unit.
- Boot `b1d7b935-b518-4a7a-97e7-6598e5f26b5c`, token
  `2c73db7f6c4a4d259898d3f8f2916df3`: private-bus RefUnit returns EINVAL after
  acknowledged dispatch. Use the ordinary system bus for references on this
  carrier. No original process observation was captured; this is not a pass.
- Boot `ef982238-82b2-4af1-9664-d3f833ad196a`, token
  `45148db87de74e949446133b78fac221`: service-based journal collection misses
  stdout attributed to the PAM-created session scope. Use a unique journal
  identifier. The process self-ended, but the missing original pin prevents a pass.

Each failed attempt's original QEMU exits 0 before a later independent boot
removes the exact old profile and writes a separate retirement record. Original
dispatch states are not rewritten. This is administrative test retirement,
not production recovery or proof of an unobserved dispatch outcome.

On boot `016c0b1d-d08d-45f9-a285-da2f7abab611`, the corrected local probe and
two public-tool runs pass. The final exact public source SHA-256 is
`758768e204b5e8dee1b291e2f8e76fc9fc83e119b292514f445a494fc0fba1cb`;
its bus helper is `92b1fb05360e821a71875984bc468587edbce60e44f2656bde26999d0e386380`.
Token `f71e186174c7416b99d963d6366b3fca` captures original PID1261,
pidfd inode1262, service invocation `b0cce5e6301148a38d9eef08f5583ec9` and
session-c3.scope invocation `2ef778356e7547dd942504c24447a9d8`.
Four UID/GID fields are 1000, effective/permitted/inheritable/ambient capabilities
are zero, and NNP is 1. The test process is unconfined_service_t, not a production
confined Settings process.

The original service/scope cgroups [29,5884]/[29,5929] end with missing names
**and held event-reader ENODEV**. The original process is dead; the service retains
its invocation, is inactive with no job/MainPID and reports successful exit.
The local session disappears. Only then are references retired, the exact PAM
profile removed and tty1 restored. The final export checks ten evidence files,
no remaining test PAM profiles/units/audio policy modules and enforcing mode.
All four original lab QEMUs (60400,59936,21268,60016) exit 0 through their held
controllers (34069,61334,54525,30755). The lab is off.

The final read-only audit matches 346 native C/header/CMake inputs to v4 and
the normal window inputs to the tested v2. It rechecks every exported member
and the exact executed public login sources. The preserved carrier retains
445 source inputs, 19 protected hashes, 31 frozen SQL/XML files, 52 normal
inputs, its original authority and loaded enforcing policy. Four foreign UI
notes remain byte-identical and outside this change. Only original QEMU9520
is running. No USB write or normal service/source promotion occurred.

## Next boundary

Complete the reviewed runtime projection including notices and protected input
provenance. Then join the actual GUI host, original process/image descriptors,
local PAM session and real graph in the isolated lab. Normal session/broker/device
policy, holder/restart coordination, release integration and HP/Bluetooth tests
remain separate. Preserve the original guest and user USB documents. NexaUI
handoff remains delivered; there is no new chat message or toolkit requirement.
