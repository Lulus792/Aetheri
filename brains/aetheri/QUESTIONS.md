# Open questions

The main language/library/ownership policy is agreed. Do not ask the original
foundation questions again. Remaining decisions belong to architecture or concrete
product behavior.

## Browser

B1. Which everyday sites define acceptance? Are browser extensions, DRM streaming,
WebRTC calls and password management required? Existing engine accepted; CEF versus
a WebKit integration and prebuilt-versus-source delivery still need a measured comparison.
No browser engine can promise universal website or ad-blocking compatibility.

## Shell, editor and sessions

S1. Design the original shell syntax with concrete examples before implementation:
commands, paths/quoting, pipes, values/variables, errors, scripts and background jobs.
Decide whether compatibility with existing shell scripts is needed at all.

S2. Define the editor/plugin capability model and terminal pane/session interaction.
Need exact Vim/Neovim compatibility only if subsequently requested.
Should terminal sessions survive closing every window, logout or reboot? These
are different requirements and affect supervision/persistence design.

## Hardware and daily use

H1. The main PC is now confirmed as the development host: Xeon W-2123, nominal
32 GB RAM, RTX 2080 Ti. Windows reports UG25F (HDMI) and two HP E223i (DisplayPort).
Exact per-monitor modes/refresh/scaling and physical cable/adapter paths still need
collection. A future AM5 upgrade must also work; exact CPU/board/GPU remain unknown.
Keep baseline CPU portability and later separate hardware acceptance profiles.

H2. Suspend/resume is confirmed and no longer an open product choice. Select and
validate sleep modes, lid/idle policy, locking and device recovery on each hardware
profile. Hibernate remains separate. Confirm microphone/special peripheral features later.

## Architecture and security

C1. Native SHA-256: OpenSSL 3.5 LTS/libcrypto is proposed and was presented to the
user for the requested library discussion. Choice remains pending. The carrier
already contains openssl-libs 3.5.5-1.fc44, but no openssl-devel/pkg-config metadata;
upstream lists 3.5.9 as the current 3.5 LTS maintenance release. Exact package,
provenance, security/compatibility and footprint review precede direct adoption.
The native fixture decoder proceeds without a new crypto dependency meanwhile.

A1. Architecture is approved: B, wlroots, selected systemd components, SQLite and
versioned base/managed-app separation. Do not reopen these settled choices.
Remaining work: exact versions/build options and compatibility, sandbox/update/boot
integration, browser embedding, filesystem implementation and plugin interfaces.
The [comparison](knowledge/architecture-options.md) preserves rationale and alternatives.
The [module/interface plan](knowledge/modules-and-interfaces.md) is now accepted
in direction. The initial concrete management contract is accepted, not installed.
The [identity/lifecycle model](knowledge/identity-and-lifecycle.md) is accepted.
The [schema/API contract](knowledge/schema-and-api.md), including SQL/XML and
starting error/deadline/retention rules, is now accepted.
The [filesystem/permissions direction](knowledge/filesystem-and-permissions.md)
is accepted, including ext4/LUKS2 and SELinux plus namespaces.
The [launch/access protocol](knowledge/launch-access-and-removal.md) is accepted.
The [minimal extension/P0 scope](knowledge/first-linux-prototype.md) is now accepted.
Its five new table designs, additive P0 interfaces and development migration/tests
are implemented in [versioned contracts](../../spec/README.md).
VM/tool setup now has actual guest evidence and pinned build tools. Next: the
C17 app/run authorization/launcher, remaining policy and transport. The two confined
protocol endpoints now run, with discovery only. Saved/direct/write/bulk document access remains
in the accepted longer-term protocol, outside this first implementation slice.
The bounded P0 fixture/owner/intent encodings now have a
[reference and frozen vectors](../../spec/runtime.md). This does not finalize
general signed packages or implement registered-volume identity. C17 owner codec
and bounded reads now have native tests. The native catalogue store also creates/
validates v2 and commits startup context; it receives trusted identity/boot values
from its caller and does not implement the registry, kernel-context sourcing or
native v1 migration. Current peer/process observations now exist, but binding them
to app/run identity remains pending. Protected P0 broker enrollment now works for
the fixed eligible test account, now backed by a durable system scope/UID row;
general account eligibility and complete principal mapping,
policy/registry identities, target-exec evidence and actual cross-authority
admission/recovery remain Linux integration gates. Isolated host tests do not
settle these gates.

A2. Explain the proposed daily-use gates: encryption protects data on storage,
sandboxing restricts app access, backups recover lost/corrupt data, and trusted boot
checks early software. Secure Boot may be included, but no keys/settings are changed.
Design separate VM/test-data and personal-data acceptance.
Encrypted personal-use storage is accepted in direction; boot/unlock,
recovery keys, policy maintenance, label/category allocation and durable allocation/
registered-volume identity still require implementation contracts. Exact owner
bytes and runtime private-root reference checks now exist; they do not settle
persistent enrollment, clone detection or public document access.

A3. Define full removal for multiple users, shared dependencies, rollback snapshots,
exported documents and externally stored data. Draft in app-lifecycle.md is a
proposal to satisfy the requirement, not an implemented guarantee.

## Resolved foundation round

| Item | Current answer |
| --- | --- |
| T1 | C17 primary, small adapters, targeted Rust allowed |
| T2 | Existing open development tools accepted |
| T3 | glibc |
| T4 | Proposed technical-library boundary accepted; exact integrations pending |
| T5 | Existing service backends allowed; no forced replacement |
| A1 | Custom lightweight ordinary browser, no feeds/promotional clutter, content filtering |
| A2 | Original editor design/plugins inspired by modal editors |
| A3 | Own shell and language immediately |
| A4 | NexaCloud Desktop confirmed |
| H1 | Up to three 1080p/1440p/4K monitors, wired/Bluetooth input, jack/Bluetooth headphones |
| H2 | F: allocated and now ready; protect ongoing Physim/NexaCloud migrations |
| H3 | Security goals accepted in principle; Secure Boot allowed if useful, explain details |
| M1 | Explicit signed update installation and recoverable previous version |
| M2 | MIT |
