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

Next assemble the signed stage with the HDA image and apply the selected
file contexts. Verify both read-only startup views, the session owner's
actual confined transition, graphical volume/mute readback and normal
teardown. Keep missing and changed mediator checks against the same signed
selection. The first-version USB gate remains open.
