# Sources

Inspected 25 October 2026. Live branches can change. Candidate inventories do not
prove selected versions, current security status, complete license compliance or runtime acceptance.

## User requirements and project structure

- User's original request and first numbered answers: product intent, hardware,
  priorities, budgets, offline operation, documentation language and pending foundations.
  Summaries are in PROJECT and DECISIONS; no raw chat dump is stored.
- [SecondBrain concept](https://github.com/Lulus792/SecondBrain/blob/main/docs/KONZEPT.md),
  read blob a5262ad7299054db5d2ee65914d214ac1e832cc8.
- [Data contract](https://github.com/Lulus792/SecondBrain/blob/main/docs/DATENVERTRAG.md),
  read blob b7d4c7c590c9d72c75c8e0fed34d1c7214a20bb0.
- [Example metadata](https://github.com/Lulus792/SecondBrain/blob/main/brains/secondbrain/brain.json),
  blob 13dd09ffc01d1c7a9aab082b48f8acfcf4c14724, and
  [generator](https://github.com/Lulus792/SecondBrain/blob/main/secondbrain.py),
  blob 58e036a65d8f9e5545f5e850eace79bf9b285c25.

## Existing projects: read-only inspection

- [NexaUI](https://github.com/Lulus792/NexaUI): README, architecture, license, CMake,
  Linux dispatcher, text/accessibility documents. First inspection local HEAD
  5f7680f7d00a950f759d17d98520405901f9419d; local origin/main
  05e28bb16c1e60ac339cd9428ff2d3afbafc21f6. No fetch or equivalence claim.
- Physim local HEAD 179c74b631694554a4f939c5564053a5e291046a:
  README, tools/build.py, platform-validation notes, project description and
  selected-workspace design documents. C17, SDL3 and OpenGL requirements found.
  Existing design selections are references, not automatic Aetheri UI decisions.
- NexaCloud local README, NexaOS README, CMakeLists and dependencies.lock.json:
  distinguishes NexaOS, NexaCore service and NexaCloud Desktop. Existing selected
  NexaUI/OpenSSL/libblkid records are not adopted as Aetheri dependencies.
  HEAD lookup in the inspected directory failed; no immutable revision claimed.
- Host inventory: read-only PowerShell and WSL queries described in
  [testing](knowledge/testumgebung.md). Laptop hardware is user-reported.

## Technical explanations

- [Linux From Scratch](https://www.linuxfromscratch.org/lfs/view/stable/chapter01/how.html):
  toolchain, userspace and boot are separate tasks.
- [Wayland](https://wayland.freedesktop.org/docs/book/Introduction.html):
  clients render; compositor manages windows/display.
- [libinput](https://wayland.freedesktop.org/libinput/doc/latest/what-is-libinput.html):
  input devices and compositor-side integration; not a mandatory part of all Wayland systems.
- [Mesa](https://mesa3d.org/): userspace OpenGL/Vulkan implementations and driver families.
- [Clang](https://clang.llvm.org/): C-family frontend/tooling for LLVM.
- [glibc](https://sourceware.org/glibc/): C/POSIX runtime APIs including allocation,
  files, dynamic loading, name resolution and threads.
- [Rust ownership](https://doc.rust-lang.org/book/ch04-01-what-is-ownership.html):
  compiler-checked memory ownership rather than required runtime garbage collection.
- [CEF sandbox](https://github.com/chromiumembedded/cef/blob/master/docs/sandbox_setup.md):
  renderer/network/GPU process roles and platform-specific sandbox requirements.
- [Chromium build requirements](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md):
  at least 100 GB free disk specified for the documented Linux source build.
- [Kitty terminal graphics](https://sw.kovidgoyal.net/kitty/graphics-protocol/):
  transmitting/displaying images in a terminal; no implementation adopted.
- [PipeWire](https://pipewire.org/): multimedia graph/service purpose.
- [PDFium license](https://pdfium.googlesource.com/pdfium/+/refs/heads/main/LICENSE):
  actual file inspected; this is not a full transitive PDFium license audit.
- [iwlwifi](https://wireless.docs.kernel.org/en/latest/en/users/drivers/iwlwifi.html):
  Intel 8265 listed; no physical laptop test implied.
- [QEMU](https://www.qemu.org/docs/master/system/introduction.html):
  system emulation, WHPX/KVM/TCG and virtual devices.
- [Secure Boot](https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/oem-secure-boot):
  trusted boot-software signature checks; unrelated to making an online account mandatory.

## Licenses and documentation references

- [MIT primary text](https://opensource.org/license/mit).
- [Apache-2.0 primary text](https://www.apache.org/licenses/LICENSE-2.0).
- [GNU license explanations](https://www.gnu.org/licenses/license-list.html)
  and [GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.html).
- [MPL-2.0 primary text](https://www.mozilla.org/en-US/MPL/2.0/): covered files,
  distribution requirements and larger works.
- [Kernel licensing rules](https://docs.kernel.org/process/license-rules.html):
  GPL-2.0-only kernel and syscall exception for userspace.
- [SerenityOS README](https://github.com/SerenityOS/serenity/blob/master/README.md)
  and [Ladybird README](https://github.com/LadybirdBrowser/ladybird/blob/master/README.md):
  documentation examples and OS/browser scope, no copied code.

## Research limits

The initial GNU glibc fetch timed out; its Sourceware project page was subsequently
read successfully. The musl about page could not be fetched reliably. The libc
comparison remains an engineering proposal pending engine/app compatibility research.
The direct GNU GPL text fetch timed out; the license text/explanation was also
available through the inspected Choose a License page and GNU license overview.
BlueZ about-page fetch timed out and cryptsetup's GitLab page yielded no substantive
README content. MuPDF licensing path returned 404. These do not establish facts
about their current releases, footprints or license graphs. Audit selected
components at their authoritative versioned source before adoption.

## Latest decisions and focused research

- Subsequent user answers on 25 October 2026: approved foundations/MIT, complete
  removal and inventory, Physim migration, main-PC GPU, peripherals and shell.
- Same-day storage clarification: F: ready; protect Physim/NexaCloud migrations,
  directories and processes and confine our work to F:/Aetheri.
- [Kernel sleep states](https://docs.kernel.org/admin-guide/pm/sleep-states.html):
  suspend variants, reduced-power operation and distinction from hibernation.
- [NVIDIA open GPU kernel modules](https://github.com/NVIDIA/open-gpu-kernel-modules):
  kernel-module sources and matching userspace/firmware requirements; no driver selected.
- [Flatpak command reference](https://docs.flatpak.org/en/latest/flatpak-command-reference.html):
  distinction between package uninstall and app-data removal, reference only.
- [MIT license](https://opensource.org/license/mit): user selected MIT; LICENSE added.

## Architecture comparison — 25 October 2026

- [Architecture options](knowledge/architecture-options.md) links the primary sources
  read for this comparison: Wayland, wlroots/libweston, systemd/logind/sleep, s6,
  RAUC/sysupdate, Flatpak portals, Linux restrictions/cgroups, SQLite and NVIDIA.
- systemd-sysupdate's inspected live manual marks it experimental; no update engine
  is selected automatically as a consequence of recommending systemd.
- The systemd sleep documentation notes frozen user.slice during low-level hooks;
  app preparation must use the supported phase/interfaces rather than waiting on
  frozen user services.
- SQLite atomic commit covers database transactions, not a combined arbitrary
  filesystem/service/boot transaction. Our lifecycle state machine is original design.
- systemd.io/SYSUPDATE was unavailable and a Chromium sandboxing path returned 404.
  The reachable sysupdate manual and earlier CEF sandbox references were used;
  no claims are based on the unavailable paths.

## Architecture approval — 25 October 2026

The user approved the architecture recommendations and explicitly selected B,
wlroots, systemd and SQLite. This is the authority for the selected direction;
upstream documentation supplies technical references, not user authorization.
Exact releases and implementation acceptance remain unclaimed.

## Module and interface planning — 25 October 2026

The new [module plan](knowledge/modules-and-interfaces.md) includes primary sources
read for D-Bus/sd-bus, peer/process credentials, systemd transient operations and
logind, portal file selection, SQLite atomic/WAL behavior, wlroots session lock,
Linux file descriptors and local sockets. These justify available primitives,
not the correctness of the proposed Aetheri integration.

In particular, FileChooser returns selected URIs; a native-handle Aetheri SDK needs
an adapter rather than an invented claim about standard portal responses. SQLite
transactions do not encompass arbitrary filesystem/service/boot changes, and a
path/grant metadata change does not automatically revoke an already-open descriptor.

## Identity and lifecycle — 25 October 2026

- The user accepted the module/interface direction and requested the next contract.
- [Identity/lifecycle draft](knowledge/identity-and-lifecycle.md) records original
  proposed rules and distinguishes design examples from test evidence.
- [RFC 9562](https://www.rfc-editor.org/rfc/rfc9562.html): UUID format/generation reference;
  UUIDv4 local IDs are a recommendation, not authority/security tokens.
- [pidfd_open](https://man7.org/linux/man-pages/man2/pidfd_open.2.html): process references.
- [systemd service API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.systemd1.html):
  runtime unit/job/invocation observations, not Aetheri's durable product identity.
- [SQLite atomic commit](https://www.sqlite.org/atomiccommit.html): database boundary;
  no claim of cross-service/filesystem exactly-once behavior.
- User storage update: F: reported clear; free-space query only, approximately
  232.4 GiB at observation. No other project was modified.

## Concrete SQLite and API contracts — 25 October 2026

The user accepted the identity/lifecycle recommendations and requested the next
concrete schema/API discussion. New SQL/XML and policy details remain proposals.

- [SQLite STRICT](https://sqlite.org/stricttables.html), [foreign keys](https://sqlite.org/foreignkeys.html)
  and [partial indexes](https://sqlite.org/partialindex.html): checked table primitives.
- [Transactions](https://sqlite.org/lang_transaction.html), [triggers](https://sqlite.org/lang_createtrigger.html)
  and [PRAGMAs](https://sqlite.org/pragma.html): settings, constraints and commit boundaries.
  In particular, rollback-journal DELETE mode needs EXTRA rather than just FULL
  for the documented stronger last-commit power-loss durability.
- [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html):
  introspection/signatures/credentials, not Aetheri application authorization.
- [clock_gettime](https://man7.org/linux/man-pages/man2/clock_gettime.2.html):
  CLOCK_BOOTTIME counts suspension and is separate from adjustable wall time.
- Local execution of [validator](../../tools/validate_contracts.py) using Python
  3.12.14 / SQLite 3.53.1: 374 assertions passed, exit 0, in-memory constraints
  and XML structure only. Proposed SQLite runtime minimum 3.37 is a STRICT feature
  floor, not a pinned or security-approved dependency release.

## Filesystem, ownership and permissions — 25 October 2026

The user accepted the initial schema/API and requested the next storage/security
discussion. The [proposal](knowledge/filesystem-and-permissions.md) links all
inspected primary sources and distinguishes them from original Aetheri policy.

- UAPI/systemd filesystem hierarchy and XDG base directories: technical conventions,
  read-only/mutable paths and configuration/data/state/cache/runtime roles.
- SELinuxProject type enforcement, security contexts and MAC/MCS: label/policy
  separation; no assumption that an existing policy implements our account/app rules.
  The direct GitHub type-enforcement page initially lacked useful body; indexed
  official project text supplied the relevant material.
- Kernel AppArmor, Landlock and Linux namespaces: alternatives and supplementary
  confinement. Landlock descriptor inheritance and unsupported-feature limits
  prevent treating it as a complete standalone Aetheri security contract.
- systemd execution/resource-control manuals: supervision/sandboxing and privilege
  limits. In particular, read-only directory views do not block socket IPC, and
  SELinuxContext is ignored when SELinux is disabled.
- openat2/unlink/chmod and portal FileChooser: bounded resolution, open-handle
  lifetime and URI selection. st_dev equality does not rule out bind mounts.
- Kernel ext4 and cryptsetup manual: storage/encryption foundations only.
  No filesystem benchmark, encryption setup or hardware test was performed.

Current work validates documentation only. No Linux confinement, deletion,
permission, label or power-loss checks were executed for Aetheri this turn.
