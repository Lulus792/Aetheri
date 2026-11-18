# Host storage and artifact retention

## Initial cleanup

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

## Follow-up cleanup

The next allocated-size inventory measured 152.99 GiB under `F:/Aetheri`.
The two developer VMs accounted for 75.00 GiB including their metadata,
`local/` for 76.42 GiB and pinned downloads for 1.54 GiB. The raw files
accounted for 37.50 GiB of the project total. The separate Steam library
was also using F:. Its download directory measured 25.68 GiB during the
cleanup, so whole-volume free space does not measure Aetheri's savings alone.

Historical raw test images were opened exclusively and released only
verified, aligned zero ranges. Each completed entry has matching full
SHA-256 digests, original path, file identity, logical length and last-write
time. The two registered VM disks and current boot and USB recovery inputs
were excluded. The Windows temporary-directory pass skipped an already
compressed WLAN image.

Four compressed root archive copies were removed after full SHA-256
comparison. The local `storage-review-2026-11-18-archives-removed.jsonl`
receipt maps every removed copy to its retained archive.

The follow-up reclaimed 31.67 GiB on F: from 120 raw images and the four
archive copies, and 1.92 GiB on C: from six raw images. The per-image
verification is in `storage-review-2026-11-18-sparse.jsonl` and
`storage-review-2026-11-18-sparse-temp.jsonl`. A fresh inventory measured
121.33 GiB under `F:/Aetheri`, including 44.75 GiB under `local/` and
75.00 GiB under `vm/`.

The writable-data and desktop test controllers now copy the test seed as
a sparse regular file and still verify it before attaching it. A direct
copy of the retained desktop seed had the original 256 MiB logical length
and identical SHA-256, with 11 MiB of allocated ranges. The temporary
verification file was removed. All 23 host image/controller tests and 87
SQL, runtime and cached-input contract tests passed on Windows with Python
3.12.14. No new VM boot was needed for this host file change.

The README and project handoff now link the relevant specs and dated test
records. The original long handoffs remain in Git history.
