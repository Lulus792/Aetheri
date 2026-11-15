# Successful private PAM authentication — 15 November 2026

The original C test greeter now completes actual pam_unix authentication and
account checks with the reviewed greetd candidate, then explicitly cancels.
An empty answer is rejected first. This advances the credential path before
PAM open_session, logind/VT setup, target Start and desktop readiness. None of
those later milestones is claimed. See [the contract](../../../spec/login.md#successful-authentication-fixture).

## Input and authority boundary

The administrative fixture generates a fresh64-character random hexadecimal
test password and a yescrypt setting using existing carrier libxcrypt4.5.2-3.fc44.
It is a test generator for the already selected PAM foundation, not a replacement
for the project's OpenSSL cryptographic core or a new app dependency. The tested
PAM is1.7.2-1.fc44. No package, production account/password or global PAM policy
is changed. The actual existing account remains UID/GID1000.

PID1 OpenFile supplies one read-only Root-owned sealed anonymous memfd to the
service. The original descriptor survives the reviewed worker/greeter exec path.
The C fixture checks fd3's mode0600, UID/GID0, zero links, size64, access mode and
all four seals, reads a bounded complete answer and closes that local FD. Its
buffer is wiped after the connection queues the answer, with an exit wipe as
well. The existing conversation wipes transmitted prefixes. No answer is placed
in argv, environment, profile JSON, source, receipt or journal. This test provider
is not a production credential-input or general native-FD authorization API.

Only the service sees a private read-only `/etc` projection, with selected copied
public NSS/loader files and original SELinux configuration. Its Shadow file is
mode000 in a Root-only holder under guest `/run` tmpfs. Auth/account use pam_unix;
password/session entries stay pam_deny. The controller joins the backend's actual
Shadow and inherited fd3 to the independently created original objects, then
publishes the same-view protected backend observation as before. It compares
global Shadow bytes before/after and checks both the generated plaintext and
hash absent from journal bytes. No host Shadow content is copied into the fixture.

Successful retirement removes the original named test hash only after actual
terminal unit/cgroup checks and held/named identity agreement, and closes the
original secret descriptor. This is explicit fixture retirement, not managed
app-data removal or a durable restart protocol. Guest `/run` and anonymous
memory are used; observed guest swap is unused zram. System-wide erasure,
swap exclusion and copies inside external PAM/libcrypt are not guaranteed.

## Session entry descriptor boundary

`aetheri-session` now closes inherited descriptors3 and above, unsharing the
descriptor table, before NSS/session lookup or creation of its own channels.
It accepts no incoming activation/document/credential descriptor. Standard
input/output/error remain. Failed close_range ends startup before a child can
start; there is no fallback. The fixed development owner retains its invocation.
This is local FD cleanup, not document revocation, app drain or lease settlement.

The actual native fork test checks a canary disappears in the child while the
parent's descriptor and child stdio survive. Its injected failure preserves the
canary and rejects admission. A separate ordinary test binary omits only the
entry cleanup. It compiles and fails the canary assertion, exit134. Its source
is `/var/tmp/a-entry-descriptor-counter-1027f68dca72`; no core is produced and it
is not installed. GCC analyzer passes the actual session-entry translation unit.

## Executed evidence

Use the same enforcing audio carrier, boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, QEMU PID16628. Final CMake Release source
`/var/tmp/a-login-cmake-6af86dc1f3ff` and memory source
`/var/tmp/a-login-cmake-533ac075cd57` build the greeter, post-login entry and tests.
Both pass all five selected CTests: login.protocol/wire/connection and
linux.session/session_entry. The independent audit matches all767 submitted
files in each tree, the current selected C bodies and actual sanitizer flags.

Actual final authentication fixtures are `/var/tmp/a-auth-live-8dc392b36506`
(Release) and `/var/tmp/a-auth-live-042ac7287888` (ASan/UBSan). The unchanged
protected886472-byte backend is SHA256
`109b9ff31401c0842d2a699bf33b8cac5c188d46a95a0c40fecd77826efce55e`.
Original source/service/image/greeter guards, protected handoff and mandatory
connection callback remain in the path. A real secret prompt is cancelled and
the actual original authentication worker pin ends before its ACK. The empty
answer yields AUTH_ERR and a confirmed cancellation; the fresh synthetic answer
yields AUTHENTICATED, followed by confirmed cancellation. Each case makes three
Create requests, one empty and one synthetic-secret answer, with no Start request
or personal-password submission. Only the original greeter worker remains at
the end of the conversation. Original-program mode drift still fences after
restoration. The memory client reports zero in its explicit LSan check; there
is no ASan/UBSan diagnostic. The first earlier positive822d50298fde is preserved
separately from the final source/canonical Shadow cases.

The existing native owner test also compiles/runs in Release and memory with
assertions enabled: six successful commands at
`/var/tmp/a-owner-regression-b155bda62a14`. This retains actual child/pidfd/barrier,
ancillary-FD and pin-failure checks. The new entry has not yet been exercised
through the full actual KMS login workflow; previous KMS records concern its
preceding entry. Do not upgrade this unit/authentication evidence into that claim.
The contract validator also passes374 assertions/14 methods; these remain
isolated SQL/XML checks, not installed login or lifecycle acceptance.

## Retained failures and final audit

Initial systemd namespace setup could not resolve the anonymous Shadow source
through its `/proc` path (ENOENT); the backend never started. A direct memfd bind
attempt then failed before service dispatch; its exact errno was not captured.
The following named-Shadow-only projection failed at namespace mount with EACCES.
No AVC establishing its specific policy cause was obtained, so it is not called
a proven SELinux permission defect. The final private `/etc` projection succeeds
without a policy module, permissive mode, altered host Shadow mode or weakened
PAM authentication. A local helper quoting error stopped before dispatch.

Two failed Root-only memory holders remain as evidence: the empty preparation
placeholder4bfa7814947d and the mode000 synthetic-hash fixture d8161f52ea13.
Neither is adopted or automatically cleaned up after its original controller
ended. The host-view audit sees no selected holder bind mount. The unsuccessful audit reader
was stopped through its pinned original diagnostic process after exact command,
parent and canonical executable checks; its tool handle is terminal.

The final independent auth audit verifies five actual unit attempts are terminal,
one further attempt never dispatched, final successful holders absent, no backend
sockets or selected mappings, unchanged RPM inventory/global fixture-PAM absence,
enforcing1 and tty1. Receipts remain in `local/login-review-2026-11-10` with
`auth-`, `entry-descriptor-` and `owner-auth-` prefixes; final audit is auth-audit-v1.
Preserve the old execution quarantine and four foreign UI-memory files.

Next construct and verify the complete selected entry/runtime view at the fixed
session path, then exercise actual PAM open/close, logind scope/seat/VT and the
native desktop through this authenticated backend. Observe actual executed image,
session and first frame separately from the scheduling ACK. Dedicated production
greeter/account/MAC/startup policy, graphical login and full first-version scope
remain open. The USB gate and NexaUI maintenance-only status still apply.
