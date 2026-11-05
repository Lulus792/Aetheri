# Development graphical terminal

The opt-in C17 `aetheri-terminal` connects the original [Aetheri shell](shell.md)
to an actual private PTY and an SDL3 Wayland/GLES view. It supports ordinary
command input/output, bounded scrollback, colors and ephemeral shell job control.
It is an early usable terminal, not the finished editor/multiplexer or a promise
of xterm/Neovim compatibility. Final visual design remains open.

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

| Control | Behavior |
| --- | --- |
| Enter, Backspace, Tab | Forward to the canonical PTY editor; rich shell editing is separate work |
| Ctrl+C / Ctrl+Z | Actual terminal interrupt/stop; the shell supplies `jobs`, `fg`, `bg` |
| Shift+PageUp / Shift+PageDown, wheel | Navigate bounded scrollback; typing returns to live output |
| Drag in the cell grid | Select visible text using cell boundaries; scrolling retains unchanged selected rows |
| Ctrl+Shift+arrows / Ctrl+Shift+A | Extend selection with the keyboard / select retained text, excluding unused trailing rows |
| Escape with a selection | Clear selection without forwarding Escape to the shell |
| Ctrl+Shift+C / Ctrl+Shift+V | Report copy/paste unavailable; never forward these chords as interrupt or literal-next |
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
| [pty.h](../src/terminal/pty.h), [pty.c](../src/terminal/pty.c) | Held PTY peer, original child/pidfd, resize, independent stream/process observations and one-shot hangup |
| [main.c](../src/terminal/main.c) | SDL input/rendering, font/texture ownership and readiness-to-owner-thread handoff |

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
advertising a complete terminal capability set; `COLORTERM` is removed.

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
selection actions and Escape-to-clear never become shell bytes. Copy/paste chords currently report unavailable without sending Ctrl+C/Ctrl+V
to the PTY: UTF-8 export is not OS clipboard ownership or transport.

PTY creation/fork happens before SDL, Mesa or worker threads. The child becomes
its own controlling-terminal session through `TIOCGPTPEER`, `setsid` and
`TIOCSCTTY(0)`, inherits fixed shell argv and closes unrelated FDs before held-image
execution. Parent ownership retains the original child and pidfd. A child exit
does not imply stream EOF, and EOF does not imply child exit or descendant drain.
No process discovery by stale numeric PID/SID is used for normal ownership.

The UI thread alone changes the model and reads/writes the nonblocking PTY.
A single readiness worker polls the master, original pidfd and wake pipe, then
waits for UI acknowledgement; it performs no terminal IO or model work. Idle has
no frame/blink timer. Input has a 4,096-byte queue; a new event that cannot fit is
refused as a whole. Output processing is capped at 16 KiB per notification.
Glyph textures are bounded to 512 entries/8 MiB and retire on size changes.
Event-post failure retries only under failure, then delivers a failure for owned
shell/stream retirement; it must not abruptly abandon the owner process.
An unrecoverable event-wait error leaves the UI loop and uses synchronous owner
cleanup, rather than repeatedly calling the failed wait.

This retains app logic for later NexaUI views. No toolkit handle enters durable
app state, and no installed authority, grant, lifecycle lease or public capability
is added by the terminal.

## Inputs and evidence

The SDL3 3.4.20/SDL_ttf 3.2.2/FreeType 2.15.0/HarfBuzz 15.0.0 and Mesa profile is
the existing reviewed Settings foundation. Direct Wayland client use is the same
existing family (carrier 1.26.0), for diagnostic frame callbacks only. A callback
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

The fixed development launch owner/dock and text selection are connected. Next add
bounded asynchronous clipboard transport/paste review, full text editing/IME,
panes/editor/plugins and durable sessions. Text selection/export now exist; shared
clipboard copy/paste does not. Complete terminal protocol coverage and declaration require their own
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

## Clipboard integration boundary — 5 November 2026

The selected SDL3 3.4.20 archive's Wayland `read_pipe` / `Wayland_data_offer_receive`
wait synchronously and grow the allocation while more bytes arrive. Its extended
read timeout is five seconds per wait, not a complete-transfer deadline or byte
budget. `SDL_GetClipboardData` is documented as main-thread-only. Do not call it
from the PTY worker or expose an unbounded synchronous paste through the UI.
The archive also loops writes with per-wait timing, so a bounded sender snapshot
alone is not a complete responsiveness contract.

Next implement owner-thread Wayland request/offer handling plus bounded nonblocking
transfer readiness, complete-transfer deadlines and cancellation. Reuse the actual
SDL connection/selected surface queue where the reviewed host contract permits it;
a second client connection cannot inherit keyboard focus by having the same UID.
`wl_proxy_get_queue` is a public API in the selected Wayland family, but this
integration is still a proposal, not executed compatibility evidence. Preserve
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
