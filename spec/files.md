# Development Files

`aetheri-files` browses a selected directory, creates a new empty file or folder,
renames a selected file/folder without replacing another entry,
and opens a text document in our
[editor](editor.md), inside a separately owned [Terminal](terminal.md) window.
Navigation and filtering use an original C17 model; SDL3 supplies the Wayland
window and drawing. The app uses the existing reviewed SDL/text dependency profile.

This is an ordinary-user development app, now launched from the fixed development
dock/search owner. It is not yet a document broker, an installed app sandbox or
the finished file manager. The initial folder
is a navigation anchor, not a security boundary. The process retains the development
user's normal rights; held directories can cross mounts or survive renames.

## Build and run

Use the reviewed Linux UI environment in [Settings](settings.md). Add
`-DAETHERI_FILES=ON` to build `aetheri-files`. To use the document workflow, also
build with `-DAETHERI_TERMINAL=ON -DAETHERI_SHELL=ON -DAETHERI_EDITOR=ON`.
The view options default off. The terminal retains its ordinary shell mode;
Files selects a separate direct-editor mode for each document.

The running Wayland session must provide the matching reviewed SDL/text libraries.
Choose an existing directory and a private, user-owned 0700 shader-cache directory:

```sh
/home/aetheri/build/session-release/aetheri-files \
  --root /absolute/document-directory \
  --font /home/aetheri/deps/ui-gles-release-02/prefix/share/aetheri-fonts/LiberationSans-Regular.ttf \
  --terminal-font /usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf \
  --cache /absolute/owned/cache-directory \
  --terminal /home/aetheri/build/session-release/aetheri-terminal \
  --editor /home/aetheri/build/session-release/aetheri-editor
```

In the fixed development desktop, choose Files from dock or global search. It
browses the session’s initially empty `documents` workspace, which is retained
after window/session close. The command above selects a separate explicit root.
These are development paths, not installed desktop entries. Root and mismatched
real/effective user or group IDs are refused. There is no X11/offscreen fallback.
`--diagnostics` adds view/request/end records; a request record is not readiness.

## Navigation and controls

Directories appear before other entries, then names sort by their original byte
sequence. Hidden entries are initially excluded. The filter is a case-sensitive
substring of the original name. It does not search document contents.

| Control | Action |
| --- | --- |
| Up/Down, Page Up/Down, Home/End | Select and reveal an entry |
| Enter or double click | Enter a directory or request a text-document window |
| Alt+Up, Backspace outside the filter, Back | Return through the held directory stack |
| Ctrl+L | Clear and focus the filter |
| Ctrl+U while filtering | Clear the query |
| Backspace while filtering | Remove one whole grapheme |
| Escape | Clear/cancel the filter |
| Ctrl+H, Show/Hide hidden | Toggle hidden entries |
| Ctrl+R | Read a fresh complete directory snapshot |
| Ctrl+N / New file | Name and create an empty file without replacing an entry |
| Ctrl+Shift+N / New folder | Name and create a folder without replacing an entry |
| F2 / Rename | Preview and rename the selected file/folder in its current folder |
| F6 / F7 / F8 | DE/EN labels, contrast, text size 100/125/150/200 percent |
| Ctrl+Q | Close Files, waiting if its document window is still alive |

Preferences are per-window and in memory. Rows scroll with keyboard selection;
native window edges resize and the title area moves the window. At a viewport too
small to show any row, entry activation is disabled and the view asks for a larger
window. This is an initial dark/contrast presentation; light theme, full accessible
semantics and final visual design remain open.

A directory snapshot holds at most 4,096 entries; navigation has at most 64 child
levels. Names retain up to 255 raw bytes. Invalid UTF-8, controls, backslashes and
selected invisible/bidirectional format characters are escaped for display without
changing the name used for filesystem operations. Labels are measured and clipped
at grapheme boundaries before creating text surfaces. A filter accepts at most
255 UTF-8 bytes. Overflow/invalid input rejects the query intent: Enter cannot
open a matching retained prefix until the user clears or cancels it.

## Creating files and folders

The naming dialog holds a descriptor for the folder selected when it opens.
Background navigation/filtering is suspended. Renaming that folder externally
does not retarget the operation to a replacement at its old path. The folder label
is an observation, not a claim that its pathname remains current.

