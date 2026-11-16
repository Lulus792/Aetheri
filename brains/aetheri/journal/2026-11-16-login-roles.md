# Login roles in the enforcing carrier

16 November 2026. The candidate uses separate SELinux domains for PID1's
selected login backend, its external PAM worker and the ordinary greeter. The
worker alone declares shadow-reader membership and uses local audit reporting.
The upstream server's selected VT wait and reset remain available to the
backend. The worker owns PAM setup and teardown. The greeter has UID/GID 992,
an exact supplementary set of 992 and empty capability sets.

The release policy input is `policy/login.cil` together with
`policy/login-startup.cil`. The carrier adds `policy/login-carrier-test.cil`
for the observed early-boot `kernel_t` datagram receivers. The policy still
uses the existing pathname socket below `/run`. Deployment needs independent
placement, a narrower writable runtime and device selection, and effective
socket/PAM/greeter admission. The regression probe uses a dedicated inherited
append-mode diagnostic descriptor. It handles synthetic data only.

The combined carrier candidate `b491c9ffa6f8` has SHA-256
`200b3cc82720dc6e7389f53f48ee986c7c7aaff57946d2c09e6841c639286b62`.
The observed loaded policy has SHA-256
`da7a32701510d24346e959f5c40a7ff43cf26ef65ba52fdc2234d15486f9a4f4`.
The published read-only policy probe verified 109 decisions in run
`61e460cc9412`, including rejected wrong digest and shadow-read expectations.
An independent ordinary-user libsepol build compiled all 426 selected inputs
with `neverallow` enforcement, and rejected three forbidden additions in
`9ba57b674679`. Fedora's `expand-check=0` default for module management made
that separate compiler check necessary. The first strict build `8f1a384550c8`
detected the missing Fedora shadow-reader declaration. The final audit matches
every compiler input to the live module store. See the [libsepol CIL API](https://github.com/SELinuxProject/selinux/blob/3.11/libsepol/cil/include/cil/cil.h).

With the final policy, the first native four-case access check `86c84daf145b`
and internal PAM test `3bb23d41a74c` passed. Two unconfined controls had the same
DAC access to synthetic files. Backend and worker enforced the expected shadow,
home, policy and Internet socket boundaries. The live test reached an actual
greeter seat0/VT8 Wayland scope, observed each original label and image, and
stopped the original backend. The original greeter and PAM worker both reported
exit 0 through retained pidfds. PAM reported open and close. The scope became
inactive and the service ended successfully.

The host reboot interrupted follow-up verification. The original QEMU PID 16628
was absent and the owned qcow2 passed the read-only check with zero errors.
The stale host-local socket was renamed to `qmp-retired-16628.sock`, preserving
the object. The same registered carrier started as PID 14356, boot
`342ed7a6-bdd5-46e2-a047-fe6f2797ad8d`. The loaded policy digest matched before
new dispatch. A fresh access check `d9f1bda1010c` and PAM test `024fea4b714e`
passed using the final C probe that explicitly owns and closes its log stream.
New process pins belong to those new cases. The older `/run` fragments and
sockets were subject to the guest reboot's normal tmpfs lifetime.

The restart left a zero-length input in the old guest memory-build tree, so
the audit retained that failure and used a fresh build. The audit also caught
that the standalone probe had missed sanitizer flags in the initial Debug
configuration. The final memory build explicitly instruments the probe. Release
`8f18d465d65e` and memory `ef9b85404aac` each pass nine CTests and verify 788
source inputs. Release predates the memory-only CMake flags and the spec update.
The final probe passes GCC analyzer and the ordinary-user ASan/UBSan/LSan control.
Audit `bb6680aed1db` confirms those inputs, loaded decisions, module sources,
native receipts, no running role processes or local seat, tty1 and unchanged
RPM inventory.

Retirement of the exact candidate in the new boot restores the complete
3,689,381-byte baseline policy, SHA-256
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.

## PAM authentication with the split worker

The test greeter's `handoff-authenticate` mode waits for source, ancestry,
backend and record admission before sending the first PAM request. It receives
a real secret prompt, rejects an empty answer and confirms cancellation. A
fresh prompt precedes the sole read from the sealed synthetic input descriptor.
The greeter clears its copy after submission and confirms authenticated state
and cancellation. The disposable service sees a private `/etc` projection
containing the synthetic UID 1000 shadow hash and the unchanged locked greeter
entry. The controller compares the actual private shadow and inherited input
objects. It checks host account bytes, checks captured logs for the synthetic
input and retires the private holder after the original greeter and worker end.

Case `7b24269add98` reached a real prompt but had `AUTHINFO_UNAVAIL` for both
answers. The kernel denied `unix_chkpwd` execution by its PAM worker. This
failure exposed the distribution PAM's use of the helper for shadow accounts.
The standard helper receives its input through a pipe and returns its exit
status to PAM. Fedora's existing `chkpwd_t` domain owns its file access. The
candidate grants a transition from the PAM worker and access to its inherited
pipes. The worker cannot execute the helper without transitioning. The
backend and greeter cannot execute it. See [Linux-PAM's password helper invocation](https://github.com/linux-pam/linux-pam/blob/v1.7.2/modules/pam_unix/support.c)
and [the SELinux reference transition](https://github.com/SELinuxProject/refpolicy/blob/master/policy/modules/system/authlogin.if).

Case `f2e5e69a75f2` passes with loaded candidate `326bc9984595`, SHA-256
`7facb1bcb22ddec1505a099604d5d1d4c44603233e86169e68a4153203fad04a`. The
backend reports `AUTH_ERR` for the empty answer. Kernel audit shows the
failed authentication and a separate successful `pam_unix` authentication and
account check in `aetheri_login_worker_t`. The acknowledged cancel leaves the
connection idle. The original greeter session closes when the caller stops
the backend, and the private synthetic holder is retired. The failed case's
available logs and protected inputs remain. Its VT retirement uses separate
terminal observation.

The loaded-policy checker `57e23606342a` verifies 159 decisions, including
helper execution and transition, pipe use and the absence of password updater
execution. Strict compilation `6a23bccd857f` checks 426 inputs and rejects nine
forbidden rule additions, including helper execution by the greeter and an
attempt to let the worker execute the helper without a domain transition.

The first memory test build `73d43487d0e2` failed the existing wrong-namespace
assertion. Moving the combined namespace/thread cases to the front shifted the
failure to the next otherwise single-threaded constructor in `d0ce4c3a00bb`.
The final fixture runs every single-threaded constructor before creating its
first pthread. The explicit live-thread check still requires `EBUSY` and the
namespace check still requires `EPROTO`. Final Release `a7e047c9e8e3` and memory
`2a88fc73ecaf` pass nine CTests each. Preserve the original outputs. The revised
order isolates the intended cases. The original unexpected return values were
not captured. The separate graphical and PAM session transition for the
admitted user still needs native evidence under the split policy.
The receipt is `/var/tmp/a-login-policy-b491c9ffa6f8/retirement.json`.

Initial test failures identify the invalid `sigterm` SELinux permission, an
unobserved stdout flush, the test diagnostic OFD's open mode, the actual
carrier notification label, the local PAM audit socket and backend VT-reset
permissions, and the carrier journal label. The PAM audit log in `2467723cdbda`
records session open/close and the controller reached its post-stop log check.
That run failed its required ordinary PAM log assertion. The distinct VT
retirement recorded afterward uses only current terminal-state evidence.
The carrier work preserves available failed files and pre-reboot receipts.
The Linux-PAM implementation treats a denied audit socket open as a system error.
See [PAM 1.7.2 audit handling](https://github.com/linux-pam/linux-pam/blob/v1.7.2/libpam/pam_audit.c).

Next join the separate-role service to the finite startup publisher and actual
protected greeter connection. Then verify normal PAM authentication, trusted
graphical display and session handoff under the selected deployment policy.

## Publication with the split backend

The publisher now reads the dedicated backend executable and configuration
types. Its connection is authorized only through the dedicated socket type.
The generic `var_run_t` socket rule moved to the older administrative fixture
supplement. The split candidate requires that generic socket write stay denied.
The publisher's compiled carrier gate compares the complete loaded policy digest
under enforcing SELinux. The first case `175fa2734777` failed at that read with
`EACCES`. A Linux SELinux hook checks `security:read_policy` separately from
the backing file's read permission. The candidate now grants that operation to
the publisher and retains the policy mutation denials. See the [Linux 6.19 SELinux filesystem implementation](https://raw.githubusercontent.com/torvalds/linux/v6.19/security/selinux/selinuxfs.c).

Case `fa0c59b38d10` passed with the dedicated roles and both socket grants.
Final case `867a85c110ea` passed with access narrowed to the dedicated socket.
The ordinary-built and independently staged publisher is 135,024 bytes with
SHA-256 `987ba5fc67d50f71d4ac60d920b56a9e0371cd3748d4ef5d687e94853371208a`.
PID1 started it under its own label and it published a 296-byte Root-owned
`AETHLGH1` record labeled `aetheri_login_handoff_t`. The controller matches the
record's boot, backend invocation, backend PID and pidfd object to the same
live process, then observes publisher exit 0. The original greeter and worker
both exit 0 after stopping the backend. The original PAM session closes and
its scope becomes inactive. The inert greeter performs no authentication.

Final candidate `ff8e0be45c5e` was loaded in boot `342ed7a6`. Its effective hash
is `925a298adf338137c0a214f44a2d5eb535e38efa0c3e0090f05131234b84e106`.
Strict compilation `40f7c46c9097` includes 426 module inputs and rejects all five
disallowed additions. The runtime checker `34f1eda96d5f` verifies 130 policy
decisions and rejects four wrong expectations or selections. The concurrent
checker `19f14b5b7545` failed closed with `EBUSY` during its final policy read.
The kernel allows one open policy snapshot at a time. The final checker ran
after the other readers ended. All three exact candidate modules were retired.
The baseline hash is restored and the VM remains enforcing.

The publisher's four source entries cover the unit, backend ELF, configuration
and drop-in. The next integration must align those ordered entries with the
original greeter's independent source selection and verify greeter-side
consumption and connection admission. The tested provider belongs to the
isolated carrier. Production still needs its selected deployment inputs,
device rights and trusted graphical path.

## Confined greeter consumes the publication

The carrier publisher and original greeter select the same five protected
sources in order. The unit fragment, backend ELF, configuration, effective
drop-in and greeter ELF bind the native handoff. The greeter uses its own
ordinary process, original worker ancestry, held live service and handoff
checks before creating an idle connection. The test sends no credential or
session request. The policy adds system manager status and read-only source,
cgroup and selected process observations. `init_var_run_t` directory open and
read are needed because the source walker opens each original ancestor before
comparing it. The first native case `fe3f6be3e765` stopped on that SELinux
denial and preserved its diagnostic log.

Candidate `376ad756c10d` has loaded SHA-256
`a4e1473ea7677f20420dcaa52a4ec491405f4372764cadeb93c925cd362cbc2a`.
Case `aab9fa4a3505` created the idle native connection and closed it on the
original worker's shutdown signal. Case `8784cfa4b31d` ran the same compiled
greeter with the explicit mismatch-test mode. Its publisher swapped the
greeter and drop-in entries and adjusted the selected drop-in index. The
publisher still wrote an independently observed record for the real backend.
The greeter returned `ESTALE` and reached the rejection marker before any
connection admission. It stayed alive until the ordinary shutdown signal.
Both cases observed publisher exit 0, original greeter and worker exit 0,
PAM close and an inactive scope. Their source profiles and receipts remain
under `/var/tmp/a-login-roles-pam-*` and the protected fixture roots.

The earlier mismatch `31841678a0ac` also returned `ESTALE`. Its greeter exited
before the publisher's post-publication check, so the publisher reported
`-ESRCH` with a possible published record. The controller retained the failed
fixture, including the record. Separate administrative observation verified
terminal unit/scope and restored tty1. The controlled mismatch now waits for
the original worker instead of ending the live observation early.

Final Release `6396287d0665` passed all nine selected CTests. Memory
`f7bd1764b427` built the targets but the first suite failed the existing
`login.startup` model assertion at line 137. The unchanged test binary passed
all nine in `6f31b2dbd0ae`. The original failure remains available and its
cause is unproven. Live authentication and the graphical greeter still need
their own integrated tests under the selected deployment policy.

The loaded-policy checker `af5fbe0754cc` verifies 146 decisions and rejects
four wrong expectations or digest selections. Strict compilation
`856aafd75b1b` checks 426 inputs and detects seven forbidden rule additions,
including a greeter write to the handoff and a signal to the original worker.
The selected candidate was retired after all typed tasks ended. The loaded
policy again matches the enforcing baseline hash
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.
