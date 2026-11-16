# Dedicated greeter identity and original PAM closure

The login path now runs with a dedicated locked greeter account and real greeter
PAM/logind sessions. Actual testing exposed a shutdown race: the daemon could
exit before its Root worker closed PAM. The optional backend integration now
keeps original worker ownership through teardown and fences abnormal/lost worker
ends in both shutdown and the normal reaper. This is disposable-carrier
integration evidence; normal deployment policy and the graphical greeter remain.

## Identity and native admission

The reviewed sysusers recipe reserves aetheri-greeter with no login shell,
/var/empty and fully expired/locked normal login. Actual systemd-sysusers dry-run
then enrollment selects UID/GID 992. Its receipt is
/var/tmp/a-greeter-account-98f6a74b36e7/receipt.json. Other passwd/group/shadow/
gshadow entries were compared byte-for-byte in memory and remained unchanged.
No account/password-derived bytes are logged. The account remains reserved;
never delete/recreate or implicitly reuse it.

Native greeter admission copies an independent strictly ordered set of up to 16
supplementary GIDs. Every identity check compares the actual bounded set exactly;
primary GID, zero capabilities, original parents/cgroups, labels and self image
remain separate checks. Model tests cover empty/exact/wrong/extra/duplicate,
ordering, bounds and errors. The actual client additionally rejects an empty
expectation against its nonempty groups. A separate real extra-membership
injection remains work.

The internal PAM profile first gates the exact service account name. Its
account-stage permit is deliberate: u! expires ordinary login, while this named
internal session must still open. It never unlocks the user or authenticates a
human. Password changes deny. pam_loginuid precedes pam_systemd, registering
class=greeter/type=wayland/desktop=aetheri. Direct libpam tests at
/var/tmp/a-greeter-pam-f752a02c1df7 accept only that identity and reject the human
and unknown identities; removing the name gates fails the unchanged probe.
The first pam_unix account-stack draft correctly rejected the expired sysuser;
that failure is preserved rather than unlocking it.

## Backend and ownership

