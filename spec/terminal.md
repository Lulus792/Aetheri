# Development graphical terminal

The optional [built-in Compose provider](compose.md) now supplies an actual
compositor-to-SDL preedit/commit path in addition to the earlier controlled v3
fixture below. Native keys reach the original shell as exact UTF-8, including
multi-scalar results. Enable it with a matching build and development-session
`start --desktop --compose`. External IMEs and trusted session integration remain
separate work; the earlier fixture results retain their original scope.

The opt-in C17 `aetheri-terminal` connects the original [Aetheri shell](shell.md)
to an actual private PTY and an SDL3 Wayland/GLES view. It supports ordinary
command input/output, bounded scrollback, colors and ephemeral shell job control.
Its held shell now receives `-i --edit`, selecting our toolkit-free
[line editor](line-editor.md) and bounded in-memory history.
It is an early usable terminal, not the finished editor/multiplexer or a promise
of xterm/Neovim compatibility. The accepted studio design still needs native
implementation and visual acceptance.

## Use and controls

Build in the reviewed Linux UI environment from [Settings](settings.md), adding
`-DAETHERI_SHELL=ON -DAETHERI_TERMINAL=ON`. Both options default off. Build targets
are `aetheri-shell` and `aetheri-terminal`. The terminal needs a running Wayland
session, the matching SDL/text libraries and an explicitly owned 0700 shader-cache
directory. For the existing carrier build, the executable accepts:

```sh
/home/aetheri/build/session-release/aetheri-terminal \
  --shell /home/aetheri/build/session-release/aetheri-shell \
  --font /usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf \
  --cache /absolute/owned/cache-directory
```

The cache directory must already exist; the command above describes the CLI,
not an installed desktop entry. In the explicit carrier desktop,
`development_session.py start --desktop` now offers Terminal in dock and search,
with one separately owned app slot beside Settings; see [desktop](desktop-ui.md).
Root and mismatched real/effective IDs are refused. The held `--shell` source is
trusted development input, not publisher/app admission. Commands run with the
ordinary development user's rights and environment; this is not an app sandbox.
The development owner's cwd is inherited; personal login/home provisioning is
still separate. Test file operations explicitly choose their owned token folder.
The opt-in [own modal editor](editor.md) can now run as a foreground command in
this PTY. It uses the same grid, native text input/Compose and text scaling;
its save/close decisions remain app-owned. It does not supply document grants,
full text accessibility or normal app/session admission.
The separate [Files workflow](files.md) selects `--editor ABSOLUTE --open-leaf LEAF`
instead of `--shell`, with an inherited directory/file witness on descriptors 3/4.
That profile executes the original editor directly in a fresh PTY. Its UI close
requests the foreground editor's save/drop/back choice; external shutdown retains
the existing hangup path. This is an internal development handoff, not an app SDK.

| Control | Behavior |
| --- | --- |
| Enter, Backspace, Tab | Own shell editor: accept, delete preceding grapheme, literal command/path completion |
| Ctrl+Shift+Z / Ctrl+Shift+Y | Send 0x1f / 0x1e for own shell undo/redo; Ctrl+Z retains job-stop behavior |
| Ctrl+R, Ctrl+S in shell search | Begin/older history match; newer match. First Enter selects, another Enter submits |
| Ctrl+G / Escape in shell search | Restore the original draft/cursor; terminal selection/preedit retains its existing Escape priority |
| Arrows, Home/End, Delete | Own shell cursor/history and whole-grapheme forward deletion; foreground programs receive the corresponding terminal bytes |
| Ctrl+C / Ctrl+Z | Actual terminal interrupt/stop; the shell supplies `jobs`, `fg`, `bg` |
| Shift+PageUp / Shift+PageDown, wheel | Navigate bounded scrollback; typing returns to live output |
| Drag in the cell grid | Select visible text using cell boundaries; scrolling retains unchanged selected rows |
| Ctrl+Shift+arrows / Ctrl+Shift+A | Extend selection with the keyboard / select retained text, excluding unused trailing rows |
| Ctrl+Shift+R in the NexaUI view | Toggle output reading focus without sending PTY input |
| Ctrl+Shift+M in the NexaUI view | Request work-area zoom/restore; observed native state selects the next action |
| Escape with a selection | Clear selection without forwarding Escape to the shell |
| Escape while composing | Cancel the draft and reset native text input; no Escape byte enters the PTY |
| Ctrl+Shift+C / Ctrl+Shift+V | Copy stable selected text / receive bounded clipboard text; these chords never become interrupt or literal-next |
| Escape / Enter in paste review | Cancel staged paste; Cancel is the initial focused action |
| Ctrl+Enter / Tab then Enter in paste review | Confirm the complete reviewed snapshot with a fresh non-repeat action |
| Arrows, PageUp/Down, Home/End, wheel in paste review | Navigate the complete escaped preview without submitting text |
| F6 | Toggle English/German terminal chrome, independently of keyboard layout |
| F7 | Toggle higher-contrast chrome/default terminal colors; preserve explicit program colors |
| F8 | Cycle text size 16, 20, 24 and 32; minimum window grows from 480×320 to 960×640 |
| Close button / Ctrl+Shift+Q | Fence new input and request shell hangup, then wait for child and stream end |
| Title and window edges | Native move/resize; compositor Super+Up/Down maximizes/restores |

