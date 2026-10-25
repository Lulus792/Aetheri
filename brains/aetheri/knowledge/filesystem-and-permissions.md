# Filesystem, storage ownership and permission boundaries

25 October 2026. Filesystem/ownership/enforcement direction accepted by the user,
including ext4/LUKS2 and SELinux plus namespaces as starting foundations.
The [launch/access protocol](launch-access-and-removal.md) is now accepted.
The next [P0 proposal](first-linux-prototype.md) narrows the initial implementation
and remains for discussion.
Paths below describe a future Aetheri installation, not directories to create
on the Windows development host.
No partition, mount, policy, encryption key or permission is changed by this document.

## Recommendation and alternatives

Keep a Linux-compatible technical hierarchy, a read-only versioned system and
content-addressed app store, exact private-data roots, and separate user documents.
Present readable ownership views in Files/Settings; IDs remain available in details.

For the first disk implementation, recommend ext4 for writable Linux storage and
LUKS2 encryption for personal-use installations. These are accepted starting
foundations, not selected releases or measured performance results. A conventional GPT/UEFI
layout with an EFI System Partition and encrypted Linux storage is sufficient as
a starting model; sizes, chosen disk, unlock/recovery and boot-image format remain
a separate reviewed installation plan. The EFI partition is outside this encrypted
data boundary. Encryption is optional for disposable VM test data, not an excuse
to weaken later personal-data acceptance.

| Alternative | Benefit | Cost / decision |
| --- | --- | --- |
| Conventional mutable Linux tree | Familiar tools and locations | Untracked installers and scattered app writes conflict with removal guarantees |
| Versioned base + managed store/data on ext4 | Clear owner boundaries without depending on snapshots | We must implement publication/recovery; recommended first direction |
| Btrfs snapshot-centred system | Snapshot, sharing and compression features | Snapshot retention/restore adds data-lifetime and accounting work; reconsider with evidence |

Ext4 is an on-disk filesystem, not our directory naming scheme or package manager.
Versioned deployments do not require filesystem snapshots. Ext4 does not make an
arbitrary directory immutable or provide verified boot by itself.
Neither filesystem choice is claimed inherently faster for Aetheri without tests.
Avoid fixed separate partitions for every logical directory initially; they can
strand free space. Additional disks/volumes need registered ownership roots and
must not turn an arbitrary mounted path into an authorized purge target.

## Host-visible tree

Paths use stable IDs internally. A login name is a human-facing home location;
AccountId remains the durable owner even if the name or Unix UID changes.

```text
/
├── usr/                                  selected system deployment, read-only
├── etc/                                  managed machine configuration
├── var/
│   ├── lib/aetheri/
│   │   ├── system/                       privileged catalogue and recovery metadata
│   │   ├── deployments/<GenerationId>/   retained system deployment objects
│   │   ├── store/sha256/<ArtifactId>/     immutable app/tool/runtime payloads
│   │   ├── staging/<OperationId>/        bounded installation work
│   │   └── users/<AccountId>/
│   │       ├── control/                  user catalogue and broker-owned grants
│   │       ├── data/<StorageRef>/
│   │       │   ├── owner                 protected ownership header
│   │       │   └── payload/
│   │       │       ├── config/
│   │       │       ├── data/
│   │       │       ├── state/
│   │       │       ├── cache/
│   │       │       └── home/             compatibility dotfiles, when needed
│   │       ├── credentials/              vault-owned records, scoped to data lifetime
│   │       └── trash/<OperationId>/      temporary quarantined roots during removal
│   ├── cache/aetheri/                    expendable manager download cache
│   └── log/                             bounded system diagnostic storage
├── home/<login>/                         independent documents and project trees
├── run/                                 volatile boot/session/service state
├── tmp/                                 volatile, isolated temporary views for apps
├── efi/                                 EFI boot assets; update authority only
├── dev/                                 device interfaces
├── proc/                                process/kernel interfaces
└── sys/                                 kernel/device interfaces
```

