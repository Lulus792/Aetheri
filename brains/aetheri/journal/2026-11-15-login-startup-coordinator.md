# Native login startup coordinator — 15 November 2026

The new C17 `login/startup.h` composes original service observation, selected
mount view, protected source/loaded guards and one-time handoff publication.
Actual authenticated desktop tests now use its native namespace selection;
the controller no longer invokes nsenter to place the publisher. This advances
the startup implementation before installing a finite helper or effective
deployment/greeter policy. See the [contract](../../../spec/login.md#native-startup-coordination).

## Ownership and effects

The constructor accepts an already connected stream and independently selected
Root service/source/destination expectations. It requires a compiled external
deployment gate. The existing service guard owns the socket-authenticated peer
pidfd and PID1 context. The coordinator obtains the mount namespace through
PIDFD_GET_MNT_NAMESPACE on that original pin, verifies its nsfs type, retains it
and enters it with setns(CLONE_NEWNS). No caller PID or peer namespace pathname
is accepted. Source/loaded/destination observations are created only in that
view. Later checks compare both the current peer and self namespaces against the
original object and recheck service/config/current ELF and external policy.

All real/effective/saved UID/GID values must be zero; /proc/self/task must expose
one thread. Future threads, privileged mutation and borrowed gate-context changes
remain excluded by the caller. Every constructor error requires caller exit
because namespace entry may already have happened. Cleanup never restores it.
The module sends no login protocol request and adds no daemon/thread/dependency.
It uses the existing kernel, glibc, libsystemd, SELinux and crypto foundation.

Only an initial missing consumed-config notice is pending. Other gate/namespace
losses fence permanently. Publish consumes one attempt before checks/effects,
calls the existing create-only flushed handoff publisher, then rechecks namespace
and external admission. A failure retains any possible record and never permits
republication, deletion, adoption or inferred rollback. A healthy completed
attempt returns EALREADY on repetition. Closing releases only local guards/FDs.
These are ephemeral startup observations, not durable operation recovery.

The current administrative fixture gate checks enforcing policy only. It does
not establish dedicated account/PAM/MAC/device/DSO/destination admission. The
test uses the carrier's existing unconfined_service_t role and ordinary UID 1000
greeter. No installed startup service, production greeter or normal app admission
is claimed. The library deliberately requires the external gate instead of
silently treating metadata checks or a test callback as complete policy.

## Executed verification

Use the same enforcing audio carrier, boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, QEMU PID16628. Final CMake sources are
`/var/tmp/a-login-cmake-a2b97dd9e187` (Release) and
`/var/tmp/a-login-cmake-c92c0f4e6733` (ASan/UBSan). Each builds the actual new
library, startup model, loaded publisher and existing greeter/session targets,
and passes six CTests: login.protocol/wire/connection/startup and
linux.session/session_entry. The ordinary model tests cover actual nsfs FD type,
source failure after entry, denied/failed namespace entry, nonzero saved IDs,
actual concurrent-thread constructor rejection, wrong-thread/fork/reentry,
pending config, policy-EAGAIN loss, namespace-drift latching, publication failure,
post-publication policy loss and one-attempt/no-replay behavior.

Final counter source `/var/tmp/a-startup-counter-deeca3d8d798` compiles the current
good startup body with GCC analyzer, exit 0, then removes only the namespace
mismatch rejection from a separate ordinary-user object. The unchanged current
model detects it, exit 134, at its expected ESTALE assertion. The mutant is not
installed or executed as Root. Earlier counter bdb2e57af34c is retained with the
preceding model object; the final counter uses the complete current test build.

Final native Release fixture `/var/tmp/a-login-desktop-70dd832a9752` and memory
greeter fixture `/var/tmp/a-login-desktop-b5660e8d706b` invoke the Root-staged
Release publisher directly, without nsenter. Its native coordinator captures and
enters the original peer namespace; ordinary greeter consumption verifies the
same source fingerprint. Both complete the actual synthetic PAM authentication,
original user-worker pin, executed entry, active seat0/tty8 Wayland user scope,
selected compositor/desktop and first-frame flow. Both 1280x800 screenshots were
viewed, showing the development desktop and dock. Retained original entry/worker
pins establish ordered shutdown, pam_unix session close and inactive scope before
backend stop. Replacement greeters report no input and do not replay the consumed
credential OFD. Only the second greeter is memory-instrumented; entry/runtime and
privileged publisher are Release. Its explicit LSan check reports no leak.

The backend, 34-crate graph and 200-file protected desktop runtime stay unchanged
from the [previous authenticated test](2026-11-15-login-desktop.md). No personal
credential, package or global PAM/account/executable change is introduced.
Independent audit `startup-complete-audit` matches all 770 submitted inputs in
each final CMake tree, actual sanitizer flags and all 200 runtime bodies/aliases.
It verifies all five new native-coordinator session fixtures plus the two prior
failed sessions are terminal, no selected code remains mapped, no greetd socket
or local seat remains, successful holders are absent, RPM inventory is unchanged,
enforcing=1 and tty1. The two older failed memory holders remain protected and
unadopted. The old execution quarantine and four foreign UI-memory files stay
untouched. Inputs/outputs/screenshots remain in local/login-review-2026-11-10
under startup-coordinator, startup-desktop, startup-verified-desktop,
startup-complete-desktop, startup-counter and startup-complete-audit prefixes.

## Preserved failures and next integration

The first expanded model run failed in both profiles because the new final
publication check converted even a pre-effect pending config result into ESTALE.
The corrected branch preserves that pending return while still consuming the
attempt; post-effect uncertainty remains fenced. The original failures are
sources 0e8fda906ed7/3bb66ab906cb. Static analysis then explored a failed stat call
with unset errno and an apparently successful return. The helper now guarantees
a negative syscall failure, falling back to EIO; a controlled unset-errno setns
case verifies rejection. This is defensive handling of an analyzer/model path,
not evidence that the real kernel returned a failure without errno. Initial
counter cd2f02fed0c2 preserves that analyzer output. Counter 37cb0a27daed passed
analysis/compile but expected a Makefile link.txt in a Ninja build; the corrected
fixture reads Ninja's command as the ordinary user and links only ordinary code.

Next build the finite installed startup executable with independently selected
deployment inputs and service ordering, admit effective dedicated greeter/account/
PAM/SELinux/device policy, and connect the original graphical greeter. The active
backend observer requires no ControlPID/job, so an unchecked ExecStartPost in the
same backend unit is not the selected integration. The current test invocation
and enforcing-only callback must never become a production fallback. Keep normal
app/lifecycle integration, hardware and full first-version scope open. No USB
image build/flash and no new NexaUI handoff message.