See [shutdown contract](../../../spec/login-backend.md#original-session-worker-shutdown).
The existing 34-crate/Tokio 1.53.3 graph is unchanged. No unsafe block, worker thread
or external library is added by aetheri_close.rs. Its source is GPL-3.0-only;
independent Aetheri C code remains MIT. Rust/Cargo/LLVM stay build-only.

The external worker blocks TERM/USR1/CHLD before PAM/fork, retains its original
parent and synchronously waits for its own unreaped child. TERM requests child
termination; USR1 can force that child, never the Root PAM worker. After child
reaping, the worker completes PAM close/delete-credentials/end with termination
signals blocked. The later ordinary exec cannot inherit its privileged control
socket. An original zero worker result is required before dropping current
ownership or starting a replacement/scheduled session. The sole reaper shares
the Context lock with those operations; abnormal exit or ECHILD with a current
slot holds the daemon/slot/lock and reports pending once. No reaped PID is signaled.

The companion service settings are KillMode=process, SendSIGKILL=no,
TimeoutStopSec=infinity and Restart=no. Their actual PID1 values are checked.
The optional source preparer still rejects changed integration sites; the
ordinary non-feature build is retained.

The feature build also propagates worker/main errors as process exit1. Upstream
otherwise prints the error and returns zero. The ordinary-user comparison at
/var/tmp/a-worker-error-exit-201421a5c1aa passes the same closed descriptor to
preceding and corrected protected programs: old exit0, corrected exit1. No PAM
session or credential is opened. Its first diagnostic assertion used the wrong
libc error spelling; that terminal failure stays separate.

Final source is /var/tmp/a-session-close-build-57162fb98028. All 2610 planned
input files match current preparation. Offline Release build passes 18 greetd
and 3 inish unit tests plus one doc test; final no-feature check
/var/tmp/a-session-close-status-default-c0a211d7701f passes. Only the three
pre-existing compiler warnings remain. The protected final actor is
/opt/aetheri-worker-test-b8d0e86e07bb/greetd, 895896 bytes, SHA-256
ceff41c3a0b869b18b15f1c7a386526d54fd30cee7fc71a21d96596c168a61e7.
Root stages host-selected ordinary-built bytes create-only and never executes
user-writable outputs or sanitizer SDK libraries. Direct libraries remain PAM,
PAM-misc, libgcc, libc and the selected protected OpenSSL3.5.9 libcrypto; no RPATH.

## Executed cases

The same enforcing audio VM uses boot fdc84399-7306-4760-8605-a9fd638cb9a3,
QEMU 16628/UUID 033ac49e-68bb-47a3-9c9c-26a812dc8300 and kernel 6.19.10.
Tests use unit-private PAM/account files and fresh synthetic secrets; host
password state stays unchanged. The root backend remains in the administrative
carrier domain; only the finite startup helper has its candidate dedicated type.

| Token | Backend | Result |
| --- | --- | --- |
| 96b5c8673d3d | Preceding2ae/d432 | Memory-client desktop and complete normal teardown |
| 8fd37d4a509c | Preceding2ae/d432 | Desktop end, deliberate original daemon death, normal worker/PAM closure |
| 9bb5f3ed3237 | Preceding2ae/d432 | Ignored child TERM keeps stop pending; worker-owned force closes PAM normally |
| d7d8807f4b9d | Preceding a835/32c9 | Memory-client desktop and complete normal teardown |
| 5a3136001428 | Preceding a835/32c9 | Deliberately killed current worker is fenced; no replacement, stop remains pending |
| b4b8ecb620ef | Final571/b8d0 | Memory-client desktop and complete normal teardown |

Only main.rs differs between a835 and final571; the worker/reaper/signal modules
are identical. The final build additionally fixes error exit propagation.

Each visible desktop is captured/viewed before the explicit observation marker.
Actual greeter UID/GID/groups992, zero capabilities, loginuid992, seat0/VT8,
class=greeter, Wayland scope and runtime0700 are checked through retained original
worker/child pins. The user session has UID1000 and class=user with the selected
entry/compositor/desktop runtime. The privileged control-socket inode is absent
from the ordinary child's FD inventory.

The five complete cases prove ten original greeter-worker exit codes0 using
PIDFD_GET_INFO, PAM open/close and inactive scopes before private shadow cleanup.
The deliberate abnormal case instead proves original exit9 and missing close,
the pending diagnostic, unchanged live daemon/invocation and no replacement.
After that observation its original daemon pin is used for explicit
administrative termination. This never counts as a normal PAM close or recovery;
its private holder, fragment and stale socket remain.

The final native C sources match three 784-input snapshots: Release e09eb7aed37f,
memory c5f7963b9aa2 and test-only ignored-TERM Release1354c766ef2a under
/var/tmp/a-login-cmake-*. Each passes 9 selected login/session CTests. The latter
profile's test client is never installed. GCC analyzer and the group-comparison
omission counter at /var/tmp/a-greeter-group-counter-806b56226e43 pass; the
unchanged assertion detects the mutant with exit 134. All 13 host preparer tests
also pass. C snapshots precede later Python/Rust/document changes; do not claim
they are complete snapshots of the final repository.

## Failed attempts, audit and retirement

Preserve old e3d117208dad's concrete missing-PAM-close failure and provisional
beb01704729a's incorrectly escaped private shadow-copy newline. Neither is final
acceptance. Earlier Rust drafts and their buffered-output test failure remain.
The provisional891800-byte backend/00fd7c1e1295 flow predates the control-FD fix.

The first daemon-death case43b05e81c2d1 reached desktop/user teardown, then failed
its controller's obsolete socket-absent assertion after deliberate SIGKILL.
Replacement PAM logs show close, but original worker pins were lost before
normal-exit confirmation: no recovered original-pin claim follows. Its stale
/run/greetd-99045.sock stays untouched. The corrected fault controller retains
the original socket while live and removes only that exact object after worker
closure. Case410f799e8e1a timed out awaiting visual acknowledgement during the
interruption. Independent current observation later proved its backend, user
scope and recorded processes terminal, then retired VT8; it remains a failed
test with private inputs/fragment preserved. Partialb9d5ed26fdbb refused before
backend dispatch and keeps its private preparation. Never replay these cases.

Case98dade58c413 reached successful authentication/Start and user-session entry
but failed the controller's first greeter exit-information assertion. The daemon
had accepted the original normal result and started the user session. A separate
32-child probe confirms that a readable original pidfd can still return mask7
without EXIT; after parent wait it returns mask12 and exit0/256. The retained
Linux6.19 pidfs source publishes that cached information during reaping. This is
a test-observation race, not demonstrated abnormal worker exit or a PAM failure.
The controller now waits up to two seconds on that same original pidfd for EXIT,
records masks and only then checks the status; unavailable information still
fails. Independent fresh administrative pins subsequently closed the98 user's
entry/PAM/session, replacement greeter and original backend normally. Its
administrative-reconciliation.json does not restore the lost original controller
pins or turn the failed test into acceptance. Its private holder/fragment remain.

Final audit a52adef6ccc5 matches all 2610 final backend inputs, all 784 files in each
C profile, current relevant C sources, all200 protected desktop runtime files,
selected helper/provider hashes and normal/fault receipts. It checks the locked
account, PAM profiles, unchanged RPM list, no selected process mappings or local
seat and enforcing1/tty1. It allows only the two identified retained stale
sockets: old99045 and expected-abnormal105462. A stale name is not authority for
cleanup. Foreign UI-memory hashes remain unchanged.

The temporary startup candidate was loaded separately at policy1be950e4ffff,
78c5b6220a42 and942948553f1b. Each exact input was retired only after no typed task/active login
attempt/local seat remained. Complete baseline policy is restored:3689381 bytes,
SHA-256 a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6.
Independent post-retirement audit /var/tmp/a-login-account-final-4ae465ac4e29
confirms all three module retirements, no active login actor/typed task/local
seat, enforcing mode, tty1 and the unchanged RPM inventory.
Failed fixtures/evidence stay; no module, production service or global PAM profile
is installed by this work. Local evidence is under local/login-review-2026-11-10
with closed-account-*, session-close-*, worker-backend-* and startup-policy-* names.

Next prove actual extra-membership refusal and supply dedicated backend/worker/
greeter MAC/device policy, then a normal deployment provider and the original
graphical greeter. General app lifecycle/recovery/removal, hardware and the full
first-version scope remain open. No USB image/flash or NexaUI handoff occurred.
