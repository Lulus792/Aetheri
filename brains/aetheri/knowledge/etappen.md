# Possible milestones

Architecture foundations are approved; the implementation sequence and schedule
remain a proposal.

| Milestone | Deliverable | Acceptance |
| --- | --- | --- |
| 0: Foundations | Agreed C17/Rust/tool/glibc/library policy and MIT, recorded workflows and hardware | Foundations and B/wlroots/systemd/SQLite selected; exact releases and integration still open |
| 1: Architecture and UI direction | At least two suitable structures, dependency/service/ownership maps, shell-language and desktop/app concepts | Consequences explained; user chooses design direction |
| 2: Build and boot | Versioned kernel/userspace build and VM definition | Own image boots to a defined serial result within budget |
| 3: Base userspace | Init/supervision, processes, files, diagnostics, original shell and original language | Defined syntax/jobs/errors, persistence and controlled failures work |
| 4: Graphic proof | Input/display path, toolkit candidate, simple real app | Text, focus, clipboard and scaling in target userspace |
| 5: Personal workflows | Terminal/editor, Files/Settings, Physim, browser integration | Agreed tasks work, including multiple monitors and failures |
| 6: Daily-use readiness | Isolation, encryption, backups, recoverable updates and installer | Personal-data and recovery acceptance gates pass |
| 7: Hardware acceptance | Laptop profile, then separate RTX 2080 Ti main-PC profile | Agreed GPU, WLAN, Bluetooth, audio, three-monitor input/display and power cases pass |

Security is designed from the start; its final acceptance does not wait until
after personal data is put on the system. Temporary components need explicit roles
and removal/review conditions. User design reviews do not certify native behavior.

Every adopted dependency needs a purpose and an installation/runtime footprint.
Do not create all future source directories or detailed app screens before they
have a concrete purpose. Planning remains revisable after practical evidence.

Packaging and complete removal are architecture-level requirements from the start.
Exercise ownership/purge cases before app distribution, not only at final cleanup.
F:/Aetheri is the project workspace; other projects' active outputs/migrations remain protected.

## Proposed first implementation slice — 26 October 2026

The user accepted the lifecycle protocol. The concrete
[P0 proposal](first-linux-prototype.md) precedes the original-image boot milestone:
versioned grant/dispatch contracts, then a headless enforcing Linux test carrier,
then actual confined launch/read/revoke/removal and race/recovery evidence.

P0 uses a maintained development carrier to test Aetheri's own services. It does
not claim an original bootable image, finished desktop or complete document system.
Five new table designs and nine method roles are proposed; document edits, bulk
streams, direct/project/saved grants and UI follow. This sequence is a proposal,
with completion defined by all P0 acceptance gates rather than a date promise.
