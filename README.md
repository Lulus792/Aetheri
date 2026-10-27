# Aetheri

Aetheri is a planned Linux-based desktop operating system for personal daily use,
with its own desktop and applications, including required suspend/resume support.

The current C17 prototype runs in a dedicated Fedora/UEFI test VM. It has tested
catalogue contracts, verified publication of a trusted test package, private-data
ownership and two confined system/user services. A start request is now journaled
before code reservation and can be withdrawn after the user service restarts,
without losing track of the reservation. A short-lived setup service now connects
that journal to private code/data mount views and independently checked teardown.
This service deliberately cannot execute application code.
Separate native execution primitives now drop credentials, seal inherited handles
and execute an exact held ELF image. An isolated enforcing test verifies the real
app-domain transition and current executable; integration into app starts is next.
A separate compatible test package is now published and activated alongside the
original, with its launch gate closed and all older package/history records retained.
It now has a distinct user binding and private data allocation as well.
Separate native execution transactions now record starter intent, binding,
preparation and arm without replaying a possibly completed external effect.
Their connection to a live executing starter remains in progress.

Actual app launch, document access and the desktop remain in progress. There is
no original Aetheri image, installer or working Aetheri application yet. The
[current state](brains/aetheri/STATE.md) records exact implementation and test limits.

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
glibc and selected maintained technical backends. NexaUI is the preferred toolkit
candidate; Physim should migrate to it when ready.

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
| src/catalogue | SQLite v2 identity, fixture, launch and setup transactions |
| src/linux | Bus/process checks, private mount views and internal image/one-way execution primitives |
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
