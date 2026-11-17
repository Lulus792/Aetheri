# Composed PAM root

`tools/verify_login_pam_root.py` reads a stable assembled Linux root. It
checks the three packaged profiles and their labels, follows the literal
module references and transitive ELF dependencies, compares the aliases
and bytes to selected pins, and checks the loader and password helper.
The helper is verified at its final owner, mode and SELinux file context.
The verifier does not write to the image.

The older desktop VM archive `image-v11/boot.raw` matched the existing
export digest `7306f0f11e5ce4e694ee0ee923f755f20b83f7abb7dc110a23147cc241f17627`.
The extracted regular ext4 copy matched the selected partition digest
`6f873239af2417231370657ff36480cb6dcb9e2c4e2a257b1f68af1f8d9355ba` on
both host and guest. Mount it only in the private VM namespace with
`ro,noload,nosuid,nodev,noexec`. The copy is checked again after the test.
The fixture combines the archive's base libraries with the pinned runtime
supplement in private `nosuid,nodev` tmpfs. It applies test image metadata
inside that mount. The archived filesystem contents remain unchanged.

Successful fixture `f216402b9182` verifies 30 ELF objects and 18 library
aliases in addition to the three PAM profiles and merged system paths. The
module from systemd-pam 259.10 matches the pinned digest
`1ee5de342a164ce4687b7c21497c7afb68e7a1cec2a1a36ed7faeebb2524b614`.
The verifier rejects the carrier's 259.5 module, a wrong TIRPC alias, a
missing transitive cap-ng library, incorrect password-helper mode, and an
incorrect fallback label. The fresh complete verify passes after restoration.

The selected dynamic loader resolves the password helper via the image's
`/sbin` alias with UID 1000. A C probe under that same unprivileged UID
loads all six modules with immediate binding. The actual image libpam
denies five calls for an unregistered service using the packaged fallback.
The probe provides no password and invokes no successful login. Its headers
come from the pinned [Linux-PAM 1.7.2 source](https://github.com/linux-pam/linux-pam/tree/v1.7.2/libpam/include/security).

Preserve `8b457be46ff8` (test launched the CLI with Python isolated mode,
which dropped its sibling imports) and `4654c0c1fb59` (test carrier lacked
PAM development headers). Both mounts were retired in each attempt.
Candidate SELinux module `38252777872a` was retired after the test. The
original policy digest `a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`
was restored with enforcing mode and zero tasks in candidate domains.
The next deployment work is the image's account data, actual installed
platform gates and successful named login with the booted image's units.