Normal status0 shell exit closes the view after PTY end. Other exit statuses stay
visible until Close. Closing can keep waiting for a job that ignores hangup;
there is no automatic force-kill or silent successful drain. Window/logout/reboot
persistence and its eventual UI are still separate product decisions.

## Model and ownership

| Module | Responsibility |
| --- | --- |
| [model.h](../src/terminal/model.h), [model.c](../src/terminal/model.c) | Toolkit-independent cell/escape/UTF-8 state, cursor and bounded history |
| [session.h](../src/terminal/session.h), [session.c](../src/terminal/session.c) | Toolkit-independent input queue, original-master output feeding, grid/PTY resize and close-intake fence for native presentation adapters |
| [pty.h](../src/terminal/pty.h), [pty.c](../src/terminal/pty.c) | Held PTY peer, original child/pidfd, resize, independent stream/process observations and one-shot hangup |
| [paste.h](../src/terminal/paste.h), [paste.c](../src/terminal/paste.c) | Toolkit-free whole-snapshot validation, CR normalization and bounded escaped preview |
| [composition.h](../src/terminal/composition.h), [composition.c](../src/terminal/composition.c) | Owned bounded preedit text, scalar selection and horizontal viewport without PTY effects |
| [clipboard.h](../src/terminal/clipboard.h), [clipboard.c](../src/terminal/clipboard.c) | Owner-thread native Wayland selection and bounded nonblocking transfer ownership |
| [main.c](../src/terminal/main.c) | SDL input/rendering, font/texture ownership and readiness-to-owner-thread handoff |

The SDL host now uses the shared session rather than implementing a second queue
or PTY pump. The session borrows the original initialized model and holds the
original PTY state. The host still allocates that model, opens the independently
selected shell/editor image before workers, authenticates input context, reaps
the original child, waits for stream end and performs final descriptor cleanup.
Never copy or replace a live session to change its presentation.

Queue admission is whole-event: 4,096 bytes maximum, no write on admission and no
selection/scrollback change on rejection. Its byte counter records accepted input,
not delivery or command execution. The separate nonblocking flush retries EINTR,
retains the exact unsent suffix after partial progress/EAGAIN/error and does not
report an empty queue merely because it returned zero. A zero-byte submission
does nothing. Close fences new intake before the existing one-shot pidfd hangup;
it does not discard pending bytes, close handles or claim child/stream drain.
The explicit editor Save/Drop/Back user-close path remains in the host.

The shared read method feeds at most 16 KiB of actual output per turn. Child
observation remains independent. Paste validation/review, composition provenance,
clipboard dispatch/deadlines, wakeups and rendering remain original host/model
responsibilities. This extraction prepares the actual Terminal/editor for NexaUI;
there is no new NexaUI terminal executable or changed default yet. See the
[session record](../brains/aetheri/journal/2026-11-06-terminal-session.md).

The model holds at most 240×100 cells, a separate alternate screen and 1,024
history rows, with up to three combining marks per cell. UTF-8 can span reads;
invalid scalar encodings are replaced. Cell width uses glibc `wcwidth` under
`C.UTF-8`. This is not complete Unicode grapheme/emoji shaping or font fallback.
Resize preserves rows and clips columns; it does not reflow paragraphs/history.

