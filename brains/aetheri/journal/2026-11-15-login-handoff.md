# Protected login observation handoff — 15 November 2026

`login/handoff.h` publishes a single protected record after the administrative
loaded-service, current ELF and consumed-config checks. The ordinary reader
verifies the exact protected file and reconstructs the observation through its
own retained live guard. The canonical format includes the original boot,
invocation, kernel peer, executable, configuration, source identities and
manager/process context. See the
[handoff contract](../../spec/login.md#protected-observation-handoff).

## Native evidence

The registered enforcing carrier remained on boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, QEMU PID16628. Final Release
`/var/tmp/a-login-cmake-0cd3ed8d0bc5` and ASan/UBSan
`/var/tmp/a-login-cmake-d37be57a37ba` built the actual targets and each passed
three existing login CTests. Four explicit live cases passed in each profile.
Root publication repeated the original observation, and a second publication
returned EEXIST. UID1000 clients read the record, verified it against the same
greetd instance and were denied writing or removing it. A wrong boot field
was rejected. An already open reader stayed fenced after observed mode drift
and subsequent restoration.

The actual backend used the existing protected startup-notice build with an
inert UID1000 test greeter and unit-private PAM files. The administrative
publisher used the good Release verifier and independently staged crypto
library. The memory profile exercised the ordinary reader with ASan/UBSan and
leak checking. These runs sent no credential or application protocol messages.
Their receipts are `/var/tmp/a-greetd-live-a3dffdc98c34` and
`/var/tmp/a-greetd-live-88eed50ebc1a`.

A separate ordinary-client counterexample omitted only the live-record
comparison. Its wrong-boot rejection assertion failed with exit1, while the
administrative publisher and backend stayed the good images. The case is
retained at `/var/tmp/a-greetd-live-0fd11452b258`. Static analysis of the actual
handoff, loaded and source translation units exited zero. An earlier standalone
counterexample link omitted the source library and stopped at the linker. The
corrected link completed as an ordinary user.

The final independent audit matched ten submitted code/build files in both
builds and confirmed actual sanitizer flags. It parsed all three current
296-byte records, compared boot/invocation/PID, encoded image metadata,
backend/config hashes and the manager/process context digest independently.
The first audit used D-Bus GetId for the authentication server ID and failed
that digest comparison. The pinned libsystemd implementation and actual
`busctl status` identify the correct server ID. The corrected audit passed
without replaying the runtime tests. Earlier uncommitted 264-byte prototype
records remain retained with their original fixtures.

All eight handoff fixture units were terminal, their cgroups empty or removed,
their exact backend sockets absent and their fragments archived. No selected
runtime mappings remained. RPM inventory, global PAM configuration and active
VT matched the carrier baseline. The Windows contract validator passed 374
assertions covering 14 methods. Host receipts are under
`local/login-review-2026-11-10` with `handoff-` and `loaded-handoff-` prefixes.

The next step is startup coordination that publishes the observation and admits
the actual original greeter before consumption and credential transport. The
complete policy and login-to-desktop tests remain part of the overall goal.
