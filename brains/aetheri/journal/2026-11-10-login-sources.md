# Protected login sources — 10–15 November 2026

The C17 source verifier opens a separately selected set of protected login
files. It reuses `core/root`, the OpenSSL SHA-256 adapter and the x86-64 ELF
verifier. The handle retains the original parent and file descriptors and
checks the named objects, ancestor identities, exact raw file labels, metadata
and capability absence again when used. A detected change fences the whole set.
The original service and the actual loaded executable/configuration still need
to be joined to this observation before the greeter sends credentials.

## Native evidence

The guest fixtures use the bytes of the previously rebuilt greetd candidate,
880,048 bytes, SHA-256
`464c907e69b8a2e7b74998839525c6985ed32bda05a0bcea44de583b9e6c9c9a`.
The host checked the archived backend digest before transferring it. The
controller copied the verified byte stream into fresh Root-owned paths under
`/opt/aetheri-login-source-tests`. The fixture config sets `source_profile`
false and is read only as data. The fixture uses existing SELinux labels under
enforcing policy. The backend was never started for these checks.

- The first Release and ASan/UBSan builds and all three login CTests passed on
  the original boot `8b915c0c-ab36-4cce-a8a3-e84e1c12627c`. The explicit native
  fixtures `f21ab6ef9c96` and `52e41c03f145` each passed all 16 cases as UID1000.
  These exercise bad content, incorrect ELF, mode/owner/label mismatches,
  hard links, symbolic links, a writable ancestor, and live observations of
  changes to bytes, mode, label, capability, file and parent directory. Every
  live case sees the change and repeated rejection after restoration.
- The `-fanalyzer` compile of the actual source translation unit exited zero.
  A separate ordinary-user build omitting only the data digest rejection failed
  the exact source-construction assertion with exit one.
- The private mount namespace bound a same-filesystem config with identical
  bytes over the selected file. The ordinary test rejected it. The same original
  path passed outside the private namespace after the child exited. See the
  receipt at `/var/tmp/a-login-source-mount-1110a6ef5580` in the carrier.
- The final Release build `/var/tmp/a-login-cmake-9bb758c894e2` added the tests
  for mutable caller strings and further input bounds. Its three login CTests
  and all 16 cases in `7da5cf3ee81e` passed. The corresponding ASan/UBSan build
  `/var/tmp/a-login-cmake-0bd800d6ace2` and its three CTests passed before the
  interruption. Its final 16-case run `8239ce1e3a14` passed after restarting the
  registered carrier on boot `fdc84399-7306-4760-8605-a9fd638cb9a3`.
- `tools/validate_contracts.py` passed 374 assertions covering 14 API methods
  on Windows. No catalogue or wire contracts changed.

Host logs and hash-bound build inputs are under `local/login-review-2026-11-10`,
with `sources-` prefixes. The first archive hash comparison stopped locally
because the expected string contained an extra `c9`. The retained archive and
the prior independent receipt agreed. The initial mount controller reached the
expected mutant failure and analyzer success, then hit the Linux argument length
limit before starting the mount child. A new controller sent only the small
mount-specific selection and recorded the successful private namespace case.

On resumption, the original QEMU PID 5548 was absent and QMP verification failed.
`qemu-img check` exited zero. The inactive socket was retained under
`qmp-retired-5548.sock`. The controller reverified the registered inputs and
started PID 16628. The final audit matched the four submitted code/build files
in both builds, observed flags on both instrumented translation units, verified
all six CTest outcomes and the two final 16-case receipts, and found the same
RPM inventory and no mappings of selected build or fixture files.

The next login step is to bind the protected file set to the actual running
backend and its loaded settings, and admit the original greeter. Follow that
with a PAM authentication and local graphical session test. The full first
version remains the release criterion for the next USB build.
