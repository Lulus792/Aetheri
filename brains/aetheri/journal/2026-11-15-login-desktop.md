# Authenticated private desktop session — 15 November 2026

The reviewed backend and original C test greeter now progress from actual
pam_unix authentication to a real local Wayland user session and visible Aetheri
desktop. Both final test profiles complete original entry/worker shutdown, PAM
close and inactive session scope before backend retirement. This is a private
integration fixture, not an installed display manager or production login policy.

## Entry and runtime

The session entry requires open stdin/stdout/stderr before closing inherited
extra descriptors. When attached to a console it directs diagnostics through
the existing libsystemd journal stream, closing its auxiliary descriptor before
session observation or child creation. No journal connection leaves the console
usable; duplicate/close failure ends startup. Non-console output stays intact.
The standard duplicates have process lifetime. No logger process/thread or new
dependency family is introduced. See the [entry contract](../../../spec/session.md#entry-from-an-existing-local-login).

The independently checked current runtime remains
`/opt/aetheri-test-runtimes/f1e806b8f2b74a9e84ca24e8dadafacc`, with all 200 regular
bodies, exact aliases/inventory and protected ownership checked against the
host-selected profile. Its fingerprint is
`be529628eb4039ba766c82cb409250d3bf4a07f7ac1bc67eb07f3e2ae2ffc5cb`.
An ordinary-user GCC build with strict warnings and analyzer binds that runtime's
program/font/library paths into the entry. Source is
`/var/tmp/a-bound-login-entry-7f7adb8752f1`. The host-selected 86432-byte result is
staged Root-owned under `/opt/aetheri-login-entry-87230ffd9159/aetheri/aetheri-session`,
SHA256 `c14b4f3852631b7c328348f5a3d577967801ab4a8b812898a890dd4aeb4898ab`.
Its only direct libraries are libsystemd/libc, without RPATH/RUNPATH. Only the
fixture's read-only `/usr/libexec` view supplies the literal login command.
There is no global `/usr/libexec/aetheri` installation.

The fixture's final pam_env entry selects protected LD_LIBRARY_PATH, EGL vendor,
DRI driver and libinput quirk paths, with user/environment-file reading disabled.
The existing owner selects software rendering and its bounded thread count.
These paths reproduce the previously reviewed runtime environment; merely
selecting libraries did not select the EGL vendor/data inputs. Base glibc/loader,
device/account/logind and system PAM remain carrier prerequisites, not a complete
Aetheri system closure. Existing missing Compose-locale warnings remain visible;
this test establishes no complete external IME/locale/input acceptance.

## Actual session transitions

Use the original enforcing audio carrier, boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, QEMU PID16628. The backend is unchanged:
greetd 0.10.3 with the reviewed startup/worker patch, 886472 bytes, SHA256
`109b9ff31401c0842d2a699bf33b8cac5c188d46a95a0c40fecd77826efce55e`.
Its Cargo graph remains 34 crates. PAM is 1.7.2-1.fc44; installed systemd-pam is
259.5-1.fc44, distinct from the selected runtime's259.10 library. No package or
host password/account/global PAM file changes.

The unit-private synthetic Shadow and sealed credential input retain the prior
[authentication boundary](2026-11-15-login-authentication.md). The test's `start`
mode reads the original credential OFD once, then closes it and wipes the answer.
A second PID1 OpenFile supplies a precreated Root0600 append-mode diagnostic
file; the ordinary test greeter redirects its console output there. This FD is
explicit test authority, not a protected audit service. Replacement greeters
inherit the consumed input position and report GREETER_NO_INPUT without another
Create request. No personal credential is used or stored in test output.

The selected free VT is 8. This fixture uses carrier devices and bounded
CAP_SYS_TTY_CONFIG for actual backend VT handling. SELinux remains enforcing,
with its existing administrative unconfined_service_t profile and ordinary
UID 1000 greeter. It does not implement a separate confined greeter account.
The private user PAM stack opens a pam_systemd user session and pam_unix session;
the greeter permit-only stack deliberately has no greeter logind registration.

Final Release `/var/tmp/a-login-desktop-b799225999e4` records original PAM worker
47588, actual entry 47607, session c3/seat0/tty8/type=wayland/class=user/UID 1000.
Final memory-greeter `/var/tmp/a-login-desktop-647d0920de2c` records the equivalent
session flow. The controller pins the original worker before Start, then the
executed entry and observes its actual credentials/cgroup/session. Independent
compositor and desktop executable checks and maps confirm the protected runtime,
with no user SDK mapped by those processes. In both cases the entry and desktop
are Release code; only the second greeter is memory-instrumented.

Both reach AETHERI_DESKTOP_READY. The 1280x800 screenshots were viewed: the dark
development desktop, pointer and Aetheri/Search/Settings/Terminal/Files dock are
visible. This is first-frame evidence, not acceptance of all application or UI
workflows. Explicit SIGTERM through the retained entry pidfd yields
AETHERI_DESKTOP_END status=ok, original PAM-worker end, actual pam_unix session
close and inactive logind scope. Only then does the controller stop the original
backend, restore tty1 and retire its original memory holder. Memory greeter
reports GREETER_LEAK_CHECKED with no sanitizer diagnostic. Both replacements
report GREETER_NO_INPUT and do not repeat the synthetic authentication.

## Native verification and retained failures

Final CMake sources `/var/tmp/a-login-cmake-534e57aaee5a` (Release) and
`/var/tmp/a-login-cmake-380f91e0cb87` (memory) each build the current greeter,
session entry and tests, and pass five CTests: login.protocol/wire/connection and
linux.session/session_entry. The independent audit matches all 767 submitted
files per tree and actual sanitizer flags against the selected inputs. Native
fork tests cover preserved output/fallback, both redirected standard streams,
auxiliary closure and missing stdio. A separate stdout-redirection omission
counterexample at `/var/tmp/a-entry-logging-counter-cae2a9a50f81` fails, exit 134;
the original entry passes GCC analyzer. The mutant is ordinary-user test code,
never an installed or privileged binary.

The first actual session35ee5b46518e reached the entry and DRM compositor but
failed desktop EGL setup because the new fixture omitted the already required
runtime data environment. Its terminal unit/journal remain. The next attempt
dce83cb40844 reached and displayed the desktop, but its controller expected
`compositor`/`desktop` instead of actual `aetheri-compositor`/`aetheri-desktop`
ELF names. Its original controller pins ended on that assertion. A separate
administrative observation joined the still-current backend invocation, worker,
entry ELF/account and c2 scope to newly acquired pins, then stopped the entry
and verified PAM close before backend stop. This is explicitly not recovery of
the original pins or a general recovery protocol. Preserve both failed Root-only
memory holders and the retained second fragment; do not adopt/clean them.

The first final visual-marker helper raced its intended entry shutdown after
publishing the marker: its unnecessary post-marker liveness assertion failed.
The original controller still completed successfully. The helper now checks
liveness before publication; the memory case passes. A separate attempted QMP
absolute-pointer event was rejected because this VM has no absolute input
handler. No application-launch/input success is claimed from that command.
Earlier header-path and analyzer-ownership build failures also stay retained.

Independent final audit `login-desktop-audit` verifies all four actual units are
terminal, no selected runtime/backend/entry code remains mapped, no greetd
socket/local seat/global fixture PAM or executable exists, both successful
holders are absent, the two new failed holders stay protected, RPM inventory
is unchanged, enforcing=1 and tty1. Receipts, local driver inputs and screenshots
remain under `local/login-review-2026-11-10` with login-desktop/session-handoff-final/
bound-login-entry/login-entry-stage/entry-logging-counter prefixes. Older failed
holders, the execution quarantine and four foreign UI-memory files are untouched.

Next implement protected installed startup coordination and effective dedicated
greeter/account/PAM/SELinux/device policy, then the original graphical greeter
and normal account/session/app transitions. The fixture demonstrates real
authenticated desktop entry without promoting the administrative test profile.
Normal app ownership/removal, lock/resume, hardware acceptance and full first
version remain open. No USB-targeted image build or flash; NexaUI handoff remains
delivered, catalogue maintenance only.
