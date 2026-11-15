# Running login executable — 15 November 2026

`ae_login_loaded_match_image` compares the original active socket peer's
current executable with the protected source entry selected by the same loaded
unit. It reuses the existing ELF and SHA-256 checks. The lower-level observer
now accepts an internally authenticated process descriptor and its exact
expected context. The established public bus-peer and self-image entry points
keep their separate admission rules. See the
[executable contract](../../spec/login.md#running-backend-executable).

## Native tests

The registered audio lab ran at QEMU PID16628 on boot
`fdc84399-7306-4760-8605-a9fd638cb9a3` with enforcing SELinux. Final Release
`/var/tmp/a-login-cmake-72de9273b234` and ASan/UBSan
`/var/tmp/a-login-cmake-c70a679ebad5` both passed three login CTests.
The eight explicit Release cases are recorded at
`/var/tmp/a-login-loaded-b2eccc2cc865`. The five ordinary-client cases with
ASan/UBSan are recorded at `/var/tmp/a-login-loaded-109364393b56`.

The short-lived administrative verifier accepted the exact running file. It
rejected and fenced an independently staged executable with the same bytes and
a different file identity. The fixture used a private unit mount to present
that separate file at the same path. The ordinary client was denied the
administrative API without invalidating its valid loaded-unit observation.
Those cases exercised inert server processes under actual systemd supervision.

The administrative verifier and crypto library were staged under a fresh
root-owned `/opt` directory and checked against host-provided SHA-256 values.
The host derived the library bytes from the pinned Fedora OpenSSL 3.5.9 RPM
and the fixture checked the loader paths and direct dependencies. The
privileged tests used the Release verifier. ASan/UBSan covered the ordinary
client regressions and the affected code was built with instrumentation.

The existing self-image test passed ten native Release cases across UID/GID
0 and 1000, including wrong identity, object and digest, with empty capabilities.
The first standalone compiler command lacked the SDK header path and stopped
before compiling. The successful command used the existing target's SDK paths.
An initial host fixture-preparation run also stopped before guest dispatch
because it reused the variable name for the loaded build record and the ELF bytes.

The final audit compared the eight submitted code/build inputs in both builds,
confirmed all seven inert services terminal with archived fragments, and found
the original RPM inventory and no selected runtime mappings. The Windows
contract check passed 374 assertions covering 14 methods. Host logs are under
`local/login-review-2026-11-10/loaded-current-*` and `loaded-live-current-*`.

The observation currently stays local to the administrative process. A login
integration needs an instance-bound handoff to an admitted greeter and a way
to attest the configuration greetd actually consumed. The pinned upstream
implementation reads `--config` before entering its server loop and passes
the constructed socket path to its greeter through `GREETD_SOCK`. That path is
a discovery input for a later verified connection. PAM and graphical session
tests follow the complete admission path.