The implemented subset includes CR/LF/BS/TAB, delayed wrap, cursor positioning,
erase, margins, position save/restore, 16/256/RGB SGR, bold/underline/reverse,
cursor/application-cursor modes and alternate-screen selection. Parameters and
parser work are bounded. Unsupported escape intermediates are consumed; control
strings such as OSC52 have no clipboard, file, URL or execution side effects.
CAN/SUB cancel a sequence; reset leaves alternate mode and clears history access.
Reset is not a secure memory-wiping promise. `TERM=dumb` deliberately avoids
advertising a complete terminal capability set; `COLORTERM` is removed. The own
shell's explicit `--edit` selects its narrow known cursor/erase contract without
inferring capabilities from TERM. Grapheme editing does not change the grid's
current shaping/font limits; the line adapter uses explicit display fallbacks.

Each retained row also has a process-local content-incarnation token and soft-wrap
extent. Tokens move with a row into history and never wrap/reuse. A content edit,
erase or changed wrap extent creates another token; eviction removes the original.
Selection retains the exact contiguous token list, not screen indices or merely
its endpoints. Unrelated output can continue without changing the selection;
any changed selected row invalidates it. Resize, reset and alternate-screen
changes clear selection. These tokens carry no filesystem/process authority.

The toolkit-free selection API exports exact scalar/combining UTF-8, omitting
wide-cell continuations and joining real soft wraps. Hard line boundaries become
newlines and trim trailing plain blanks. Wide-character wrap padding is excluded;
meaningful spaces at a soft wrap remain. Size measurement precedes copying; a
small destination fails without writing any byte. The retained-grid bound limits
the maximum export; it is not an unbounded document buffer. Invalid/empty ranges
return size0, while a selected blank range can export just a terminating NUL.

Mouse drag and Ctrl+Shift+arrows/A operate the same model. Focus loss ends a drag
without discarding a still-valid selection. Accepted PTY input clears selection;
selection actions and Escape-to-clear never become shell bytes. Copy takes an
independent immutable export snapshot; later selection/content edits cannot
change its in-flight bytes. Native clipboard publication is distinct from a
delivery receipt. Clipboard persistence after this owner closes is not promised.

PTY creation/fork happens before SDL, Mesa or worker threads. The child becomes
its own controlling-terminal session through `TIOCGPTPEER`, `setsid` and
`TIOCSCTTY(0)`, inherits fixed shell argv and closes unrelated FDs before held-image
execution. Parent ownership retains the original child and pidfd. A child exit
does not imply stream EOF, and EOF does not imply child exit or descendant drain.
No process discovery by stale numeric PID/SID is used for normal ownership.

The UI thread alone changes the model and reads/writes the nonblocking PTY.
A single readiness worker polls the master, original pidfd, wake pipe and stable
clipboard epoll descriptor, then
waits for UI acknowledgement; it performs no terminal IO or model work. Idle has
no frame/blink timer; only active transfers add an absolute deadline. Input has a 4,096-byte queue; a new event that cannot fit is
refused as a whole. Output processing is capped at 16 KiB per notification.
Glyph textures are bounded to 512 entries/8 MiB and retire on size changes.
Event-post failure retries only under failure, then delivers a failure for owned
shell/stream retirement; it must not abruptly abandon the owner process.
An unrecoverable event-wait error leaves the UI loop and uses synchronous owner
cleanup, rather than repeatedly calling the failed wait.

This retains app logic for later NexaUI views. No toolkit handle enters durable
app state, and no installed authority, grant, lifecycle lease or public capability
is added by the terminal.

## Opt-in NexaUI presenter

6 November 2026. With the reviewed `AETHERI_NEXAUI_PREFIX` Settings configuration
and `AETHERI_TERMINAL=ON`, build `aetheri-terminal-nexaui`. It accepts the same
shell/direct-editor arguments as the original terminal, plus `--bold-font`.
For the existing carrier:

```sh
/home/aetheri/build/session-release/aetheri-terminal-nexaui \
  --shell /home/aetheri/build/session-release/aetheri-shell \
  --font /usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf \
  --bold-font /usr/share/fonts/liberation-mono-fonts/LiberationMono-Bold.ttf \
  --cache /absolute/owned/cache-directory
```

The bold default is that same reviewed carrier path. Both faces are from the
existing `liberation-mono-fonts-2.1.5-15.fc44.noarch` package, with file/license
pins in [ui-inputs.json](ui-inputs.json). This introduces no new library family.
The SDK remains 0.1.18/e71ee30. This is an explicit development executable.
The [development owner](development-session.md) now accepts a separate
`start --desktop --terminal-ui nexaui-owned` choice for normal Terminal and
Files' direct-editor windows. Omitting it retains the original terminal.

