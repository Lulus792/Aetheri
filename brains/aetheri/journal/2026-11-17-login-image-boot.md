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

## Installed Terminal presentation

The installed Terminal uses the existing NexaUI RGBA raster and converts it
to an opaque Wayland XRGB8888 buffer. SDL continues to own the Wayland
window, events and text input. The desktop and Terminal share the anonymous
file allocation and release-tracked buffer operations. Terminal keeps only
two slots and blocks in its existing wait path when both are retained by
the compositor. The development OpenGL ES paths compile separately.

The first assembly `5c0a59b9cafa` rejects build RPATHs on the installed
PTY starter, shell and editor. Release `ba38f908c8d0` builds those helpers
without RPATH and passes 18 selected tests from 844 inputs. The selected
Terminal ELF SHA-256 is
`32c1915045b937678359ecf566e4184414e065ce65e1324878fdb820cc1f4885`.
The selected archived authorizations and effective policy remain the same
as the earlier app image. The final window uses the current Files binary.

Signed image `a63130f871f1` boots normally. Its second cold boot
`1729a072` verifies the refactored dock and shows Terminal's welcome
screen and shell prompt. In the inspected screenshot the user input
`pwd` reached the original shell. The shell then returned 125 after
refusing job admission with `cannot select original job process group`.
The Terminal draws eight frames, closes through Ctrl+Shift+Q with
`failed=0`, and the desktop logs out normally. The crash in the old
Terminal's GLES path is no longer reproduced by this presentation path.
Its original fault address was never captured.

The shell's foreground selection calls `getpgid` on original job members
before `tcsetpgrp`. The exact deployed policy readback denies
`aetheri_terminal_t` to itself for `process/getpgid`. The proposed
permission is limited to that role. Offline build `7a795650f9d0` compiles
and the 35 decision checks pass with the new permission and the earlier
denials. The on-disk policy SHA-256 is
`57371932ca91ca7ac1e262f475dc43d6ddef89a2407eabe14a0ec5145c31caab`.
Boot readback and actual corrected shell command execution are pending.

The independent Files case `be1e7007` creates and displays the file.
Enter starts and pins the original Terminal document child. This child
returns status two and no editor window is observed. The exact rejected
preflight remains open. The selected file and original VM traces are
preserved. All three QEMU processes terminated, and the read-only boot
image stays SHA-256
`244d40c381fc557f52b6a602e05a99fe487ab99f440a567d76dfe00b0a3f75ee`.

The two older unmounted successful VM roots were reclaimed from the lab
after verifying their exact decompressed hashes against both preserved
archives. The compressed artifacts and Windows test images retain the
root contents. Failed raw fixtures remain available. Next boot the
job-control candidate, test actual output, and localize the document
preflight before testing save and reopen.

## Shell job selection and document handoff

Policy probe `75803813ed08` boots twice in `82f5f59b4c41`. Both
enforcing reads produce the same effective SHA-256
`1a48acc071186fecc2c4b96f2c15a4c5c149b3c5cc896579d0e2d5bd3d5d8a08`.
The readback passes 35 decisions, including the Terminal-only process
group lookup and the generic executable denials. The lab prepares the
image roots on tmpfs to fit the remaining disk space. Verified archives,
readbacks and boot logs are exported to the Windows workspace.

Signed image `5c0b5e09cd37` starts normally. In session `3dd8c5e6`,
the Terminal's actual shell runs `pwd`. The inspected frame contains
`/` and a fresh prompt. The current installed working directory is
`/`. Terminal closes with `failed=0` and the desktop logs out
normally. This check covers the built-in command; the generic external
execution policy still denies `bin_t`.

Session `a8b36713` reproduces the Files-created document start failure.
Terminal's preflight identifies a non-regular descriptor 4 and the
kernel audit denies `file/ioctl` on the held `O_PATH` document during
the Files-to-Terminal exec transition. Files now reopens the exact
pinned object as a read-only FD and compares type, device and inode.
The editor still checks the final leaf against that original witness
before reading. The policy still grants document reads without adding
file ioctls. The launch test replaces the named file and verifies that
the exec child receives the original readable object.

Release `8b1290550c8f` passes 18 installed-profile and seven additional
Files CTests. The corrected Files ELF is
`2686702c9b23f835ed455ef545244b02787a83ce8c1450aa1fc7cece1a10f482`.
The document VM uses a separate copy of the existing labelled data
fixture. Keep the original failed preflight case and data for comparison.

Signed image `a2906a8c4658` boots and opens the Files-created `vmprobe`
in the actual editor in session `4febad72`. Enter opens the original
pinned child, insert mode takes `validated`, and Ctrl+S displays
`Saved` and a clean buffer. The editor and Files both close with status
zero. Offline ext4 verification replays the journal on a memory copy
only and confirms the exact nine bytes `validated`, a regular mode
0600 file and the original SELinux document type. Original data SHA-256
`44febdbba9c0ca63a655b8b429f031ed4aa57d9af3e170c1d4fe4ed79cbb12bb`
remains unchanged during that inspection.

