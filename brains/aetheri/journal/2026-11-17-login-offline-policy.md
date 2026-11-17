# Offline login policy for the VM image

`tools/prepare_login_policy.py` compiles the production login startup,
backend and graphical CIL in a fresh, explicitly rooted copy of the
targeted distribution store. It stages the resulting enforcing policy
and login path contexts under `root/etc/selinux`. The compiler runs with
reload disabled and the store is kept separately. The receipt hashes
the selected inputs and installed output files.

Final test `441b9c463a9e` builds twice from 1,285 distribution store files.
Both receipts agree. The installed `policy.35` SHA-256 is
`e3f133155d5db5192590136991744808b3c8eadc615e262171ac754585281e7b`.
An existing output is refused. The carrier's effective policy remains
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.
The baseline carrier's disk policy and kernel readback already differ at
the same size, so the installed file cannot stand in for the signed
effective policy.

The VM-only policy boot uses the selected archived UEFI and ext4 image
layout. A new copy of the ext4 partition receives the offline policy, a
small snapshot service and file labels. The archive and the carrier's
live policy are preserved. The probe reads the selected kernel's policy
while enforcing, under `aetheri_login_startup_t` on the read-only root.

The final read-only media is `local/policy-boot-4904237e9396/boot.raw`,
SHA-256 `ab7be74f06eb82d5159c97009e7c87f7001dbe51d466bf095b92959529babbbd`.
Both cold boots use fresh UEFI NVRAM and one virtual CPU. They return
3,723,713 policy bytes with SHA-256
`5e4e5370a2755687f5dd7a47d2a468b0fabe2d5e843a7c1f1762cc69d55c407e`.
Both `boot-1/effective-policy.bin` and `boot-2/effective-policy.bin` have
that value. The controller verifies the compressed-stream checksum,
complete ordered frames and final size. It stops each originally pinned
VM after capture and rechecks the read-only disk digest. Full login
activation and normal guest shutdown remain separate.

Preserve the initial store-root failure `d5495668aa0a` and diagnostic
`151b4afdd74f`, which found that semodule prefixes the store path with the
selected root. Preserve the raw serial timeout `4a8c8d0d1f95`, compressed
two-CPU attempt `7303b5a29486` and first single-CPU parser attempt
`0352de770c54`. The final parser validates declared frame lengths even
when a kernel console line follows the complete payload before its newline.
Continue with the signed VM login image using the exact observed policy and
the selected backend, startup, greeter, session and display artifacts.
