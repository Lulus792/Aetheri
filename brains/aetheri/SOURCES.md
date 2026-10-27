# Sources

Initial inspection: 25 October 2026; later entries are dated individually. Live
branches can change. Candidate inventories do not prove selected versions,
current security status, complete license compliance or runtime acceptance.

## Private code reservation — 27 October 2026

- Original [catalogue API](../../src/catalogue/reservation.h),
  [owner implementation](../../src/catalogue/store.c),
  [typed codec](../../src/authority/reservation.c) and service callbacks in
  [main.c](../../src/authority/main.c). Uses existing reviewed SQLite/OpenSSL/
  sd-bus/held-object primitives; no new dependency or policy permission.
- [Independent canonical plan/request encoder](../../tools/test_reservation_native.py),
  [native tests](../../tests/test_reservation.c) and
  [independent wire fixtures](../../tests/test_reservation_wire.c): statement/
  commit faults, fresh-process exits, exact retry/withdrawal, fence changes,
  restart uncertainty, bounds and unchanged failure outputs.
- [Enforcing controller](../../tools/p0_services.py): actual current-store startup
  verification, registered broker RPC, canonical digest reconstruction from actual
  committed rows, exact RootId/revision reuse, terminal cause/released lease,
  unchanged user database, absent app unit and stale-profile denial. Source
  inventory/labels remain sealed; this does not establish app dispatch or readiness.
- Integration found the bounded small-document helper's 65536-byte maximum was
  inappropriate for manifest metadata's larger independent bound. Admission now
  opens/checks the exact regular manifest directly with no-follow openat2 and the
  manifest limit; document transport's bound is unchanged. No policy relaxation.

## First app data enrollment — 27 October 2026

- Original [native catalogue contract](../../src/catalogue/data.h),
  [implementation](../../src/catalogue/store.c) and protected administrative mode
  in [the authority](../../src/authority/main.c). Reuse the previously tested
  AETHALC1/held-root primitives; no dependency, policy permission, SQL schema or
  public protocol changed.
- [Independent Python config encoder](../../tools/test_data_native.py) and
  [native tests](../../tests/test_data.c): malformed bytes, closed-gate transaction,
  statement/COMMIT faults, process exits before/after COMMIT, no reattachment or
  resurrection and overflow. Controlled database future states are not installed
  production lifecycle APIs.
- [Carrier controller](../../tools/p0_services.py), data-prepare/data-check and
  [same-UID app probe](../../tests/probe_service.c): actual preallocated object
  binding, native publication/enrollment, exact retries, cross-boot reopening,
  separate old storage fixture and enforced metadata/payload denials. Enforcing
  policy, NNP and empty capabilities remain in force. This proves the fixed P0
  fixture boundary, not general dynamic allocation or production volume trust.

## Typed installation observation — 27 October 2026