A separate cold boot `a63a9dbc` runs against another copy of that
original data disk. Files shows the existing entry immediately and the
editor displays `validated` in a clean buffer. The original editor
child and Files both exit with status zero. Desktop logout closes PAM
and logind removes the session. The two full images retain their
read-only boot digests and all new QEMU processes terminated. Next
admit the required shell utilities through the fixed release selection
and integrate the fixed apps into the public image assembly.

## Installed shell cat

Baseline image `a2906a8c4658` session `cd39ee52` uses a copy of the
saved document volume. Typing `cat /home/aetheri/Documents/vmprobe`
displays `Permission denied` and a fresh shell prompt. This reproduces
a failure in the current installed profile; the older laptop's exact
command and output are unavailable.

The original Aetheri shell now implements byte-stream `cat` using its
existing pipeline child, redirections and file permissions. It handles
partial writes, interrupted I/O, standard input and multiple files.
The tests cover copying binary data through two pipeline stages with
PATH unavailable, a dash-prefixed filename, input-error continuation,
and refusal to append a file to itself. Release `7afc331c0588` passes
all eight shell CTests and the 18 selected installed-profile CTests.
Its shell SHA-256 is
`82b386f97b216ea39ce1388ba74bbdf5628bc5331c02d28e83f49b908abf1ff2`.
Signed VM image `ca26a6fdcabf` retains the same effective policy and
starts through the protected login startup path.

Session `f72d8c78` displays the saved `validated` file through
Terminal. It logs out normally but the harness quits QEMU before the
redirected file reaches the recovered data disk. Session `de132e86`
displays both the original and redirected file with identical visible
text. Replay on a separate memory copy verifies that both files
contain the same nine bytes and that the copy has the document type.
The original captured data disk SHA-256 is
`ef5d30306cdb976b0c11abd6d3cc07a14706abd466d98e293aaee29638644732`.

The latter session also reveals a separate power-off gap. After normal
logout and logind's removal of user session 2, the ACPI power key starts
systemd shutdown while the replacement greeter opens PAM session 3.
The 24-second harness deadline expires without login-service stop,
data unmount or kernel power-down. The harness then ends QEMU.
Session `b4f9129e` independently matches the replacement account
prompt before requesting power-off. Its 45-second deadline expires at
the same service-stop boundary. Thus the observed stall also occurs
after the replacement prompt is visible. Logind removes the replacement
scope, but its worker's PAM close is absent from the trace. The stop
cause stays open.
Both attempts ended through the harness and their data are preserved.
Next diagnose installed idle-greeter shutdown with original-worker
observations before claiming clean full-system power-off.

## PAM worker hangup during power-off

The exact enforcing policy allows PID1 to signal the backend, the backend
to signal the worker, and the worker to signal the greeter. It also allows
the relevant child-exit notifications. Diagnostic images derived from the
same signed inputs add a test-only proc observer and leave the backend,
greeter, effective policy and signed selection unchanged.

In session `d127a653` the original replacement worker remains a zombie
under the original backend after the greeter/display disappear and the
scope is removed. The second image opens pidfds during the session.
Session `5b8f6781` confirms wait status 1 for the original greeter and
display. The unreaped worker is poll-ready with mask `0x7`, which lacks
authoritative exit information. Linux 6.19 records that information in
the task-release path. [Kernel task release][pidfs-release].

Session `f2d08a75` waits until all three original processes are observed
in the replacement scope before ACPI power-off. The pinned worker 257,
greeter 266 and display 267 then each report wait status 1 with the
`EXIT` bit present, establishing SIGHUP termination. The earlier greeter
and user workers in the same flow exit zero. The backend continues
waiting after the replacement worker's abnormal exit. The trace proves
the terminating signal and the original identities. It leaves the signal
sender unresolved. Preserve all attempts, including `ed74b1b1` where the
replacement worker was not sampled before power-off.

The worker now handles SIGHUP in its original synchronous signal loop
alongside TERM. It sends one orderly TERM request to its original child
and keeps HUP blocked through PAM teardown, while the unprivileged child
still restores the inherited mask before exec. The service's normal
worker-exit requirement and infinite-stop fence remain.

The pinned offline backend build `ae38890e7a49` passes 24 Rust unit
tests including the hangup regression, one documentation test and
checks for the startup-only and feature-disabled profiles. The source
preparer passes 13 tests. The initial /dev scratch attempt is preserved;
systemd rejects a mount-namespaced unit whose working directory is
under /dev. The successful build used /run scratch.

Signed image `9d2ac4bd4a92` contains backend SHA-256
`92d68e7e8cdf7b48d6d1f786c681f1da492dd646cbd173965891367b6d720627`.
Session `98e79dc6` logs out and shows the replacement account prompt
before ACPI power-off. The replacement PAM session closes and systemd
stops the login service. Session `2103af80` requests ACPI power-off
while the authenticated desktop and Terminal are still active; the
original user PAM session closes and systemd stops the login service.
Session `e7b22020` requests ACPI immediately after the user session
is removed, with no wait for a replacement frame. The new greeter's
PAM session also closes and the login service stops. All three runs
confirm ext4 data unmount, kernel power-down and QEMU exit zero.
Offline checking of a read-only copy from `98e79dc6` confirms ext4
consistency and the original nine-byte document. The successful
and failed diagnostic guest images and their session disks remain
exported on Windows. Redundant unmounted copies of their roots
were reclaimed after checking both compressed and decompressed hashes.

