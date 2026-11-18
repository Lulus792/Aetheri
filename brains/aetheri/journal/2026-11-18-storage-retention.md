# Host storage and artifact retention

The allocated inventory on F: was about 189 GiB inside Aetheri. About
113 GiB belonged to local build and test artifacts, 75 GiB to the two
development VMs and 1.5 GiB to downloads. The project source and its
Git history occupied only a small fraction. The USB test media have a
29.3 GiB logical capacity each and use sparse allocation.

The two large files are the P0 Fedora development disk and the active
audio and hardware test carrier. They hold guest tools, SDKs and guest
state. The smaller test cases accumulated full copied boot media for
signed login, USB, shutdown and negative tests. Most of these runs have
their own compressed root archives and logs.

The first cleanup pass retired 56 generated media files on F: with
33.87 GiB of allocation and 31 temporary boot and root copies on C:
with 19.25 GiB of allocation. It checked active QEMU use and retained
backing paths, then recorded each removal in the local cleanup receipt.
The current test bases, physical USB recovery image and test records
remain available.

The next pass removed 14 superseded desktop, root, data, device and USB
image generations, reclaiming another 2.34 GiB on F:. On C:, the root
partition of the retained boot image reproduced the exact SHA-256 of a
separate 295 MiB extraction. The cleanup retired that duplicate.
The data-seed audit compared full SHA-256 digests against the selected
desktop seed after checking the retained backing paths. It selected
zero additional removals.

The active audio lab had zero free space in its guest filesystem. Eight
retired Rust build target directories were checked for live references
and removed. The guest gained 1.29 GiB of free space. Guest free space
is tracked separately from the Windows volume and the QCOW2 allocation.

P0 was offline. An attempted lossless conversion read about 1.29 GiB
in three and a half minutes. The conversion was stopped and its temporary
output was removed. The registered P0 system disk is the original
44.71 GiB file. The active audio lab occupies about 29.73 GiB.

The host cleanup retired about 36.21 GiB on F: and 19.54 GiB of
test media and duplicate data on C:. Exact file entries are in the local
`storage-review-2026-11-18-removed.jsonl`,
`storage-review-2026-11-18-old-generations-removed.jsonl` and
`storage-review-2026-11-18-root-copy.json` receipts.
