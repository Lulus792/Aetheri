# System scope

Status: explanation, not an approved architecture.

Linux supplies kernel facilities such as scheduling, memory management, network
primitives and many device drivers. A daily-use desktop also needs userspace.

| Area | Required work |
| --- | --- |
| Build and boot | Toolchain, kernel configuration, UEFI path, initial filesystem and reproducible images |
| Base userspace | libc, PID 1, service supervision, device handling, users, permissions and diagnostics |
| Graphics/input | Userspace drivers, display composition, windows, keyboard/pointer, clipboard, fonts and input methods |
| Desktop | Session, lock screen, launcher, dock, global search, notifications and settings |
| Services | WLAN, audio, Bluetooth, power management, time, removable storage and permissions |
| Applications | UI, behavior, formats, persistence, failure handling, isolation and help |
| Maintenance | Packages/images, signed updates, recovery, installer and backups |

The first daily-use workflows are browser, terminal/editor/document viewing,
Physim and multiple monitors. Original UI and efficient operation are priorities.
An existing web engine is likely; its adoption is not yet decided.

A terminal emulator and a shell are separate components. The editor is another
program or shared component, even if shown within the terminal. Shared PDF/image
rendering can support both terminal viewing and a standalone viewer.

The browser engine includes HTML/CSS, layout, JavaScript, network/TLS, media,
storage and isolation. Building a modern engine is a large independent project.
The system can have an original browser UI while retaining an external engine.

Every planned app needs tasks, navigation, focus, keyboard bindings, empty/loading/
error states, scaling and accessible semantics. Shared tokens and controls form
the design system. Prepare alternative visual directions later for user selection;
do not treat the macOS influence as an approved screen design.

See [technology options](technology-options.md) for the original-code boundary
and [sources](../SOURCES.md) for the LFS, Wayland and browser references.
