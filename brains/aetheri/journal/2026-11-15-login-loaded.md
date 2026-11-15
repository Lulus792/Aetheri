# Loaded login unit — 15 November 2026

The C17 loaded-unit guard joins an actual Unix peer and its original active
systemd invocation to the protected source selection. It observes the exact
loaded fragment and ordered drop-ins plus the original `ExecStartEx` command
record. All reads address the unique bus name bound to the original PID1.
The [contract](../../spec/login.md#loaded-login-unit) defines the remaining
checks before credential transport.

## Native checks

The registered audio lab ran under QEMU PID16628, boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, SELinux enforcing. The first CMake
configuration inherited an audio prefix in `/run` from the previous boot and
stopped before compiling. The login-only test profile disabled the unrelated
audio options and reused the reviewed C and crypto SDK.

The first inert Root unit exercised the exact command. The client rejected the
unexpected Fedora package drop-in at
`/usr/lib/systemd/system/service.d/10-timeout-abort.conf`. The 596-byte drop-in
sets `TimeoutStopFailureMode=abort` and has SHA-256
`ae6b234f92bc22f1201a7572b59b454c9809f33c80d13f361b9674e1801acc37`.
The controller observed the original process exit. Its later fragment rename
failed with EXDEV across `/run` and `/var/tmp`. A separate audit checked the
terminal unit, archived the exact fragment bytes and retired the original path.
The revised implementation admits only the independently selected and verified
drop-ins in exact manager-reported order.

The final Release and ASan/UBSan builds were
`/var/tmp/a-login-cmake-0f5e59b9d98e` and
`/var/tmp/a-login-cmake-9d1955768350`. Both pass three login CTests and all five
explicit native client cases. The accepted command uses its actual protected
program and config. The same content at a different path is rejected. Omitting
the actual Fedora drop-in, adding the systemd ignore-failure flag or an actual
pre-start command also rejects. ASan/UBSan flags were checked on the submitted
loaded, service and source translation units. The Windows contract validator
passed 374 assertions over 14 API methods.

The UID1000 client received EACCES opening `/proc/PID/exe` for the actually
connected Root peer in both profiles. The Root controller independently
observed the selected file as the current executable, original invocation,
socket PID and live pidfd before the inert quit. This is a fixture measurement
with the installed Fedora policy and the observed UIDs, rather than a claim
about every possible production configuration.

The two final receipts are in `/var/tmp/a-login-loaded-43f02aa2cc94` and
`/var/tmp/a-login-loaded-90b4a0ecb554`. The host keeps the hash-bound build inputs
and execution logs in `local/login-review-2026-11-10/loaded-*`. The closing
audit checked nine submitted source/build files in each build. All seven inert
units were terminal, their fragments archived, and selected runtime mappings
absent. The RPM inventory matched the baseline. Fixtures exercised an inert
program and its configuration without authentication or session creation.

Next establish a narrowly scoped way to obtain current Root image and consumed
configuration evidence without giving the greeter cross-process inspection
privileges. The loaded-unit observer supplies a binding to the original service
instance for that work. The next USB build still depends on the first-version gate.