The shared host still owns the SDL Wayland window, native events/text input,
borrowed-display clipboard connection, original PTY/session, editor handoff and
readiness worker. NexaUI creates no second window, Wayland connection, parser or input
queue. It paints the complete view: rectangles, labels, composition/paste-review
overlays and the original model's visible cell grid. The host uploads one complete
RGBA8 canvas to its existing renderer. There is no idle frame timer or automatic
fallback to the old painter after a failure. SDL_ttf remains for the original
host's font/cell metrics; NexaUI's own font/shaping/raster APIs draw the new view.

The view borrows independently loaded regular/bold font bytes. Per-cell shaping
uses actual NexaUI glyph advances and mark offsets in each terminal cluster;
it does not apply paragraph bidi/reflow to the terminal. Selection, reverse and
underline styles and default-color contrast come from the original model.
The cache is bounded to 512 shaped clusters and 64 label layouts; the complete
RGBA canvas has a 64 MiB limit and 8,192-pixel dimension limit. These are prototype
resource limits, not final multi-monitor/performance acceptance. Missing glyphs
still use the selected face's .notdef; CJK/emoji font coverage is not supplied.

Completed contexts contain one full read-only snapshot per visible row. This
preserves a 240x100 Unicode grid even when its combined text exceeds the SDK's
65,536-byte per-object limit. Rows retain original content-incarnation identity;
offscreen history is not copied into another text model. Only a live, visible,
unselected input cursor is represented as a row caret. Other rows expose no
invented caret or clipped selection. The native transport below now publishes
these read-only snapshots. They do not make the PTY an editable text field or
establish a complete accessible terminal profile. Human assistive acceptance
remains open.

`terminal.view` checks actual pixels, fonts, styles, density, row snapshots,
dense Unicode bounds and unchanged model state. `terminal.view_native`,
`terminal.view_clipboard` and `terminal.view_ime` run the original native workflows
against the new executable. `files.terminal_nexaui` runs the real Files/editor
workflow with that terminal. The wrapper checks font/license pins, the actual
NexaUI-frame logs, ordinary binary symbols and the selected SDK metadata/library.
It preserves each old driver's result/logs. See the
[executed record](../brains/aetheri/journal/2026-11-06-terminal-view.md).

This first presenter retains the existing opaque geometry. The accepted compact
chrome/glass design, startup mark with truthful system information, tabs/panes,
complete assistive integration, optimized presentation and physical acceptance
remain work; the design studio is not copied as fake shell output or operations.

### Native read-only text transport

The same pinned SDK provides `linux_accessibility_host.h` for an application
that owns its window/event loop. The NexaUI Terminal now uses it to publish
completed row snapshots and a localized press-only Close control. That command
calls the original `user_close`: the editor still owns save/drop/back and the
host still waits its original child and PTY. Native text/caret/selection mutation
is refused; terminal editing remains input to the original running program.
The ordinary SDL terminal does not gain a NexaUI dependency.

Only the UI thread creates, publishes, pumps, routes and retires the bridge.
Its geometry uses logical window size, observed SDL keyboard focus and shown/
minimized state. Screen position is unavailable, so coordinates are window
relative. Focus/visibility changes refresh the completed snapshot metadata even
when no pixel redraw is required; an action request never fabricates focus.
This does not identify physical occlusion or prove human-visible presentation.
The current adapter does not acquire an inactive native window on a bridge
request. The reading focus below is logical focus inside an already active
window; it is never evidence of native activation. A complete screenreader
reading path remains later integration work.

The existing readiness worker additionally polls the bus readiness descriptor
and the earlier of clipboard/accessibility deadlines. The UI thread retains a
CLOEXEC duplicate under a mutex; the worker copies it under that same lock and
closes its own duplicate before acknowledging readiness. Reconnect cannot reuse
the polled file object. No SDK call or snapshot crosses to that worker, and no
new permanent thread/window or constant idle repaint is added. An unavailable
optional bridge is retried using a bounded wait; ordinary terminal operation
continues. A connected idle bridge uses its actual native wait/deadlines.
The worker joins before bridge/context retirement.

