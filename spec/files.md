# Development Files

`aetheri-files` browses a selected directory and opens a text document in our
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

Still required: normal admitted desktop/app integration, brokered document access and
revocation, richer navigation/details, file operations with previews/recovery,
image/PDF handlers, multiple documents, accessible semantics and hardware/monitor
acceptance. File mutation in this first workflow belongs to the editor; Files
does not yet copy, rename or delete entries.
