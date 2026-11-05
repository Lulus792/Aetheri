# Local modal-editor file adapter

5 November 2026. Adds original synchronous C17 local file I/O to the tested
document/modal libraries. See the [contract](../../../spec/editor.md) and
[file API](../../../src/editor/file.h). There is still no executable modal editor,
terminal file view, command/save/quit UI or brokered document transport.

## Behavior

The caller chooses a directory FD/leaf. The adapter admits a current-user-owned,
non-publicly-writable directory, pins an existing regular no-follow single-link
file, and copies bounded bytes/metadata between repeated original observations.
Invalid/oversize text, set-ID, foreign ownership and unsupported metadata refuse
without modifying a target. New bindings create no file until explicit save.

Save owns a copied content generation and random exclusive same-directory stage,
preserves ordinary mode/GID and readable xattrs, flushes file and directory,
rechecks original metadata/content, then renames. New names use no-replace;
existing replacement is not CAS against another writer. It assumes an admitted
local Linux filesystem and no hostile same-UID writer. Exact post-rename bytes/
object observations, directory flush and writer close precede clean acknowledgement.

Unknown rename, later flush/observation/close or uncertain cleanup fences all
later saves. No retry, rollback or automatic deletion of a potential artifact.
UNKNOWN is independent of content cleanliness: even a previously clean document
can have an uncertain save outcome. The future UI must guard close accordingly.
The fence is process-local; there is no durable restart/volume/grant authority.

## Executed checks

Final root `/home/aetheri/build/editor-file-check-mseopabs`, existing unprivileged
Fedora enforcing carrier and Release / Debug ASan+UBSan build profiles:

- Four affected groups per profile: `text.graphemes`, `editor.document`,
  `editor.modal`, `editor.file`; Release 2.92s / ASan+UBSan 6.55s, all run with
  no skips or failures.
- Actual 8MiB file load/edit/save and full-byte comparison; LF/CRLF and missing
  final newline preserved; 0600 creation, existing 0750 mode, user/empty xattrs
  preserved. Repeated save of a new file works with inherited security metadata.
- Actual symlink/hardlink/FIFO/invalid-text/oversize/read-only/path refusals;
  same-name/inode/content/permission/directory changes and a new-name race cannot
  adopt observed foreign contents. These do not prove CAS under arbitrary races.
- Allocation failures in load/snapshot/metadata paths retain content and release
  descriptors. Real `/proc/self/fd` counts return to baseline.
- Controlled short writes, EINTR, zero/ENOSPC writes, pre/post-rename fsync failure,
  both sides of a lost rename reply and a close error exercise actual file effects.
  Target bytes, dirty/uncertain state, retained potential stage and no repeated
  rename are checked. A separate case covers a clean document with UNKNOWN save.
- Copied source first passes the same tests. Omitting only the post-rename
  directory-fsync result makes the unchanged test fail with exit 1. The control
  uses independent source copies and preserves the test and Unicode license.

Initial compile rejected one signed flag conversion, corrected before runtime.
The first file test attempted two real 40KiB xattrs; this carrier returned ENOSPC
while constructing that fixture. It was not an adapter failure or passing limit
test. Ordinary real xattr round trips remain; the oversized-admission case now
injects only a 65,537-byte size query for an actually listed attribute. The later
clean-document uncertainty check caused a new final run; earlier results remain
separately retained. Local drivers/evidence: `local/editor-file-2026-11-05`.
The initial evidence collector tried to read the intentional FIFO as a regular
file and blocked. Its exact live collector process was verified/pidfd-stopped;
tests were already terminal. The corrected collector copies only regular files
and records FIFO/symlink lstat metadata without opening/following those nodes.

The final read-only audit verifies 312 exact host/snapshot/guest source files,
28 frozen SQL/XML and 19 protected old hashes without SQLite sidecars. It checks
120 copied regular evidence files, separately records eight special nodes, and
resolves 1,023 local Markdown targets. The enforcing carrier is idle on tty1,
with no known native app/test, Aetheri service or seat0 session. The old intent
remains running/recover-handoff with its original bytes.

## Remaining work

Actual terminal view/key decoding and interactive save/conflict/unsaved-close,
document undo, save-as/export, durable crash reconciliation and ordinary brokered
document admission remain. No new runtime dependency, service, policy, SQL/XML,
NexaUI SDK/consumer change or other-chat message. The old quarantined runtime
is separate; this work cannot settle its lease or provenance. The full OS/apps,
image/installer, hardware and early full experiment-desktop objective stays active.