[pidfs-release]: https://github.com/torvalds/linux/blob/v6.19/kernel/exit.c

## Fixed desktop apps in release selection

`AETHLS02` appends Settings, Files, Terminal, PTY starter, shell and editor
to the original signed layout. The format-2 staging tool requires all
six x86-64 inputs. The installed startup retains their protected source
objects in both mount views independently of the core runtime and
ten-entry backend handoff fingerprint. The policy grants the startup
domain read access to the four app executable types and forbids execution
and mutation.

Release `8b66d256141c` uses the public
`aetheri-login-image-artifacts` target and passes 18 selected CTests,
including the Python/C signature round trip. The Python staging suite
rejects a missing input and mutation of each of the six staged ELFs.
Offline policy build `06decdb9490a` passes 32 app observation decisions.
Both cold boots of `63aff78237c6` give the same enforcing policy readback,
SHA-256
`134ef9bb6b880b3211420a4b9032bf843e39ccb071a57b0bdd5bbe070cb496de`.

Signed image `f6d7bcad58f5` composes the six ELFs through the public
stage and selection tools. It verifies their loader closure in the
composed image. The test keeps the existing lab release anchor, while
the production recipe requires a fresh release identifier. The base
image starts successfully. Its one-byte editor counter `ebaacb5e5591`
fails startup with `-EBADMSG`. The missing editor counter
`a911e718c475` fails with `-ENOENT`. Both retain the original signed
selection and their serial traces contain no startup completion.

Terminal `3a9b431b` shows the saved file with `cat` and powers off
normally after logout. Settings `a54d20bb` refreshes display information
on F5 and closes. In `28872687` the original QMP connection times out
with the new-file dialog open. The original VM and its QMP UUID are
verified again. The test cancels the partial draft, resumes on that VM,
creates the file, opens and saves it in the editor, and powers off cleanly.
The inspected editor shows `Saved`. Read-only ext4 checking of a separate
copy confirms all nine bytes on disk. All new QEMU instances are terminal.
The two refused images and all original data disks are preserved.

## Desktop HDA kernel layer

