# Standalone signed login VM

The VM uses a copied ext4 system partition from the offline policy
test, the archived UEFI layout and a separate writable data disk.
The offline assembler uses the existing deployment stager, PAM runtime
selection and account verifier. It enrolls the locked greeter as UID
and GID 999 with exactly group 999. The user account and Ed25519 key
are disposable test inputs. The private key and user secret are kept
in the lab and excluded from the repository.

The login image CMake profile builds the session entry, compositor
and desktop without development-library RPATHs. The session entry
uses the installed input quirks and EGL vendor files under
`/usr/share`. The display and user compositor are separately installed
files of the same DRM compositor build. The greeter uses its private
Wayland descriptor mode. The image loader resolves the selected ELF
dependencies inside the composed root. The first assembly
`b52ba68923e1` refused the older display binary's RPATH before staging.
An intermediate link required an explicit build-only `-rpath-link`
for the selected input and graphics libraries. The installed ELF
checks reject RPATH and RUNPATH.

The image uses the extracted Fedora systemd 259.10-1.fc44 tmpfiles
program and unit from the same package version as the base. The RPM
was independently checked against the Fedora signing key, header and
payload hashes. The distro timeout drop-in is from the same RPM and
matches the earlier carrier's exact bytes.

The first assembled boot `c5388d81e024` passed the read-only root and
data checks. D-Bus and logind started. Enforcing SELinux denied
`systemd_tmpfiles_t` creation of all four private login directories.
The stock unit accepts `CANTCREAT`, so PID 1 proceeded and the backend
failed its namespace setup before executing. The exact serial log and
available screen remain in the local run directory. The original VM
ended and the read-only media digest remained unchanged.

The correction gives the existing tmpfiles domain directory creation
and metadata access for the three login-specific directory types.
The backend requires the setup unit and asserts all four directories
before use. The original bind mounts, labels and application
permissions stay in place. Release build `529bf4c500be` passes 13
selected Linux CTests, and the packaging suite passes four tests.
Offline policy compilation `9bbf4fb69847` produces `policy.35`
SHA-256 `7543c1ded127fd64aea7bc6fccf8038c3b219698c3da6bb347befc34783b7bbd`
from the same distribution source and preserves the carrier policy.
Both cold boots of policy probe `005c24b7f301` read 3,723,749 bytes
under enforcing SELinux, with SHA-256
`56d94034e740d0c091098023fdbe219e032d27c691c84b64e069d0589a48dbdd`.
The standalone assembly `e536a921bc7e` signs that readback. Its root
partition passes offline ext4 checks and hashes to
`341e25f923ec33a375999ffb010a18fefa07d5d309d954862841c1f654daed25`.

The second standalone boot finished the tmpfiles unit with no login
directory refusals, passed the four assertions and started the backend.
logind created greeter session 1 on seat0. PAM opened and closed the
original greeter session. The VT8 screenshot identifies the worker
panic at `greetd/src/session/worker.rs:282`, `unable to exec: ENOENT`.
The selected backend source calls `execve("/bin/sh", ...)` even when
`source_profile = false`. Offline ext4 inspection confirms that
`/bin/sh` and `/usr/bin/sh` are absent and that the selected greeter
ELF exists. The startup helper had selected the backend, then failed
in its namespace phase after the backend ended. Neither published
handoff nor graphical login was reached. The user runtime and system
manager audit entries remain separate findings. The original VM
exited and the system disk retained SHA-256
`0516e6ac7bbb25237cc636427adc74a6532145a9e27253e3a935729147843514`.

Next verify direct execution of the exact configured greeter and
literal Start arguments in the installed backend profile. Retain the
original PAM child ownership and shutdown behavior. Rebuild and
re-sign the backend selection before the next standalone test.
