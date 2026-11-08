# Explicit laptop boot parameters

The first physical HP boot reaches the read-only root but fails its nonce-count
preflight. The video does not show the actual kernel arguments. Separate UEFI
controls demonstrate that overriding the embedded command line can cause the
same failure. This development profile uses the already selected systemd259.10
boot manager to pass one complete, independently checked options line to the UKI.
It keeps the original root/data guards. Physical acceptance remains a separate test.

The [pin](boot-manager-inputs.json) selects one additional EFI payload from the
same cached signed RPM used for the existing stub. Whole-RPM signature/hash and
payload size/hash are checked; the EFI binaries remain unsigned. Original Aetheri
code is MIT; existing systemd third-party licensing is retained. No new compiler,
bootloader family, package installation, key enrollment or firmware setting is
introduced. Production Secure Boot and advisory review remain work.

The ESP contains only the fallback manager, `/EFI/Aetheri/system.efi`,
`/loader/loader.conf` and `/loader/entries/aetheri.conf`, with their directories.
The entry contains the exact existing tty0/initrd/root/target/nonce arguments.
There is no command-line editor, automatic OS/firmware entry or persistent random
seed in this fixed test configuration. This is not a final boot menu or update policy.

Preparation runs under the ordinary Linux builder:

```sh
python3 tools/build_boot_manager.py prepare --image /private/laptop-base --packages /private/rpms --output /private/efi-prepared
python3 tools/test_boot_manager.py --image /private/laptop-base --prepared /private/efi-prepared
```

This stage needs the base manifest and original UKI; it verifies those resources
and exact profile arguments. It does not claim to inspect an absent complete
base disk. Formatting and copying target a new regular126MiB ESP file only.
The independent FAT reader checks both FATs, bounded chains, cross-links, exact
directories/files and the two fixed long filenames with their VFAT checksums.
Every copied file must match. Preparation records signed-RPM evidence and hashes.

Assembly independently validates the complete unchanged base disk:

```sh
python3 tools/build_boot_manager.py assemble --image /private/laptop-base --prepared /private/efi-prepared --output /private/laptop-managed
```

It reserves a NEW output directory, requires8GiB headroom and copies the complete
base into a new regular sparse image, hashing all logical bytes. It replaces every
ESP byte, including zeros. Independent hashes prove all bytes outside the ESP,
including both GPTs and the complete root/data regions, unchanged. The resulting
manifest binds both EFI files, the preparation and fresh immutable-range hashes.
No existing image, physical disk, service or firmware variable is modified.

`uefi_probe_image.inspect` accepts this extra tree only with the exact explicit
managed profile and pinned manager. Legacy direct-UKI media retain their old
strict tree. Managed inspection verifies the actual entry/config against the
unchanged embedded UKI arguments and checks every UKI resource. Metadata hashes
are integrity/provenance observations, not publisher or volume authentication.

The complete candidate must then pass the existing two-cold-boot no-UART desktop
test, actual capture review and independent read-only filesystem/proof-file
inspection. A successful small RAM experiment is insufficient. Physical flashing
needs the user's renewed authorization, a fresh protected intent, the exact
current Intenso layout/held identity and complete physical write/readback.
