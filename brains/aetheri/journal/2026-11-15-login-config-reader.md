# Consumed login configuration reader — 15 November 2026

`ae_login_loaded_check_config` obtains the original invocation ID inside the
service observer and reads effective `NotifyAccess` and `StatusText` from the
retained unique name of PID1. It compares the full text to the canonical
invocation and digest of the selected protected configuration. Calling it
requires the notice for subsequent ordinary and administrative checks on that
handle. Initial absence permits a bounded wait in the caller. Wrong status,
wrong notification policy or loss after acceptance permanently fence the guard.

## Native results

The QEMU audio lab ran enforcing on boot `fdc84399-7306-4760-8605-a9fd638cb9a3`.
The Release and ASan/UBSan builds at `/var/tmp/a-login-cmake-4337ab688c89`
and `/var/tmp/a-login-cmake-e7f6debc0556` each passed three login CTests and
nine explicit inert fixtures. The C client passed initial pending and matching
status, rejected a different digest and invocation and effective access `all`,
and stayed fenced when the original notice was restored after observed loss.
The two initial runner attempts used the wrong private crypto library path and
stopped with the existing version check. The corrected runner selected the
3.5.9 library explicitly.

The actual pinned backend from the startup-notice build ran as Root with a
unit-private PAM directory. The test greeter used the fixture-only PAM service
and ran `/usr/bin/sleep` under UID1000. The separate fixture user service used
`pam_deny`. The ordinary clients sent no application protocol data or credentials.
Both Release and instrumented ordinary clients matched the actual consumed
configuration via the peer socket and PID1. The Release administrative observer
also matched the current program file against the same source and invocation.
The verified results are at `/var/tmp/a-greetd-live-f5dbd6be5a8e` and
`/var/tmp/a-greetd-live-8c73aa266138`, with final builds at
`/var/tmp/a-login-cmake-f135a32d757b` and `/var/tmp/a-login-cmake-85a45d063333`.
Both final builds also passed the three existing login CTests.

The first live attempt observed the config notice before the test greeter
failed its UID change. It did not establish stable backend readiness. Two
separate diagnostic runs located the failure at executable entry: effective
capabilities were `0x61`, with `CAP_SETUID` absent despite bounding set `0xe1`.
Independent commands compared the explicit Root user/group unit with the system
default identity. The versioned systemd source shows the relevant drop after
explicit user setup. The working fixture uses the system manager's default
Root identity and unchanged capability bounding set, with observed UID/GID0
and effective `0xe1`. The corrected readiness check requires a live UID1000
greeter in the original cgroup. No diagnostic library was used in the passing
fixtures. The production policy remains a separate design and test task.

The closing audit matched eight source files in each of four native builds and
confirmed terminal units, empty or removed live fixture cgroups, and removal of
the exact live sockets. It preserved the failed fragments and diagnostic logs.
Global PAM configuration, RPM inventory and active VT were unchanged. The
Windows contract check passed 374 assertions over 14 API methods. Host receipts
are under `local/login-review-2026-11-10`, with `consumed-` and `loaded-consumed-`
prefixes. The integration now needs the protected handoff of the administrative
observation to the original greeter and real authentication/desktop session.