This is a responsibility tree, not a requirement to allocate every directory
before use. UUIDs/digests use validated fixed canonical encodings as path segments;
display names, URLs and app-supplied paths are never interpolated into privileged
storage paths. Long IDs should not dominate the normal UI.

Keep merged-/usr compatibility links such as /bin and /lib only where the selected
toolchain/loader expects them; they are aliases, not duplicated installations.
A root deployment supplies the static directory skeleton. Mutable /etc, /var,
/home and volatile/API mounts are deliberate exceptions to the read-only base.
The final boot/mount mechanism still needs specification; this tree does not select
an updater, image filesystem or boot loader.

An app update must not scatter executables into /usr or run arbitrary root
post-install scripts. Base programs belong to the versioned deployment; independent
apps/tools/runtimes belong to the managed store. Integration adapters generate
owned registrations from validated declarations.

System defaults travel with the base under /usr. Deliberate machine overrides live
under /etc with component ownership and compatibility rules. OS rollback changes
code, not automatically all mutable configuration/private data. The activation
preflight must handle incompatible overrides before declaring a usable rollback.

System services need writable state too. Their selected backend state directories
(e.g. device pairing or network configuration) receive explicit service ownership,
component inventory and recovery policy. Do not force maintained backends into
invented paths if that breaks their contracts. These directories are not orphan
app state and are not deleted by removing a Bluetooth settings window.

## Three different meanings of owner

1. **Lifecycle owner:** ComponentId, AccountId and DataEpochId determine why data
   exists and which reviewed removal can target it.
2. **Unix owner/group/mode:** kernel DAC permissions determine ordinary access
   across accounts and services.
3. **Security domain/label:** mandatory policy distinguishes the broker, app,
   document tools and other same-UID processes.

These must agree. A filename, the creator app, a reused UID, a familiar directory
name or the ability to read a file is not a deletion authority.

Independent documents belong to the user. A Physim experiment, downloaded PDF or
source repository does not become app-private just because Physim, Browser or
Terminal created it. A record of "created by" is useful provenance, not permission
to delete it with the app.

The component view should show code, configuration, durable state, expendable
cache, documents granted externally, credentials, services and retained versions
as distinct categories. Count shared physical storage once; logical size is not
the same as immediately reclaimable space. A retained old deployment needs an
explicit recovery reason and cleanup policy, not indefinite accidental retention.

## StorageRef and the private root

The accepted data_epochs.storage_ref becomes the lookup key for
users/<AccountId>/data/<StorageRef>. Component and DataEpochId remain catalogue
facts; a path is not another independently editable identity registry.

The protected owner header binds format version, machine/account identity,
StorageRef and DataEpochId to the allocated root. It contains no secrets or user
payload. Only trusted storage management can create/change it; apps see payload
only. Header bytes and volume-registration details still need a versioned format
and recovery fixtures before implementation. The header is evidence to reconcile
with the catalogue, not permission to silently adopt an unknown root.

Trusted allocation checks a registered volume and creates the root below held,
protected directory descriptors. Root-owned account anchors prevent ordinary apps
from renaming the management hierarchy. Broker-owned data parents cannot be
modified by app domains; app-writable payload subdirectories have the data epoch's
scope label. Inode numbers/mount IDs are useful observations but are not universal
persistent identities across reboot, backup restore or filesystem cloning.
An unexpected root/header/volume identity blocks destructive work.

One data epoch initially groups config/data/state/cache and any compatibility home.
Caches can be reclaimed separately; they must never hold the only copy of valuable
work. An app cannot keep private state in unrelated global dotfolders. Multiple
profiles/plugins and machine-wide service-private data need explicit extensions,
not informal new roots outside the ownership inventory.

Credentials use a separately confined vault with opaque references scoped to the
owning account/component/data lifetime. A purge must include its vault deletion
receipts. This does not prevent an app from holding credential material in memory
or having cookies in its own profile; those are still within its removal scope.
No promise of deleting remote sessions or already exported copies follows.

