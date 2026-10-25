# Aetheri

Aetheri is a planned Linux-based desktop operating system for personal daily use,
with its own desktop and applications, including required suspend/resume support.

The project is in architecture and product planning. There is no bootable image,
installer or working Aetheri application yet.

[Project](brains/aetheri/PROJECT.md) · [State](brains/aetheri/STATE.md) ·
[Decisions](brains/aetheri/DECISIONS.md) ·
[Architecture options](brains/aetheri/knowledge/architecture-options.md) ·
[Module and interface plan](brains/aetheri/knowledge/modules-and-interfaces.md) ·
[Identity and lifecycle](brains/aetheri/knowledge/identity-and-lifecycle.md) ·
[Schema and API](brains/aetheri/knowledge/schema-and-api.md) ·
[Storage and permissions](brains/aetheri/knowledge/filesystem-and-permissions.md) ·
[Launch and access protocol](brains/aetheri/knowledge/launch-access-and-removal.md) ·
[First Linux prototype proposal](brains/aetheri/knowledge/first-linux-prototype.md) ·
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
glibc and selected maintained technical backends. NexaUI is the preferred toolkit
candidate; Physim should migrate to it when ready.

The selected architecture uses an original wlroots-based Wayland compositor,
selected systemd components, SQLite component metadata, a versioned read-only
system base and separately managed apps. Exact releases and runtime behavior
remain to be validated.

The laptop is the first hardware test target. A later main-PC target has four CPU
cores, eight threads and an RTX 2080 Ti; graphics support needs separate acceptance.

## Project memory

The project memory follows [SecondBrain](https://github.com/Lulus792/SecondBrain).
Open the `brains` workspace and select `aetheri`, or read
[START.md](brains/aetheri/START.md).

The `spec/` directory holds the agreed initial SQLite and D-Bus contracts. The `tools/`
directory holds their disposable validation, runnable with
`python tools/validate_contracts.py` (Python 3.10+, SQLite 3.37+).
These checks do not start an OS service or create a production catalogue.

## License

Original Aetheri code and documentation use the [MIT license](LICENSE).
External components retain their own licenses.
