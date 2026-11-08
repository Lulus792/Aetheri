# First laptop-test image

This development profile extends the single-medium USB desktop for the HP
EliteBook 840 G4. Implementation and isolated tests do not certify physical
hardware. Consult STATE and the dated journal for actual executed boundaries.
It is a live test image, not the installer or the finished everyday system.

## Additional components

The [inventory](laptop-probe-inputs.json) selects 18 compressed modules from the
same 6.19.10-300.fc44 kernel, Kaby Lake DMC 1.04 and its unchanged Intel licence:
20 non-executable files, 2,488,128 bytes before overlap with the existing runtime.
No package transaction or carrier module load is performed. The ordinary Linux
builder copies them into a NEW tree and regenerates only that tree's module
dependency/alias metadata.

| Component | Purpose |
| --- | --- |
| i915 and its DRM dependencies | Intel display modesetting and scanout |
| Kaby Lake DMC 1.04 | Display power-state support requested by this kernel |
| I2C HID, multitouch, I801 and Sunrise Point pin control | Laptop input/controller paths |
| HP WMI and its dependencies | Platform/input events where matching hardware exists |
| Existing bochs, built-in PS/2 and USB HID paths | Keep the virtual display/input test profile |

Only bochs/i915 are explicitly selected for module loading; the additional input/
platform drivers follow hardware modaliases. Optional GuC/HuC blobs are not selected
or enabled. The installed files match the trusted carrier's RPM database digests;
recorded RPM header key IDs are not a fresh downloaded-RPM signature check. The
review reuses the current kernel ABI, not a silent kernel upgrade. Broader security/
update policy remains work before everyday use. Original Aetheri code stays MIT;
third-party kernel/firmware terms remain intact.

## Display selection and logging

The development owner now asks the compositor for `--drm ... auto`. Its existing
active logind/libseat session calls the selected wlroots GPU enumerator. Kernel
primary-device ordering is used rather than assuming `/dev/dri/card0`. Enumeration
has a spare slot and refuses saturation; unselected devices are closed. An explicit
WLR_DRM_DEVICES override is refused in auto mode; controlled explicit card paths
remain available. This selects one GPU, not a general hybrid/multiple-GPU compositor.

The reviewed wlroots constructor consumes its device on some failures but leaves
it with the session on early failures. Aetheri records an integer address before
the call and closes only a still-owned list member afterward. No freed candidate
pointer is read. Controlled native tests use real FDs and heap objects. The first
candidate also selects the virtual GPU in actual boots; physical Intel selection
still needs hardware evidence. Rendering remains the existing
development software profile; GPU acceleration/performance is not accepted here.

The laptop UKI requests ONLY tty0. The first dual-console candidate passed serial
tests but shut down early without a UART; it is not accepted for physical use.
The corrected single-console build must pass new actual no-UART boots before
acceptance. The separate old serial-only diagnostic remains available.
Desktop stdout/stderr use the existing bounded volatile journal,
whose console target is `/dev/console`, instead of opening ttyS0 directly.
No logging daemon beyond the already selected journal is added.

## Tests before physical flashing

Build with `tools/build_laptop_probe.py` as the ordinary Linux builder. Run the
existing two-boot USB desktop controller only for serial-console diagnostic
artifacts. For the laptop's virtual-console profile, run
`tools/test_laptop_no_serial.py` on a NEW copy of the actual artifact. It explicitly
removes serial/parallel devices, retains local AF_UNIX QMP and authenticates the
original VM UUID on its single connection.

The no-UART test records native desktop/Terminal/document captures for engineering
review. A review record binds the exact capture hash; it is not user approval of a
product design. Fixed real keyboard input writes the document and current boot,
process, mount, console and selected service observations. Unknown input/replies
stop the attempt and preserve it. There is no reconnect/replay. Normal shutdown
requires the original process exit and one QMP guest-shutdown event; reset, panic,
host termination or EOF alone cannot pass.

After both boots, independent ordinary-user e2fsck/debugfs must verify a clean
unchanged data copy, exact document, two distinct boot IDs, UID/GID 1000, zero
effective capabilities/NoNewPrivileges, read-only root, read-write nosuid/nodev
data/home, tty0-only active consoles and successful data/device-check units.
The controller deliberately ends as `observed_requires_offline_verification`
until that additional evidence is checked. EFI/system/GPT ranges must remain
unchanged after each boot. These files are test observations, not process/volume
authentication or production app admission.

The controller normally requires 8 GiB free host space. Its lower 4 GiB floor is
restricted to local NTFS sparse sources whose kernel allocation query proves at
most 512 MiB possible data. The measured allocation/free space are recorded before
copying; incomplete attempts are preserved. This is a development test budget,
not a disk-space reservation against other programs.

`test_usb_data_filesystem.py` creates a separate NEW regular copy and corrupts
only the primary data superblock UUID. The deliberately stale metadata checksum
is retained: this is a corruption negative, not a valid reformatted filesystem.
Require the exact data-filesystem rejection AFTER root handover, before writable
mount/GUI, normal poweroff and unchanged source/negative bytes. Its result parser
also retains the distinct wrong-parent outcome; mismatched reasons cannot pass.
The dated record distinguishes tested artifacts from later console changes.

Physical Intel input/display, multiple monitors, audio/WLAN/Bluetooth, suspend,
acceleration, normal login/MAC/lifecycle, the installer and daily-use acceptance
remain separate work. The already authorized flash requires exact Intenso device
reidentification and complete write/readback; development continues afterward
without waiting for the user's later hardware test.

Primary references: the selected
[wlroots 0.20.2 session source](https://gitlab.freedesktop.org/wlroots/wlroots/-/raw/0.20.2/backend/session/session.c)
(also read from the pinned local source),
[Linux console rules](https://www.kernel.org/doc/html/latest/admin-guide/serial-console.html),
[kernel DMC mapping](https://raw.githubusercontent.com/torvalds/linux/v6.19/drivers/gpu/drm/i915/display/intel_dmc.c),
[firmware inventory/licensing](https://gitlab.com/kernel-firmware/linux-firmware/-/raw/20260309/WHENCE),
[Fedora package](https://packages.fedoraproject.org/pkgs/linux-firmware/intel-gpu-firmware/fedora-44.html)
and [QEMU 11.1 shutdown causes](https://raw.githubusercontent.com/qemu/qemu/v11.1.0/qapi/run-state.json).
