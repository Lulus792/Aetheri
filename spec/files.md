# Development Files

`aetheri-files` browses a selected directory and creates, renames or moves entries
without replacing existing targets. It opens text documents in our
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

## Application controller and presentation

`src/files/session.h` now defines the per-window application state and commands
independently of SDL or NexaUI. This is a Files controller, not a desktop login
session, persistent app session or authority interface. The existing SDL view
uses it for navigation feedback, held naming/rename/move inputs, operation
completion, unknown outcomes and the original document-child close fence.
The lower model, creation, rename/move and child-launch contracts are unchanged.
Painting, toolkit events, font/window resources and native input stay in the view.

The controller belongs to one UI owner thread and must not be copied while live.
Its one operation worker receives the already prepared original operation.
The adapter supplies two narrow callbacks: a false start guarantees no worker
or effect; a successful start retains that exact worker, and join waits for its
end. Only the worker applies the operation and release-publishes completion.
The owner acquire-observes it, joins, then reads the result and releases operation
inputs. Keep its operation pointers, kind and referenced storage immutable until
join. Closing while work is active waits rather than cancelling or duplicating it.
The SDL adapter retains its existing thread and event delivery; no additional
background worker or daemon is introduced.

An unknown result retains the original operation in the controller and blocks
new mutation. Disposing a view or its in-memory state does not undo an observed
filesystem effect. A still-live document remains an independent close obligation.
Future toolkit adapters must retain the existing input rejection, menu/modal and
selection rules; the controller's internal mutable state is not an untrusted SDK
or permission API. No NexaUI Files executable or accessibility bridge follows
from this extraction; those are the next integration work.

`files.session` exercises deferred completion and original join, worker-start
failure without effects, close during pending work, real rename/move with retained
bytes, a real mkdir with lost acknowledgement, rejected name prefixes and the
document-close fence. It runs without a display or UI library. The existing
`files.native` suite separately exercises the real SDL worker, native input,
document/editor and injected uncertain-operation paths.

## Navigation and controls

Directories appear before other entries, then names sort by their original byte
sequence. Hidden entries are initially excluded. The filter is a case-sensitive
substring of the original name. It does not search document contents.

| Control | Action |
| --- | --- |
| Up/Down, Page Up/Down, Home/End | Select and reveal an entry; grid Up/Down follows columns |
| Left/Right in the grid | Select the previous/next entry |
| Ctrl+1 / Ctrl+2, list/grid icons | Switch list and card views without changing the selection |
| Enter or double click | Enter a directory or request a text-document window |
| Alt+Up, Backspace outside the filter, Back | Return through the held directory stack |
| Alt+Home | Return directly to the held starting directory and clear the query |
| Ctrl+L | Clear and focus the filter |
| Ctrl+U while filtering | Clear the query |
| Backspace while filtering | Remove one whole grapheme |
| Escape outside a menu/dialog | Clear the filter and unsubmitted move selection |
| Ctrl+H, Show/Hide hidden | Toggle hidden entries |
| Ctrl+R | Read a fresh complete directory snapshot |
| Ctrl+N / New file | Name and create an empty file without replacing an entry |
| Ctrl+Shift+N / New folder | Name and create a folder without replacing an entry |
| F2 / Rename | Preview and rename the selected file/folder in its current folder |
| Ctrl+X with list focus | Retain one selected file/folder for a move in this window |
| Ctrl+V with list focus | Preview moving that selection into the current folder |
| F10 / … / right click | Open the keyboard/pointer actions menu |
| F6 / F7 / F8 | DE/EN labels, contrast, text size 100/125/150/200 percent |
| F4 | Hide or restore the real-location sidebar |
| F9 / information icon | Toggle information; use a read-only overlay when no pane fits |
| F11 / fullscreen icon | Request native fullscreen or restore the previous window |
| Ctrl+Q / close icon | Close Files, waiting if its document window is still alive |

Preferences are per-window and in memory. Keyboard selection reveals whole rows
or cards. The sidebar shows the selected root, held ancestors and actual root
subdirectories; it does not invent Recent/Trash providers or home bookmarks.
Root captions use an escaped basename, not a reconstructed authority path.
Native edges resize and the compact title area moves the window.

