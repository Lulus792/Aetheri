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
