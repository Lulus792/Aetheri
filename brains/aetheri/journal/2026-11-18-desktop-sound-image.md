# Signed desktop Sound selection

The Sound image has an explicit format-3 artifact manifest. It stages
`aetheri-audio-desktop-control` and the corresponding SELinux sources.
The Ed25519 selection adds one executable row. Both compiled login
providers select `AETHLS03`. Startup observes the mediator's protected
path, size, mode, label and contents in its original and backend mount
views. The common handoff still has ten entries. The base image selects
format 2.

Release build `e2cd43820800` compiles both image providers in both profiles.
Both `login.selection` and `login.selection_packaging` pass in each
profile. The packaging suite covers a missing helper, modified staged
bytes, signed Python-to-native record parity and the unchanged v2 profile.
The image's own dynamic loader resolves the mediator's dependency graph
from the read-only HDA base with no build RPATH. This check lists resolved
libraries without running the mediator.

The offline policy builds from a fresh, independent carrier store copy.
Its policy.35 SHA-256 is
`e218650eaabe3bc165a89d5c0378553d90ce49ccc844175ff73c4a0350d9bcea`,
and the mediator path selects `aetheri_audio_control_exec_t`. The 139
offline decisions include read-only startup observation, the desktop
transition, the received session socket, access to the graph, and the
denials for direct sound access, arbitrary app connection and process
control.

Strict compilation of all 426 CIL inputs exposed the distribution's
conditional `domain_can_mmap_files` grant against the existing audio
generic tmpfs map assertion. The earlier audio policy independently
reproduces this conflict. The strict audit copy excludes only the map
half of that inherited assertion. It compiles, and all seven injected
permission grants are refused. The installed policy sources retain the
existing assertion. Both the compiled and kernel-readback access checks
confirm that the mediator's generic tmpfs map is denied with the selected
boolean state.

UEFI preflight `7a9f758ed52b` boots the candidate policy on the read-only
root. The scoped probe captures 3,758,885 enforcing kernel policy bytes.
Their SHA-256 is
`0444e00fdf5a276dc7a743859d69f90889ce72a280d525645de734198ea9a8d2`.
All 139 decisions also pass against those bytes. The original QEMU process
ends with code zero, the boot media hash remains consistent, and the
carrier's loaded policy is unchanged. Preserve the frame stream and
`effective-policy.bin` in `local/policy-boot-737d22d7006e/boot-1/`.

The VM staging build uses a fresh isolated test release and rebuilds both
providers with its public anchor. The carrier lacks the OpenSSL CLI, so
the fixture signs with the pinned OpenSSL 3.5.9 library. The first signing
attempt rejects the fixture's 992 account expectation. The archived HDA
root and its original signed selection independently identify UID/GID 999
and sole group 999. The corrected stage verifies the enrolled account,
signs the 1,007-byte selection against the observed effective policy and
passes the native reader with the actual build-selected anchor. Its record
SHA-256 is
`11bbfde9798e4c45a122966e59fe76ce49b11e70e74436c616ef9f2deff69fa6`.
The checked mediator ELF SHA-256 is
`1e70cc65f916028200531e8c46631e3c788a1cf9bc6bfb7d54ef48f1708ce52b`.

The stage, policy layer, public anchor and receipts are retained in
`local/login-review-2026-11-10/desktop-sound-signed-stage-e2cd43820800.tar.gz`
with SHA-256
`677c9c71170a7fba1e0c8338bd4db03bbb0d55d2a58001a6b2d97202a6a8896a`.
The detailed builds and strict-audit evidence are in the matching
`desktop-sound-image-e2cd43820800-evidence.tar.gz` archive.

## Installed HDA Sound

The assembled VM image preserves the 1,007-byte selection and adds the
mediator at its fixed path with mode 0555 and
`aetheri_audio_control_exec_t`. The reviewed ELF, audio packages and
SELinux policy remain tied to the preceding stage. The raw ext4 root
passes read-only checking and its copied bytes match
`ffdf998bfa34b24a14b6a49670214c9e01fd9f70075e8d64eea3d26938a51366`.
The original HDA archive is intact. The test media SHA-256 is
`763eb2f0e3b20abaab8a3cdd1266cf0409b7a739dbffa3c84740fdf0ec4cc89c`.

Authenticated VM case `659ead1f` passes installed startup, PAM login and
the visible desktop. The emulated Intel controller reports an HDA control
node and PCM node. WirePlumber creates the analog stereo output and
Settings labels it Built-in Audio Analog Stereo. Through native keyboard
focus and action, the observed volume changes from 0.399993891 to
0.389999389 in revision 5, and mute changes from 0 to 1 in revision 8.
Settings renders the confirmed state and 39% volume.

The original Settings PID 274 and its mediator PID 275 both exit zero
after closing the window. PipeWire and WirePlumber retire on logout, PAM
closes the user session and the greeter returns. An ACPI request then
ends the VM normally, including the guest kernel's power-down marker.
The read-only boot hash is unchanged.

The missing mediator case `cac5a6eb` returns `-ENOENT`. The same-size
one-byte change case `59d7e72d` returns `-EBADMSG`. Both derive from the
successful image, retain the exact signed selection and pass offline
ext4 checking. Neither startup reaches completion or the user desktop.
Both QEMU processes end at zero after the expected refusal and each
read-only boot hash is unchanged.

Without an emulated audio controller, case `0b85af2f` boots the same
signed media and reaches Sound through the same Settings entry. The
mediator sends a ready snapshot with zero outputs. Closing the window
retires the app and mediator, logout returns to the greeter, and the
kernel powers off normally.

The first composition filled F: during final media readback. The second
uses a temporary directory on C:. First boot `b1808344` reached startup
completion and HDA. Its controller initially applied its workspace-only
socket boundary to that test directory, then resumed the same process
after confirming its QMP UUID. That attempt ended at the fixture password
lookup. Boot `c90385da` reached the desktop and the HDA route but the
controller typed before search focus settled. The final case waits for
the actual focus, frame and route. Those fixture failures are retained.

The locally retained evidence is
`local/login-review-2026-11-10/installed-sound-659ead1f-evidence.tar.gz`,
SHA-256 `4fff6d192098bdfb23a13e8c9fffb67dbf87e77dc7466203e240076e636e7120`.
It includes the serial traces, screenshots, media receipts and the exact
counterexample deltas. The successful VM's muted Sound screenshot is
`local/login-review-2026-11-10/installed-sound-659ead1f.png`.

Physical laptop playback, Bluetooth and suspend still need verification.
The first-version USB gate remains open.