The SDL renderer now follows the selected studio's compact toolbar, sidebar,
list/card content and optional information hierarchy, using an opaque dark
material fallback and contrast mode. At 640×420 with 200-percent text, optional
panes collapse and at least one full entry remains usable. F9 still opens an
information overlay there; Escape, its close icon or an outside click dismisses
it without activating the list. Information shows observed name/type/size/location
and generic icons. It reads no document content and supplies no PDF/image preview.
Long labels can be ellipsized; this is not a full-name or accessible-text reader.
Complete glass, light theme, assistive semantics and physical acceptance remain
open. The [compositor correction](desktop.md#surface-profile-and-focus) now hides
the compact dock under a foreground fullscreen window, retaining explicit search,
normal-window activation and restore. The original overlapping-dock image remains
a historical negative, separate from the new tested behavior.

A directory snapshot holds at most 4,096 entries; navigation has at most 64 child
levels. Names retain up to 255 raw bytes. Invalid UTF-8, controls, backslashes and
selected invisible/bidirectional format characters are escaped for display without
changing the name used for filesystem operations. Labels are measured and clipped
at grapheme boundaries before creating text surfaces. A filter accepts at most
255 UTF-8 bytes. Overflow/invalid input rejects the query intent: Enter cannot
open a matching retained prefix until the user clears or cancels it.

`ae_files_ancestor` selects an already-held ancestor by depth. A new complete
snapshot must succeed before deeper descriptors are closed. Failure retains the
current stack, snapshot, selection and query; success clears the query/selection.
Selecting the current depth only clears that query/selection without rescanning.
Ancestor labels describe the names originally entered; they do not authenticate a
current path. A replacement at an ancestor's old pathname is not adopted.
`ae_files_snapshot_entry` borrows the complete snapshot independently of filtering
and hidden visibility; it does not grant permission to open or modify that entry.
The display-only formatter also escapes Unicode line and paragraph separators.

## Layout and input

`src/files/layout.h` owns the SDL-independent geometry for drawing and hit testing.
It supports list/card cells, an inline-naming budget and grid-aligned selection
reveal. The inspector also collapses when height is insufficient. No model item
is created merely to show a naming placeholder: it is a separate presentation
slot, with the original target descriptor still owned by the naming operation.

Diagnostics emit bounded `CONTROL` rectangles before the matching `VIEW` frame.
Native tests use those actual rectangles rather than old screen coordinates.
They are development observations, never item identity, permission or SDK grants.
Native text, input, rendering and operation tests complement the pure geometry
checks; logical cell containment alone does not prove text fit or accessibility.

## Creating files and folders

The inline naming view holds a descriptor for the folder selected when it opens.
Background navigation/filtering is suspended. Renaming that folder externally
does not retarget the operation to a replacement at its old path. The folder label
is an observation, not a claim that its pathname remains current.

Enter submits; Tab/Shift+Tab moves between the name, Cancel and Confirm. The
inline X and check controls provide the same pointer actions. Escape or Cancel
ends an unsubmitted edit without a filesystem effect. Ctrl+U clears the name;
Backspace removes one grapheme. Long input shows its editable tail.

Clicking elsewhere inside Files confirms a valid inline name and consumes that
original press/release; it cannot also navigate or activate another item. Invalid,
empty, composing or rejected names are not submitted as a retained prefix. Window
focus loss alone retains the draft; an interrupted name composition rejects the
intent until explicitly cleared. Full preedit/provider acceptance remains open.
An unchanged rename closes the edit without preparing a filesystem operation.
Moves and stale/unknown operation inspection keep the separate review surface.
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

F2 or the Rename menu action opens an inline same-folder edit bound to the
original selection and its retained escaped label, with an
editable new literal name. The original name is prefilled when valid for the new
name rules; Ctrl+U replaces it. Confirming an unchanged name ends the edit
without a worker. Cancel has
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
Cross-folder moves use the separate profile below. Cross-filesystem copy/move,
bulk operations and removal are not supplied by this same-folder operation.

## Moving between folders

Select a file/folder and press Ctrl+X, or choose Cut from the actions menu. This
does not rename, delete or hide it. Navigate to a destination, press Ctrl+V or
choose Paste, then review the original label, source folder, destination folder
and new name. The original leaf can remain unchanged. Confirm only in this
preview. Arrow navigation leaves filter focus; Ctrl+X/Ctrl+V do not steal the
filter's text-editing shortcuts. F10 exposes the same actions without a mouse.

The selection is window-local. It owns an independently held source parent and
O_PATH object witness with literal/escaped name copies, not a system clipboard
path or public file grant. A newer valid selection replaces it; a failed selection
keeps it. Menu Escape closes the menu, preview Escape keeps the selection for
another target, and ordinary Escape clears it without touching files. A source
replacement invalidates the preview and drops that selection. A Files-owned
document must end before selecting/submitting a move.

The … control and right-click menu use the same Cut/Paste/Rename commands. Disabled
actions cannot dispatch. Up/Down/Tab skip them; Enter/Space activates the focused
command. Matched pointer press/release belongs to the menu, including when it
closes; outside clicks, focus loss and resize cannot activate a background row.
The menu is bounded to the current window at 200-percent text size. With no room
for a list row, the empty-folder label is omitted instead of overlapping status.

`ae_files_move_prepare` extends the existing no-replace operation with a separately
held destination. Both parents are rechecked as owned, private and writable.
Preparation and the worker each inspect up to 128 destination ancestors before a
directory move: self/descendant targets are rejected before dispatch; an ancestry
error/limit is not success. This is bounded current-context observation, not
hostile namespace or durable volume authentication. The source comparison retains
the same non-CAS limitation as rename. A separately opened original directory FD
permits flushing its own changed metadata. Apply flushes that directory (when
needed), the destination parent, then a distinct source parent, before checking
the target and old-name absence. Ordinary existing file contents are not copied,
read, rewritten or force-flushed by this metadata operation.

The worker retains one attempt and its original result. A destination conflict
does not replace any entry; success refreshes/selects the destination and clears
the move selection. Close waits for the original worker. UNKNOWN retains both
parent/object bindings independently of the UI selection: clearing the selection
cannot settle it, undo it or authorize a new move. All new mutations and document
starts remain blocked as for unknown rename. There is no durable restart journal.
At most 256 move worker attempts are admitted per Files lifetime.

Moves currently rely on same-mounted-filesystem rename support. There is no
automatic cross-filesystem copy/delete fallback. A post-dispatch EXDEV is UNKNOWN
under this conservative profile; it does not become an instruction to copy or
delete the source. General copy, byte progress/cancellation and recoverable
cross-filesystem move need their own content/publication/removal protocol.

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
`files.layout` checks complete-cell geometry, control containment/nonoverlap and
grid-aligned reveal over the supported scales and pane/naming modes. Native
cases separately exercise list/grid keys and pointer controls, real sidebar
navigation, read-only information at minimum size and fullscreen/restoration.
These properties do not replace rendered-text or human accessibility acceptance.
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
Native cases add inline confirm/cancel/unchanged-name completion, the non-drag
menu action, valid-field blur with consumed pointer input, German 200-percent
minimum-viewport use, source replacement, navigation after directory rename,
own-document admission fencing and exact save after file rename. The alternate
test binary additionally holds/loses a real rename acknowledgement and proves
responsive inspection, no mutation bypass and worker-owned close.

`files.move` checks both held parents, unchanged-leaf moves, source replacement,
target conflicts, private-mode drift, moved-directory/parent flushes, ancestry
limits/overlap (including a target moved after preparation), and lost real rename/
flush replies without replay even after the original object is moved back. Its
EXDEV adapter is controlled evidence of no copy/delete fallback, not a real
cross-filesystem transfer test. Native cases exercise selection/navigation,
cancel/clear distinctions, keyboard/context menus, 200-percent layouts, file and
directory moves, exact bytes and lost-move/worker-close behavior. Clearing the UI
selection after UNKNOWN must not drop the independently retained operation.

Still required: normal admitted desktop/app integration, brokered document access and
revocation, richer navigation/details, file operations with previews/recovery,
image/PDF handlers, multiple documents, accessible semantics and hardware/monitor
acceptance. Files does not yet copy, move across filesystems or delete entries.
Creation, rename and same-filesystem move here are ordinary local development I/O, not a
production document-grant write API.