The optional desktop HDA payload selects the eleven previously reviewed
HDA modules and the Conexant codec driver from the same kernel release.
The carrier's kernel-modules RPM metadata and actual file bytes agree on
the codec digest. The supplement is motivated by HP's EliteBook audio
driver list. The tested module aliases resolve Intel PCI `8086:9d71`
and Conexant `14f1:2008`, alongside the emulated `8086:2668` controller.
This is alias coverage for the expected laptop family.
[HP driver record](https://ftp.hp.com/pub/softpaq/sp95001-95500/sp95006.html).

The public image preparer stages 12 modules and 591,588 compressed bytes,
writes one modules-load file, and emits the selected hashes. The graphical
developer builder runs depmod over the combined module tree. Three payload
tests and 85 existing HDA contract tests pass. The previously captured
enforcing policy denies all twelve tested direct sound-device operations
from Settings, Files and Terminal.

Signed desktop VM derivative `ab3a3c4a71ea` keeps its app selection and
effective policy. Its test-only observer confirms that the existing boot
loader loads `snd_hda_intel`, the emulated controller binds, and Linux
creates control and playback character devices with major 116.
The control node is root:audio at mode 0660. The signed app startup
completes. With HDA present, authenticated session `350b99ca` opens
Settings, refreshes the display list and closes the original window.
The same image with no HDA hardware in `53b57e05` reaches the desktop,
reports absence, and powers off cleanly. All new QEMU processes are terminal.
The tests cover detection and login compatibility. Next stage the admitted
audio graph and routing policy and connect the Settings sound controls.

## Desktop audio files

The checked PipeWire 1.6.11 and WirePlumber 0.5.19/Lua 5.5 prefixes from
`1678b5502917` produce 254 files (8,102,494 bytes) and six links. The
HDA desktop root already contains four matching files. The stager copies
250 and preserves the existing Lua 5.4 library used by libinput. The root
loader resolves all 28 selected ELF files using the composed image. Both
installed daemons report matching compile-time and linked versions.

Root `76e4dde39a97` passes the read-only ext4 check and keeps the original
signed app selection. The VM reports `AETH_HDA_READY` and
`AETHERI_LOGIN_STARTUP_COMPLETE`, exits zero on QMP quit, and leaves its
boot medium hash intact. The signed app selection covers the existing
app set. The audio inputs have their own staging receipt.

In carrier case `f42f2e9bb725`, the read-only desktop root runs both daemons
as UID/GID 1000 with private mounts, network and a mode-0600 graph socket.
The graph contains the `WirePlumber (smoke)` client and the `settings`,
`schema-sm-settings` and `sm-settings` metadata. The original manager maps
Lua 5.5. Both original processes exit zero after SIGTERM over their pidfds.
The image hash is unchanged. The earlier fixture `2fe1a5912158` timed out
because its observer expected an unqualified client name and `default`
metadata from a profile with hardware routing disabled. This was a test
expectation error. The fixture uses temporary config and a temporary graph
probe. Installed service policy, hardware playback and Settings are separate
acceptance steps.

## Desktop owner audio lifecycle

The opt-in desktop entry now starts the graph and routing policy after
desktop readiness and ties both children to the original session process.
Graph-only prefix preparation did not include the ALSA SPA plug-in. The
first owner fixture `cff1b3927c1e` exposed that missing file and the upstream
settings module's attempt to open a state directory despite stateless routing.
The stager now accepts the pinned ALSA prefix. The session owner prepares a
volatile state directory that it removes when empty after orderly retirement.

In `688ebdd1473e` the public stager selects 259 files and seven aliases
(10,228,824 bytes including shared files). Against the previous root it reuses
254 files, adds five and leaves the original root hash intact. The composed
root's loader resolves the ALSA plug-in. The original WirePlumber process maps
both that plug-in and Lua 5.5. Its graph entries and settings metadata are
visible in the isolated observer. The fixture has read-only sysfs and no
`/dev/snd` access.

The linked owner harness passes orderly stop and policy loss. Both retire the
original processes and remove the temporary runtime. A separate owner SIGKILL
causes both originals to exit zero. The first parent-death assertion in
`f17a5beb3541` required zero and failed. The diagnostic rerun `52c36fef11c9`
records WirePlumber exiting on SIGPIPE. The final owner sets SIGPIPE to ignore
for audio children. The follow-up `c20cd9429943` observed graph metadata
before the ALSA library was mapped. The final observer waits for each
independently. All three cases pass with the real pinned daemons in a private
read-only-image overlay and no installed desktop policy change. Release CMake
`784400f315a9` builds the feature and passes `linux.session` and
`linux.session_entry`. Fourteen payload tests pass.

## Desktop audio policy

The optional image compiler stages a dedicated graph domain and routing
domain. The session owner creates a separate labeled `audio` directory and
holds both it and the routing state directory. WirePlumber has sound-device
access within the pair. The graph and policy domains share IPC and their own
buffers. The installed desktop apps lack direct access to either.

Release build `564717d719fc` passes `linux.session` and `linux.session_entry`.
The first signed assembly `31dd6ef1e26e` recreated the selection file without
its original SELinux context. Startup rejected it with `-EACCES`. Assembly
`63e7e121e283` preserves the file context and reaches the desktop. Session
`46e7dafe` starts and retires both audio processes, clears the runtime and
returns to the greeter. ALSA reports a permission denial on the `/dev/snd`
inotify watch. The separately verified distribution policy labels the
directory `device_t` and the sound character nodes `sound_device_t`.

Offline compile `5c485cfcdd1b` adds WirePlumber's directory read/watch
permission. Its two snapshot boots read the same enforcing policy, SHA-256
`0b05857a7730c0e6aab8a3cd852e88094477de1b4bd179379accbaeb63582ca9`.
The resulting policy passes 78 checked decisions. Build `94eda0c97a69` failed
on the carrier's full `/run` and is preserved; the successful build used
shared-memory scratch. The carrier remains on its original enforcing policy.

Signed image `819df4d9e095`, session `b3f5a90b`, boots with virtual HDA,
authenticates a normal user, reaches the desktop, and starts both original
audio actors. Its test-only logging fragment records ALSA monitor activation
and successful inotify startup. Both children exit zero on logout, the
runtime directory is removed, PAM closes and the replacement greeter appears.
The test boots are terminal and leave their read-only media hashes intact.

The ALSA log has no admitted card or output route. The current root includes
`70-uaccess.rules` and the seat rules, but its `78-sound-card.rules` is
absent. PipeWire 1.6.11 requires the resulting `SOUND_INITIALIZED` property
before considering a card. Add the exact rule from the selected 259.10 udev
package, check the original card and control-node ACL in the live session,
and verify playback with a mediated client. The carrier's installed 259.5
copy has different provenance. No physical playback or Settings control
was exercised in these runs.

## Desktop HDA discovery

Fedora's signed `systemd-udev-259.10-1.fc44.x86_64` RPM supplies
`78-sound-card.rules`. The carrier verified the package signature, header
and payload digests. A literal GNU cpio extraction matched the RPM header's
4,816-byte file record. The earlier audio-only archive reader refused the
full udev package's member names. No package script ran. The rule's bytes
also match the earlier carrier observation, and the image uses the exact
259.10 package input. `spec/audio-udev-inputs.json` keeps these pins separate
from the frozen desktop input manifest used by libinput preparation.

Signed image `bbbdaf5a4579` uses the existing enforcing policy. The 79
readback decisions include WirePlumber's existing directory `open` access.
A candidate explicit allow (`d77491923c3b`) compiled to identical policy
bytes, so the redundant rule was omitted. The native stager verified the
final 260-file, seven-alias projection against the read-only image and
reused every file. Five focused native and 15 host payload tests passed.

With virtual Intel HDA in `62527d4e`, the normal desktop login starts both
audio actors. A test-only WirePlumber log fragment records successful
access checks on `/dev/snd/pcmC0D0p`, publication of the ALSA device, selection
of `output:analog-stereo` on `alsa_card.pci-0000_00_04.0`, and application of
`analog-output-lineout` and mixer volume. These logs show the ordinary
session's device and route behavior. They do not independently inventory the
kernel ACL or measure a rendered PCM stream. In `776d5a4a`, the same image
without HDA starts the audio pair and reports no sound device. Both sessions
reap both original children with exit zero, remove runtime, close PAM and
return to the greeter. All QEMU instances ended and media hashes agree.

Next bind an authorized playback/control client to the original graph and
verify the emulated PCM output and Settings. Physical laptop testing remains
after VM acceptance.

## Desktop HDA playback

The CMake desktop audio profile builds `aetheri-audio-test-tone` from the
finite playback diagnostic. The session owner selects the graph path and
passes its original PipeWire pidfd. The client verifies the kernel peer
before joining the graph. It submits 48,000 stereo frames at 48 kHz with
440 Hz on the left and 880 Hz on the right. The standalone lab diagnostic
keeps its existing options. Release build `87939b44a506` passes `audio.cli`,
`audio.tone_cli`, `linux.session` and `linux.session_entry`. The first CTest
attempt `76dc0f6cf05c` lacked the profile's library path and exited 127.
The installed-profile executable has no build-tree library search path.

The dedicated tone role can join the graph and exchange audio buffers.
Direct sound-device access and routing-state writes stay outside that role.
The first signed test `0c19e381b6e3`, session `df233e64`, reaches the
original graph, but the ALSA plug-in reports `Unknown PCM front:0`. The
PipeWire graph needed sound-device permission for playback. WirePlumber
continues to probe and configure the device. In offline policy `00f02fa0e750`,
both graph and routing roles still require the device ACL. Two cold policy
boots in `d5dedf0ec43b` yield identical enforcing readback bytes, SHA-256
`40f40dd0b0cc352884fd8ca170a06dad4fefa26f1072d3cbe4a9f9c97a7cff88`.
All 101 checked policy decisions pass.

Signed image `5dca38648c9a` uses a test-only session-owner build that launches
the tone once WirePlumber reports the analog stereo volume. The first two
capture controllers (`5d2199c8`, `faf52c14`) rejected the pinned QEMU's
deprecation warning. The corrected controller handles that exact warning.
Authenticated WHPX session `f8acdc2d` observes the original graph peer and
`DRAINED frames=48000`, then reaps the tone at zero. Both audio actors also
exit zero. The owner removes its audio runtime, PAM closes and the next
greeter is visible. The original VM exits and its media hash agrees.

The 48 kHz signed-16-bit stereo WAV contains the two requested frequencies
on their respective channels. Of 194,332 frames, 65,374 are nonzero and the
longest silent interval inside the active range has 17,497 frames (364 ms).
The graph reports `snd_pcm_avail after recover: Broken pipe`. A stream
drain confirms that the graph consumed the submitted buffers. The measured
emulated output fails the continuous playback requirement. The recording's
SHA-256 is `d75c30ea441541884d222d10f18e8ccc21de3eeb62a7b5d58844c136a34bb914`.

Instruction-clock session `dac1ef60` reaches the greeter, submits the
username and is still on the sending screen at the fixed password display
check. The controller exits the original VM cleanly without sending the
password. The follow-up waits for each expected greeter frame before sending
the next input.

The follow-up `2fecae0d` reaches the original password frame. During the
host restart the registered carrier process (PID 14356) and its volatile
fixture credential were lost. The controller cannot load the answer and
exits the original playback VM cleanly. Both instruction-clock attempts
leave their media hash intact. Neither reaches the stream.

The offline qcow2 check finds no errors on the existing carrier disk. The
stale localhost socket is preserved as `qmp-retired-14356.sock`. The same
registered carrier boots as PID 16648 with an observed QMP identity, new
boot ID `5c89d4e7-612e-4444-9625-6cb7e539c1dc` and SELinux enforcing.
For the next playback attempt `aa42d3f8909b` clones the archived ext4 root
and gives its synthetic `aetheri` account a fresh value through the same
SHA-512 password hashing profile. The clone preserves the shadow inode,
owner, mode and SELinux label. The signed selection, fixture owner and
validated tone executable have their original hashes. The new root passes
offline `e2fsck -fn` and hashes to
`3598c5c72f72a26674a942ebb10475b8e8f304f9f3683f261335b800abf56727`.
The test credential lives in a root-only directory on the carrier's disk.

In two-vCPU WHPX session `8a2a1208`, the clone boots and authenticates.
The tone verifies the original graph, drains 48,000 frames and exits at
zero. Logout ends both audio actors and returns to the live greeter. The
WAV hashes to `cbd417464449ec97848aa268e4bd3ea81de750b552d40365e75f33f07b21aaa1`.
It has both channel frequencies and three interior intervals of exactly
1,024 silent frames (21 ms each), so it still fails continuous playback.
The earlier `Broken pipe` line is absent in this run. The recorder and
console logs alone cannot establish where the missing frames arise.

Instruction-clock session `cadd2a22` uses the same clone. The original
graph admits the tone and drains it about one guest second after connection.
The WAV contains both requested frequencies and one interior interval of
1,024 silent frames shortly after the onset. It hashes to
`ce19acb8d89843875cc3d26b49515e5d0033c581c2a4c29641fbcab151ef3bc3`.
The gap therefore also occurs under instruction-counted emulation. The
host capture does not locate the missing period in the player, graph or
device. Both final clone VMs close PAM, reap both audio processes at zero,
return to the greeter and terminate with the original read-only media
hash `2246f765a5e802f2c22300e890e1eaa1496aaf75dfcd9e6c5131236bf5b40c3c`.
The host suites pass 85 HDA contract tests, seven PCM verifier tests and nine
carrier lifecycle and isolation tests.
Next test an independently observed, settled output when playback starts
and connect the normal Sound page. Physical playback remains untested.

### Playback with a running stream

The diagnostic variant prepends 24,000 frames of silence to the same
one-second stereo signal and uses the existing stream throughout. This
tests whether opening the stream earlier is sufficient to prevent gaps.
It uses the same original PipeWire descriptor, graph, WirePlumber profile,
virtual HDA hardware and fixture owner. The C17 build uses the existing
PipeWire 1.6.11 SDK and the exact library found in the image.

The first offline copy, `77d9a2633f6b`, stops on an invalid label
observation in the carrier's mount view. The carrier does not load the
desktop audio types. The second copy, `4e2b9f16c2ab`, uses `debugfs` to verify
the stored type before and after replacing the unsigned test child. Its
original inode, permissions and raw label survive. The fixture session
owner and the authenticated selection remain byte-identical. The replaced
test client hashes to `da6eb6a5b141d4470d6ff9c412922e3f5cd7d3b495bbb37923971bbc3c286efe`.
The original root hash is unchanged, and the clone passes `e2fsck -fn`.

WHPX session `dc0687be` logs normal authentication, the original graph peer,
72,000 drained frames and orderly logout. Its WAV contains both tones,
but has two silent intervals inside the audible second: 1,024 and 621
frames. The first is more than 0.8 s after audible onset. A half-second
of prior silence is insufficient on this VM. The capture hashes to
`c86936beeb292b89e45a9851271c5ec096f7651d439f83e80024595849b50629`.

The host verifier now accepts an explicit one-second expectation when
the test selects a known mixer gain. It preserves the two-second default
for the diskless cases. The 88 host contract tests include the 1,024-frame
dropout and the one-sided 320-frame tail loss. Both fail the verifier. A
complete one-second signal after silence passes.
The earlier sample-exact diskless capture `playback-final-v7` still passes
at 96,000 frames. A one-second excerpt of its observed signal passes at the
same known gain with less than 0.5 LSB error.

The instruction-clock session `19bce8cd` opens the same warmed stream and
drains 72,000 frames. Its right channel matches the full 48,000-frame sine
to within two LSB at a common gain estimated from the first 1,200 frames.
The left channel matches up to frame 47,680, then has 320 zero samples.
Both channels have no shared interior silent interval beyond the expected
sine crossings. The exact per-channel check still fails. This sole missing
tail is the remainder of 72,000 frames modulo 1,024. The PCM does not
identify which layer dropped it. The WAV hashes to
`8b1232fd3650a4a8e2777c3ebdaefb05322f640364169b28266821637fdf6630`.
Both runs on `4e2b9f16c2ab` retire their original peers, close PAM and return
to the live greeter before ending the VM.

The final-period counter `b231a92fb6fd` keeps the same 24,000 leading zero
frames and audible signal, and appends 704 zero frames. Total submission is
72,704 frames, exactly 71 blocks of 1,024. The derivative replaces only the
fixture tone and verifies the signed selection, raw label, root hash, full
host media readback and `e2fsck -fn`.

In instruction-clock session `ada4d8a7`, the finite client joins the
original graph and drains. The owner reaps it at zero. All 48,000 audible
frames in both channels pass the strict WAV verifier at the gain estimated
from the preceding unpadded run (`0.6258733866969072`). Maximum error is
1.931 LSB. The recorded
WAV hash is `0f2536cb6a09dfff6088f3a21008b84871d53ca5850f86f4aaf2c945aaecdee3`.
The user session closes normally and returns to the greeter. This is a
result for the test variant and tested virtual clock. The installed client
would need to derive a final buffer size from the negotiated stream turn.

The first WHPX counter `ba1bfaa6` stops before audio. The original greeter
reports `-ETIMEDOUT` and startup reports `-ESTALE` during publication. The
controller sends no credentials and retires the original VM at zero.
The controller now recognizes the terminal startup marker before waiting
for a greeter frame. No cause for the startup failure was established.

The two-vCPU WHPX retry `ac7f0a27` logs in on the same read-only media and
drains 72,704 frames through the original graph. It passes all 48,000 audible
frames on both channels. The gain estimated from the earlier unpadded WHPX
run (`0.9119864427150365`) also yields the same 1.417 LSB maximum error. The
WAV hash is `0e2c8b208fd0024101ef22c577837f43d0b5f9501d3072e8ced3e3d4c2795b5c`.
Both successful padded runs reap the three audio processes at zero, close
PAM and reach the replacement greeter. Each original VM exits at zero and
the media still hashes to
`8694c42a082ff350413a499b2e1525e481cc5f07cef730f1f5d083c4e0bee964`.
The fixed padding validates the known VM period for these cases. The
regular client still needs padding derived from the actual stream request,
and Settings and physical audio require their own acceptance.

The host stored three terminal boot-time data copies with NTFS compression
and confirmed their before/after SHA-256 hashes. The additional media and
data copies were created sparse and verified byte-for-byte at readback.

### Request-sized final buffer

The desktop tone now queues the requested playback length within the mapped
buffer capacity and its 8,192-frame turn bound. It copies the remaining
prepared frames and zeroes only the rest of a requested final turn. The
buffer and chunk size include the padding. The signal count advances by
prepared frames. A zero request keeps the short buffer. The separate lab
player retains its existing queue lengths.

Native case `e1654f8b2b8a` exercises the actual process callback for six
turns. They cover padding, a request larger than capacity, the turn bound,
zero request, exact end and a nonfinal turn. The test checks two-channel
sample contents, silence, untouched space and queue/drain accounting.
The previous client fails on the expected queue-size check. The current
Release and ASan/UBSan builds pass. The first ordinary-user launch failed
to load the selected shared library through a root-only SDK directory.
The passing launches use a byte-identical readable library copy.

CMake Release `286dbb597cec` verifies 252 pinned SDK files and builds both
players and the callback test. All five selected CTests pass, including
the separate lab player's full private-graph playback. The initial build
preparation calls failed on their arguments or ephemeral work path. The
final CTest uses the reviewed toolchain's loader wrapper.

The first runtime image `7a4c18b0e935` uses request-sized padding alone.
Instruction-clock session `299a9a77` authenticates, drains, reaps the
original children and returns to the greeter. Its independent WAV still
has 1,024 consecutive silent frames shortly after audible onset. The WAV
hash is `1d4a50e6b32dea2bd00a6356687f21655f0845b586fdbfd70e72f528f57c687d`.
This preserves the measured startup failure with the new final-buffer code.

The CMake target now prepends 24,000 silent frames in the desktop tone
profile. The signal begins at phase zero and still has 48,000 frames.
Socket and pidfd admission, CLI and diagnostic frame counts keep their
existing contract. A separate Release build with the final source passes
the five selected CTests. The image `9ab6c4e3d2f7` stages that exact tone ELF,
SHA-256 `be895f9198c673529889b96a836e853513da26635770897bbf5221e2bfdf5e2d`.
The root and boot projection checks preserve the signed selection, file inode
and raw exec label. This uses the existing session-owner fixture trigger.

The final ASan/UBSan callback test and all 88 host HDA checks pass. In
two-vCPU WHPX session `502af9e3`, the exact CMake client connects and drains
48,000 tone frames. Its WAV has two interior gaps of 1,106 and 1,024 frames
and fails strict validation. This happens before the final turn. The
earlier fixed-length WHPX acceptance does not establish repeatability.
The capture hashes to
`4b8b9ace1c9965ec17abefa4ebe7be0ce4ab9561201e836c9ef0dfcaa9fafb25`.
The original processes exit zero, PAM closes and the next greeter appears.

Instruction-clock session `73cd84eb` passes all 48,000 frames per channel
with the same gain and 1.931 LSB maximum error. The WAV is byte-identical to
the successful fixed-padding WAV from `ada4d8a7`, SHA-256
`0f2536cb6a09dfff6088f3a21008b84871d53ca5850f86f4aaf2c945aaecdee3`.
The signed test image reaps the tone, policy and graph, closes PAM and shows
the next greeter. The VM ends at zero and verifies the read-only media hash
`1734129e8029194998ff1050415b84c9ee3a1832529c713dfad8066cbab2843b`.

The same media also passes 48,000 frames per channel in two-vCPU WHPX retry
`54508ece` with 1.417 LSB maximum error at the previously measured WHPX gain.
The WAV hash is `89bbb1a92092b2f93a1ba930eac92d834a2873145bdccc195bedd9678e7b25fa`.
A kernel audit message splits the tone reap line. Preserve the controller's
`tone_end=false` observation. The session owner's final original-child
summary records status zero for the tone, WirePlumber and PipeWire. PAM
closes, the replacement greeter appears and the VM ends at zero with the
same read-only media hash. The WHPX results include a pass and a dropout
on identical media, so the remaining repeatability question is open.

Next connect the authenticated session owner's audio mediation to the
normal Sound page. The playback button and settings need their own native
acceptance. Physical HDA and Bluetooth have yet to be measured.

## Sound settings channel

The existing Sound model now has a separate client adapter for a private
Unix seqpacket pair. The versioned codec carries a bounded device snapshot,
the exact expected revision and output reference for an intent, and the
original request's submission result. It uses explicit field sizes and
validates texts, numbers, packet lengths and reserved fields on receive.
It rejects ancillary truncation. A pipe-lifetime test confirms that Linux
discards the sent ancillary FD on that path.

The owner-side dispatcher consumes increasing request numbers and calls
the authority callback once for each admitted request. A repeated request
retires the channel. If a completed dispatch cannot send its result, the
owner marks the channel retired. The Settings adapter consumes one local
intent at a time, rejects mismatched replies and retains unknown feedback
when the authority may have acted before disconnect. The existing model
still requires a matching observed command to confirm the value.

Native CMake builds in `5b6fdf4e0032` pass `settings.sound_channel` in
Release and ASan/UBSan. The test uses real Linux sockets and the existing
Settings model with a controlled owner callback. It covers the maximum
64-output snapshot, volume submission and readback, stale responses,
duplicate handling, full reply queue, invalid input, unintended descriptor
delivery and peer loss. The existing native `test_settings_sound` also
passes a strict Release compile and execution in the same carrier.
The normal desktop still needs to provision these endpoints and bind the
live owner. That step includes original child and session admission, the
Settings native event loop and a joined HDA test.

## Sound channel native Settings entry

The ordinary NexaUI Settings executable now accepts the inherited Sound
channel on FD 3 with the explicit launch marker. On Linux 6.19.10, the
launcher test confirms the kernel peer credentials on an unnamed seqpacket
pair across fork. It rejects a self-created pair and a stream, and checks
that the admitted descriptor has close-on-exec set. The selected parent
must still be admitted by the desktop launch and installed policy.

The window wait includes both the existing quit pipe and the Sound FD.
The receive budget is eight packets per turn. A failed channel is retired
and closed while the other Settings pages keep processing Wayland input.
The private view test launches the real Settings ELF and clicks Sound and
Mute through the test compositor. An idle view renders a synthetic output
snapshot. The original request carries its selected revision and output
identity. The submission leaves the mute state unchanged; matching
readback confirms it. Closing the owner FD fences Sound, then clicking
Displays works and Escape ends Settings at zero. The test also checks
that the launch marker without an inherited descriptor returns 2.

The focused Release build `05da0db34a11` passes six CTests with the
original ctest executable and pinned library environment. Earlier attempts
include a mistyped build target, a missing `libmtdev.so.1` in the compositor
loader path and a silent compositor exit with an overlong private Unix
socket path. The fixture now puts its runtime socket in a short private
directory. The toolchain ctest wrapper dropped the required loader paths;
the unchanged final binaries and fixture pass under the original ctest.
The logs remain in the carrier's `settings-native-981144e1ae6a` and
`settings-native-05da0db34a11` work directories.

Next issue the pair from the authenticated desktop owner's fixed Settings
launch, bind the original live graph and audio authority, and exercise
the complete path on a signed HDA image. The private fixture uses a
synthetic owner. Physical audio, Bluetooth, WLAN, suspend and the USB gate
remain open.

## Live desktop Sound mediator

`AETHERI_DESKTOP_SESSION_SOUND` adds a fixed private Settings launch and
one mediator for the lifetime of that window. The desktop owner stays
single-threaded. It captures the original child PIDFD, remaps its
inherited descriptors after the launch gate and leaves unrelated
descriptors close-on-exec. The mediator opens the live graph through the
original-listener check. It rechecks the local session and the graph,
routing and Settings process descriptors on publication and submission.
The mediator retires first on desktop shutdown.

Release `96a8f8690897` builds the owner, mediator, Settings and private
compositor against the checked PipeWire 1.6.11, WirePlumber 0.5.19 and
GLib 2.88 profiles. Four CTests pass. The session-entry test exercises a
real gated exec with reversed overlapping descriptors 3–6 and an
additional close-on-exec descriptor. The preceding strict compile caught
the signed complement of the GLib event flag and was preserved.

Carrier case `8a4c67bbf1` uses a controlled wire client in an actual
PAM/logind Wayland scope. It observes live mute readback and channel
retirement on original routing loss. Final case `f3d0924ca9` uses the
actual Settings executable and a private Wayland compositor in session
`165`. The software sink reports mute off. A real click changes the
observed value to on in revision 4. Ending the original WirePlumber child
fences Sound while PipeWire remains live, and the user can still navigate
to Displays and close Settings. All five children exit zero. The
carrier returns to VT1 and the test seat session is absent.

The first live case lacks `libpipewire-module-client-device` in its
fixture search path. The first GUI case gives the capture directory
the wrong mode. Two later cases pass the Sound workflow and record a
compositor exit of 1 after premature closure of its test input controller.
The final run stops the compositor while retaining that controller.
The carrier's PAM configuration reports its existing `pam_lastlog2`
loader warning during each login. The retained evidence includes those
cases and the final ELF hashes in
`local/login-review-2026-11-10/settings-live-sound-96a8f8690897-evidence.tar.gz`.

The active test scope uses the carrier's unconfined user domain and a
software sink. Next sign and stage the mediator, admit its dedicated
SELinux role, and verify the ordinary authenticated desktop path on
virtual HDA. Physical audio, Bluetooth, suspend and the USB gate remain.
