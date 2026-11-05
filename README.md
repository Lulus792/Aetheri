# Aetheri

Aetheri is a planned Linux-based desktop operating system for personal daily use,
with its own desktop and applications, including required suspend/resume support.

The C17 prototype runs in a dedicated Fedora/UEFI test VM. Separate system and
user services manage verified test packages, private data and durable app-start
records. An isolated test under enforcing SELinux joins the actual caller,
supervisor, private mounts and running executable before accepting app readiness.

That test now also closes a successful app run. The user service records
quiescence for this document-free fixture; the system service verifies that the
original process and its entire supervised group have ended, and each owner
records its own result. The code
lease is released atomically; the original start receipts remain unchanged.
This works for the fixed test profile. General session provisioning, restart
recovery, document access and full app removal are still in progress.
See the [runtime contracts](spec/runtime.md#separate-durable-run-closure).

Normal app launch, document access and the desktop remain in progress. The first
[native Settings development view](spec/settings.md) reads actual virtual-display
information and supports keyboard input, per-window themes, text size and DE/EN.
An original Aetheri image, installer and admitted daily-use applications remain
work. The [current state](brains/aetheri/STATE.md) records exact implementation
and test limits.

The first [original command shell](spec/shell.md) now runs noninteractive commands
and scripts, with its own parser, quoting/variables, pipes, file redirections and
basic cwd/environment builtins. Explicit `-i` adds foreground terminal ownership,
Ctrl+C/Ctrl+Z and jobs/fg/bg. An opt-in [graphical terminal](spec/terminal.md) now
connects it to a real PTY, bounded Unicode/color grid and SDL3 view, with native
DE/US/AltGr input, scrollback, text sizing and stable mouse/keyboard selection.
The terminal now selects our [own line editor](spec/line-editor.md), with
whole-grapheme editing, bounded undo/redo and in-memory history, plus literal
command/path Tab completion and reversible history search. The syntax is experimental;
advanced completion/richer search, panes/modal editor, persistent sessions and admitted app/session
launch remain work.

The opt-in [own modal editor](spec/editor.md) now opens, edits and saves local
files inside that terminal, with grapheme-aware Normal/Insert controls, preserved
line endings/metadata and explicit close choices. Uncertain saves stay fenced;
create-only export can retain a recovery copy. Its bounded undo/redo restores
typing groups and the saved content state. Literal document search adds reversible
previews and forward/backward repeat. Character/line selection and a private
editor register now support copy, cut, change and paste. Block/mouse selection,
richer search, plugins,
panes, accessible document integration and ordinary brokered access remain work.

The opt-in [Files app](spec/files.md) now provides directory navigation, keyboard
selection, filtering and create-only empty files/folders, and opens a selected text
file in an owned Terminal/editor. Its creation worker keeps the view responsive;
unknown outcomes block further creation rather than replaying an operation.
It checks a held file witness before reading and preserves the editor's dirty-close
choice. Files is now available in the fixed development dock/search alongside Settings
and Terminal. This remains an ordinary-user development workflow; normal app
admission, brokered file access, durable creation recovery and copy/rename/removal
are still work.

An optional [built-in Compose provider](spec/compose.md) now routes real dead keys
and Compose sequences through native Wayland preedit/commit to the terminal and
shell. `start --desktop --compose` selects it in a matching development build.
External IMEs, full text accessibility and trusted session integration remain work.

A separate [private Wayland compositor](spec/graphics.md) now renders native
windows on three virtual outputs or inside a named parent Wayland display.
Native keyboard/pointer focus, move/resize, fullscreen and independent teardown
pass protocol, composed-pixel and sanitizer checks. Clipboard bytes now transfer
between separate native clients with focus/serial checks and source cleanup.
Physical display/input, a trusted session and ordinary app integration remain work.

The optional [development session](spec/development-session.md) now starts the
compositor and Settings on the carrier's actual virtual display/input. It has a
repeatable start/status/stop command and an optional native QEMU console. This
continues desktop development while normal session/app admission remains closed.

The original [development desktop](spec/desktop-ui.md) now provides a compact
dock, DE/EN keyboard search, current-window selection and separate Settings/Terminal
launch. Either app can close and reopen while the other and desktop stay running.
The owner waits for original processes and output pipes before ordered shutdown.
Use the controller's explicit
`start --desktop` mode. Its [private compositor connection](spec/desktop.md) remains
separate from ordinary app clients; the design and normal app admission are unfinished.

[Project](brains/aetheri/PROJECT.md) · [State](brains/aetheri/STATE.md) ·
[Decisions](brains/aetheri/DECISIONS.md) ·
[Architecture options](brains/aetheri/knowledge/architecture-options.md) ·
[Module and interface plan](brains/aetheri/knowledge/modules-and-interfaces.md) ·
[Identity and lifecycle](brains/aetheri/knowledge/identity-and-lifecycle.md) ·
[Schema and API](brains/aetheri/knowledge/schema-and-api.md) ·
[Storage and permissions](brains/aetheri/knowledge/filesystem-and-permissions.md) ·
[Launch and access protocol](brains/aetheri/knowledge/launch-access-and-removal.md) ·
[First Linux prototype](brains/aetheri/knowledge/first-linux-prototype.md) ·
[Versioned contracts](spec/README.md) ·
[P0 runtime contracts](spec/runtime.md) ·
[Application lifecycle](brains/aetheri/knowledge/app-lifecycle.md)

## Direction

The first useful system should support ordinary web browsing, an original
terminal/editor with document viewing and integrated pane/session management,
Physim, and up to three monitors. The desktop should feel calm and compact:
dark themes, glass, restrained animation and strong keyboard support.
Normal operation must work offline without a cloud account.

Original code, clear structure and efficient operation are priorities. The system
will explain component/file/process ownership and offer complete removal of an
app's managed state, not just its executable. A custom browser should avoid feeds,
promotions and sponsored clutter, with content filtering under user control.

The agreed starting point is C17, targeted Rust and small adapters where justified,
glibc and selected maintained technical backends. Early app UI can use suitable
working foundations such as SDL3 while NexaUI develops, then migrate per app when
its required scope is ready. Physim should migrate to NexaUI when ready.

The selected architecture uses an original wlroots-based Wayland compositor,
selected systemd components, SQLite component metadata, a versioned read-only
system base and separately managed apps. Exact releases and runtime behavior
remain to be validated.

The laptop is the first hardware test target. The current Xeon W-2123/32 GB/
RTX 2080 Ti development workstation is also a main-PC target. A later AM5 upgrade
must remain supported within x86-64/UEFI; exact hardware and drivers require
separate acceptance.

## Project memory

The project memory follows [SecondBrain](https://github.com/Lulus792/SecondBrain).
Open the `brains` workspace and select `aetheri`, or read
[START.md](brains/aetheri/START.md).

The [spec/](spec/README.md) directory holds frozen v1 and current v2 SQLite
contracts plus the additive D-Bus interfaces. Run
`python tools/test_contracts.py` with Python 3.11+ and SQLite 3.37+ for migration,
crash-reopen, constraint and wire checks. Test databases are disposable; these
checks do not start an OS service or create a production catalogue.

P0 fixture/ownership/intent formats have a development reference and frozen byte
vectors. Run `python tools/test_runtime_contracts.py` for format/payload checks and
`python tools/test_vm_inputs.py` for download-cache checks. Python remains test/
preparation tooling; the two prototype service roles are implemented in C17.

`python tools/prepare_vm.py fetch` downloads the pinned development-carrier inputs
under ignored downloads; `verify` checks cached bytes. Neither installs software,
executes an installer or starts a VM. The reviewed Windows provisioning path reads
MSI/NSIS archives as data and extracts local tools, preserving their licenses.
Read [test environment](brains/aetheri/knowledge/testumgebung.md) before using the
disposable guest. These carrier files are not an Aetheri system image.

## Development guest

With the pinned inputs present, use Python 3.11+ on Windows:

```text
python tools/provision_windows.py
python tools/run_vm.py install
python tools/run_vm.py --offline boot
python tools/run_vm.py status
python tools/sync_guest.py
python tools/run_vm.py poweroff
```

install and boot stay attached while QEMU runs. Use a second terminal for status,
SSH, source transfer and poweroff. Build tools are pinned separately in vm-inputs;
the carrier image alone does not freeze package repositories.

Install runs only on the registered 48 GiB virtual disk, after guest firmware,
product, disk serial and size checks. Reinstallation requires --retry-install;
an unobserved/live start never silently becomes a second VM. QMP uses a protected
local AF_UNIX socket, not a guest-reachable TCP listener. SSH binds localhost,
uses a guest-only key/known-host file and ignores personal SSH configuration/agents.
Offline boot restricts guest networking while retaining that host SSH connection.

sync_guest copies an explicit source snapshot through SSH. No host folder or
physical drive is passed into the guest. Native build/test commands and their
limits are in [runtime contracts](spec/runtime.md).

| Directory | Contents |
| --- | --- |
| src/core | Ownership/allocation records, fixture metadata, bounded storage, staging/publication and SHA-256 verification |
| src/catalogue | SQLite catalogues, lifecycle journals and explicit migrations |
| src/linux | Bus/process checks, original lifetime observation/handle transfer, private mount views and internal image/one-way execution primitives |
| src/authority | System/user endpoints, protected identity, broker enrollment and administrative fixture staging |
| src/launcher | Short-lived no-exec view preparer; real app-start integration remains work |
| policy | Disposable fixture and service policies; production launch policy and category allocation remain pending |
| tests | Native functional and private mount-namespace probes |
| tools | Host preparation, source transfer and development checks; not resident OS services |
| spec | SQL/XML/byte contracts and pinned carrier/build inputs |
| brains/aetheri | Requirements, decisions, evidence and remaining work |
| local, downloads, vm, out, build | Ignored tool/cache/guest/test artifacts; never public source |

## License

Original Aetheri code and documentation use the [MIT license](LICENSE).
External components retain their own licenses.