Keep app diagnostic payloads in bounded state/log storage within the data epoch.
Do not copy ordinary app stdout/stderr or private crash dumps into an undeletable
global journal by default. System diagnostics retain minimal administrative codes
and correlations under the disclosed receipt/log policy. Debug exports are explicit
independent files. Crash dumps need scoped private storage and consent, not a hidden
global accumulation of app memory after removal.

## What an app sees

A managed app receives a constructed filesystem view, not the host's whole tree:

| View inside app | Access |
| --- | --- |
| /app | Exact verified app payload, read-only |
| /runtime | Only the resolved runtime/tool payloads, read-only |
| /app-state | Its own data epoch payload |
| /documents or /workspace | Explicit selected document/project grants |
| /tmp | Private bounded temporary storage |
| /run/user/<uid> | Private runtime view with only approved service endpoints |

Set HOME=/app-state/home and XDG_CONFIG_HOME, XDG_DATA_HOME, XDG_STATE_HOME and
XDG_CACHE_HOME to the corresponding absolute paths below /app-state.
XDG_RUNTIME_DIR points at the private runtime view. These variables support
cooperating software; they are not enforcement. Programs using hardcoded paths
must still be contained or rejected as incompatible.

Prevent hardlinks and rename operations across ownership roots, including private
data versus granted documents on the same underlying filesystem. Construct separate
mount views and verify the relevant policy; do not rely on differing path strings.
An explicit copy/export becomes an independent document with disclosed ownership.

Apps do not receive the host's general user bus, management directories,
unrestricted /proc, raw input devices or arbitrary systemd/supervisor endpoints.
Wayland/audio/management connections are explicit capabilities. Read-only mounts
do not block communication through sockets, so IPC requires its own filtering and
authenticated sender policy.

Temporary work needs limits and tracked lifetime. Do not erase an app's only
unsaved recovery material under the label "cache." Clean its owned tmp/runtime
objects only after relevant processes and grants are settled.

## Permission enforcement

Recommend layered isolation: normal Unix permissions, a confined filesystem/
process view, mandatory per-domain policy and small authenticated broker APIs.
Children inherit confinement. Merely setting umask or chmod 0700 does not separate
two processes running as the same Unix user.

| Enforcement option | Assessment |
| --- | --- |
| Same UID + directory modes/environment only | Too weak for the accepted broker/app boundary |
| Namespaces + Landlock alone | Useful confinement layers, but do not alone establish all IPC, supervisor, credential and same-user boundaries |
| AppArmor profiles + namespaces | Viable alternative; policy/path/mount and communication rules need a full design |
| SELinux type enforcement + allocated app/account categories + namespaces | Recommended candidate for same-UID broker protection and owned-data separation; more policy/build/recovery work |

SELinux is accepted as the mandatory-policy foundation; no policy/tools are
installed or validated here. Its kernel enforcement and maintained userspace tools
are foundations; Aetheri owns the product policy. Do not build a new security engine or claim generic
reference policy already implements our boundaries.

Use distinct policy domains for system authority, user broker, vault, desktop
document services and managed apps. Allocate category bindings for account/data
lifetimes through trusted state; never hash IDs into a small category space and
assume collisions cannot happen. Reuse requires proof that old subjects, objects
and references are gone. A label by itself does not enforce equality: the compiled
constraints, transitions and IPC rules must implement the intended separation.

The user broker can stay unprivileged under the user's Unix UID in its own security
domain. Its control database directory can be 0700 and database 0600, while mandatory
policy also denies other same-UID domains. That avoids silently converting the
per-user broker into a root daemon. A narrow system-authority setup path allocates
protected anchors and authorized launch contexts. It must authenticate the actor,
scope, executable and allowed transition; no caller-supplied arbitrary label.

Protect account anchors as root-owned; protect broker control roots and root headers
against app create/unlink/rename/relabel, not just file writes. Label SQLite journals,
temporary replacement files, backups and recovery outputs consistently. SELinux
policy also needs to prevent unauthorized ptrace, process-memory access, signal/
descriptor theft, context changes and bypass through service activation.
This is a policy requirement, not a shipped policy or an executed isolation test.

