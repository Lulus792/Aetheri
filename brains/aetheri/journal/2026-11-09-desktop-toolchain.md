# Private native desktop tooling — 9 November 2026

The preceding input/Sound milestone requires a current whole-desktop rebuild.
The separate enforcing audio carrier lacks CMake, g++ and most development SDKs.
Keep its original libstdc++16.0.1 unchanged because the retained instrumented
libinput/Lua fixtures pin it. Use a private16.2.1 C++ tool/runtime projection
instead of a global upgrade. Read the [contract](../../../spec/desktop-toolchain.md).
This advances build preparation, not the user's first-version USB gate.

## Reviewed inputs and failures

Host review is under `local/full-desktop-build-2026-11-09`. Two incomplete
host-only bundlesv1/v2 have no guest dispatch or completed intent; retain them.
V3 queries six signed packages. V4 adds CMake's three missing libraries. V5 also
inspects cmake-filesystem: it contains directories only and contributes no
payload to the private prefix. Final metadata has no scriptlets. The nine-package
profile and all selected regular hashes derive from these signed immutable RPM
snapshots. Both new CMake RPMs use the previously selected4.3.0 baseline; all GCC/
Ninja inputs were already pinned. The libuv1.52.1 package is an explicit tool-only
selection, not a normal runtime upgrade. Source licenses are retained separately.

The original `query-tools.py`v5 dispatch exits0. Its protected input directory is
`/root/aetheri-desktop-tool-review-9ff3445d226c4e80a11f5c344dd8fb65`;
`tool-query-v5.json` retains full requirements, signatures, metadata and inventories.

Native preparationv1 fails only because `/home/aetheri/deps` does not exist;
its selected output is absent. An ordinary-user owned parent is created forv2.
V2 prepares successfully but relocated g++ cannot locate the existing native
linker plugin during CMake's C++ link test. Both failed attempts remain at
`/run/a-dts-1d187ed4e1e1` and `/run/a-dts-a4d92ac6c945`; no failed prefix is adopted.
The fix explicitly adds the original native libexec/libgcc search directories.
No compiler feature, linker plugin, warning or test is disabled.

Freshv3 dispatch exits0: controls, preparation and complete native proof pass.
Its original source prefix is `/run/a-dts-b684db424dcb`, private tool prefix
`/home/aetheri/deps/desktop-toolchain-b684db424dcb`, and first proof directory
`/home/aetheri/deps/desktop-tool-proof-b684db424dcb`. The profile contains2,607
original regular files/93,659,654 bytes plus six notices; total regular prefix
files2,618, plus six aliases. Root's receipt and all signatures are retained.

The repository's explicit native proof strengthens the test-inventory and
installed-output checks. Its first independent run fails because a broad fixture
placeholder replacement also changes the success marker; all three compiled tests
had passed. That failed harness is retained at `/run/a-dtn-f2a0bc20612a` and
`/home/aetheri/deps/desktop-tool-public-f2a0bc20612a`. A unique path-only placeholder
fixes the harness. Fresh publicv2 exits0 at `/run/a-dtn-1b12b92ac766`, with work
`/home/aetheri/deps/desktop-tool-public-1b12b92ac766`. The known successful tool
prefix is reused read-only; neither failed harness nor failed compiler attempt is
restarted or relabelled as passing.

## Verified boundary

All ten archive/profile controls pass on the host and natively. A private source
counter removing only the hardlink-header comparison fails exactly with the
expected missing ValueError. Both native proof runs build and execute the C17,
C++23 and C/C++ shared-adapter cases. The final public test inventory names all
three tests explicitly;16 build/test/install/loader rows exit0. Vectors, exceptions,
jthread/atomics, filesystem path values and the actual private runtime mapping
are checked. Installed C/C++ probes also run. The final result retains51 distinct
loader-selected ELF hashes and seven native helper/object hashes, rechecked by
the independent exporter. No application/runtime-performance claim follows.

The complete package inventory is byte-identical before/after. SELinux stays
enforcing; udev remains PID693/invocation`1f3734ab799e47ab91ea4d27abbac917`.
Final independent process-map inspection finds no selected tool or proof code
still mapped. No package transaction, network-policy change or new service occurs.
All synchronous original controllers have returned terminal exitcodes.

Full export:2,837 files,35,556,902 bytes, SHA256
`aae8aa9bcb59330c56065d41d3d4b4ae368ae319197325ab4a678c790c5ed082`.
Protected copy:
`/root/aetheri-desktop-tool-evidence-91290033c6804b57bad040a2299bb651/export.tar.gz`.
Host copy: `local/full-desktop-build-2026-11-09/tool-evidence-v1.tar.gz`;
receipt `tool-evidence-verified-v1.json`. Every exported file, all three original
submitted source inventories, final public test bytes,2,618 prefix regular files,
build/test outputs, aliases and runtime/helper hashes are checked. No base image,
USB image, physical medium or quarantined P0 runtime is changed.

Audio lab remains the same live UUID`033ac49e-68bb-47a3-9c9c-26a812dc8300`,
QEMU5548/original controller15559, boot`8b915c0c-ab36-4cce-a8a3-e84e1c12627c`,
restricted networking. Reobserve actual VM state before dispatching more work.
The four unrelated UI-memory file hashes remain protected; do not stage them.

Next assemble the exact graphics/UI/system SDK inputs and rebuild current app
sources. No complete current desktop/SDK or new bootable image is claimed. The
full first-version scope, regular app/device/service ownership, hardware and
release acceptance remain outstanding. NexaUI handoff stays closed.
