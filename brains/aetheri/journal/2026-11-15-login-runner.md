# Finite login startup executable and service — 15 November 2026

The native coordinator now has a separate C17 executable and explicit deployment
provider contract. Actual private login tests start it through its own systemd
unit; it publishes and exits before the ordinary greeter authenticates the test
user. Both final flows reach the visible desktop and complete original-pin
PAM/session shutdown. No installed production deployment or greeter policy is
claimed. See the [contract](../../../spec/login.md#finite-startup-executable).

## Selected inputs and lifetime

`login/runner.h` defines a compile-selected profile: startup unit/raw label,
reviewed socket prefix, bounded initial wait, backend/source/destination selection
and mandatory policy callback. AETHERI_LOGIN_STARTUP requires an explicit absolute
AETHERI_LOGIN_DEPLOYMENT_SOURCE implementing ae_login_deployment_select. There is
no default provider, runtime file/plugin selection or argument parser. This
avoids treating candidate metadata as its own trusted deployment selection.
The actual carrier providers are Root-created, host-reviewed/hash-selected C
inputs, with exact original backend/unit/config/greeter source expectations.
Their enforcing-only callback remains explicitly test policy.

The executable closes inherited extra descriptors before provider selection.
It uses the literal host system bus, captures its own authenticated peer and
original service context, and requires the compiled unit/label and its active
MainPID/no-control/no-job context before effects. Initial activation waits retain
that original context. Backend selection freezes PID1's unique name, unit path,
MainPID and invocation before waiting for the socket. The pathname is only a
connection hint. The actual kernel peer/service must match that frozen tuple
before the coordinator enters the original namespace or publishes. No login
protocol request, peer-PID reopening, reconnect, automatic service control or
publication replay is added. No process/thread remains after the helper exits.

The runner bounds readiness waits with one monotonic budget and one-second bus
call timeouts; the unit bounds lifetime with RuntimeMaxSec. Neither is a kernel
file-I/O cancellation/recovery protocol. Every runner return requires caller
exit because namespace entry may already have happened. Failure-phase diagnostics
contain no credentials and establish no admission or readiness authority.

The packaging template selects Type=exec and a separate backend dependency,
NoNewPrivileges, private networking/temp, read-only system/home, AF_UNIX only and
a bounded capability set. Its unresolved MAC-context placeholder has no default
permissive production role. No template was installed or enabled on Windows or
as a permanent carrier service. Test fragments remain explicit disposable units.

## Actual carrier integration

Use the original enforcing audio VM, boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, QEMU PID16628. Final CMake sources
`/var/tmp/a-login-cmake-d91974ce407d` (Release) and
`/var/tmp/a-login-cmake-f0a44d156719` (ASan/UBSan) each pass eight CTests:
login.protocol/wire/connection/startup/runner/runner_entry and
linux.session/session_entry. The model checks frozen manager/path/PID/invocation,
deadline/error bounds and rejection before the policy gate for an ordinary
caller. Entry fork tests verify actual extra-FD closure, failed-cleanup refusal,
missing stdio, argument rejection and preserved parent descriptors.

Counter `/var/tmp/a-runner-counter-9a1a6d2a4eb1` passes GCC analyzer on the current
good runner, then removes only invocation mismatch rejection from a separate
ordinary-user translation unit. The unchanged model detects it, exit 134. It is
never installed or executed as Root. Native contract cases at
`/var/tmp/a-runner-contracts-74833c13f012` reject missing feature dependencies,
missing/relative/directory provider selections. Both final compiled helpers also
reject execution outside their selected supervisor, without runtime-tree change.

Final separate-unit flows are `/var/tmp/a-login-desktop-cc32a3b49d97` and
`/var/tmp/a-login-desktop-102cbc5b89d6`. Their compiled provider packages are
`/var/tmp/a-runner-package-c15788073934` and
`/var/tmp/a-runner-package-0522cf1ec116`. Each actual CMake build runs as ordinary
UID 1000 using Root-protected source/provider inputs. The resulting bytes are
fetched to the host, independently hashed and Root-staged create-only before use.
Both 130672-byte executables have no RPATH/RUNPATH and only systemd, SELinux,
libcrypto and libc direct dependencies. The existing protected OpenSSL 3.5.9 is
selected; Root never executes user-writable SDK code or sanitizer DSOs.

Each helper's Type=exec unit uses the selected ordinary backend dependency and
an explicit administrative s0 process role. Even a deliberately wrong ambient
DBUS_SYSTEM_BUS_ADDRESS does not divert its literal bus connection. Actual
selection and completion markers precede terminal Result=success/ExecMainStatus=0.
The helper fragment is archived/retired after completion; its process is absent
while the user desktop runs. Ordinary greeter handoff consumption confirms the
same original namespace/source observation. Both actual PAM authentication,
entry, active seat0/VT8 Wayland user scope and first-frame flows pass. Both 1280x800
screenshots were viewed. Original entry and PAM-worker pins stay retained through
ordered end, actual PAM close and inactive scope before backend stop. The second
greeter also reports its explicit leak check without sanitizer diagnostics.

## Permission evidence and retained failures

The initial four-capability helper selected the backend but failed with EACCES.
The diagnostic build identified phase=connection. An independent pathname probe
recorded the actual socket owned by UID/GID 1000, mode0755; the pinned backend's
server source explicitly chowns it to the greeter. Linux stream-socket connection
requires write permission. DAC_READ_SEARCH does not supply that write override.
Adding only DAC_OVERRIDE to the selected set permits connection while retaining
the socket ownership/mode. SYS_ADMIN/SYS_CHROOT remain necessary for setns,
SYS_PTRACE for privileged-peer inspection and DAC_READ_SEARCH for protected reads/
AT_EMPTY_PATH linking. No setuid/setgid/kill/VT capability, global permission
change or permissive-policy switch was added. Dedicated MAC/device/account/PAM
policy remains required before product admission.

Preserve failed fixtures 0c8841a69e0f/8aa2fa8fcfb2, their failed helper fragments
and protected memory holders. No AVC was found by the bounded journal query;
this is not called a proven SELinux denial. Journald's stream SELinux label was
init_t while the process admission checked the configured actual s0 role; do not
substitute stream metadata for the native current-process label observation.
The first Windows dispatch 26c244076cc0 failed before SSH with command-length
error 206; later dispatch binds the host-selected Python body by SHA-256 over
stdin. It does not load a candidate script or loosen native admission.

Attempt 7e2e1f8bee62 reached the visible desktop after the socket-capability fix,
but the controller's visual-observation window expired. Its original pins ended
with the controller. A separate current-process administrative observation then
verified the original backend invocation, actual entry/worker/ELF/session and
new pins before ordered shutdown/PAM close. This is not recovery of the original
pins or a general recovery protocol. Retain that third new protected failed
holder. Final successful runs use a longer explicit test observation window and
retire their original holders normally. A first audit used the wrong provider
file directory; the corrected audit checks its actual output path and independent
host-selected provider/program hashes, without changing the fixture.

Final independent audit `runner-final-audit` matches all 776 submitted files per
CMake tree, actual instrumentation, all 200 protected runtime bodies/aliases and
the five selected compiled provider/program pairs. All five backend and helper
unit attempts are terminal, no selected code/socket/local seat remains, successful
holders are absent, RPM inventory is unchanged, enforcing=1 and tty1. Three new
failed holders, earlier failures/quarantine and the four foreign UI-memory files
are preserved. Local evidence uses runner-* prefixes in
local/login-review-2026-11-10. No USB-targeted image or flash occurred.

Next admit effective dedicated startup/backend/greeter/account/PAM/MAC/device
policy and supply the normal deployment provider, then build the original
graphical greeter and installed account/session integration. Compiling a fixture
provider or starting its disposable unit is not that admission. Full normal app
lifecycle, hardware and first-version scope remain open; NexaUI stays maintenance
only, with no new handoff message.
