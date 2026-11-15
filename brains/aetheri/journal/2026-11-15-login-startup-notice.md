# Login configuration startup notice — 15 November 2026

The opt-in pinned greetd build sends a bounded status notice after parsing its
configuration. It uses the same byte snapshot for parsing and SHA-256, the
original backend still owns PAM, and the unprivileged greeter can compare the
notice through PID1 once the receiver is integrated. The notice carries only
the invocation ID and the digest. The new module adds no Cargo dependency and
uses the reviewed OpenSSL 3.5 LTS runtime.

The ordinary-user offline build `/home/aetheri/startup-build-fd2ada661a47`
passed eleven tests and one doctest, including the independent `abc` SHA-256
vector and rejected invocation identifiers. The protected stage held 2,608
inputs, independently checked against the host. The only differences from the
default prepared tree were the new module and its three integration sites.
The executable measured 885,184 bytes, SHA-256
`411a1631dcdf6ab82d83cedfdc1ce4f019467eb6baec24230917326321e862a8`.
The direct dependencies were PAM, PAM misc, libcrypto, libgcc and libc.
The original thirteen host preparer tests and 374 contract assertions passed.

The native preflight `/var/tmp/a-startup-notice-test-f1172ca93926` ran the actual
backend as UID1000. Both valid configurations produced the exact expected
digests over pathname and abstract sockets. The files differed only by a
comment. Fifteen rejected cases produced no notice, including automatic login,
user profile loading, write access, a symlink, FIFO, oversize, bad encoding,
argument overrides, missing or invalid notification context and old libcrypto.
A `Type=exec`, `NotifyAccess=main` systemd service recorded the expected status
with its actual `InvocationID`. The test selected an absent PAM service and
observed the corresponding pre-PAM exit. The controller retired the exited
fixture and confirmed no selected mappings remained.

The first build failed at CHDIR because `PrivateTmp` hid the source path under
`/var/tmp`. The next build bound that source in read-only. The initial direct
tests sent the requested notices, then hit greetd's existing memory-lock limit.
The first systemd test also failed to execute the user-home build. The final
test used a digest-verified protected staging path and a fixture-local 64 MiB
memory-lock limit. The earlier failures and their logs are retained. The carrier
remained enforcing, the RPM inventory matched the baseline, and all five test
and build units were terminal. The integration did not install a PAM policy or
use real credentials.

Next consume `StatusText` alongside the loaded-unit checks and the original
live invocation, then join the protected executable and greeter observations.
A full authentication and desktop session test remains part of the goal.
