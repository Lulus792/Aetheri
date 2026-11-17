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