Enter submits; Tab/Shift+Tab moves between name, Cancel and Create. Escape or
Cancel closes an unsubmitted dialog without a filesystem effect. Ctrl+U clears
the name; Backspace removes one grapheme. Long input shows its editable tail.
Names are literal UTF-8 leaves up to 255 bytes, excluding slash, dot/dotdot,
controls and selected invisible/bidirectional format characters. Invalid or
overflowing input rejects the whole intent: its retained prefix cannot be used
until cleared or cancelled. No extension, template or example file is inserted.

Creation requires an ordinary non-setid user and a user-owned writable/searchable
parent without group/world write or special mode bits. The standalone C17
`files/create.h` operation owns a duplicate of that parent and immutable name/kind.
Preparation has no filesystem effect. Apply attempts at most once; every repeated
call returns the original result without replaying writes or flushes.

An empty file reuses the editor's create-only staged save, including 0600 mode,
`RENAME_NOREPLACE`, required flushes and verification. A directory uses
`mkdirat(0700)`, joins held/named observations and flushes the directory and parent.
An existing entry, including a dangling link, is a conflict and remains unchanged.
These ordinary-user checks do not exclude a hostile concurrent writer, authenticate
a volume or enroll a managed StorageRef. The directory observation is not a
durable allocation witness.

One SDL worker performs the attempt. The view stays responsive and waits for the
original worker before closing; after an effect is possible, Cancel is not a
rollback. On confirmed success, Files refreshes, clears its filter and selects the
new entry. A newly created dotfile is revealed. A failed refresh is reported
separately from successful creation, without inviting another creation attempt.
An empty file can then be opened and saved through the existing document route.
At most 256 prepared worker attempts are allowed per Files lifetime.

An unacknowledged mkdir/flush can leave an entry despite an error. UNKNOWN retains
the operation and any possible staging name, blocks further creation for this
window and opens inspection instead of another submission. Browsing and existing
document work remain possible. Closing releases memory/FDs only, never deletes
possible entries or stages. This prototype has no durable restart journal:
reopening Files does not reconcile an earlier unknown outcome. Inspect the
original target rather than assuming absence or automatically retrying.

## Renaming a selected entry

F2 or Rename opens a same-folder preview with the original escaped label and an
editable new literal name. The original name is prefilled when valid for the new
name rules; Ctrl+U replaces it. An unchanged name cannot be submitted. Cancel has
no filesystem effect. Name validation, rejected-prefix fencing, keyboard focus,
text scaling and the single worker are shared with creation.

Files holds the selected parent and an `O_PATH` witness for a regular file or
directory, checking the snapshot's device/inode/type. Links and other object kinds
are not admitted. A document owned by this Files window must end before a rename
preview can open; this does not fence independently opened external writers.
The separate `files/rename.h` operation duplicates the parent/witness and copies
both literal names during effect-free preparation. Source ownership/type and the
private writable parent are checked before dispatch. A replaced/missing source
invalidates the preview: cancel and refresh instead of adopting its replacement.

The destination uses Linux `renameat2(RENAME_NOREPLACE)`, with no replace, copy or
unlink fallback. A destination already observed before dispatch is a conflict and
remains unchanged, including a dangling link. A racing destination is still
protected by the kernel's no-replace flag. After dispatch, any error is UNKNOWN;
do not infer that no rename occurred from an error, including on a network
filesystem. A successful call is followed by parent fsync and new/old-name
observations. Existing file contents are not rewritten or hashed. Directory
contents, permissions and other hard links retain their normal filesystem meaning.