Use no_new_privs and remove unnecessary capabilities from app launches.
Namespace, seccomp and resource controls supplement the policy. systemd can supervise
the resulting process group; supported directives and privileged setup differ
between system and user services. Browser subprocess sandbox/JIT requirements
must be tested rather than disabled for convenience. noexec and W^X-related switches
are not a universal policy for every browser/tool workload.

The trusted setup sequence must verify the authorized domain transition and apply
privilege drops/confinement in a compatible order. Close inherited descriptors
except an explicit capability allowlist; a private mount view does not neutralize
a leaked database, directory or device descriptor.

Minimum required enforcement is checked before managed launch. Missing/disabled/
permissive required policy or an unverified transition blocks normal launch with
a clear recovery explanation; never label an unconfined fallback as sandboxed.
Landlock may add restrictions only after ABI probing; it is not a replacement for
the mandatory layer. Policy updates must be boot/recovery-compatible and cannot
silently loosen an existing grant.

A kernel compromise or deliberately authorized administrator changing enforcement
is outside this app-isolation guarantee. Normal user actions should not require
disabling enforcement or making everyday apps administrators.

## Role and access matrix

| Role | Code/base | Administrative catalogues | App-private data | Documents/devices |
| --- | --- | --- | --- | --- |
| System authority | Publish/reclaim exact verified targets | System writer | Narrow authorized allocation/delegation; no broad UI browsing | Privileged actions only by checked request |
| User broker | Read metadata, request lease | Own account writer; no root catalogue edits | Allocate/attach/quiesce/purge within account | Issue/revoke checked grants |
| Managed app | Read/execute granted payload | API only | Own active data epoch | Selected grants; no default whole-home/raw-device access |
| Files / document UI | Own app code | API only | User-approved mediated inspection | Broad user-document role, disclosed and separately confined |
| Terminal / developer tools | Granted toolchain/project execution | API only | Own state; no broker catalogue access | Project/home grants according to visible mode |
| Compositor/device backends | Selected service code | No arbitrary catalogue edits | Own service state | Required seat/render/device rights, not inherited by all apps |
| Administrator/recovery | Explicit repair/update role | Reviewed maintenance path | Explicit scoped recovery | Can deliberately change system policy; never implicit app privilege |

Service duties are scoped permissions, not full trust merely because a service
is original Aetheri code or signed by the expected publisher. Backups and indexers
also need declared access; they do not automatically inherit every app's secrets.

Proposed DAC defaults supplement mandatory policy, rather than replacing it:

| Object | Unix owner and ordinary mode | Additional restriction |
| --- | --- | --- |
| System catalogue directory / file | root, 0700 / 0600 | System-authority domain only |
| Verified store directories | root, 0755; read-only app mounts | No app publication, metadata or ownership changes |
| Account anchor | root, 0711 | Account-scoped traversal policy; children remain private |
| User control directory / catalogue | Account UID, 0700 / 0600 | Broker domain, including SQLite sidecars/replacements |
| Storage parent / protected header | Account UID, 0700 / 0600 | Broker/storage domain controls create, rename and relabel |
| App payload directories / private files | Account UID, 0700 / 0600 by default | Exact app/account/data scope; executable tool/project content has a separate declared role |
| Independent home directory | Account UID, 0700 by default | Document-role or explicit project/home grants |

Broker caller identity combines verified bus credentials/security label with the
registered launch and current scope. Never trust a caller-supplied ComponentId,
PID, executable basename or UID alone as proof of app identity. Bus/proxy credential
preservation, label/category mapping and peer lifetime checks need a concrete
implementation contract before untrusted clients are admitted.

File selection should default to a file/directory and read or read/write access.
A developer project grant can include create/rename/delete beneath its root.
Keep saved grants visible and revocable. An adapter is needed for XDG portals:
FileChooser returns URIs, not an invented standard direct-FD-only API.

Grant revocation prevents future broker admission. Already open descriptors,
mappings and copies need separate treatment; closing a portal or changing a path's
mode does not universally retract them. For strong private purge, settle all
holders and stop affected process groups or report the remaining blocker.
Shared/delegated handles must be part of the lifecycle design before use.

