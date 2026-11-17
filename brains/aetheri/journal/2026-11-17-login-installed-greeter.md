# Installed graphical greeter adapter

The [greeter entry](../../../src/login/greeter_main.c) can select the
[installed adapter](../../../src/login/installed_greeter.c) from its explicit
build-time provider. It uses the signed UID, GID and supplementary groups and
checks them against the process before entering the native runner. It
projects the fixed service, seat, runtime and display image. The existing
native runner still verifies the original session, greeter/worker/backend
relationship, handoff and display child. The compile-selected platform gate
remains mandatory.

The font is a bounded protected asset. The adapter verifies its source and
copies the bytes from the held file, verifies that copy against the signed
digest and retains the original source for later host checks. Both installed
adapters include the signed selection in their common handoff source set.
They use the same protected-record path, file identity and digest in the
backend mount view. The policy, account and font expectations are therefore
covered by the source fingerprint already carried by the handoff.

The Release `133e22a30b08` and ASan/UBSan `3e799ec75bfc` configurations
build from 826 recorded inputs and pass all eleven selected CTests each.
The first two builds `27704c47a2eb` and `97a132f1238b` stopped because the
existing group test called a helper whose signature had changed. The final
test drives the shared group check directly while preserving its original
cases. Retain the failed build records.

VM case `fe0059739895` uses the actual greeter entry under ordinary UID
1000 and a test-only build anchor and platform callback. The case projects
the fixed paths privately under enforcing SELinux and uses the reviewed
Liberation Sans font. Both profiles allow the correct selection and resident
font and reject a mismatched UID, supplementary groups, font bytes, font
label or platform callback. The original run stays fenced after a
byte-identical font replacement, even when the original file returns. The
runner wrapper observes the actual adapter's profile and gate. Display,
logind, original ancestry and handoff transport are outside this test.

VM case `da5c58a8732e` repeats the startup adapter's four mount-view and
policy checks in each profile with the new common source set. Candidate
`f99ec70ee8a5` was retired to the original effective policy with no typed
processes. Next select the image platform gates and verify the complete
installed systemd, PAM and graphical lifecycle. The first-version USB
gate still applies.