- [systemd v259 message construction](https://raw.githubusercontent.com/systemd/systemd/v259/man/sd_bus_message_new.xml)
  and [message sealing](https://raw.githubusercontent.com/systemd/systemd/v259/man/sd_bus_message_seal.xml):
  primary API reference for connected-bus message construction, ownership and
  immutable local test bodies. Installed carrier systemd remains 259.10.
- Original implementation: [catalogue projection](../../src/catalogue/projection.h),
  [wire codec](../../src/authority/projection.c), [service callbacks](../../src/authority/main.c),
  [native catalogue tests](../../tests/test_projection.c),
  [wire tests](../../tests/test_projection_wire.c) and
  [enforcing controller](../../tools/p0_services.py). Test mocks establish bounded
  failures; real VM evidence establishes the distinct transport/owner boundary.
- Actual carrier evidence: UID-0 user-bus authentication rejection; explicit
  self-domain messaging denial; systemd USER_AVC status denial for GetUnitByPIDFD.
  Narrow policy adjustments preserve enforced confinement and empty capabilities.
  No upstream implementation code copied or dependency version changed.

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

## Launch, file access, revocation and removal — 25 October 2026

The user accepted filesystem/ownership/enforcement recommendations, including
ext4/LUKS2/SELinux foundations, and requested the concrete combined flow.
The [protocol proposal](knowledge/launch-access-and-removal.md) links the primary
references inspected for this discussion.

- systemd service API: transient units/jobs and identity observations; a returned
  job path is not a target-app readiness/launch completion proof.
- systemd kill manual: control-group stopping and automatic SIGKILL after timeout.
  Graceful interactive removal needs an explicit policy preventing silent escalation.
- Kernel cgroup v2: populated observes the recursive process subtree; external
  delegated descriptors still need their own holder accounting.
- Linux open/unlink/openat2/fsync: descriptor lifetime, bounded resolution and
  required directory durability, not universal revocation or filesystem/SQL atomicity.
- D-Bus credentials and XDG FileChooser: authenticated observations versus user
  selection; URI strings need adaptation into checked local resource grants.

These sources support primitives. The reservation/ready/dispatch protocol, native
grant/edit model, limitations with direct writers and proposed record/API extensions
are original Aetheri recommendations. No live Linux/security/protocol tests ran.

## Minimal extension and first prototype — 26 October 2026

The user accepted the lifecycle protocol and requested minimal DB/API additions
and the first Linux prototype's scope.

- [SQLite ALTER TABLE](https://www.sqlite.org/lang_altertable.html):
  supported schema-change procedure for constraint/table rebuilds.
- [SQLite PRAGMAs](https://www.sqlite.org/pragma.html):
  user_version is application-managed, independent from SQLite's own schema cookie.
- [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html):
  typed interfaces and capability/version handling.
- [QEMU introduction](https://www.qemu.org/docs/master/system/introduction.html):
  WHPX and TCG execution modes.
- [Fedora Server download](https://fedoraproject.org/server/download/):
  Fedora Server 44 x86_64 media and verification workflow currently offered.
  This is a proposed development carrier, not a downloaded/verified image or final base.
- Fedora Quick Docs SELinux getting-started fetch returned an access-denied
  challenge; no guest-enforcement conclusion relies on that page.
- Read-only PowerShell observation: QEMU tools absent from PATH, WSL Alpine-Dev,
  F: approximately 232.4 GiB free. No WHPX capability or tool-installation claim.

The [P0 contract/scope proposal](knowledge/first-linux-prototype.md) is original
Aetheri design. New migration, API, VM and runtime tests have not been executed.

## Versioned contract implementation — 26 October 2026

- [SQLite rebuild procedure](https://sqlite.org/lang_altertable.html):
  foreign-key sequencing, replacement tables and validation before commit.
  The first www.sqlite.org fetch timed out; the sqlite.org copy was read.
- [SQLite backup API](https://www.sqlite.org/backup.html):
  consistent snapshots before migration.
- [Python 3.12 sqlite3](https://docs.python.org/3.12/library/sqlite3.html):
  backup/transaction behavior and executescript's implicit-commit caveat.
- [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html):
  static introspection/type conventions; XML annotations do not implement security.

Executed tools/test_contracts.py using Python 3.12.14 / SQLite 3.53.1:
23 named tests passed, exit 0. Includes 20 precommit-fault subcases, six actual
child-process terminations/reopens and engine SQLITE_FULL through a page limit.
This is development-host database evidence, not real full-volume/power-loss or
Linux service/enforcement acceptance. See [contract guide](../../spec/README.md).

## P0 runtime formats and carrier preparation — 26 October 2026

The user asked to begin the next implementation slice, then clarified the current
host as the main-PC target and added future AM5 portability. These are user
requirements; read-only Windows observations are recorded separately in
[test environment](knowledge/testumgebung.md).

- [Runtime specification](../../spec/runtime.md), development codec and frozen
  vectors: original Aetheri design. Three vector byte strings were constructed
  without importing the encoder under test. No C17 service/decoder is implied.
- [D-Bus specification](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-get-connection-credentials),
  [sd-bus sender credentials](https://www.freedesktop.org/software/systemd/man/latest/sd_bus_query_sender_creds.html)
  and [credential fields](https://www.freedesktop.org/software/systemd/man/latest/sd_bus_creds_get_pid.html):
  process pinning, labels, augmentation and unsafe executable-path assumptions.
  The separate augmented-mask manual URL was unavailable.
- [openat2](https://man7.org/linux/man-pages/man2/openat2.2.html) and
  [systemd execution](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html):
  containment/setup primitives, not an Aetheri enforcement result.
- [QEMU downloads](https://www.qemu.org/download/#windows),
  [Windows provider](https://qemu.weilnetz.de/w64/) and
  [v11.1.0 installer source](https://github.com/qemu/qemu/blob/v11.1.0/qemu.nsi):
  Windows build 2026-08-11/product 11.1.0, provider checksum, privileged installer
  behavior. Downloaded bytes match SHA-512; Windows reports an expired signing
  certificate. No installer/QEMU execution occurred.
- [Fedora Server download](https://fedoraproject.org/server/download/) and
  [signing fingerprints](https://fedoraproject.org/security/): release 44-1.7
  net installer/checksum. Local GPG VALIDSIG matched full fingerprint
  36F612DCF27F7D1A48A835E4DBFCF71C6D9F90A6. This authenticates the checksum list;
  the separate media digest must also match before use.
- [Microsoft connector enum](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmdt/ne-d3dkmdt-_d3dkmdt_video_output_technology):
  observed connection codes 5/10 mean HDMI/external DisplayPort. The earlier
  wingdi URL was unavailable; the driver-header page supplied the definition.
- [WHvGetCapability](https://learn.microsoft.com/en-us/virtualization/api/hypervisor-platform/funcs/whvgetcapability):
  read-only capability query returned S_OK and HypervisorPresent=true. No partition
  or virtual processor was created; no QEMU/WHPX guest execution claim.

Executed 14 format and 7 cache tests, exit 0; reran 23 catalogue/API tests, exit 0.
Total 44 named host tests. Native Linux, actual bus credentials, policy, filesystem
containment, process recovery, performance and hardware remain untested for Aetheri.

## Carrier execution and first C17 core — 26 October 2026

User goal: implement the full agreed project. Current changes/evidence are an
installed development carrier and native prerequisites, not final OS acceptance.

- [Host extraction](../../tools/provision_windows.py): actual MSI read-only stream
  access/native cabinet extraction and hash-verified NSIS data extraction succeeded.
  [7-Zip release](https://github.com/ip7z/7zip/releases/tag/26.05) supplied asset
  SHA-256; retained [license](https://www.7-zip.org/license.txt). No installer actions.
- Microsoft [MSI database](https://learn.microsoft.com/en-us/windows/win32/api/msiquery/nf-msiquery-msiopendatabasew)
  and [record stream](https://learn.microsoft.com/en-us/windows/win32/api/msiquery/nf-msiquery-msirecordreadstream)
  documentation: container reads, not installation authorization.
- [Pykickstart](https://pykickstart.readthedocs.io/en/latest/kickstart-docs.html):
  source for the bounded guest installation template. The Fedora f44 install-guide
  URL was unavailable; no conclusion depends on it.
- [EDK2 PlatformPei](https://github.com/tianocore/edk2/blob/master/OvmfPkg/PlatformPei/Platform.c)
  and [Intel MSR definitions](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Register/Intel/ArchitecturalMsr.h):
  references for the observed early feature-control fault. Bundled firmware bytes
  are separately recorded by extraction hashes; current upstream source is not
  asserted identical to those prebuilt bytes.
- [Windows AF_UNIX](https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/):
  socket paths and host filesystem access. Native Winsock/QEMU exchange succeeded
  despite this Python build lacking socket.AF_UNIX. TCP control was removed after
  an actual guest 10.0.2.2 reachability probe; old port now refuses connections.
- [QEMU pipe source](https://github.com/qemu/qemu/blob/v11.1.0/chardev/char-pipe.c)
  and [Windows backend](https://github.com/qemu/qemu/blob/v11.1.0/chardev/char-win.c):
  examined before choosing the reconnecting AF_UNIX socket path instead.
- Native [owner](../../src/core/owner.c), [storage](../../src/core/storage.c) and
  [tests](../../tests/test_core.c) are original code. openat2 flags were rechecked
  against the [Linux manual](https://man7.org/linux/man-pages/man2/openat2.2.html).
  Actual guest syscall/namespace cases pass, not just static configuration checks.

Guest reports retained locally: EFI/ext4, systemd running/PID 1, enforcing policy,
cgroup v2, private mount namespaces and ProcessFD/label bus capabilities. Native
tests pass in Debug ASan/UBSan and Release; catalogue/format tests also pass on
Python 3.14.3/SQLite 3.51.2. Exact versions and limitations are in
[test environment](knowledge/testumgebung.md). Aetheri authority/app-domain,
recovery, desktop, performance and physical hardware acceptance remain pending.

## Native connection credentials — 26 October 2026

- [systemd v259 public header](https://github.com/systemd/systemd/blob/v259/src/systemd/sd-bus.h),
  [credential implementation](https://github.com/systemd/systemd/blob/v259/src/libsystemd/sd-bus/bus-creds.c)
  and [bus control](https://github.com/systemd/systemd/blob/v259/src/libsystemd/sd-bus/bus-control.c):
  reviewed ProcessFD ownership, augmented masks and bus-supplied credential handling.
  These are API references, not an assertion that the Fedora build is an identical tag.
- Actual Fedora package metadata/transaction: systemd-devel 259.10-1
  (LGPL-2.1-or-later AND MIT), sqlite-devel 3.51.2-2 (blessing) and
  libselinux-devel 3.11-2 (LicenseRef-Fedora-Public-Domain). Signature verification
  enabled, eight installed/twelve upgraded, about 16 MiB downloaded/7 MiB extra.
- [Peer integration test](../../tests/test_peer.c): real user-bus sender pin,
  matching known child credentials, exit, output preservation and balanced FDs.
  Negative provenance cases use explicit test-only wrapping. Four native tests
  pass in both instrumented Debug and Release after offline reboot.

Both bus roles still require actual Aetheri domain/registry/invocation authorization.
No service or app policy was installed through this library/test addition.

## Enforcing fixture policy — 26 October 2026

- SELinux Project [CIL types, 3.11](https://github.com/SELinuxProject/selinux/blob/3.11/secilc/docs/cil_type_statements.md)
  and [access-vector rules](https://github.com/SELinuxProject/selinux/blob/master/secilc/docs/cil_access_vector_rules.md):
  type/attribute and explicit allow/neverallow syntax. The role-page fetch at 3.11
  was unavailable; no assertion relies on that missing page.
- Fedora [MCS policy](https://github.com/fedora-selinux/selinux-policy/blob/rawhide/policy/mcs)
  and [domain interfaces](https://github.com/fedora-selinux/selinux-policy/blob/rawhide/policy/modules/kernel/domain.if):
  reviewed attribute/category semantics, not claimed identical to installed policy 43.3.
- Executed [original fixture module](../../policy/p0-fixture.cil),
  [C17 probe](../../tests/probe_policy.c) and [controller](../../tools/test_linux_policy.py)
  on the recorded carrier: two disjoint-category/same-UID positives and required
  negatives pass, with enforcing AVC evidence and exact new-file labels. The final
  probe also verifies actual empty capability sets and NoNewPrivileges.

Initial mounton/entry-execute denials stopped target execution. Reviewed explicit
permissions were added to init_t for only the new types; no audit2allow-generated
blanket policy, permissive mode or app privilege grant. Failed owned fixtures
were verified against loaded CIL and settled units before cleanup/retry.
Production policy, category reuse, IPC/ptrace/FD-transfer enforcement and the full
Aetheri lifecycle remain unaccepted. The temporary module/root were removed.
Existing carrier tools used policycoreutils/libsemanage 3.10-1 and libsepol 3.11-1;
no policy-development package was installed for this CIL proof.

## Native catalogue startup — 26 October 2026

- SQLite [opening](https://sqlite.org/c3ref/open.html) and
  [connection configuration](https://sqlite.org/c3ref/c_dbconfig_defensive.html):
  normal Unix VFS, explicit create/open and checked defensive settings.
- SQLite [corruption/locking cautions](https://sqlite.org/howtocorrupt.html):
  do not independently close a descriptor to an inode while SQLite owns POSIX
  locks; do not reuse connections across fork or hand-delete a hot journal.
- SQLite [file controls](https://sqlite.org/c3ref/c_fcntl_begin_atomic_write.html)
  and [atomic commit](https://sqlite.org/atomiccommit.html): moved-file observation,
  transaction/acknowledgement boundaries and hardware assumptions.
- CMake [FindSQLite3, 4.3](https://cmake.org/cmake/help/v4.3/module/FindSQLite3.html):
  current imported target and documented older-tool alias. Only recorded 4.3.0
  actually built here; no older-tool execution is claimed.
- Executed [native implementation](../../src/catalogue/store.c),
  [failure/process tests](../../tests/test_catalogue.c) and
  [migration interop](../../tools/test_catalogue_native.py): eight native CTest
  cases pass with/without sanitizers, both catalogue kinds; populated migrations
  retain rows/file bytes. Test-only commit/flush fault injection is distinguished
  from actual SQLite page-limit FULL and real process exits.

The existing development canonicalizer was made literal-preserving with scoped
SQL parser/real schema-drift regressions. The expanded 26-test suite passes on
guest Python 3.14.3/SQLite 3.51.2. No production authority/registry/MAC readiness,
native migration or physical flush/power-loss evidence follows from these tests.

## Confined protocol services — 26 October 2026

- [D-Bus daemon policy](https://dbus.freedesktop.org/doc/dbus-daemon.1.html):
  SELinux send_msg/acquire_svc, explicit own-name associations and min_fds on
  destination/type rules. The actual carrier uses dbus-broker 37; live app and
  unowned-name/FD negatives verify the relevant behavior there.
- [SELinux reference-policy D-Bus interfaces](https://github.com/SELinuxProject/refpolicy/blob/master/policy/modules/services/dbus.if):
  scoped messaging, connection and fd-use patterns. This is a reference, not a
  claim that Fedora 43.3 policy is identical. Fedora rawhide dbus.if URLs returned
  404 and are not evidence.
- [systemd execution](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html):
  ProtectHome/namespace bind behavior and RestrictSUIDSGID/openat2 interaction.
  The latest manual currently describes 262; installed integration is 259.10.
  [v259 seccomp implementation](https://github.com/systemd/systemd/blob/v259/src/shared/seccomp-util.c)
  and actual ENOSYS/capability/namespace observations delimit the selected carrier.
- [sd-bus reads](https://www.freedesktop.org/software/systemd/man/latest/sd_bus_message_read.html):
  typed borrowed message values and descriptor lifetime. Local installed headers
  were also checked for method-vtable argument-name ordering and macros.
- Executed original [C17 authority](../../src/authority/main.c),
  [client](../../tests/probe_service.c), [CIL](../../policy/p0-services.cil) and
  [controller](../../tools/p0_services.py): both confined discovery endpoints,
  method-shape/real-boot checks, app positives/negatives, actual restarts and
  removal/recreation of the inspected empty bootstrap. These are partial P0
  results, not a complete app launcher, permissions or removal implementation.

Enforcing AVCs identified missing supervisor mounton/entry/state/notify setup
permissions. The final socket mount uses a dedicated precreated placeholder;
no permission to create arbitrary user_tmp_t files was added. The controller
initially requested an unsupported systemctl JSON option; the final drain check
uses the actual typed systemd ListJobs reply. Kernel proc attr/current observations
include a terminating NUL, handled explicitly by the controller's comparison.
Raw receipts/logs stay ignored; published notes omit host-private paths and IDs.

## Process and service invocation evidence — 26 October 2026

- [systemd manager API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.systemd1.html):
  GetUnitByPIDFD returns object/name/invocation; ControlGroup belongs to Service.
  Latest manual is 262, while actual calls were verified on installed 259.10.
- [pidfd login observations](https://www.freedesktop.org/software/systemd/man/latest/sd_pid_get_unit.html)
  and [v259 implementation](https://github.com/systemd/systemd/blob/v259/src/libsystemd/sd-login/sd-login.c):
  descriptor-based cgroup observations and post-read process verification. The
  Aetheri observer also repeats/checks its own kernel/manager context.
- [Linux proc attributes](https://man7.org/linux/man-pages/man5/proc_pid_attr.5.html),
  [v6.19 proc implementation](https://github.com/torvalds/linux/blob/v6.19/fs/proc/base.c)
  and [SELinux hooks](https://github.com/torvalds/linux/blob/v6.19/security/selinux/hooks.c):
  current process attributes and access controls are distinct from connection labels.
- [v6.19 cgroup identity](https://github.com/torvalds/linux/blob/v6.19/include/linux/cgroup.h):
  kernel cgroup identity uses its kernfs node ID. Actual PIDFD_GET_INFO cgroup IDs
  agree with opened cgroup-v2 directory inodes in this carrier; injected disagreement
  is rejected. No claim of identical behavior on untested kernels/filesystem views.
- Installed glibc 2.43 sys/pidfd.h and kernel 7.2.4 UAPI headers were read directly.
  Their pidfd_info/mask/ioctl definitions delimit the actual ABI tested against
  running kernel 6.19.10. Including Linux pidfd.h alongside libc fcntl.h caused
  structure redefinition; the implementation uses glibc's provided wrapper instead.
  The attempted PIDFD_GET_INFO manual and remote glibc-tag URLs were unavailable;
  pidfs.c retrieval was rate-limited. No claim depends on those missing pages.
- Executed [observer](../../src/linux/process.c), [native test](../../tests/test_process.c)
  and updated [authority](../../src/authority/main.c): kernel/manager positives,
  seven stale expected fields, actual same-name restart, departed sender, repeated
  FD balance and test-only missing/changed/unsupported observations. The real
  confined System1 also passes its self-context check without added capabilities.

Initial USER_AVC system/status and cgroup filesystem/getattr denials were resolved
with read-side permissions only. Full principal registration, app-domain peer
observations, cross-run FD/ptrace enforcement and target-exec proof remain open.

## Broker enrollment and lifetime — 26 October 2026

- [D-Bus name ownership](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-name-owner-changed):
  authenticated daemon NameOwnerChanged fields, unique-name lifetime and
  NameHasOwner. The cache subscribes to one exact sender before its presence fence.
- [sd-bus match rules](https://www.freedesktop.org/software/systemd/man/latest/sd_bus_add_match.html):
  synchronous match installation, callback/slot lifetime and exact signal filters.
  The separate sd_bus_match_signal manual URL was unavailable; this shared manual
  was read instead. Actual integration uses installed systemd 259.10, not the
  latest manual's 262 runtime.
- [sd-event I/O sources](https://www.freedesktop.org/software/systemd/man/latest/sd_event_add_io.html):
  epoll readiness, source disable/unref and explicit descriptor ownership. Broker
  clear disables/unrefs the source before closing its retained ProcessFD.
- Executed [identity cache](../../src/authority/broker.c),
  [cache tests](../../tests/test_broker.c), [negative caller](../../tests/probe_broker.c)
  and [controller broker-check](../../tools/p0_services.py): real enforcing enrollment,
  copied-ID rejection with working transport, departed pin 12→11, fresh user and
  root-only restart with user PID/runtime preserved. Cache context is injected in
  the isolated cache test; its independent daemon-name and pidfd events are real.

The internal XML is original Aetheri bootstrap design, not a D-Bus-specified app
authorization protocol. Existing public v1/XML and SQL remain unchanged. The
single UID-1000 eligible slot is not general account enrollment, volume identity,
app principal registration, target-exec proof or full lifecycle acceptance.

## Durable account enrollment — 26 October 2026

- [SQLite transactions](https://www.sqlite.org/lang_transaction.html): IMMEDIATE
  writer acquisition, explicit commit/rollback and error cases. Native enrollment
  inspects binding after acquiring its write transaction and commits row/revision
  together; an error does not imply that a commit never happened.
- [SQLite parameter binding](https://www.sqlite.org/c3ref/bind_blob.html): typed
  BLOB/int64 inputs and SQLITE_TRANSIENT lifetimes. UID conversion preserves the
  full accepted unsigned range without treating identity as SQL text.
- [Native store](../../src/catalogue/store.c), [tests](../../tests/test_catalogue.c)
  and [controller](../../tools/p0_services.py): eleven native tests pass in Debug
  ASan/UBSan and Release; actual fixed-account missing-row startup denial, explicit
  enrollment/idempotent database hash, broker restart and bootstrap recreation pass.
  Twenty host guards cover administrative receipt roles and preservation of other
  account records. Actual integration uses the unchanged pinned carrier SDKs.

Retirement/UID-reuse cases mutate only closed native-test fixture catalogues.
They prove API behavior, not production Linux-user retirement, NSS synchronization
or access to old app data. No production account lifecycle is claimed.

## Native fixture metadata and hash-library proposal — 26 October 2026

- [Frozen fixture vector](../../spec/vectors/runtime-v1.json), accepted
  [byte contract](../../spec/runtime.md#fixture-manifest-v1), original
  [C17 decoder](../../src/core/fixture.c), [native reader](../../tests/test_fixture.c)
  and [interop driver](../../tools/test_fixture_native.py): 8 valid metadata
  projections, 81 malformed records and every frozen-vector truncation pass in
  Debug ASan/UBSan and Release. Full native suites pass 12/12 in 25.99/21.62 s.
  This proves metadata behavior, not payload verification or publication.
- [RFC 3629](https://www.rfc-editor.org/info/rfc3629/): valid UTF-8 scalar sequences,
  shortest encoding, surrogate exclusion and upper code-point boundary. Aetheri's
  additional C0/C1 exclusion comes from its own accepted text contract.
- [OpenSSL releases/licensing](https://openssl-library.org/source/): current 3.5
  LTS release 3.5.9, dated 29 September 2026, branch support to 8 April 2030,
  Apache-2.0 license. [3.5 digest API](https://docs.openssl.org/3.5/man3/EVP_DigestInit/):
  maintained EVP digest operations, failure returns and context lifetime. These
  support a proposed integration, not a completed dependency/security audit.
- Actual offline guest rpm/pkg-config inspection: openssl-libs 3.5.5-1.fc44 is
  installed; openssl-devel, the openssl executable and libcrypto metadata are absent.
  The user was offered a library choice. No answer, direct dependency, package
  transaction or version change is recorded in this step.

## Runtime root identity and retained storage — 26 October 2026

- [Linux statx](https://man7.org/linux/man-pages/man2/statx.2.html): required-mask
  semantics, descriptor lookup with AT_EMPTY_PATH and STATX_MNT_ID_UNIQUE since
  Linux 6.8. The unique mount ID is not recycled while the system runs; the older
  mountinfo ID is a different field interpretation. Runtime snapshots are not
  persistent volume identities.
- [Linux fstatfs](https://man7.org/linux/man-pages/man2/fstatfs.2.html): held-FD
  filesystem type/ID observations and filesystem-specific/undefined fields.
  A filesystem ID alone is not a genuine-volume or clone-authentication proof.
- Original [root observer](../../src/core/root.c),
  [storage refs](../../src/core/storage.c), [tests](../../tests/test_storage_ref.c)
  and [catalogue integration](../../src/catalogue/store.c): actual same-inode/device
  bind substitutions produce different unique mount IDs and are rejected; copied
  owner headers cannot replace expected root identity. Native fault tests cover
  missing/legacy fields, unsupported statx and changing directory metadata.
- Fifteen native tests pass Debug ASan/UBSan (27.91 s) and Release (25.02 s).
  Additional expected-root mismatch cases pass in the focused storage repeats
  (0.08/0.04 s totals). These private storage fixtures are unconfined; they do not
  replace required future same-UID MAC admission and document-grant integration.
- Actual enforcing service startup logged filesystem/getattr denial on fs_t;
  [P0 policy](../../policy/p0-services.cil) adds only that authority read permission.
  Broker replacement/root-only restart, full restart and confined fresh bootstrap
  initialization pass. A 5.033 s idle sample has one task each and zero reported
  CPU/context-switch deltas; PSS 2252/2292 KiB, RSS 6764/6676 KiB,
  MemoryCurrent 1646592/1667072 bytes. Services then stopped normally.

No SDK/package or crypto adoption occurred. No physical hardware, durable volume
registry, independent allocation record, clone detection, public read/revoke or
private purge acceptance is inferred from these runtime observations.

## Immutable allocation witness and cross-boot test — 26 October 2026

- [Linux O_TMPFILE](https://man7.org/linux/man-pages/man2/open.2.html): unnamed
  inode lifetime, supported filesystems and linking an owned FD through proc without
  CAP_DAC_READ_SEARCH. No capability was added and no named-temp fallback is used.
- [Linux linkat](https://man7.org/linux/man-pages/man2/link.2.html): create-only
  destination semantics, existing-name collision and symlink-follow source flag.
- [Linux fsync](https://man7.org/linux/man-pages/man2/fsync.2.html): file persistence
  and the separate directory flush required for a durable name. Failed/lost replies
  do not justify assuming that an effect never happened.
- Original [allocation codec/IO](../../src/core/allocation.c), independent
  [vector](../../spec/vectors/allocation-v1.json), [native tests](../../tests/test_allocation.c)
  and [interop driver](../../tools/test_allocation_native.py). Sixteen native tests
  pass Debug ASan/UBSan (28.27 s) and Release (25.39 s); 22 host guards pass (0.194 s).
  The new case exercises real immutable publication/reopening/concurrent retries,
  copied-root rejection, all owner fields and pre/post-link IO/process-exit faults.
  A final close-error acknowledgement case passes in focused repeats (2.14/1.99 s)
  and preserves the committed record. Actual empty-bootstrap cleanup refusal leaves
  the fixture available for a successful unchanged native reopen.
- Actual [authority administrative actions](../../src/authority/main.c),
  [protected setup/controller](../../tools/p0_services.py) and
  [same-UID app probe](../../tests/probe_service.c): enforcing cap-zero user-domain
  publish/verify passes; ordinary same-UID read works while app metadata reads with
  the known StorageRef are denied. Different unit invocations reopen the same record.
- Actual guest restart: configured -no-reboot produced QEMU exit 0, verified PID
  absence preceded offline boot of the same disk, and a new kernel BootId preceded
  cross_boot_reopen=true with unchanged record hash/inode. Broker checks pass again.

Synthetic fixture machine/volume IDs and inode/birth witnesses are not genuine
volume enrollment, whole-filesystem clone authentication, an allocation journal,
app admission, public document access or private purge acceptance. The private
fixture is retained; empty-bootstrap cleanup refuses it by design. SDK/package
versions are unchanged. User evidence now accepts OpenSSL 3.5 LTS and delegates
well-supported technical choices; exact package adoption is still unimplemented.

## OpenSSL 3.5 LTS adoption — 26 October 2026

- [Official releases](https://openssl-library.org/source/): 3.5.9, released
  29 September 2026, belongs to the accepted 3.5 LTS branch, supported through
  8 April 2030. [Security advisories](https://openssl-library.org/news/vulnerabilities/)
  include fixes in 3.5.9, including CVE-2026-35189. Its affected X.509/CRL path
  is not used by this SHA-256 wrapper; the maintained patch is still preferred.
  This dated review is not a claim of universal vulnerability absence.
- Actual DNF cached metadata resolved 1:3.5.9-1.fc44; the web Fedora package page
  still showed 3.5.5. Reviewed both repository and local-RPM dry runs. Downloaded
  exact libs/devel x86_64 RPMs from the official Fedora HTTPS update mirror, not
  the HTTP mirror returned by repoquery. [Input lock](../../spec/vm-inputs.json)
  records actual byte counts/SHA-256. rpmkeys verified both OpenPGP/header/payload
  signatures/digests against full existing Fedora 44 fingerprint
  36F612DCF27F7D1A48A835E4DBFCF71C6D9F90A6. A locally calculated hash alone was
  not treated as publisher authentication.
- Actual RPM headers identify source openssl-3.5.9-1.fc44.src.rpm, Apache-2.0,
  installed sizes 9654845 bytes for libs and 4736693 for devel. Package scriptlet
  listings were empty; ordinary package-manager triggers remain possible. Existing
  dependencies satisfy glibc/zlib/crypto-policies/ca-certificates/pkg-config needs.
  Offline DNF with localpkg_gpgcheck=True installed exactly the reviewed pair:
  one upgrade, one addition, 0 bytes downloaded and about 5 MiB net extra. No
  added daemon. Installed RPM verification is clean; libcrypto pkg-config is 3.5.9.
  The package retains /usr/share/licenses/openssl-libs/LICENSE.txt (Apache 2.0).
- [EVP digest API](https://docs.openssl.org/3.5/man3/EVP_DigestInit/),
  [library contexts](https://docs.openssl.org/3.5/man3/OSSL_LIB_CTX/) and
  [default provider](https://docs.openssl.org/3.5/man7/OSSL_PROVIDER-default/):
  explicit provider-constrained SHA256 fetch, checked lifecycle and call-local
  cleanup. Original [wrapper](../../src/core/digest.c) and
  [fixture checks](../../src/core/fixture.c) use these APIs without default-context
  changes/global cleanup. They do not implement original cryptography or TLS.
- Actual ELF dynamic entries: test_digest needs libcrypto.so.3 and libc.so.6;
  authority needs libcrypto, sqlite3, selinux, systemd and libc. No direct libssl
  dependency was added. This is not a claim about every transitive carrier library.
- [Tests](../../tests/test_digest.c) and [independent vectors](../../tools/test_digest_native.py):
  native Debug ASan/UBSan 17/17 (30.46 s), Release 17/17 (25.14 s); host VM/cache
  guards 29/29 (0.272 s); all five download-cache hashes pass. Actual enforcing
  cap-zero user-domain allocation publish/retry/verify reports native_sha256=true
  and preserves the existing witness. No SELinux permission was added.
- After the crypto update, actual graceful poweroff, QEMU exit 0 and verified PID
  absence preceded offline boot of the same disk. All nine carrier prerequisites
  pass. Added runtime-version rejection cases pass in focused Debug/Release repeats
  (0.84/0.43 s). The enforcing storage check again passes native_sha256 and unchanged
  cross-boot witness reopening. Earlier full suites precede only these additional
  version tests; the focused repeats include them.

Hashes verify observed content only. Protected inventory/copy/publication,
publisher registry and app/run admission remain unimplemented; frozen SQL/XML and
byte contracts are unchanged. The package pins apply to this development carrier,
not a finished production base or a performance/hardware acceptance claim.

## Native payload staging — 26 October 2026

- [openat2](https://man7.org/linux/man-pages/man2/openat2.2.html): required contained
  resolution and NO_XDEV's coverage of bind mounts. [O_PATH](https://man7.org/linux/man-pages/man2/open.2.html)
  permits descriptor-level classification without opening file content/device IO.
  The original [stager](../../src/core/import.c) pins each declared leaf, checks
  regular type/ownership/mode/link/size and reopens its own proc FD; trusted procfs
  and protected namespaces remain caller prerequisites.
- [readdir](https://man7.org/linux/man-pages/man3/readdir.3.html): explicit EOF/error
  handling and no reliance on d_type. [listxattr](https://man7.org/linux/man-pages/man2/listxattr.2.html)
  can omit inaccessible names. Accordingly the stager rejects visible unsupported
  xattrs, imports none, and leaves hidden-attribute exclusion plus SELinux policy
  validation to trusted preparation. No broader metadata-audit claim is made.
- Actual [native probes](../../tests/test_import.c) and
  [driver](../../tools/test_import_native.py): 19/19 Debug ASan/UBSan (47.59 s),
  19/19 Release (43.60 s). Forty-two ordinary fixtures cover copied/frozen bytes,
  changed content, exact/extra/missing inventory, links/FIFO/socket, metadata,
  interrupted/short/error IO, corruption, retained partial state and resync.
  The 32-file/32-MiB bounds and 127-level directory ancestry pass. FD counts and
  borrowed root offsets are retained. Actual same-inode/device file/directory
  binds fail EXDEV in a private mount namespace; a listed character device is
  rejected before the content-open counter advances. A root-fsync EIO followed
  by a fresh process repeats both file and all three directory flushes.
  A strengthened multi-chunk source substitution probe then passes in focused
  Debug/Release repeats (18.80/19.65 s including mount probes): after the first
  chunk, a different same-length inode replaces the source path. The copy retains
  the original held inode's bytes and the operation rejects the changed source.

These are unconfined development tests on the enforcing carrier, not proof of
the installed authority's MAC/operation-journal integration. No physical power-loss
test, store publication, publisher enrollment or app admission was performed.

## Protected P0 fixture operation journal — 26 October 2026

- [SQLite transactions](https://www.sqlite.org/lang_transaction.html),
  [autocommit state](https://www.sqlite.org/c3ref/get_autocommit.html) and
  [synchronous EXTRA](https://www.sqlite.org/pragma.html#pragma_synchronous):
  one owner-controlled IMMEDIATE transaction, explicit rollback handling and
  preservation of uncertain commit outcomes. The existing DELETE/EXTRA settings
  remain unchanged; filesystem publication is not part of SQLite atomicity.
- Original [native catalogue API](../../src/catalogue/store.h),
  [journal tests](../../tests/test_fixture_journal.c) and
  [independent intent projection](../../tools/test_fixture_journal_native.py).
  Before-commit IO failure rolls back; simulated acknowledgement failure after
  real COMMIT leaves one accepted operation. Reopen/retry retains IDs; changed
  operation/key/stamp or evidence, wrong scope UID, stale boot and deadline fail.
  Direct READY without executing evidence is refused. READY retains unpublished/
  preparing metadata and no outcome. Accepted replay after a new boot succeeds.
- Original [system administrative path](../../src/authority/fixture.c) and
  [configuration decoder tests](../../tests/test_fixture_config.c): protected
  AETHFIC1 byte bounds/IDs/number ranges, independent source/stage object witnesses,
  root-selected paths and SELinux labels. Actual preallocated infrastructure is
  supplied by the [root controller](../../tools/p0_services.py); business rows
  are written by C17, not by a Python installation implementation.
- Actual native suites: 21/21 Debug ASan/UBSan and 21/21 Release (45.53 s Release).
  Twenty-nine host guards pass (latest 0.231 s); the isolated validator passes
  374 assertions using host SQLite 3.53.1, separate from native guest 3.51.2.
  Actual enforcing system-role units with
  empty capabilities/NoNewPrivileges stage the compiled ELF probe, then replay
  without changing files/operation revision. No new dependency/package family.
- Actual source/stage executable mmap probes fail. Filtered guest AVC records
  identify aetheri_system_t -> aetheri_fixture_source_t / aetheri_staging_t,
  denied map, file class, permissive=0. No mapped bytes are executed. A non-TTY
  audit query initially waited on its input pipe; its exact read-only process was
  verified/pinned and terminated. --input-logs then read the actual guest audit log.
- After normal service checks/stop, graceful VM poweroff returned QEMU exit 0;
  verified PID absence preceded offline boot of the same disk. Native fixture-check
  reports cross_boot_reopen=true with the same protected configuration/root
  witnesses and pending publication. Existing endpoint/denial and private allocation
  checks pass after boot; neither private data nor the live installation is removed.
- Actual fixture-rebind-check creates identical bytes under a distinct inode on
  the same filesystem, binds it only inside a system unit's private namespace and
  observes native ESTALE/administrative exit 4. Catalogue hash is unchanged. The
  drained, enumerated temporary copy is retired; normal fixture-check passes again.

This is trusted administrative fixture preparation, not general client consent,
publisher signatures, dynamic allocation, whole-filesystem clone authentication,
atomic store publication, app launch or complete crash/power-loss recovery.

## Journaled fixture-store publication — 27 October 2026

- Linux [rename(2)](https://man7.org/linux/man-pages/man2/rename.2.html) documents
  RENAME_NOREPLACE, mount/EXDEV constraints and failure handling;
  [fsync(2)](https://man7.org/linux/man-pages/man2/fsync.2.html) separates file and
  directory persistence. The original [publisher](../../src/core/publish.c) uses
  no copy/overwrite fallback and repeats all file/directory/parent flushes on retry.
- Linux [6.19 xattr implementation](https://github.com/torvalds/linux/blob/v6.19/fs/xattr.c)
  delegates security.* writes to filesystem/LSM checks without ordinary VFS file
  write-mode permission. This supports cap-zero sealing of owner-read-only files;
  it does not bypass SELinux or read-only mounts. Actual final labeling succeeds
  under enforcing policy with no added Linux capabilities.
- Original [native tests](../../tests/test_publish.c) and
  [driver](../../tools/test_publish_native.py): 34 ordinary fixtures exercise both
  renames, before/after IO failures, process exit, separate-process reopen, full
  resync counters, original root inodes, final 0555 retry, no replacement, conflicts,
  links and visible unsupported metadata. Four private-namespace probes reject
  same-inode store/container/payload/file bind mounts with EXDEV before publication.
  These are unconfined filesystem tests, not power-loss or authority-admission proof.
- Extended [journal tests](../../tests/test_fixture_journal.c) cover read-only
  lookup, immutable publication evidence, legal/illegal transitions and before/
  after real COMMIT acknowledgement failure at each phase. Reopen/retry preserves
  original IDs/evidence/revision; verified artifact and committed generation remain
  unactivated/preparing, without a result or lease. No schema/XML/version changes.
- Complete native suites pass 23/23 Debug ASan/UBSan (77.94 s) and 23/23 Release
  (55.99 s). Native/host SQLite and all SDK/dependency pins are unchanged.
  Host orchestration/input guards pass 31/31; migration/wire tests 27/27 (46.744 s);
  isolated validator 374 assertions/14 methods on host SQLite 3.53.1.
  The final copied-container replacement case, retaining original held FDs while
  substituting a complete byte-identical tree, passes focused Debug/Release
  publication/mount repeats (12.00/10.77 s) with ESTALE and no recovery adoption.
- Actual offline guest: root-provisioned AETHSTO1, original roots, native journal
  execution, no-replace namespace moves and final exact modes/labels all pass in
  aetheri_system_t/NoNewPrivileges/cap-zero units. Separate native replay preserves
  operation revision and object inode/mode/ctime. Following the full suites, final
  owner-chmod probes were built and pass in the installed service. Actual enforcing
  AVCs identify store-code map and store-dir/code/data setattr denials. Ordinary
  endpoint/app-denial and retained private-allocation checks also pass.
- Graceful shutdown returned QEMU exit 0; verified process absence preceded an
  offline boot of the same disk. The first SSH banner observation timed out while
  boot continued; no replacement VM was started. Native publication replay then
  reports cross_boot_reopen=true, with retained witnesses/operation, exact labels,
  original inodes and denied final chmod/map. Normal protocol/app-denial and private
  allocation reopening pass after boot. Owned authority/probe units are stopped.

Publication is a trusted fixed administrative fixture, not general package trust,
dynamic allocation, registered-volume/clone authentication, activation, app launch,
full recovery coordination or physical power-loss acceptance. No service, toolchain
family or public capability is added.

## First activation and historical completion — 27 October 2026

- Existing v2 [activation/gate constraints](../../spec/schema/system-v2.sql),
  [native API](../../src/catalogue/store.h) and
  [SQLite transactions](https://www.sqlite.org/lang_transaction.html). Initial
  activation, applied step and original install result commit in one owner
  database; there are no post-write filesystem/supervisor effects in that step.
  Existing record_activation advances activated_once/generation revision. The
  install gate remains closed; this creates no launch/data grant or public method.
- Protected [administrative path](../../src/authority/fixture.c): new internal
  AETHACT1 is 72 bytes, binding publication evidence, three revisions and an
  administrative completion stamp. Decoder preserves output on invalid length/
  ranges. Before first activation, native publication checks repeat on the exact
  sealed object. Completed retry verifies its immutable receipt without rereading
  code as authorization or rearming a historical generation.
- Expanded [journal tests](../../tests/test_fixture_journal.c) and independent
  [Python projection](../../tools/test_fixture_journal_native.py): exact bytes/hash,
  stale scope/boot/revisions, missing runtime, open/closing/owned gates and live
  recovery/dependency references. Failures before/after the install update and
  COMMIT acknowledgement preserve either no activation or one whole completion.
  Separate fresh child connections exit before/after real COMMIT and reopen;
  inherited SQLite connections are never used. Retrying after a newer generation,
  removal/reclamation or later gate opening preserves that current state and returns
  original evidence/result/stamp. Those future states are controlled unconfined
  database fixtures, not implemented production update/removal APIs.
- Native complete suites: 23/23 Debug ASan/UBSan (72.63 s), 23/23 Release (65.20 s).
  Final expanded receipt/runtime cases and decoder checks then pass focused
  journal/config repeats in Debug 18.80 s and Release 15.57 s. Host guards 31/31,
  runtime vectors 14/14, migration/wire 27/27 (31.065 s), isolated validator 374
  assertions/14 methods on host SQLite 3.53.1. Native SQLite remains 3.51.2.
- Actual offline enforcing cap-zero system units: protected activation preparation,
  first execution and separate completed retry pass with original IDs, exact
  generation/receipt rows, unchanged published inode/mode/ctime and unchanged
  catalogue hash on retry. A follow-up store check verifies actual sealed content
  and reports activated/completed. Normal protocol/app-denial and retained private
  allocation checks pass. No SELinux permission, dependency or resident service added.
- After graceful QEMU exit 0, verified PID absence and an offline boot of the same
  disk, fixture-activation-check reports cross_boot_reopen=true. The original
  completion result, stamp, revisions and closed gate persist without dispatch.
  Store verification still reports original sealed objects; normal endpoints/
  app-denial and retained private allocation also pass. Owned units are stopped.

This proves fixed-fixture first activation and retained receipts, not launch
readiness, public installer/update/rollback, general recovery, physical power-loss
or a finished Aetheri app/system image.

## Durable user launch journal — 27 October 2026

Primary implementation/evidence in this repository:
[owner API](../../src/catalogue/launch.h), [root receipts](../../src/catalogue/reservation.h),
[native owner implementation](../../src/catalogue/store.c),
[actual authority path](../../src/authority/main.c),
[independent launch tests](../../tools/test_launch_native.py),
[mutation/recovery tests](../../tests/test_launch.c),
[wire tests](../../tests/test_reservation_wire.c) and
[guest controller](../../tools/p0_services.py). API versions and exact boundaries
are in the [runtime contract](../../spec/runtime.md#durable-user-launch-acceptance-and-withdrawal).
No new external dependency/version or policy rule was selected.

The [journal](journal/2026-10-26-first-prototype.md#durable-user-launch-journal--27-october-2026)
records executed native/host tests and real enforcing service recovery. Both owner
intent digests are reconstructed independently from committed rows. Actual user
broker replacement preserves the original root lease until explicit withdrawal;
terminal retries preserve database hashes. Orderly QEMU exit 0, PID absence and
offline re-boot precede cross_boot_reopen=true with unchanged business history.
This is pre-dispatch metadata recovery, not view/readiness, exec/drain, power-loss
or complete app lifecycle evidence.

## Private mount views — 27 October 2026

- [open_tree(2)](https://man7.org/linux/man-pages/man2/open_tree.2.html): a detached
  bind clone can be created from a held FD with AT_EMPTY_PATH/OPEN_TREE_CLONE;
  recursive cloning requires an explicit extra flag. Closing a detached mount
  handle is distinct from removing an attached mount or its remaining references.
- [mount_setattr(2)](https://man7.org/linux/man-pages/man2/mount_setattr.2.html):
  mount attributes can be added on a detached mount FD, with a separately chosen
  propagation type. The implementation never clears preexisting restrictions.
- [move_mount(2)](https://man7.org/linux/man-pages/man2/move_mount.2.html): source
  and destination can be held FDs with the corresponding empty-path flags.
- [systemd.exec 259](https://www.freedesktop.org/software/systemd/man/259/systemd.exec.html)
  and [execveat(2)](https://man7.org/linux/man-pages/man2/execveat.2.html) were also
  inspected for later starter/runtime integration; that integration is not claimed
  from the mount primitive. The online 259 manual identifies itself as 259.6;
  installed systemd is 259.10, so exact runtime acceptance remains separate.

Implemented [view API](../../src/linux/view.h)/[source](../../src/linux/view.c),
[actual native tests](../../tests/test_view.c) and
[independent parent observer](../../tools/test_view_native.py). Real private
namespace clones/attachments, inherited restrictions and fault handling are tested
on the existing kernel 6.19.10/glibc 2.43 carrier. The administrative test context
is unconfined_t with global SELinux Enforcing. No launcher-domain permission,
target exec, catalogue readiness or complete sandbox is inferred. Executed results
and next integration boundaries are in the
[journal](journal/2026-10-26-first-prototype.md#native-private-mount-views--27-october-2026).

## Supervised no-exec setup — 27 October 2026

Primary documentation: [systemd 259 D-Bus API](https://www.freedesktop.org/software/systemd/man/259/org.freedesktop.systemd1.html)
(served page identifies 259.6), including RefUnit and SELinuxContext's (bs)
signature. [systemd v259 dbus-unit.c](https://github.com/systemd/systemd/blob/v259/src/core/dbus-unit.c#L641)
checks start authorization for RefUnit and tracks its bus sender; UnrefUnit removes
that reference. These explain API semantics, not acceptance of Aetheri code.

Actual carrier evidence uses installed systemd 259.10-1.fc44 and the existing
kernel/glibc/library pins. Enforcing audit recorded RefUnit's denied system/start,
the setup namespace FD/propagation and notification requirements. Rules were
added only for the responsible trusted domains. PID1 left allowed SYS_ADMIN in
the helper's inheritable set; the helper now drops it before registration/effects.
The first policy refresh used a changed basename and installed a separate module;
after byte/hash checks it was repaired and the unintended module removed. Staging
now preserves the original basename. No enforcing-mode exception was used.

The real procfs label included a terminating NUL. Python initially hashed that
terminator while libselinux's C string excluded it. Reconstructing the hash from
the retained observation after removing exactly that terminator matched native
evidence; the controller now accepts only the exact expected label with optional
single NUL. [Setup source](../../src/authority/setup.c),
[supervisor rejection tests](../../tests/test_setup_supervisor.c),
[controller](../../tools/p0_services.py) and
[runtime contract](../../spec/runtime.md#supervised-no-exec-setup) delimit this
same-runtime, fixed no-exec proof. It is not target-exec or production recovery.