Internet, microphone, screen capture, removable devices and privileged settings
are distinct grants, not consequences of file access. Prefer maintained device
services and compositor/portal mediation over direct raw-device access. A network
grant is not automatically fine-grained domain filtering.

## Terminal, shell and development

Recommend project-oriented operation by default: choose a workspace, then shell,
editor, compiler and child jobs inherit its visible grants. Generated files stay
in that project or declared tool caches. Executing a project-built binary inherits
the developer context; it cannot gain broker or root privileges from its filename.

Offer an explicit broader user-document mode when wanted. It still cannot edit
broker/vault internals or system policy. Administrative requests use a trusted
authorization flow and bounded APIs rather than a permanently root terminal.
Terminal, editor and PDF/image/plugin subprocesses retain tracked launch ownership.
Session-survival policy remains an existing separate product question.

An unconfined host shell or manually launched unmanaged binary is visibly outside
the managed guarantee. Never claim to prevent arbitrary authorized administration
from modifying the machine. Do not use an unconfined fallback to make a failed app
launch appear successful.

The process view should show component, launch, purpose, parent/children, active
grants and resource use. One logical module need not create another resident
service; use the accepted broker/authority and systemd supervision boundaries.

## Removal, publication and recovery

Full removal requires enforcement, ownership metadata and recoverable operations:

1. Persist the exact authorized install/data/storage targets and expected effects.
2. Gate new launches, background activation, reattach and relevant grants.
3. Stop/reconcile app descendants and all capability holders; preserve unsaved-work
   policy. A failed/uncertain stop blocks private purge.
4. Verify the held storage root, protected header, volume and current catalogue
   binding. Move that exact root into its same-volume, broker-controlled trash
   location and flush required parent changes under the journal protocol.
5. Walk by directory descriptors with bounded traversal and no mount escape.
   Remove symbolic links as links; never recurse through them. Reject special or
   unexpected objects and quarantine for review rather than guessing.
6. Remove scoped vault entries and registrations and settle delegation receipts.
   Release code only after dependencies, leases and recovery pins allow it.
7. Verify postconditions and report actual removed, retained and blocked objects.
   Purged data IDs never become new data; remove empty owned scaffolding when safe.

For directory traversal, openat2 containment flags can prevent path/magic-link/
mount escapes; RESOLVE_NO_XDEV also rejects same-filesystem bind mounts.
st_dev equality alone is insufficient. Resolve flags do not replace protected
parents, quiescence, authorization, target identity or a correct unlinkat walk.
Do not fall back silently to string-prefix validation if the required resolver
or confinement feature is absent.

Publication must separate unprivileged download/unpack from trusted ownership/
metadata validation. Reject escaping paths, unapproved special files, setuid/
capability metadata and links into mutable/unowned content. An old writable handle
can survive chmod/chown; stop/reconcile writers and verify the final trusted copy
before declaring the store immutable. App code must never run from mutable staging.

Rename visibility is not a durability receipt. Required file/parent flushes must
succeed or remain pending for recovery before reporting completion. No atomic
transaction spans SQLite, filesystem deletion, supervisor and vault. Cross-volume
moves need a separate copy/verify/publish protocol, not a pretend atomic rename.

Keep crash residue identifiable by OperationId. Trash is active recovery storage,
not a second indefinite recycle bin. Open-unlinked files, shared payloads and
retained backups explain why visible removal and immediately reclaimed bytes differ.
Complete managed live-state removal is not forensic erasure or remote/backup deletion.

## Budgets, backups and user understanding

Bound diagnostic logs, cache, temporary space, staging and completed receipts.
Expose sizes, purpose, retaining owners and cleanup controls. Preflight updates
for the new payload, retained recovery version and working space. Keep enough
catalogue/recovery space to fail cleanly on a full volume. Exact byte quotas and
project-quota setup remain measurement/implementation choices; do not pretend
directory accounting itself enforces a disk limit.

