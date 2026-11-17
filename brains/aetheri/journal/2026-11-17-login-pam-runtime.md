# Packaged PAM fallback and runtime supplement

The standalone image stages a deny-only `/etc/pam.d/other` as mode 0444.
The fixed installed adapters check its exact bytes and dedicated context
in the original backend's mount view. It becomes the tenth common source
in the handoff fingerprint. The signed selection keeps its existing
format and two named PAM rows. The systemd drop-in limit remains five.

Private fixed-path case `cca9c3f75300` passes 20 checks in each build,
including the full ten-entry source set, absent fallback and a fallback
changed to another module. It uses Release `d2878044a0de` and ASan/UBSan
`f4d1d0a8d67f` with 829 inputs each. A separate ordinary-user Linux-PAM
call against the exact packaged default rejects authentication, account
validation, password change, session open and session close for an
unregistered service. No conversation is invoked.

The final Linux Release `3aa846620716` and ASan/UBSan `bcb66d6c642e` builds
each pass all 11 selected CTests with 831 inputs, including the fixed layout
regression for the staged fallback. The four packaging methods also pass
on Windows.

The runtime supplement stages 18 pinned files totaling 2,041,339 bytes
and eight aliases. It adds `pam_loginuid`, `pam_succeed_if`, `pam_unix`,
`unix_chkpwd`, the additional ELF dependencies and their notices. Each
supplemental file's observed bytes and metadata agree with the installed
RPM header. The glibc notices already selected in the desktop inventory
cover the glibc addition. The ordinary-user stager reads the 13 new ELF
objects with `readelf` and checks their references against the three
inventories. The password helper stays at 0755 in this staging tree.
Its intended final Root ownership, mode 04755 and context appear in the
receipt. The final image assembly still has to apply and verify them.

Native staging case `e3009c4bd86b` verifies 18 copies, eight aliases, the
ordinary helper mode and the final mode in the receipt. It rejects an
existing destination, missing ELF dependency alias and missing final helper
metadata. The target inventory selects systemd-pam 259.10. The carrier
has 259.5 with a different module digest. The staged supplement therefore
provides a dependency inventory and a verified offline copy, while final
base/module validation and login through the assembled image remain open.

The VM retained its boot through the host interruption. The native candidate
`262f35828c4f` was retired to the exact baseline SELinux bytes with enforcing
mode and no remaining candidate tasks. Continue with the target base and
installed platform gate before release image testing.