`terminal.view_atspi` compiles an independent C17/libatspi client against the
existing verified private fixture. On the actual test compositor it checks real
Unicode shell output, character counts/read-only state, refused mutations,
bounded window-relative row geometry, focus loss/return, same-process bus
replacement and localized Close/defunct old objects. The same client rejects
a native window with its bridge disabled. This is scoped automated transport
evidence, not an Orca/human screenreader, complete selection/caret/event/geometry,
paste-review/preedit or normal-session acceptance. The other terminal workflow
wrappers retain their narrow input/render scope; they do not test the native
transport merely by linking it. See the
[accessibility record](../brains/aetheri/journal/2026-11-06-terminal-access.md).

### Output reading focus

In `aetheri-terminal-nexaui`, Ctrl+Shift+R clears reading focus or selects the
visible input row; in scrollback it selects the first visible row. Ordinary
typing still belongs to the original shell/editor. Reading focus adds a blue
row outline and a short footer. It neither sends shortcut bytes to the PTY nor
changes the terminal cursor/selection. Native Component focus can select another
visible read-only row while the window is already active. The pinned SDK refuses
that request from an inactive Wayland window; it cannot manufacture activation.

Each row's semantic ID is derived from its original content-incarnation token,
with checked offset arithmetic. A model-size change starts an ID range above
every previously emitted ID; returning to an earlier size cannot revive a node.
Moving unchanged content within one size preserves its node and focus.
Edits/erase, replacement, reset/resize or leaving the visible projection
retire the old target; focus is not transferred to different content at the same
screen position. A queued stale target cannot focus the replacement. One original
model lifetime belongs to a view. Tokens remain local content observations,
not account/run/file authority. Identity exhaustion fails without wrapping.

Preedit, paste review and closing disable new reading targets. Native text/caret/
selection writes remain refused. Logical focus can be retained through ordinary
window blur, while native FOCUSED is reported only with actual window focus.
The live input row still exposes its actual model caret; other snapshots do not
invent a caret merely because their row is focused. Selection/range navigation,
full screenreader behavior and native activation remain separate work.

The view test verifies unchanged model bytes, row movement/replacement, stale
queued focus, disabled targets and a visible outline. An independently compiled
positional-ID negative detects the movement regression. The native client checks
row focus, the actual prompt caret, shortcut/input separation and stale Component
refusal after original content changes. See the
[focus record](../brains/aetheri/journal/2026-11-06-terminal-focus.md).

### Compact window controls

The NexaUI presenter has left-hand Close, unavailable Minimize and Zoom/Restore
controls on a compact opaque header. It uses the accepted studio's dot treatment,
with 22-by-26 logical targets and a smaller independently scaled title. The native
fallback is 32 pixels high at ordinary text size and 38 at 200 percent; final
28-pixel/glass material integration remains separate. No tab strip is shown for
the current single session. Font cache entries distinguish text point sizes.

Close follows the original shell/editor rules, including Save/Drop/Back and
waiting for the actual child and output stream. Pointer release must match the
originally pressed control. Escape, focus/pointer departure and geometry/state
changes cancel pending presses; a body-origin selection cannot become a button
action. The existing SDL hit-test excludes the controls from native caption and
resize regions. Disabled controls consume their own press without sending PTY
input or another action.

Zoom and Ctrl+Shift+M use current SDL flags to request maximize or restore. A
successful request does not acknowledge the native state; later observations
change the Zoom/Restore name and actual grid geometry. The adapter refuses zoom
while closing, composing, reviewing paste or fullscreen. Fullscreen is separate.
Minimize is explicitly disabled because the current development compositor
advertises only maximize/fullscreen and lacks a complete hide/restore lifecycle.
The independent native peer checks its disabled state and lack of actions.

View tests cover density/text scaling, localization, high contrast, unavailable
and stale actions, and identical label pixels with or without a differently
sized title. Native input and independent AT-SPI checks exercise actual zoom/
restore, original-press rejection and original-shell Close. Compiled font-cache
and pointer-owner counterexamples detect their corresponding regressions. See
the [chrome record](../brains/aetheri/journal/2026-11-06-terminal-chrome.md).
Final materials, title double-click/snapping, complete human/native activation,
minimize/fullscreen integration and physical acceptance remain work.

## Inputs and evidence

The SDL3 3.4.20/SDL_ttf 3.2.2/FreeType 2.15.0/HarfBuzz 15.0.0 and Mesa profile is
the existing reviewed Settings foundation. Direct Wayland client use is the same
existing family (carrier 1.26.0), for diagnostic frame callbacks and clipboard
selection/transfer. The terminal build now requires that exact client version;
the compositor already uses this reviewed family. A callback
is a compositor scheduling observation, not scanout, physical acceptance or app
READY. `--diagnostics` records bounded counters/states rather than terminal text.