The destination nonreplacement guarantee is atomic. The source identity check
is **not a source-inode compare-and-swap**: another writer can replace a name
between checking and the syscall. This local prototype assumes ordinary cooperative
use and does not claim hostile-writer exclusion, a write grant, mount/volume
identity, app admission or a managed rename transaction. See the
[Linux rename contract](https://man7.org/linux/man-pages/man2/rename.2.html).
Production brokered mutation fencing remains separate work.

Apply attempts once; repeated calls only return the original outcome. Lost
rename/flush acknowledgements retain the original operation and both names.
There is no automatic retry, reversal, candidate deletion or restart journal.
UNKNOWN in either creation or rename blocks both kinds of new mutation for this
window. Buttons reopen read-only inspection. An unknown rename also blocks new
document starts, while directory browsing remains possible. Closing waits for
an in-flight worker and releases memory/FDs only. Reopening the app is not recovery.

Confirmed success refreshes, clears the filter, reveals a dot-name if necessary
and selects the new name. A refresh failure is separate from confirmed rename.
The prototype admits at most 256 rename worker attempts per Files lifetime.
Cross-folder moves, cross-filesystem copy/move, bulk operations and removal are
not supplied by this same-folder operation.

## Selected document handoff

Files retains directory descriptors and does not follow symlinks. Enter and Open
recheck the selected entry's device/inode/type. A failed complete scan or navigation
keeps the old snapshot; an explicit refresh is needed after external changes.
There is no watcher daemon, recursive scanner or background indexer.

For a regular file, Files obtains a parent-directory descriptor and an `O_PATH`
file witness. The separately configured Terminal receives them as descriptors
3/4 with the exact leaf as one literal argument. Files uses `posix_spawn` and file
actions after SDL has started threads, closes other extra descriptors in the child,
removes inherited `WAYLAND_SOCKET` and starts a separate process group. It never
constructs a shell command from the selected filename.

Terminal's direct-editor profile creates a fresh controlling PTY before starting
SDL threads, pins the editor executable and passes the same 3/4 inputs to
`aetheri-editor --selected-fds LEAF`. The editor compares the named regular file
against that witness **before reading its contents**, then uses its existing
bounded load/conflict/save rules. The witness remains held through handoff, avoiding
reuse of that original inode during the comparison. This does not authenticate an
earlier directory snapshot, a durable volume, an app, a StorageRef or a document grant.

## Window ownership and closing

One document Terminal is allowed at a time, with a bounded lifetime start count.
Files retains its original direct-child slot and observes it with `waitpid`; an
available pidfd also pins that child. Lost/unknown observation does not permit a
replacement start. New document starts stop once Files begins closing.

Closing the direct-editor Terminal through its UI sends the foreground original
editor a close request. Dirty content keeps the editor's Save/Drop/Back decision;
cancel returns to editing. External shutdown/error handling retains the existing
Terminal hangup path and editor emergency behavior. A foreign foreground context
does not gain the graceful direct-editor shortcut.
The direct document entry refuses Ctrl+Z/SIGTSTP suspension with a visible notice:
there is no shell in that window to resume a stopped job. The ordinary shell-run
editor retains its existing suspend/`fg` behavior. External SIGSTOP remains outside
this cooperative UI path.

Files waits for the original Terminal to end, then refreshes the current folder
so an editor save's replacement inode is visible. Process creation alone is not
editor readiness; PTY EOF alone is not process end. This direct-child lifetime is
not descendant/FD-holder drain, lease settlement or durable restart recovery.

## Verification and remaining work

`files.model` exercises held navigation, rename/replacement, escaped names, filter/
hidden state, allocation/list limits and selected-witness editor load/save.
`files.launch` executes a separate native process and checks literal argv, exact
3/4 handoff, closed extra descriptors, environment/signal mask/process group and
original-child end ownership. `files.native` runs actual Wayland keyboard/pointer,
three-output pixels, resize/contrast/200-percent states, rejected-prefix activation,
idle observation and Files → Terminal → Editor saves, dirty-close cancellation and
ordered close. It needs the explicit compositor/Settings/Terminal/Editor test build
and reviewed fonts. These are disposable-carrier tests, not physical acceptance.

`files.create` checks private modes, collisions, retained-parent rename/replacement,
allocation failure, changed permissions and lost mkdir/file/directory-flush
acknowledgements. Its syscall wrappers execute the real operation before losing
the reply. Native UI cases additionally create a folder at 640×420/200 percent,
cancel/reject/conflict without unwanted entries, create/open/edit/save an empty
file and use a separate test-only binary to hold then lose a real mkdir reply.
That binary proves responsive UI, blocked repeat attempts and close waiting for
the original worker. The ordinary Files executable contains no fault hook.

`files.rename` checks held source/parent identity, same-folder file/directory
rename, retained contents, collisions, raw old names, private-parent drift,
allocation failure, destination races and lost real rename/flush replies.
Native cases add preview/cancel, the non-drag pointer button, German 200-percent
minimum-viewport use, source replacement, navigation after directory rename,
own-document admission fencing and exact save after file rename. The alternate
test binary additionally holds/loses a real rename acknowledgement and proves
responsive inspection, no mutation bypass and worker-owned close.

Still required: normal admitted desktop/app integration, brokered document access and
revocation, richer navigation/details, file operations with previews/recovery,
image/PDF handlers, multiple documents, accessible semantics and hardware/monitor
acceptance. Files does not yet copy, move across folders or delete entries.
Creation and same-folder rename here are ordinary local development I/O, not a
production document-grant write API.
