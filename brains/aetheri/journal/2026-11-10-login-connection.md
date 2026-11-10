# Native login stream and original peer

10 November 2026. The original C conversation now has a bounded Linux connected-
stream adapter. Native kernel/fault cases and all three CMake login tests pass
in Release and ASan/UBSan/leak builds. The actual daemon/configuration/greeter
admission callback, deployed service and real PAM/session transitions remain work.

## Boundary and implementation

The preceding goal turn made implementation progress: the pinned greetd build and
offline preparer were committed/pushed as 565a9c3. This continuation revalidated the
same restricted audio lab UUID 033ac49e-68bb-47a3-9c9c-26a812dc8300, QEMU 5548,
boot 8b915c0c-ab36-4cce-a8a3-e84e1c12627c and enforcing 1. Original p0 remains closed.

The independently reviewed source's server creates its listener as Root and then
chowns the socket to the greeter. Connection-time credentials alone do not prove
its current process. Linux 6.19 sock.c and pidfd UAPI plus the actual carrier were
checked for SO_PEERPIDFD/PIDFD_GET_INFO behavior. No numeric-PID fallback is used.

`src/login/connection.{h,c}` owns the connected CLOEXEC socket, kernel peer pidfd
and fresh model. The independently selected original pin must match the kernel
socket pin by pidfs object identity. Current real/effective/saved/filesystem UID/
GID, liveness and the mandatory caller-owned admission check precede effects;
receive bytes are rechecked before application. Snapshot success is not a lock
against later drift. The compiled callback still owes original image/config/
supervisor/policy/greeter admission; the test fixture grants none of those.

One receive-first pump reads at most 4,096 bytes and attempts at most one send.
MSG_DONTWAIT/NOSIGNAL/CMSG_CLOEXEC, sent-prefix wiping and whole-model UNKNOWN
fences preserve the conversation's uncertainty semantics. All ancillary input
rejects; delivered rights/pidfd descriptors close, including truncated control
data. The kernel closes undisclosed truncated rights. No received FD is accepted
as an app/native grant. Owner process/thread checks, callback reentry/close
rejection and fork-local close keep the lifetime explicit. Close has no cancel,
reconnect, process stop, drain, lease or restart-reconciliation effect.

No new production library family, daemon/thread/timer or installed API is added.
The opt-in login CMake profile builds the separate library; pthread is used only
by its wrong-thread test. Existing model/API/SQL and the old runtime quarantine
remain unchanged. No real username/password, PAM profile, account, service or
production policy is installed in this work.

## Actual tests

Final standalone source `/var/tmp/a-login-connection-1261ddbbb984`, ordinary work
`/home/aetheri/login-connection-1261ddbbb984`, passes both Release and memory
compilation/execution with strict warnings and GCC's analyzer. Preserve the first
duplicate sys/pidfd.h inclusion compile failure and the second analyzer finding;
the test includes that header once, and process-info output initializes before
any early return. No compiler warning or sanitizer is disabled.

Real socket tests cover fragmented synthetic prompts/authentication/fixed start,
explicit denied/cancel/acknowledged retry, partial transmissions and prefix wipes,
EAGAIN/EINTR, early input before send, truncated/oversized replies and permanent
loss of the mandatory check. Complete 253-FD and deliberately truncated control
messages reject without descriptor leaks. Original peer exit is tested with a
child-created socketpair: both endpoints are retained independently after the
original child exits, yet no new input from that surviving socket is accepted.
Wrong process/thread/reentry and fork-close restrictions pass. Scoped syscall
faults affect the included test subject only, not the sanitizer or product target.

The same-UID/wrong-live-process mutation removes only the independently selected
pin comparison. It compiles, then fails the exact ESTALE constructor assertion,
exit 134. Source `/var/tmp/a-login-connection-counter-d50e940987f8` remains test-only;
core dumps were disabled for that counterexample, not sanitizer coverage.

Pinned CMake Release source `/var/tmp/a-login-cmake-78805bec1b16` and memory source
`/var/tmp/a-login-cmake-b587031f9946` each configure/build the actual library and
pass login.protocol/login.wire/login.connection with no skips. Exported compile
commands prove actual instrumentation on all four selected translation units in
the memory variant. The independent Python protocol peer still passes. Preserve
the earlier configure failure whose copied fixture omitted the required protocol
directory; the corrected copies include it, without bypassing its hash checks.

## Evidence and next step

Protected evidence
`/root/aetheri-login-connection-evidence-a4f513a237b04ba9aa8f678dcfbaa79a`
exports 5,646,636 bytes, SHA256
`4e724abdf272d5b3121da40edcf6db6224ed0b160cefe2c782cd2f4967e46938`.
Host `local/login-review-2026-11-10/connection-audit-v1.json` independently verifies
all 1,572 recorded files plus its manifest and all 1,506 submitted source inputs.
Final connection/model bodies match the current worktree; CMake recipes, binaries,
archives, test logs, source failures and the counterexample are retained.

Original RPM list/udev 693/invocation, enforcing 1 and tty1 remain unchanged; no
selected protected-runtime/backend-review code remains mapped. No local seat or
fixture PAM session remains. Preserve the four foreign UI-memory files. Native
fixtures establish transport/model behavior, not authentication, normal app
admission, trusted lock/suspend, installed sessions, hardware or first-version
acceptance. Next implement the real protected daemon/configuration/socket/greeter
admission context and bounded initial connection, then test PAM rejection/cancel
and greeter-to-user transition with the built candidate. USB and NexaUI gates stand.
