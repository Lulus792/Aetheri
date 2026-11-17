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

## Wayland shared-memory desktop

The installed desktop uses SDL software surfaces backed by anonymous
files from the private login `/dev/shm`. It attaches the finished pixels
directly to the custom-role Wayland window. Each view waits for original
buffer release before reuse, has a second slot for an in-flight frame
or resize, and reserves the bounded pixel storage before drawing.
The development renderer stays on OpenGL ES.

Build `d159781eb1fd` passes all 13 selected Linux CTests across 841
source inputs. A separate compile check exercises the development
translation unit. The compositor, startup, greeter and session entry
match the preceding signed binaries byte for byte. The desktop's ELF
SHA-256 is `d14a65defb4946287d2f912341f7ff4f64893402d708f58865ff7a60ae5df686`.
Image `b0bfb4471e8b` uses the same selected effective enforcing policy.

Two VM cold boots reach the startup handoff. In session
`5ac700d0`, actual keyboard login opens UID 1000's original Wayland
session. The owner reports desktop readiness, the compositor
confirms presentation, and the inspected frame shows the dock at
1280 by 800. The keyboard opens search and changes the surface
from 1280 by 84 to 1280 by 800. The desktop presents six frames
with two buffer slots. The close action reports `CLOSED frames=6
failed=0`, and the session owner reports `AETHERI_DESKTOP_END status=ok`.
PAM closes the user session, logind removes session 2, and the
replacement greeter in session 3 visibly matches the account prompt.

Both original VMs exited. Each run rechecks system image SHA-256
`52351566b82a66fc32d1cbd12b9e68e0b18b0adf78f7d9c590112163c87ffe42`.
The VM test is single-output desktop evidence. Terminal, Files and
Settings launches in the installed profile and hardware validation
remain open. The USB release gate still applies.

## Installed app execution boundary

Three independent logins in signed image `b0bfb4471e8b` reach the dock.
The controller verifies the corresponding search query on a presented
frame, then uses Enter to launch Terminal, Files or Settings. All
three fork, exit 126 before app readiness and cause the current
session owner to close the failed session. The original PIDs, signed
media digest and screenshots are retained.

Build `d3bc5e6f781b` adds the original child exec pathname and errno
to the existing session diagnostics before exit 126. The diagnostic
does not act as an app-ready or cleanup observation. All 13 selected
Linux tests pass. A new signed image `f528ac353c7f` proves the actual
`execv` call returns `EACCES` for each of the same three applications.
The compositor and desktop close, the session runtime directory is
removed, PAM/logind close the user session, and a new greeter starts.

Offline inspection of the original read-only ext4 root finds the
three executable files, the PTY starter, shell and editor at their
intended paths. Each file is Root-owned, regular and mode 0755 with
no embedded RPATH/RUNPATH. Each dependency closure resolves through
the image loader under UID/GID 1000. The six files have label
`bin_t`. An independent userspace decision query of the exact
effective policy captured from the signed image denies
`aetheri_session_t` execute and execute-no-transition on `bin_t`.
Positive controls allow the selected compositor and desktop file
permissions. The shared carrier policy is preserved.

Next add exact installed entrypoint and role selection for the
applications and their needed helpers. The session's existing
generic `bin_t` denial is covered by the policy checker. Keep the
earlier user and system runtime authority work separate and verify
actual app readiness, input and shutdown after the targeted
admission. All seven new VMs are terminal and the USB gate applies.
Four older unmounted fixture ext4 files were made sparse, reclaiming
1,017,806,848 allocated bytes with their full SHA-256 values preserved.

## Fixed app entrypoints and Settings

The installed policy now declares separate Settings, Files and Terminal
domains. It selects the fixed executables and the Terminal-only helper
type. The shared permits cover the private Wayland connection and session
buffers. Files and Terminal can work with an explicitly labelled
document tree. Settings has no document permission. App packages and
durable lifecycle remain covered by the authority contracts.

Offline policy build `74712fbbc0d2` compiles to `policy.35` SHA-256
`f0a8b5c92a5ded235f807102c5773754fa4ee5cbf2ebcb2be26a475eea4373d1`.
Two cold boots of `173ef9f34d53` return exact enforcing policy SHA-256
`78e5a89462bdc3941ba741e36cd777d0a83e175605591af4a4f774fe7ef7b82f`.
Thirty-four decisions pass against both the compiled and kernel-read
policy bytes. The public live checker now has an opt-in app matrix.
Release `c80bb696b0ad` passes 13 Linux CTests from 842 inputs. The
four packaging controls pass.

The first labelled data copy `3ab1db227712` assigned `var_t` to
the volume root. Three independent VM attempts failed before login
because PID 1 could not rename its service directories there. A
fresh copy `6ef800c75f25` preserves the base volume root and applies
labels only to the three home/document paths. The original data
volume, the failed data copy and the carrier policy are preserved.
The corrected test volume SHA-256 before use is
`ed6eb9f15634008852e1121ea7b15a79631b16c11c6cb533c9350448f31d44a0`.

Signed image `140c4d10fc7f` labels the six exact archived app/helper
executables, with their SHA-256 values checked during image assembly.
These file hashes are assembly inputs. The existing signed login
record verifies the effective policy and its previous file roles.

On the corrected data volume, Settings starts normally in
`ce82361b`. The inspected frame shows Virtual-1 at 1280 by 800, and
F5 produces another display observation. Escape ends the original app
with status zero. The desktop then logs out normally, PAM closes and
logind removes the user session.

The original Files process in `cfd876a8` ends with status two after
successful exec. Its exact archived ELF contains the older argument
list without `--terminal-bold-font`, which the current launcher supplies.
Release `a9cfe52b0b1d` builds the current Files source with no build RPATH
and passes 15 tests, including normal and high-DPI Files view checks.
Its ELF digest is
`29e088fe3048ddf2dc896bfe54473cb360ad98f09c373e757eb97f392bd6245f`.

Terminal in `6b2cf1d5` starts its shell and reaches the SDL/NexaUI
renderer-ready marker. The process ends with status 139 before a
visible app frame. The prior desktop renderer failure makes a
similar Mesa path a diagnostic lead. The Terminal fault address is
still unobserved. Its cache warning does not establish the cause.

Signed image `0f697ed804cd` overlays the current Files ELF while
keeping the selected policy and other login executables. The Files
process in `8da33030` opens Documents and accepts Ctrl+N, the literal
name `vmprobe`, and Enter. The create result is confirmed, the next
view displays the entry immediately, and Ctrl+Q returns status zero.
The file was verified afterward via journal replay on an isolated
memory copy of the terminated VM's data disk: regular, 0600, size
zero, UID/GID 1000, `aetheri_documents_t`. The original captured
disk bytes were left intact. Guest normal shutdown and reboot
persistence are separate from this acknowledgement check.

Both signed images retain their read-only media digests. All new
QEMU instances terminated. Next diagnose and replace the Terminal
presentation path as needed, test shell output and file viewing,
and add release selection for the fixed apps to the public
assembly flow. The two confirmed apps run only in the selected
single-output VM. The hardware and USB gates remain.
