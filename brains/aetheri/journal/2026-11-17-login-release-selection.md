# Login release selection

Both installed login providers need the greeter's independently selected ELF
digest. The greeter must be built before that digest is available. The
release selection therefore carries the expected files and account separately,
under an Ed25519 signature. The provider's build supplies the public key and
unique release identifier. Release signing happens after staging. This lets
the Root verifier and the ordinary greeter consume the same expectations.

The [offline encoder](../../../tools/prepare_login_selection.py) checks the
staged bytes against the reviewed receipt. It adds the separately selected
distribution-wide systemd drop-in and complete effective kernel policy.
The [C verifier](../../../src/login/selection.c) uses the current OpenSSL 3.5
LTS family and maintenance floor through a private default-provider context.
It leaves the caller's output untouched on refusal. It checks the exact
record size and release identifier, accepts only the selected key, and
requires the account and group fields and every file row to be well formed.
The [deployment specification](../../../spec/login-deployment.md#signed-login-selection)
defines the fixed byte layout, row order and signing step.

The offline tests pass on Windows. The enforcing Fedora VM passes all 11
selected CTests in both Release `42b8160c9343` and ASan/UBSan
`7c3c491ecb4b`, from 819 recorded source inputs per configuration. The
Python-to-C test stages distinct role files, creates the body, signs it
using a test-only key and verifies it with the native decoder. Native
checks reject a different key, release or greeter digest and reject malformed
signed account, reserved, size and group fields. The first Release
`c2e569aa65db` and memory `c39da3b4e6f0` builds failed because a C definition
used pointer parameters while its declaration used bounded array parameters.
The final definition matches the declaration. Preserve those build records.

The tests leave the VM enforcing and at VT1. The packaged units and the login
executables still use their existing explicit provider contracts. Next select
the trusted image key and identifier, add the fixed-path reader and wire it
into the normal providers. Verify the installed source and unit observations,
enrolled account, loaded policy, runtime and actual graphical login in the
image. Keep the USB release gate.
