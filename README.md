# Aetheri

Aetheri is a Linux-based desktop operating system in development for personal,
offline-first use. It has its own desktop, shell, Terminal, editor, Files and
Settings. The first hardware target is the HP EliteBook 840 G4. The target
architecture is x86-64 with UEFI.

## Project status

The current USB development image boots on the EliteBook and preserves edited
documents across boots. Signed VM builds have verified graphical login,
virtual Wi-Fi and Sound controls, and suspend with a lock screen. The
[current state](brains/aetheri/STATE.md) links the exact test results and lists
what remains for the first version. The [laptop test guide](spec/first-laptop-test.md)
describes the existing USB image.

Aetheri uses C17 for most original code, with targeted Rust and small adapters
where needed. Linux, glibc, wlroots and selected systemd components provide the
platform. A versioned read-only system base and a SQLite component catalogue
support separately managed apps. The
[project requirements](brains/aetheri/PROJECT.md) cover planned daily-use
workflows, app and file ownership, complete removal of managed app data and
the resettable experiment desktop.

## Development

Run the host contract tests with Python 3.11+ and SQLite 3.37+.

```text
python tools/test_contracts.py
python tools/test_runtime_contracts.py
python tools/test_vm_inputs.py
```

The [test environment](brains/aetheri/knowledge/testumgebung.md) covers
pinned downloads, Windows provisioning and the Fedora development guest.
Use the [development session guide](spec/development-session.md) to start
and inspect the desktop in a VM. Follow the
[storage guide](spec/development-storage.md) when building and retaining images.

| Path | Contents |
| --- | --- |
| `src/` | Desktop, apps and Linux service implementations |
| `tests/`, `tools/` | Native tests, build tools and VM controllers |
| [`spec/`](spec/README.md) | Versioned contracts, component references and build inputs |
| [`brains/aetheri/`](brains/aetheri/START.md) | Requirements, decisions and test records in the [SecondBrain](https://github.com/Lulus792/SecondBrain) structure |
| `local/`, `downloads/`, `vm/` | Local test artifacts, downloads and development VM disks |

## License

Original Aetheri code and documentation use the [MIT license](LICENSE).
External components retain their own licenses.
