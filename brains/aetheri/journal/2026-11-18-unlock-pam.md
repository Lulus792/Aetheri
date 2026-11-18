# Local password verification for unlock

The finite C verifier authenticates the already selected local account through
a fresh `aetheri-unlock` PAM transaction. It checks the original name and UID
through NSS before and after PAM. The conversation responds once to a hidden
password prompt and discards service text. A changed `PAM_USER`, repeated prompt
or need for a new authentication token prevents success. The password limit is
512 bytes because the selected `pam_unix` truncates longer passwords.
The worker's caller retains responsibility for wiping its input and for matching
the result to the original live lock. [Linux-PAM 1.7.2 password verification](https://github.com/linux-pam/linux-pam/blob/v1.7.2/modules/pam_unix/support.c)

## Native checks

The development headers come from the signed Fedora 44 `pam-devel` RPM
documented in the [SDK pin](../../../spec/login-unlock-pam-inputs.json).
The initial configuration lacked the explicit app SDK paths. The first
header projection missed `_pam_compat.h`. Both failures are retained.
The verified Release and ASan/UBSan builds pass `login.unlock_pam` with
15 controlled transactions and six rejected preflight inputs. GCC's analyzer
also passes. The native object imports password verification, account
checks, result observation and PAM cleanup.

## Private Linux-PAM fixture

Fixture `646ae8304fba` uses synthetic accounts with a private mount namespace
for NSS inputs and PAM policy. The carrier remains enforcing. The actual
library, `pam_unix` module and `unix_chkpwd` helper match the already frozen
desktop and login runtime hashes. The tested verifier has SHA-256
`3e7d7c0dbecddca36fd981e9c65c78700f1c9b20e08fd8ddec0fc3eef87ca64d`.

The real stack accepts the correct password in both the ordinary-user and
root test processes. The ordinary process establishes that the synthetic
shadow file is unreadable before it invokes PAM. Wrong passwords, a foreign
account's password, mismatched account identity, empty input, disabled password,
expired account, expired password and missing service profile all fail.
Embedded NUL and 513-byte responses fail validation. Direct PAM session and
password-change requests return 14 and 20, respectively. The synthetic shadow
digest stays the same across these denied requests. The private configuration
disappears from the carrier's live view when the fixture ends.

The installed desktop path still needs an admitted credential worker that
binds each result to the original covered session, a trusted password dialog,
and the suspend integration. The first-version USB gate remains in force.

Receipts, fixture sources and logs are in
`local/login-review-2026-11-10/unlock-pam-2026-11-18-evidence.tar.gz`
with SHA-256
`51706431993a7fbe8e3957aeef43f7de35fd79193912dc6d44d036bf80fba77d`.
