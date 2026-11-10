# Current wlroots SDK and builds — 10 November 2026

Continue after the real Mesa shader proof. The preceding goal turn was progress:
commit be59d1d completed the private renderer test. This turn supplies the current
wlroots/input SDK and actual Release/instrumented builds, without USB activity,
normal image promotion, P0 restart or NexaUI handoff.

## Genuine source and package inputs

Twelve signed package queries, headers/requirements/scripts and original PC text
are retained at `/root/aetheri-wlr-sdk-review-e148b60b53094d188ede70c86466b6c8`.
All signatures/metadata and empty scripts pass. Exact systemd-devel bytes match
the existing source-build pin; its matching libs are separately hash/signature
checked. The package half has 94 original files/4,291,579 bytes and 19 aliases.
The input half has seven files and two explicit development/SONAME aliases.

The first attempted source was `/run/a-li-fa96b3460844/candidate`, a distinct
debugoptimized test runner. Header and PC versions alone cannot establish the
selected Release artifact. The guard rejects it before SDK directory creation.
Retain source `/run/a-wlr-2d6e1a1fc534` and its failed logs; no output is adopted.

The original Release-v5 2,465-file export has SHA256
`606f70ee8b37909f5ce231d7e70b2dc1dc211782591d88ffbb53f3f075c6a70f`.
Every entry is independently reverified before recovering six original source/
header/metadata/receipt inputs. It provides the 637,664-byte selected library
`ca4472b257663239f9356e5ec07fcd1983193311cf755d325c7006c94cc69b9b`.
The old record has no build_type/sanitizers keys: its primary Meson command is
explicitly Release. Its old profile hash 8bc6636a… stays historical, distinct from
the current instrumentation-extended profile; no receipt is rewritten.

The original installed/source header hash is
`cf36c7eb7dcd784318e0519624d5342c0c828ef32b9478f4f06990822cdbfcd4`.
The original generated 244-byte PC hash is
`d4aab2d8995018a48eeb3e8ee7b153e5098b33a07e32869547c24d5fb72591a2`.
The actual Release receipt is 8,905 bytes,
`5d2b8b85678457175addb48c4f2c551515feaa0f67070edba95afa2bbd555193`.
The selected original header also matches the independently pinned upstream
archive. Record/source recovery supplies build data, not runtime/volume identity.

## Actual builds and consumer attempts

Original host controller 53655 exits one only after SDK preparation and both
complete builds have passed; its first consumer preprocessor assertion is wrong.
Protected source `/run/a-wlr-8d232f0bfa37`;
SDK `/home/aetheri/deps/wlroots-session-sdk-8d232f0bfa37`;
Release `/home/aetheri/deps/wlroots-desktop-release-8d232f0bfa37`;
ASan/UBSan `/home/aetheri/deps/wlroots-desktop-memory-8d232f0bfa37`.
The original build profile/options and both exact integration patches remain.
No build is repeated to relabel a failed consumer or verifier.

Retain the separate consumer failures:

- Initial proof `/home/aetheri/deps/wlroots-release-proof-8d232f0bfa37` treats
  disabled feature macros as absent; actual generated headers define them as 0.
- `/run/a-wlr-proof-ed55d7239f4a` fixes that check, then its link misses a direct
  Wayland dependency. The consumer now links the original Wayland-server PC
  explicitly; the WLR library and its private dependencies are unchanged.
- `/run/a-wlr-proof-a30a732d7ac4` runs the real native program successfully but
  its verifier attempts to hash an already-unlinked shared pixel buffer as a
  library file. Preserve its original failure and output. The correction admits
  only the known wlroots `rw-s` data-buffer name/class; unknown deleted code fails.
- `/run/a-wlr-proof-88c239187ff3` completes the final Release proof, then
  overwrites its own script-path variable with CompletedProcess. The second
  Python invocation exits 2 before executing the memory consumer. The verified
  Release is retained; only memory is dispatched by a new single-command host.

Final Release work `/home/aetheri/deps/wlr-release-proof-88c239187ff3`;
original input `local/full-desktop-build-2026-11-09/wlr-proofs-intent-v3.json`.
Final memory source `/run/a-wlr-memory-proof-8e8aeabf5a4b`;
work `/home/aetheri/deps/wlr-memory-proof-8e8aeabf5a4b`; its original controller
actually exits 0. Both variants' configure/build/program commands exit 0. They
check the actual runtime 0.20.2, native feature flags, headless 64×64 output,
Pixman/allocator lifecycle and unassigned input context with zero device opens.
ASan/UBSan symbols/dependencies and consumer instrumentation are present, with
leak detection enabled and no suppression or policy change. External libraries
are not all instrumented. These are not DRM presentation or normal admission.

## Independent evidence and next work

Final host SDK/toolchain/graphics regressions: 45 pass; final native controls: 10.
The Root export initially places ordinary-user controls under inaccessible `/root`;
no control code executes there. Its separate corrected attempt uses a new `/run`
source, preserving `/root` protection. The successful export is unchanged after a
host verifier corrects its expectation about two deliberately excluded cached
inputs; those bytes are checked against their independent original sources.

All 787 exported files and 252 submitted inputs match. Both 472-file wlroots source
inventories exactly reconstruct the original archive plus the two accepted
patches. All 94 package files, seven input files, notices/aliases and actual
artifacts match. Independent map-origin review binds Release's 25 and memory's 28
filesystem mappings to independently selected SDK/original providers or the new build (30
unique files). No old libinput/Lua 5.4 body is selected.

Export `/root/aetheri-wlr-evidence-42b19eb0e12948e8b7850af1752b2ed6/export.tar.gz`;
host `local/full-desktop-build-2026-11-09/wlr-evidence-v2.tar.gz`;
15,024,604 bytes; SHA256
`2ab2368fec872b23269178fae3bf91581ddb9d35f2f4cbb456d7e9a5d66bef4e`.
Receipts `wlr-evidence-verified-v2.json` and `wlr-map-origin-audit-v1.json` in
that local directory. No source/build/test replay follows from export corrections.

Same-boot audit confirms unchanged packages, enforcing 1 and original udev 693 /
invocation 1f3734ab799e47ab91ea4d27abbac917. No selected SDK, library or final probe
remains mapped after original controllers end; no general lease/cgroup drain
is inferred. The original restricted audio lab remains running; revalidate it.

Next supply remaining genuine app SDK inputs and rebuild the current Aetheri
compositor, desktop and apps together, then normal GUI/native regressions with
the actual Mesa runtime. Do not promote these prefixes into normal installed
profiles merely because library builds pass. The agreed first-version/USB gate
and full OS scope remain open.
