# Installed login selection

The [selection preparation tool](../../../tools/prepare_login_selection.py)
can append an offline signer's raw 64-byte Ed25519 signature to the reviewed
body. It creates a 0444 output at the chosen staging path. The image reserves
`/usr/share/aetheri/login/selection.bin` with the login configuration SELinux
file type. The offline tool has no authority to admit the signed selection.

The [installed reader](../../../src/login/installed.c) accepts only the fixed
path and an independently selected public key and release identifier. It
authenticates the bytes first, then pins a protected observation against those
authenticated bytes. Existing source verification checks the real directory
ancestry, label, owner, mode, capabilities, mount boundary and original object.
It returns fixed backend and display source expectations by value and keeps
checking the named and held selection. These expectations still require the
runtime checks in the startup and greeter providers.

Native Release `6dfa127ec605` and ASan/UBSan `9cea9f444b1c` each build from
822 recorded inputs and pass the eleven selected CTests. The packaging test
exercises the actual command, checks exact attached bytes against the native
test signer and confirms that a short signature or an existing destination
is refused. The four offline packaging tests pass on Windows too.

In VM case `3c5e5fe9f62b` the tool creates the signed record from the actual
staging directory with inert role artifacts. The projected `/usr/share`
exists only in a private test mount namespace. The ordinary user exercises
the exact literal path while SELinux enforces. Each profile passes eleven
checks. It admits the correctly labelled record, checks its role projection
and rejects missing, symlinked, short, long, incorrectly signed, wrong-mode,
wrong-label, hard-linked or leaf-bound inputs. After a byte-identical file
replacement it reports stale on the same open handle. Restoring the original
file does not rearm that handle. Case `ac8909fb9433` failed at the fixture's
raw xattr assertion because the kernel returned a terminating zero byte.
The final fixture handles that representation before comparing the label.
Preserve the original failure and both build/test receipts.

Candidate module `6a313795bebf` was retired with exact original carrier
policy bytes restored, enforcing mode and zero typed processes. The live
VM retains its original system paths and stays on VT1. Neither provider was
started in this test. Next wire both providers, including separate source
observations across the startup helper's namespace entry, then validate the
installed systemd, account, policy and graphical lifecycle.