The existing carrier's `liberation-mono-fonts-2.1.5-15.fc44.noarch` supplies the
fixed-width Liberation Mono font. Exact font/license hashes and the limited RPM
metadata evidence are in [ui-inputs.json](ui-inputs.json). `rpm -V` passed and the
installed signing metadata names the accepted Fedora44 signer; this is not fresh
independent RPM signature verification. OFL-1.1-RFN remains applicable; retain its
license and reserved-name conditions. No new library or global package is installed.
`build_ui.py` still copies Sans only; it does not yet package this explicit Mono
asset. An Aetheri distribution/font package remains future work.

Run `ctest --test-dir BUILD -R '^terminal[.]' --output-on-failure` after building this opt-in profile, including its shell and test compositor.
The groups cover:

- `terminal.model`: split UTF-8, wide/combining cells, style/wrap, alternate/reset,
  history/resize and 20,000 deterministic hostile feeds.
- `terminal.selection`: exact forward/reverse and wide/combining UTF-8, hard/soft
  line boundaries, retained-row movement/eviction, changed interior rows, keyboard
  extension, maximum retained history, token exhaustion and atomic copy bounds.
- `terminal.pty`: actual shell IO/resize/hangup, retained slave writer after shell
  exit, and a still-live trusted helper after PTY EOF. Early close remains EBUSY.
- `terminal.native`: actual Wayland keyboard input and SDL buffers, exact German
  and AltGr output bytes, colors, history, stop/fg/interrupt, maximize, language/
  contrast/200% text, idle and normal/fault shutdown. Its separate event-failure
  executable redirects event-post/wait calls in test code only, including a
  persistent wait failure.

Release and ASan/UBSan runs, original failures, native image hashes, the abrupt-
exit negative control and exact artifact roots are indexed under
`local/terminal-2026-11-05`. Third-party SDL/text/Mesa in this profile remain
Release libraries; this is not instrumentation of the entire graphics stack.

Initial tests caught reversed `<`/`>` keys and missing client focus/accepted-input
synchronization in the harness. The corrected native case requires exact output
bytes, not just a file's presence. A separate model regression reproduces SGR
cancelling delayed wrap. The unchanged native fault assertions reject a build-
local abrupt-worker-exit mutation. Original failing artifacts are retained.

## Remaining work

The fixed development launch owner/dock, text selection, bounded native
clipboard/paste review and SDL preedit view are connected. Next add the actual
compositor/provider IME route and full text editing,
panes/editor/plugins and durable sessions. Complete terminal protocol coverage and declaration require their own
compatibility corpus. Images/PDF, accessibility and normal admitted app/session
integration remain required. Physical mixed-DPI/GPU/input/suspend, light-theme and
final visual acceptance are not established by virtual software-rendered images.
Profile startup/steady/peak memory and driver threads before choosing performance
changes: a first llvmpipe RSS sample is not an Aetheri memory budget or a speed
comparison with NexaUI. The OS image, installer, apps and full experiment desktop
remain part of the active project.