LUKS2 protects locked storage; it does not sandbox an app after unlock. It also
does not imply that suspend erases keys from RAM. Suspend remains required;
lock/resume, encryption unlock and recovery-key handling need their own acceptance.
Secure Boot checks trusted boot software, not confidentiality of all stored data.
Snapshots/backups may retain removed data and must be visible in a removal report.
Do not restore an old catalogue/data backup without epoch and ownership reconciliation.

Normal Files views should be System, Apps, App Data, Documents, Projects and
Temporary/Recovery Storage. Selecting an item explains who owns it, why it exists,
whether it is shared and what removal would affect. Technical paths and IDs stay
inspectable. This is a later UI requirement, not a generated design or working UI.

## Required validation before implementation acceptance

This document has been researched and checked as documentation only. No Linux
permissions, labels, namespaces, grants, encryption or destructive tests ran.

A later isolated Linux prototype must cover:

- Same-UID app cannot open/replace broker DB, its journal, owner header or vault,
  even after environment changes, direct syscalls and child execution.
- App A cannot read/modify app B or another account, or enter broker/admin domains
  through D-Bus, user-systemd, ptrace, /proc, descriptor passing or relabeling.
- Valid file/project grants work; missing rights fail; inherited/held descriptors
  have truthful revocation/removal behavior.
- Symlink, renamed-root, hardlink and same-filesystem bind-mount cases cannot
  redirect publication/purge; unexpected mounts block.
- Stopped/uncertain app, new reinstall, full disk, fsync failure and interruption
  produce bounded recovery and never remove the new installation's data.
- Missing/permissive policy prevents managed launch; valid desktop/browser/tool
  workloads operate with enforcement enabled, including graphics/JIT as required.
- Disk/backup restore, account rename/UID reuse, encrypted locked accounts,
  suspend/resume and rollback maintain identity and disclose retained copies.
- Resource overhead, launch time, wakeups, storage and category allocation/retirement
  are measured on selected VM and later real hardware.

## Primary references

- [UAPI filesystem hierarchy](https://uapi-group.org/specifications/specs/linux_file_system_hierarchy/)
  and [systemd hierarchy](https://www.freedesktop.org/software/systemd/man/latest/file-hierarchy.html):
  conventional technical directories and read-only/mutable separation.
- [XDG base directories](https://specifications.freedesktop.org/basedir/latest/):
  configuration, data, state, cache and runtime conventions.
- [Ext4 design](https://docs.kernel.org/filesystems/ext4/overview.html) and
  [cryptsetup manual](https://man7.org/linux/man-pages/man8/cryptsetup.8.html):
  filesystem and LUKS foundations; no Aetheri benchmark or installation implied.
- [SELinux type enforcement](https://github.com/SELinuxProject/selinux-notebook/blob/main/src/type_enforcement.md),
  [contexts](https://github.com/SELinuxProject/selinux-notebook/blob/main/src/security_context.md)
  and [MAC/MCS](https://github.com/SELinuxProject/selinux-notebook/blob/main/src/mac.md):
  policy concepts, not a pre-existing Aetheri policy.
- [AppArmor](https://docs.kernel.org/admin-guide/LSM/apparmor.html),
  [Landlock](https://docs.kernel.org/userspace-api/landlock.html) and
  [namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html): alternatives/layers and limits.
- [systemd execution](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html):
  sandbox/SELinux configuration and user-service/IPC caveats.
- [D-Bus credentials](https://dbus.freedesktop.org/doc/dbus-specification.html):
  sender credential/security-label observations; unsupported fields may be absent.
- [openat2](https://man7.org/linux/man-pages/man2/openat2.2.html),
  [chmod](https://man7.org/linux/man-pages/man2/chmod.2.html) and
  [unlink](https://man7.org/linux/man-pages/man2/unlink.2.html): descriptor/path/lifetime semantics.
- [FileChooser](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html):
  portal selection contract.

Inspected 25 October 2026. The direct GitHub type-enforcement page initially returned
no useful body; the indexed official project source supplied the relevant text.
The path layout, ownership direction and SELinux selection are original Aetheri
design decisions accepted by the user. They are not an upstream promise,
implemented security policy or executed security test.
