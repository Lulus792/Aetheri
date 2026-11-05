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
not an installed desktop entry. `development_session.py` and the dock still
launch Settings only. Terminal selection/ownership integration is the next task.
Root and mismatched real/effective IDs are refused. The held `--shell` source is
trusted development input, not publisher/app admission. Commands run with the
ordinary development user's rights and environment; this is not an app sandbox.

| Control | Behavior |
| --- | --- |
| Enter, Backspace, Tab | Forward to the canonical PTY editor; rich shell editing is separate work |
| Ctrl+C / Ctrl+Z | Actual terminal interrupt/stop; the shell supplies `jobs`, `fg`, `bg` |
| Shift+PageUp / Shift+PageDown, wheel | Navigate bounded scrollback; typing returns to live output |
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

Run `ctest --test-dir BUILD -R '^terminal[.]' --output-on-failure` with all four
terminal/test executables, shell and test compositor built. The groups cover:

- `terminal.model`: split UTF-8, wide/combining cells, style/wrap, alternate/reset,
  history/resize and 20,000 deterministic hostile feeds.
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

Next connect the terminal to the fixed development launch owner/dock, then add
selection/copy/paste, full text editing/IME, panes/editor/plugins and durable
sessions. Complete terminal protocol coverage and declaration require their own
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
