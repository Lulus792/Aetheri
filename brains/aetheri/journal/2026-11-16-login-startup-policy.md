# Confined finite login startup — 16 November 2026

The finite startup helper now completes the actual authenticated desktop flow
in its own enforcing SELinux domain. This advances startup confinement; the
backend/PAM worker and ordinary test greeter still use the carrier's
administrative domain. No normal installed login policy/provider/account or
graphical greeter is claimed. See the [contract](../../../spec/login.md#candidate-startup-confinement).

## Policy and test boundary

`policy/login-startup.cil` defines the startup process/executable and dedicated
runtime/handoff types. It grants the selected read/namespace/bus observations,
the existing five capabilities and create-only publication in that directory.
It excludes shadow/home access, TCP/UDP creation, policy mutation, service
mutation and backend process ptrace. Native inspection still identifies the
particular original service, invocation, process, namespace and files.

The backend attribute has no normal member. The separately named
`login-startup-carrier-test.cil` admits only the existing administrative backend
type for these explicit carrier tests. Never ship that supplement as production
admission. The compiled test provider's enforcing-only callback stays test-only;
account/PAM/device/runtime admission and dedicated backend/greeter domains remain
required. Journal stream labels are not substituted for actual process labels.

The new C probe is excluded from default builds, installation and CTest. Its
same-byte administrative control and confined service have the same Root IDs,
five capabilities and NoNewPrivileges, without a private network, address-family
filter or read-only filesystem view. Synthetic targets are DAC-accessible in
the control. The confined process rejects six file opens (shadow, home read/write,
foreign runtime write, policy load and enforce write) and TCP/UDP socket creation.
Its own O_TMPFILE/create-only-link/label/flush publication succeeds. No personal
password, external connection, policy write or process signal is attempted.

`AE_LOGIN_TEST_CONFINED_STARTUP` changes only the explicit test greeter's expected
handoff label. The default remains the earlier fixture label. No normal runtime
selector, public API, library family, daemon or resident thread is introduced.

## Actual builds and desktop transitions

Use the original enforcing audio VM: QEMU PID 16628, UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, kernel 6.19.10-300.fc44.x86_64.
Its libselinux is 3.11-2.fc44 and targeted policy 43.3-1.fc44; no RPM was installed.

CMake source trees `/var/tmp/a-login-cmake-ed8ea3727e83` and
`/var/tmp/a-login-cmake-b35807aa7c04` each pass the eight existing login/session
CTests, with actual Release versus ASan/UBSan flags. All 779 submitted files per
tree are independently checked. These snapshots precede the final probe-only
corrections and policy refinements. The current probe is separately built as
ordinary UID 1000 and passes GCC analyzer at
`/var/tmp/a-startup-probe-a37309b58ca6`. Its protected selected ELF is 13,808 bytes,
SHA-256 `dc67d37e3526369da475f89bcac5c567cc76ad0d6fadddafbde7f36b68327923`.
Root never executes user-writable build output or sanitizer DSOs.

The actual successful flows are `/var/tmp/a-login-desktop-eea485ec2e6e` and
`/var/tmp/a-login-desktop-9e26f5cca3ad`. Their provider packages are
`/var/tmp/a-runner-package-63ed7b06142e` and
`/var/tmp/a-runner-package-81766bd08d85`. Both 130,672-byte startup programs are
host-hash-selected and create-only Root-staged with the dedicated executable
type, no RPATH/RUNPATH and only the previously reviewed direct libraries.

Each separately supervised helper runs as `aetheri_login_startup_t`, binds its
actual self MainPID, selects/observes the original backend, enters its original
mount view, publishes the dedicated-label handoff and exits successfully. A
deliberately wrong ambient DBUS_SYSTEM_BUS_ADDRESS still cannot redirect it.
The ordinary greeter consumes the record, authenticates a fresh synthetic
credential and starts the original selected session entry. The active user
session has seat0/VT8, Wayland and the actual UID 1000 scope. Both 1280x800 desktop
screenshots were viewed. Original entry/worker pins remain held through entry
end, PAM close and inactive scope before backend retirement. No credential is
replayed. The second ordinary greeter passes explicit LSan without sanitizer
diagnostics; both Root helpers and desktop runtimes are Release.

## Permission evidence and retained attempts

Four complete pre-session attempts are terminal and preserved:

- `77f81a3636f1`: PID1 denies `systemd_unit_file_t:service status` during actual
  GetUnitByPIDFD. Add status only; bus permission and `init_t:system status` alone
  do not provide it.
- `9bc34cc5ce2e`: pidfd filesystem observation needs `pidfs_t:filesystem getattr`.
- `e3e15ab5c73d`: original source paths on `/run` require tmpfs filesystem getattr.
- `c38d7567065e`: the protected source ancestor walk opens directories read-only;
  `/run/systemd` needs init_var_run_t directory read/open. Complete the selected
  root/run/library ancestor reads without granting file writes or manager mutation.

Each denial is recorded from the actual audit log with permissive=0, separate
from journal-stream metadata. The controllers prove backend/helper termination
and no local seat before returning to tty1. Preserve all four failed helper
fragments and protected synthetic-memory holders. Never replay their attempts.
The final policy also removes the initially unnecessary process ptrace grant:
kernel read-mode peer inspection uses the file-read check instead. The actual
successful namespace/desktop flows pass without process ptrace permission.

Earlier test-only failures remain separate: a missing protocols directory in
the first source export (`9c8325bf5344`), shell-stripped macro quotes
(`ef9dea679fe7`), a control executable lacking the selected role's entrypoint,
an incorrect getcon success comparison, and an xattr assertion that omitted
SELinux's trailing NUL. The latter already had successful control/confined
processes; it was an audit comparison failure, not a changed object identity.
The current probe explicitly performs the publisher's label operation. The first
standalone compile omitted the second SDK include root (`dca6a4a1ea61`); the
corrected build and analyzer pass. None required a permissive switch or a wider
capability set.

## Independent audit and retirement

The final direct probe at `/var/tmp/a-login-policy-3f4663ddae66-final` passes all
eight denials and its own publication. Root's read-only loaded-policy queries
confirm status allowed; start/stop/reload/enable/disable and peer ptrace denied,
with no permissive-domain flag. The independent audit checks six terminal
backend/helper attempts, both complete session receipts, all selected provider/
ELF hashes, all 779 source files per build, the 200-file protected desktop runtime,
unchanged RPM inventory, no selected mappings/socket/local seat and tty1.

The tested loaded candidate policy hashes to
`de502fbca0472b547c04ec42b31635bb818be148bfcd4a0dd7dac0efd48d7e33`.
The final assertion review targets policy-mutation neverallow at the actual
security initial SID (`security_t`), rather than kernel_t. Strict recompilation
produces the same complete loaded policy bytes; load_policy, setenforce and
setbool are also denied at that actual SID. This assertion correction
changes no runtime grant and does not replace the native denials.

The final assertion input is at `/var/tmp/a-login-policy-af6eeee1acce`; its
combined source SHA-256 is
`1bdf4489999ced43e02725567b13634aaeb40d7cabf3b95cec0af93435808847`.
Both candidate module runs were removed only after proving no typed task or
active attempt remained and matching the exact owned input. The restored
complete baseline is 3,689,381 bytes,
SHA-256 `a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.
Sources, receipts, failed fixtures and earlier quarantine remain. Local evidence
uses confined-startup-*, startup-policy-* and startup-probe-* prefixes under
local/login-review-2026-11-10. No USB image/flash or NexaUI handoff occurred.

Next supply dedicated backend/worker/greeter and account/PAM/device admission,
then the normal compiled provider and original graphical greeter. Full app
lifecycle, hardware and agreed first-version scope remain open.
