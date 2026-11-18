# Installed suspend guard

The installed sleep job waits for the original desktop to cover every output
and pause the password dialog. The guard runs as its own confined systemd
service. It identifies the active local Wayland session through logind and
checks the original desktop endpoint before sending a request and again
before reporting readiness. The desktop checks the guard's own original
endpoint and delegates preparation to the lock broker.

`systemd-suspend.service` uses `BindsTo` and `After` to require the guard.
The guard keeps the same connection open while the stock sleep job runs.
The desktop uses connection closure to begin the wake cover. The existing
broker requires fresh presentation and a fresh authentication before
revealing the session. The deployed path requires an authenticated Aetheri
desktop on `seat0`. The systemd helper still performs the kernel power
transition. See the [session contract](../../../spec/session.md#suspend-and-resume-lock-coordination)
and [unit dependency semantics](https://github.com/systemd/systemd/blob/v259.10/man/systemd.unit.xml).

## Signed inputs

Format 5 adds the guard executable, its systemd unit, the sleep job drop-in,
and four independently pinned stock systemd inputs. The stager checks all
four stock digests from the systemd-udev 259.10 profile. Startup observes the
seven files in both mount views. The common backend handoff has ten original
sources. Sound remains optional and its presence is selected at build time.
See the [image contract](../../../spec/login-deployment.md#signed-login-selection).

## Verification

Native Release build `4480e66acbec` passes nine selected CTests and the
separate privileged authority test. The abstract socket fixture checks real
pidfds and rejects a message from a different process that inherited the
endpoint. It supplies mock SELinux labels. The native selection and
packaging tests cover both format-5 Sound combinations. The independent
deployment suite passes seven tests. Offline policy `c85b05dd9e42` passes
72 decisions each for lock plus suspend and Sound plus lock plus suspend.
The carrier's running policy stays at its original digest.

The enforcing policy boot `5a333e8628` passes 211 readback decisions. Its
effective digest is `ef3d9b9b3d643e69abf34a0e841b9946de2901fc2aa6fa840aab249845f7fa7a`.
The first signed image `cb7b6a3d3d2e` passes a read-only ext4 check. Its
single-vCPU TCG boot `9ceb0c20` stops in handoff publication with
`-EBADMSG`. The source of that error is unobserved. WHPX boot `1d540ed5`
uses the same image and reaches the authenticated desktop. The virtual
sleep key starts the guard. SELinux denies its read of logind's `seat0`
state. The guard exits with status 1, and systemd refuses the suspend job
with a dependency failure. The kernel stays awake. The next policy adds
read-only access to this logind file type and checks that writes are denied.

Release `1d83ba12cbb7` repeats the nine selected native tests and the
privileged authority test. Offline policy `a2ada2e46aff` passes 76 decisions
in each profile. A new signing anchor selects the corrected kernel policy.
The enforcing policy boot `74c2c5f7a1` passes all 215 readback decisions with
effective digest `f8ad1bcb9e2544a871f62726b8b8837e7cc8db2bb6abb1f9b3e00990c25b1c90`.
The composed image `dbba7b473a08` passes the read-only ext4 check. TCG with
two vCPUs (`07422871`) and WHPX (`9d0322d6`) both publish the signed handoff
and start the authenticated desktop. Both block the sleep job after the
virtual sleep key. The TCG trace proceeds to the original desktop cgroup
and shows a denied directory search under `/proc/1` in the guard. Native
logind session discovery uses the manager's cgroup path. The candidate
permits read-only access for that observation and checks that manager
file writes and ptrace stay denied. The recorded `0x542a` ioctls retain
their existing SELinux decisions. See [the pidfd session lookup](https://github.com/systemd/systemd/blob/v259.10/src/libsystemd/sd-login/sd-login.c#L118)
and [the cgroup root lookup](https://github.com/systemd/systemd/blob/v259.10/src/basic/cgroup-util.c#L646).

Release `af4f75f6419c` passes nine CTests and the privileged authority test.
Offline policy `5a0a7f3ff2fb` passes 80 checks per profile with the cgroup
read. The enforcing kernel readback passes 219 decisions with effective
digest `b79aaecd64995e56b286148f9cabf4ba27b7727d177c6b9089f2c3d1f77524e4`.
The signed image `c87cf60a8f17` passes the read-only filesystem check.
TCG case `7178be3f` reaches the desktop and still refuses the sleep job.
The serial trace records the systemd dependency failure and no kernel
sleep entry. The guard's return path is unobserved in this run.
WHPX control `b93fea3b` reaches the same desktop without an HDA controller.
It waits for startup to settle and repeats the refusal. Its guard reaches
the desktop and PID 1 cgroup files. The measured `0x542a` ioctl denials
also appear in the otherwise working desktop. They establish a trace
location and leave the final rejection unresolved. Every QEMU attempt
is terminal. Capture the guard's exact admission or protocol result before
changing the policy again. The positive S3, fresh unlock and physical
hardware checks remain open.

The first native build rejected a redundant `_GNU_SOURCE` definition under
`-Werror`. The working build uses conditional definitions where the target
already provides the flag. The original Alpine development environment
failed to start `debugfs` with `getpwuid(0)` I/O errors. A separate Alpine
tooling instance was used for image copies after one instance startup
timeout. The project snapshots and the USB stick remained outside that
tooling change.

The build and policy receipts, selected binary records, VM traces and
screenshots are retained in
`local/login-review-2026-11-10/installed-suspend-guard-2026-11-18-evidence.tar.gz`
with SHA-256 `3f3ea97f0c854bbe9e3fb9eb59353d9c0947878ad70b32f220fc7a9cd5bbd6b9`.

## Installed endpoint diagnosis

Release `d53c1a431c7a` adds phase-specific systemd exit statuses. Nine
native CTests and the separate authority test pass. The signed copy
`53a39b56ca89` uses the original enforcing policy. Its WHPX session
`74ad32dc` starts the desktop and reports status 17. The guard has already
admitted the original active session and fails while receiving `HELLO`.
The audit stream splits the systemd status line in this trace.

The isolated handshake probe `5c6917227a63` also passes the selected
native tests. Signed image `a57b49f304d3` and WHPX session `e3ccb051`
reproduce the failure without an HDA controller. The desktop accepts the
socket and its first peer admission returns `-EACCES`. The guard sees the
connection close and reports the diagnostic status 31 (`-ESTALE` while
waiting for `HELLO`). The desktop has no peer-credential, label or pidfd
rejection in the instrumented path. The first effective-policy query assumed
the directory search was denied. The revised query finds the search allowed
and `security_t` file `open` and `read` denied in the desktop domain.
The endpoint uses `security_getenforce()` before it reads peer credentials.
[libselinux enforcement check](https://github.com/SELinuxProject/selinux/blob/3.11/libselinux/src/getenforce.c)

The installed suspend policy permits those two file permissions for the
desktop domain. Policy changes, write access and privileged policy reads
stay denied. The SELinux directory search remains inherited from the base
policy. The kernel cgroup ioctl rules are unchanged.

The test image copies were edited in fresh private mount namespaces in
the Fedora carrier after two temporary WSL registrations timed out. The
editor checks the original image digest, file modes, owners, labels and
old and new bytes. It checks ext4 read-only before returning the edited
copy. The carrier's running policy stays at its original digest.

## Signed suspend and fresh unlock

Release `6dce5b7918af` passes nine selected CTests and the privileged
authority test. Offline policy `4c5d68f6afea` passes 87 decisions in each
profile. The enforcing kernel readback in `25b1c1b857` passes 226 decisions
with digest `4eb2990bab4b43aeefac35165d7f62f4b5f82515ed6b77917dac9d51f329b6b8`.
The first desktop composition controller failed before filesystem changes
because its embedded Python was truncated by host string substitution.
The corrected controller creates fresh signed image `ac97b7682ba1` and
verifies the ext4 root. The instrumented actor binaries are absent from this
normal Release build.

TCG session `87ef5424` authenticates and presents the desktop with one
virtual display and Intel HDA. A virtual sleep key starts the guarded stock
sleep service. QMP reports `suspended`, Linux records deep suspend entry,
and a wake request returns it to `running`. Linux records suspend exit
and systemd reports successful guard deactivation. The focused lock dialog
refuses a wrong password through actual Linux-PAM. A fresh correct attempt
restores the original desktop. Normal logout closes its PAM session and
starts a replacement greeter. An ACPI power-button request powers down
the guest at exit zero. The signed boot image retains its original digest.

TCG session `d30a0607` boots the same image without an HDA controller and
repeats the complete transition twice within one login. Two distinct sleep
helper processes enter S3 and each guard deactivates successfully after
wake. The original desktop process stays alive. Both new password dialogs
refuse an incorrect attempt through PAM and require a fresh correct attempt
before restoring the desktop. Logout, replacement greeter and ACPI
power-off pass again. All new QEMU attempts are terminal.

The guard's stderr markers are absent from this serial trace. Admission
is witnessed by systemd starting the signed `Type=notify` unit's dependent
sleep job, the kernel entering deep sleep, and the successful unit
deactivation after resume. The cgroup ioctl denials remain visible during
the successful transition. Physical HP validation remains a release item.

The endpoint probes, signed composition receipts, policy decisions, finite
VM traces and screenshots are in
`local/login-review-2026-11-10/installed-suspend-endpoint-2026-11-18-evidence.tar.gz`
with SHA-256 `9f37b757c4b5fb92657a1d2258c0b2786dfc78cddd5828c78ec5a212ba98614b`.
