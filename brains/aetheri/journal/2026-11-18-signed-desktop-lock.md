# Signed desktop lock

The installed lock selects `AETHLS04`. The two new rows cover the private
dialog ELF and the unlock PAM profile. The existing greeter font row also
covers the font used by the lock. Sound occupies its existing row and may
be absent in this format. The compiled providers require the selected
format and Sound presence. Startup holds the lock inputs in each original
mount view alongside the existing runtime and application observations.
The protected handoff still contains ten sources.

The stager accepts format 4 with or without the Sound mediator. The offline
PAM verifier includes the unlock profile's contents, file metadata and module
references for this profile. The image combines the typed dialog, shared
memory and inherited authority permissions with the original privileged PAM
worker. See [deployment](../../../spec/login-deployment.md#signed-login-selection)
and the [privileged unlock handoff](../../../spec/login-backend.md#privileged-unlock-handoff).

## Policy verification

Offline build `7cc1453334db` retains the distribution store and the loaded
carrier policy. The lock-only and Sound-plus-lock profiles pass 39 access
decisions each. The Sound-plus-lock profile retains all six preceding
module source digests and changes the compiled policy and local file
contexts. Its on-disk `policy.35` SHA-256 is
`18cb8a50f825817f3eeab804f70318bef9a317bec8dc612789cc8e6edc9e6397`.

UEFI preflight `c38aea8374d2` uses a separate read-only boot medium and
captures 3,732,073 bytes from enforcing selinuxfs. The complete probe
stream reconstructs the kernel's serialized policy with SHA-256
`f9f189d7c4692ab5b39d1a06781ae1df7c147829a4589ff34de1b02f59c76b8b`.
An offline reader checks all 139 previous Sound policy decisions and all
39 new lock decisions against those bytes. The exact ext4 clone passes
read-only `e2fsck`. The probe emitted its end marker. The QEMU controller
did not observe a natural exit and retired the original process after its
timeout.

## First installed attempt

Release `3d2527a6cf4d` compiles the full native artifact set with Sound,
lock and both installed providers. It passes eight selected CTests and
the separate privileged authority test. The stage uses the previously
tested privileged greetd binary, the reviewed UID/GID/group 999 greeter
account and a fresh VM-only signing anchor. The 1,079-byte selection SHA-256
is `4285f9497fa236a41e107271e286b5dab8961de00473c580d5949519c30d0fac`.
The composed ext4 root passes read-only `e2fsck`. The boot medium SHA-256
starts with `86f471e4af99`.

Graphical boot `af1760af` passes the installed startup and actual password
authentication. The original desktop enters its runtime and starts the
compositor. That build lacked Compose while the fixed desktop arguments
included `--compose`. The compositor prints its usage and exits with code
2. PAM returns to a replacement greeter. The controller closes the VM.
The original failure and its serial evidence stay available.

The installed image build now requires Compose when it builds the desktop
owner. The existing reviewed Fedora table and license are the inputs for
the corrected profile. Both names for the compositor ELF retain their own
image role and SELinux file label.

Separate media from the first assembly retain the exact authenticated
selection. Case `9b0a7508` omits the dialog and startup refuses it with
`-ENOENT`. Case `3533df94` changes one profile byte in place and startup
refuses it with `-EBADMSG`. Both QEMU instances end at zero after the
expected refusal. The source media and both read-only boot hashes remain
consistent.

The final Compose-enabled build `c89a9ddb223b` passes the same eight CTests
and the privileged authority test with the original table and license. The
test's runtime inputs still use the frozen distribution and NexaUI pins.

## Received keyboard map

Image `88216498e006` reaches the authenticated desktop in session
`3122bd62`. The lock dialog reports that the `wl_keyboard.keymap` event
arrived without a descriptor. The dialog reaps with exit 1, the original
broker reports `-EPROTO`, and the lock cover remains black. Rate-limited
kernel output has no AVC for the specific descriptor in this attempt.

The pinned wlroots source creates the keymap with `shm_open`, opens a
second read-only descriptor, unlinks the name and clears the inode mode.
The installed compositor uses the private `/dev/shm` directory. The
policy projects that file as `aetheri_session_shm_t`. The dialog now has
read, getattr and map permissions for the received keymap and an explicit
denial for opening or writing files of that type. Its own pixel buffers
retain their separate type.

Policy build `274418829ad3` passes 45 decisions for each of the two lock
profiles. The Sound-plus-lock disk SHA-256 is
`f4f149287c60d2ade13c577aace7abbcd5c9e1c606be3e7bf49efa18dbc2d538`.
UEFI preflight `aelp-86b039ab7f` captures the enforcing readback
`420f786a18815838a1afea3f79b59d88643bb3aca356af343a761a9f4e0fbb28`.
The original QEMU process ends at zero after the complete marker, with
the read-only media hash consistent. All 184 checks pass against the
reconstructed kernel bytes. Build `d15bc8f7392f` passes eight CTests
and the privileged C test with the fresh VM anchor.

The Compose-enabled root also passes `e2fsck`, the independently
selected greeter account check and the PAM closure check with all four
profiles. The Fedora carrier initially interpreted the SELinux labels
using its own policy. The subsequent offline fixture reads raw SELinux
attributes directly from the same ext4 bytes and sees the required types.
The image's own dynamic loader resolves all dependencies of the dialog.

## Installed manual lock

Image `23bcbeebbda657dfd85cda7250489c0641cdcf617a955268a491c29eed7ac7d6`
binds the final PAM profile and dialog in the 1,079-byte selection
`b6465aa323620a650bdea2c036283a7b37ecc258f2f0cfbc12a6c517eb1280f7`.
The exact read-only ext4 root passes `e2fsck`, the greeter account check,
PAM closure verification and the dialog loader check. The previous Sound
image remains intact.

With enforcing SELinux and a virtual HDA controller, `c639ec7d` reaches
the original UID 1000 desktop. Super+L covers the output and presents the
focused password field. An incorrect password keeps the cover and displays
the refusal, confirmed by `pam_unix(aetheri-unlock:auth)` in the guest log.
The correct password restores the same desktop image and original session.
The compositor, desktop, lock view, PipeWire and WirePlumber each exit at
zero on logout. PAM closes, a new greeter appears and ACPI power-off reaches
the kernel's power-down marker. The QEMU exit status is zero and the boot
media SHA-256 stays consistent.

This installed VM has one virtual display. The earlier carrier tests
cover three displays under their own profile. The installed lock still
needs suspend and resume coordination and confirmation on physical test
hardware. The USB gate remains.

The policy, build, root checks, screenshots and serial logs are in
`local/login-review-2026-11-10/signed-desktop-lock-2026-11-18-evidence.tar.gz`
with SHA-256
`b4f020607bd47427cbf11abae1009f2be3a6b43827c5a94abc56b70d54d863e8`.
