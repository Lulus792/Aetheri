# Confined graphical user session

The enforcing Fedora VM ran two fresh synthetic logins through the split
backend, PAM worker, publisher, greeter, session entry, compositor and
graphical shell. Both runs used candidate module `184122363465` and the
previously verified Release entry and protected compositor and desktop
binaries. The effective policy SHA-256 was
`7dc0f95005aa601c4c83e29a260db6f11806679f18bacca7db5f30e3148a757d`.

The compositor reads selected device metadata and opens a receive-only udev
monitor. The libseat logind path acquires the device descriptors. Both test
processes have the expected SELinux separation. The controller records
`/dev/dri/card0` (226:0) and three input event nodes (13:64 through 13:66)
on the original compositor. Loaded policy queries deny direct opens of those
types to the session and deny their use to the shell and three login roles.
[libseat logind backend](https://github.com/kennylevinsen/seatd/blob/master/libseat/backend/logind.c)
· [systemd device monitor](https://github.com/systemd/systemd/blob/main/src/libsystemd/sd-device/device-monitor.c)

The fixture projects a typed directory from the private tmpfs holder over
`/dev/shm` in the login unit. The controller ties the mount to the held
directory and compares it with the carrier's separate host directory.
The compositor and graphical shell can create shared buffers there. Mesa's
anonymous memory gets the dedicated type through the current kernel's shmem
label transition. The policy denies writes to generic tmpfs files. The
graphical shell runs under a separate type and receives the entry's original
Wayland, launch and diagnostic descriptors. Its executable-memory access for
LLVMpipe is in the carrier supplement. The test unit uses its own protected
loader cache built from the selected runtime. Native process maps show the
selected SDL3, SDL3_ttf, EGL and Gallium libraries from the protected runtime.
[LLVMpipe](https://docs.mesa3d.org/drivers/llvmpipe.html)

Case `ff65b1671440` visibly reaches the dock on the 1280×800 DRM output.
Its QMP capture SHA-256 is
`7ba13673a9cf1fdb6f1a6fe995bc202451db03518a559367b3955cd2367d197c`.
Case `36ea39e113cc` sends Super+Space and then the letters of `files`
through QMP while the original processes are live. The desktop reports a
settled frame with that query, and the observed image shows the Files result.
Its QMP capture SHA-256 is
`91d3970cd5828234ac792d8f1ed25d00b6c9fb0f32c1454eb200e00a71732d66`.
Both runs confirm the original entry's normal end, original PAM worker's
exit record and PAM close, an inactive scope, idle replacement greeter and
normal backend stop. The synthetic private holders retire after confirmation.
The screenshots and case traces remain in the local review directory and the
named guest result directories.

Preserved attempts show where each startup stopped. `073f85076fb8` reached
DRM and was denied the Wayland lock. `61a55902cca3` reached modeset and was
denied keymap shared-memory creation. `ae2e4eaa0588` failed before backend
exec because the direct labeled tmpfs mount required filesystem relabel access.
`4589db81465f` reached the compositor and the shell failed allocating Mesa
anonymous memory. `e29b65d742ed` initialized the renderer but still hit
LLVMpipe's executable-memory denial. In `84ec8adf9993`, the shell transitioned
and exited 127, and audit recorded denials against its old library search
paths and diagnostic pipe. The final case supplies the private loader cache
and scoped diagnostic pipe access. Both the earlier immediate PAM journal reads
and the separate VT retirement evidence remain in the review directory.

The loaded decision check `262dd52c8a88` verifies 228 queries with two
wrong-access and two wrong-policy controls. Strict compile `8752f0f7bfbb`
checks 426 selected inputs and rejects 16 unsafe additions, including execution
of writable memory in the compositor, uevent writes, raw input access by the
shell and shell writes to generic tmpfs. The candidate modules were retired
after confirming zero typed tasks. The VM is enforcing on tty1 with the exact
3,689,381-byte baseline policy, SHA-256
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.

The tested VM path covers the first frame and original session retirement.
The normal protected provider, actual graphical greeter, managed app starts
and laptop hardware remain to be verified before the first-version USB gate.
