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

## Direct session execution

The new `aetheri-direct-exec` backend feature prepares literal, absolute
argv before acknowledging session arguments. It executes the selected
program after dropping to the session identity. The startup-only
carrier retains the upstream behavior. Linux Release build
`d1a75296af9f` passes 23 Rust unit tests and one documentation test.
The startup-only and no-feature profiles also compile. The 13 source
preparer checks pass on Windows. The selected backend ELF SHA-256 is
`923a6690b9b13a711b750f3de36652b3a1c9d11005405d0214e7e98a8f6139a8`.

Signed image `7b1412821e82` boots enforcing with the same selected
policy and an unchanged archived base. The startup helper reaches
`AETHERI_LOGIN_STARTUP_COMPLETE`. VT8 shows the real Wayland account
prompt. A second cold boot with fresh VM state and the same system
medium reaches the same account prompt. The QMP driver checks the
original VM identity and takes screenshots before account input,
the synthetic password and the Start action. The UI confirms the
password and reports `start_ack=1`. logind opens session 2 as Wayland
class user and PAM opens the session for UID 1000.

The session entry starts the user compositor, which gets a Bochs DRM
output. The desktop process logs `READY wayland opengles2 outputs=1`
then exits with status 139 before the session owner reports desktop
readiness. The original worker closes PAM, session 2 is removed, and
the new greeter session 3 displays the account prompt. No usable user
desktop was observed. The shader-cache warning occurred before the
crash. Its causal role is unverified. Next localize the desktop
fault in a diagnostic copy of the same image and repeat the
authenticated start, desktop readiness and intentional logout.

Both original QEMU processes terminated. The read-only system image
keeps SHA-256
`85a05b531e9d46dacccc1d7a2c1b677826a0b91016e29358dc1b4cb584d59892`.
The local case preserves the serial output, screenshots and distinct
data disks. The synthetic secret and signing key stay in the lab.

## Desktop fault and renderer control

Two signed diagnostic images, `17372f4f0f73` and `e2b8b401f407`,
reproduce the desktop crash after actual password and Start. The
selected startup, greeter, session entry and compositor executable
bytes match the preceding production build. The instrumented desktop
uses the same image runtime and enforcing policy. The original
account, password and authentication screens match the previously
inspected frames before each keyboard action.

The first diagnostic run locates the instruction fault outside the
loaded ELF objects. The second records x86-64 `SEGV_ACCERR`: the
instruction pointer equals the fault address, and the live
`/proc/self/maps` row is anonymous `rw-p`. The return address is inside
the loaded `/lib64/libgallium-26.2.5.so`. This establishes an attempted
instruction fetch from a non-executable page during the selected
Mesa render path. The trace does not identify the earlier operation
that left the mapping non-executable. The previous shader-cache
warning and the kernel audit stream do not establish that operation.

A counter using the same signed runtime and policy selects SDL's
window software renderer with implicit framebuffer acceleration
disabled. Build `af4fc050284c` passes the 13 selected CTests.
Actual login in image `9cbc88e7d55b` reaches the renderer and reports
`Window framebuffer support not available`. The exact SDL 3.4.20
Wayland backend has no window-framebuffer implementation. The
counter was reverted after the VM test. It is not a deployment
candidate. All six VMs ended and their read-only system-image digests
were rechecked. The diagnostic sources, serial logs and screenshots
stay in the local test directories.

Next test explicit Wayland shared-memory presentation with an SDL
software surface for the confined desktop. Retain buffer ownership
through compositor release and verify first frame, normal logout
and the new greeter in a signed VM. Keep the user app renderer and
the original crash evidence separate.
