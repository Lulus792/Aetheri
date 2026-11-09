# Audio manager privilege boundary

The previous enforcing-carrier probe returned EACCES when observing the existing
user manager's executable. That receipt establishes an access denial, not its
SELinux cause. Before adding policy, inspect the actual original process and
reproduce the privilege difference independently.

## Findings and implementation

Read-only PID 1/pidfd-bound facts retain original MainPID 954920 and invocation
`b008327cb0a547d7b52c2e8e7438cdd1`. The carrier manager has
CAP_WAKE_ALARM effective/permitted/inheritable, ambient zero and NoNewPrivs=0.
The new C observer now requires the selected Aetheri ordinary-manager profile:
all four active/inheritable capability sets zero and NoNewPrivs=1. Its bounded
status reader runs inside the repeated process observations, before/after image
checking. No permission or manager mutation is added. The already selected
private PAM user@ configuration supplies the empty bounding set/NNP.

An explicit root-only native probe uses its own controlled child in the enforcing
registered carrier. A cap-zero reader first observes CAP_WAKE_ALARM and EACCES
for the current executable; after that same child clears the one capability,
profile checking and the executable open succeed. The original live pidfs
object, UID/GID, NNP=1, dumpable=1, image and SELinux label stay unchanged. Each
reader and the original child actually exit and are waited for. No ptrace call,
policy change, unit creation, device or original manager is involved. The probe
is an explicit executable, never a default host CTest.

The unchanged original carrier manager is now rejected earlier with EPERM at
`ordinary-manager`. This is expected profile rejection. It is not an invitation
to grant ptrace or broadly permit proc reads. Commoncap's documented capability
comparison already supplies an independent cause for the old denied read;
normal confined roles may impose additional MAC restrictions.

## Evidence

`local/audio-manager-security-2026-11-09` retains sealed source/export archives,
root-probe source/binary hash/log, ordinary native tests and original VM receipts.
The first isolated build passes four native groups in Release and ASan/UBSan,
the explicit same-process capability probe and all image builds. The first
actual ordinary-profile PAM boot and all five subsequent loss/regression boots
pass. Four native groups pass both build modes; 85 host contract cases pass.

The initial audit-reader command waited on forwarded SSH stdin rather than an
explicit audit file. Its exact command/parent/pidfd were inspected and only that
owned reader was terminated. Its empty output is not absence of AVC evidence.
The causal proof comes from the controlled kernel observations above.

## Final acceptance

The independent final audit verifies both sealed source/export archives and
their exact helper retirements, 568 unchanged current code/pin inputs, the
Release probe binary/hash/log, all four native groups in both modes, the
read-only original-manager rejection and the PID 1/pidfd-bound manager facts.
It revalidates every original final image/serial/QMP/PCM receipt. All four session
roots differ only in the fixed selector. Runtime remains 86 files, ten aliases
and 10,302,437 bytes; no package, normal service or public API is added.

| Final cold boot | Captured frames | Reference start | Result |
| --- | ---: | ---: | --- |
| Playback | 133,504 | 15,712 | Full reference and normal original stop/logout |
| VT loss/return | 137,824 | 16,736 | Full reference; no automatic restart |
| Server exit | 133,504 | 15,712 | Full reference and original end |
| Policy exit | 133,984 | 15,712 | Full reference and original end |
| Native regression | 133,024 | 15,872 | Existing direct profile retained |
| No card | — | — | Rejected before manager/actors |

All five captures include every one of the 96,000 reference frames, with maximum
error 0.49795693 LSB against the unchanged two-LSB limit. No samples/headers are
repaired. Explicit instruction counting is functional evidence, not physical
timing or acceptance of the retained default-clock APIC issue. Preservation again
checks 445 original sources, 19 protected hashes, 31 frozen SQL/XML inputs,
52 normal files and the four unchanged foreign-work hashes.

## Sources and scope

Linux [ptrace access checks](https://man7.org/linux/man-pages/man2/ptrace.2.html)
describe the proc filesystem credential mode and commoncap comparison against
the target's permitted capabilities. SELinux enforcing mode alone cannot explain
an errno. The attempted exact Linux 6.19 commoncap source retrieval was rate
limited; no unseen source body is cited as inspected.

Normal confined graph/policy/app roles, protected configuration/library/current
image admission, delegation/holders, recovery and physical audio remain work.
No normal service, original carrier input, authority, frozen contract, USB or
foreign UI/project work is changed. The full OS goal remains active.