Primary API references: [SDL event posting](https://wiki.libsdl.org/SDL3/SDL_PushEvent),
[event allocation](https://wiki.libsdl.org/SDL3/SDL_RegisterEvents),
[current modifier state](https://wiki.libsdl.org/SDL3/SDL_GetModState),
[PTY ioctls](https://man7.org/linux/man-pages/man2/ioctl_tty.2.html) and
[xterm control reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html).
The last is a protocol reference, not a compatibility claim.

## Selection checkpoint — 5 November 2026

The five affected terminal/actual-owner groups pass Release (18.44 s) and Debug
ASan/UBSan (23.84 s). Current native terminal tests have sixteen cases and thirty
images per profile; the real-owner fixture adds seven cases/six images. All 72
raw image hashes verify. The unchanged test rejects a build-local omission of
selected-row validation. Device removal exercises pointer and keyboard together;
it does not isolate keyboard focus loss while the original pointer stays held.

The actual final Debug DRM session also checks normal/200% selection, unavailable
copy/paste chords, exact pixel restoration after Escape and original ordered
shutdown. Ten console images form five matching pairs. Original logs/results and
exact sources/binaries are under `local/terminal-selection-2026-11-05` and the
[project source index](../brains/aetheri/SOURCES.md). The preexisting optional PAM
warning is unchanged. These are software/virtual results, not physical GPU,
Windows host input, native assistive or complete clipboard acceptance.

## Bounded native clipboard and paste — 5 November 2026

The adapter borrows the actual SDL Wayland connection/surface and its selected
queue using public client APIs. SDL keeps connection/event dispatch ownership;
the adapter creates no second reader, native thread or persistent service. One
seat is supported; absent/ambiguous capabilities are unavailable. An independent
keyboard listener observes this exact surface's native focus/serial and a
nonreused local focus epoch. Focus loss drops old offers; only a newly received
offer may be used after re-entry. This is UI intent validation, not app identity,
session trust, a file grant or a security boundary against a hostile compositor.
The app constructs one adapter for its SDL connection lifetime. Its registry
request has connection-lifetime server bookkeeping under the Wayland contract;
local adapter close/FD stability is not proof that repeatedly creating registries
on a still-live borrowed connection releases that server state. SDL disconnects
after the terminal closes. Do not turn this internal one-shot integration into
an unbounded adapter recreation loop.

Budgets are 64 offer objects, four retained sources, eight outgoing transfers,
8 MiB maximum text per source and 16 MiB aggregate adapter text allocation.
The terminal requests at most **4,096 raw bytes** per paste. There is one receive
and at most one unacknowledged FD-bearing receive request: cancellation closes
the local reader, but a sync barrier must acknowledge the original request before
another native request can be issued. It is not remote-FD revocation.

Only nonblocking pipes/stream sockets are accepted for outgoing native writes.
Each ready transfer processes at most 16 KiB per pump; a five-second absolute
monotonic deadline covers the entire transfer and does not restart on progress.
Per-thread SIGPIPE handling preserves the process-wide disposition and existing
pending signals. In-flight sends retain their original snapshot after source
replacement, until completion, failure or deadline. Stale epoll observations carry
nonreused cookies, so FD reuse cannot select a new transfer. The readiness worker
polls only a stable descriptor and an atomically copied deadline; all actual
protocol/IO/model changes remain on the UI owner. Close joins it before releasing
the adapter and before SDL destroys the borrowed surface/connection.

Paste validates complete strict UTF-8 before changing the input queue. NUL,
terminal escape/control bytes, DEL/C1 and bidi-formatting controls are rejected.
CR/CRLF normalize to LF. Ordinary single-line text is literal input; LF/TAB and
Unicode line separators require review. The solid modal shows line/byte counts
and every preview row through bounded scrolling; backslashes, line breaks, tabs
and nonprinting/zero-width scalars have visible escapes. Actual normalized input
bytes remain unchanged by preview escaping. F6/F7/F8 and Close stay available.

The initial action is Cancel. Confirmation requires an actual non-repeat key
event or matching button press/release after the first SDL review-frame
submission. That timestamp is not proof of physical display/scanout.
Before accepting received/reviewed text, the app rechecks native focus epoch,
the original live shell, current PTY foreground-group observation, screen mode
and open input/stream state. Ordinary new input, relevant focus loss or close
cancels pending/staged paste. Accepted text is queued once; a later uncertain
write is never retried as a new paste. The foreground group is an observation,
not authorization or process-recovery identity. This profile has no bracketed-
paste support yet and retains `TERM=dumb`; normal app admission, complete VT,
IME, secret clipboard policy and lock/session integration remain work.

The [pure paste regression](../tests/test_terminal_paste.c) covers UTF-8/control
rejection, atomic state, normalization, exact limits and full preview traversal.
The [native peer test](../tools/test_terminal_clipboard.py) uses the actual
terminal/shell and independent libwayland sources/receivers; its
[adapter contract client](../tests/test_terminal_clipboard_api.c) separately checks
wrong-thread/reentrant rejection and retained borrowed objects/FD counts. Test
source code is not evidence that a particular candidate passed: current executed
results and any remaining gates belong in STATE and the evidence record.

## Application-owned composition view — 5 November 2026

The terminal now distinguishes `SDL_EVENT_TEXT_EDITING` drafts from committed
`SDL_EVENT_TEXT_INPUT`. The copied C17 composition model holds at most 1024 UTF-8
bytes, scalar-to-byte offsets, cursor and selection. A complete strict UTF-8,
single-line/control check precedes replacement; invalid model updates are atomic.
Empty preedit clears it. SDL positions are Unicode scalars, not UTF-8 bytes,
graphemes or cells; unspecified `-1` follows SDL's contract. Supported reverse
ranges are normalized for painting. This is not complete Unicode bidi/grapheme
editing or font fallback.

The view paints a transient highlighted/underlined draft over the current cursor
without altering cell/history content tokens. Its horizontal viewport follows
the supplied cursor and keeps full bounded text in the model. Font/contrast/
locale changes preserve that draft. The candidate rectangle uses actual logical
window coordinates and is updated only when changed. Rendering retains the
terminal's existing fixed-width/glibc-width/font limits; this does not establish
arbitrary-script shaping, fallback fonts or a complete screenreader interface.

Editing/confirmation keys do not become speculative PTY bytes while a draft is
visible; Escape cancels and the existing Close/clipboard/F6-F8 controls remain
available. Commit rechecks the original live shell, foreground-group/screen
observations, focused window and queued-event timestamp. The whole bounded text
must be valid before a single queue admission. The current committed-text profile
rejects multiline/tab/control input; multiline clipboard uses its separate review.
Local input-queue acceptance is not proof that an arbitrary terminal application's
canonical/raw input buffer consumed every byte. Rich own-shell editing remains
separate work.

The pinned SDL Wayland backend has no effective `ClearComposition` callback:
the public call can return success without a native reset. The adapter instead
stops and, when appropriate, restarts actual text input. It is inactive while
unfocused, closing, shell/stream-ended or displaying paste review. Already queued
old editing/text events are fenced by owner time. This is a local event-queue
boundary, not authentication of an IME provider, remote revocation or proof that
all future provider messages belong to a current protocol transaction. Correct
provider admission, serial handling and input-method key routing remain required.

The [pure model test](../tests/test_terminal_composition.c) covers scalar/byte/
cell distinctions, reverse/unknown positions, atomic invalid input, bounds and
horizontal viewing. The [native integration](../tools/test_terminal_composition.py)
uses actual SDL and shell/PTY with a
[controlled Wayland-v3 producer](../tests/compositor_text_input.c). Its manager is
created only by `I on` over the test server's inherited private socket. Production
`aetheri-compositor` does not link this fixture, expose its commands or install an
IME service. Native tests must not be described as completed normal Aetheri IME
integration, installed engine compatibility, human input or accessibility acceptance.
Executed candidate-specific results belong in STATE and the handoff record.

Primary references: [SDL editing events](https://wiki.libsdl.org/SDL3/SDL_TextEditingEvent),
[composition reset API](https://wiki.libsdl.org/SDL3/SDL_ClearComposition) and
[candidate area](https://wiki.libsdl.org/SDL3/SDL_SetTextInputArea). Version-specific
reset and event behavior was additionally read in the already-pinned SDL archive;
the generic API documentation does not establish the backend's actual effect.

## Initial SDL clipboard boundary — historical investigation

The selected SDL3 3.4.20 archive's Wayland `read_pipe` / `Wayland_data_offer_receive`
wait synchronously and grow the allocation while more bytes arrive. Its extended
read timeout is five seconds per wait, not a complete-transfer deadline or byte
budget. `SDL_GetClipboardData` is documented as main-thread-only. Do not call it
from the PTY worker or expose an unbounded synchronous paste through the UI.
The archive also loops writes with per-wait timing, so a bounded sender snapshot
alone is not a complete responsiveness contract.

This investigation led to owner-thread Wayland request/offer handling plus bounded nonblocking
transfer readiness, complete-transfer deadlines and cancellation above. It required the actual
SDL connection/selected surface queue where the reviewed host contract permits it;
a second client connection cannot inherit keyboard focus by having the same UID.
`wl_proxy_get_queue` is a public API in the selected Wayland family. Preserve
window/clipboard/transfer ownership independently; add no persistent clipboard
helper or host bridge implicitly. A complete validated snapshot must precede any
PTY bytes, with explicit review for multiline paste and no execution of control
strings. Normal app/grant/lock integration remains separate work.

Primary references inspected for this boundary:
[SDL clipboard retrieval](https://wiki.libsdl.org/SDL3/SDL_GetClipboardData),
[clipboard publication](https://wiki.libsdl.org/SDL3/SDL_SetClipboardData) and
[Wayland client queues](https://wayland.freedesktop.org/docs/html/apb.html).
Versioned source behavior above was read from the already pinned local SDL archive;
live documentation is not evidence that another release has identical behavior.
