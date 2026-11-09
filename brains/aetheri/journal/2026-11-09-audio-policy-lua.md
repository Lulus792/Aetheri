# WirePlumber Lua module rebuild — 9 November 2026

## Result and boundary

The original WirePlumber 0.5.19 Lua scripting module now builds against the
reviewed patched Lua 5.5.1 glibc SDK. All eleven upstream wplua model cases pass
against the original RPM core. Actual dependency resolution selects the exact
old core/GLib/PipeWire prefixes and new Lua; no unresolved symbol remains. The
candidate module has its own SONAME, no RPATH/RUNPATH and is 253,072 bytes, SHA256
`fd62b2a0697479170fc05ff7b2381c0227640c5608c342b0be7e2eb90d906f67`.

This is a native build/model milestone. No normal runtime/image manifest,
installed service or USB is changed. Actual graph/PCM/GUI/policy retirement tests
and protected normal service/session/device admission remain work. The separate
PCRE2 provider release review remains open.

## Inputs and implementation

Read [the build contract](../../../spec/audio-policy-lua.md). The official source
tag is `4ab992fa2931ae2b17ef73508b1370030b7878c3`, with the previously checked
714,665-byte archive SHA256
`f9454eae3a5a6ca903c05ead2a7e47d7d3957fb0ffac3ebb11d92842603ae80f`.
The source contains 546 regular files. The three exact source aliases are
inventoried and omitted; no alias or upstream instruction file is followed.

The C recipe follows upstream's eleven-source/two-resource module boundary,
using the already verified SDK/control/policy snapshots and six independently
pinned files from the earlier glibc Lua export. Existing prefix validators run
before and after all child processes. The original source and MIT notice are
retained in the export. Compiler target is `x86_64-redhat-linux`, GCC 16.2.1;
this is not the earlier musl artifact. No new library family is introduced.

The full Meson route was inspected first. Five build RPMs were copied as inert
inputs into a new protected lab stage and checked through sealed memfds: exact
hash/size, full Fedora signer, payload digest, metadata, dependencies and scripts.
The offline DNF preview cannot resolve extra GLib, PCRE2 UTF16/32, packaging and
Vim-filesystem prerequisites. No install command followed. All five have empty
package-script listings. Instead, only the existing GLib development RPM's
resource compiler is extracted as data and checked by its own hash. Meson,
Ninja, PCRE2-devel and libffi-devel are not installed or runtime additions.

The direct recipe avoids rebuilding/replacing the already tested core merely to
compile its Lua module. It uses upstream XML/resource registration and original
source, with explicit GNU C17/hardening flags. This choice is scoped to this
private candidate, not a new general packaging framework.

## Actual execution and retained failures

The separate enforcing audio lab is the same registered UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 43164/controller 10491,
boot `32baadd1-a444-4aa8-8c32-56eb3ce22c21`. Normal guest networking remains
restricted. The original P0 carrier stays stopped; none of its old handles or
fixtures are adopted. A fresh private mount namespace/tmpfs drops to UID/GID1000,
empty supplementary groups and no-new-privileges for compiler/tests. The parent
waits for the original build process before exporting; namespace exit retires
the temporary mounts. This is build scratch, not durable publication/recovery.

Local evidence is `local/audio-policy-lua55-2026-11-09`:

- v1 fails Windows process creation before any SSH dispatch because an embedded
  full manifest exceeds the command-line bound. v2 sends the manifest inside the
  hash-bound archive instead; no old intent is retried.
- v2 fails the original control validator because an exported GLib alias was not
  restored. v3 restores only the exact independently selected profile aliases.
- v3 rejects the three source links. Inspection establishes their exact names
  and targets; the source reader omits precisely those, retaining the type fence.
- v4 completes compilation and all eleven upstream tests. Its 1,477-file export
  SHA256 is `39db768745ea468656052ae48a19ee1d7baebb25241cf95d13987b026b8915da`.
- v5 also adds explicit SONAME, complete TAP-count and selected loader-path checks,
  and post-build input rechecks. Host controller 94767 exits0. All 1,477 exported
  files have independently checked lengths/hashes; submitted public source and
  profile match the originals. Export SHA256 is
  `3e90049ac7a0f29c9de25e6793c9540b6bafe7ec503034544ac76e14eaf9fd64`.

v2/v3 exports and their exact failures are independently checked and retained.
Each attempt has new UUID, root and output. No failed directory is adopted,
cleaned or rerun. Protected v5 evidence lives at
`/root/aetheri-wplua-build-715a73dbf50746d6907bb9dc7766d262`.
Upstream's sandbox-negative subcases intentionally log rejected Lua operations;
all eleven TAP cases pass. This is WirePlumber's Lua-level sandbox test, not an
Aetheri VM/app isolation or security acceptance claim.

Five host archive controls also pass: reviewed archive, changed byte, traversal,
unreviewed link and changed reviewed-link target. No compiler is needed for them.

## Next

Prepare a fresh candidate policy projection with explicit rebuilt provenance,
new Lua/notice and unchanged original RPM files. Keep the old normal validator
and defaults strict. Repeat real graph/default-sink/gain/mute/PCM, connected GUI,
enforcing policy, source loss and exact original-resource retirement before
selecting any new normal runtime or image inputs. The audio lab remains running
under the original controller for that next step; no build worker remains live.
Foreign UI/planning files are neither edited nor staged. NexaUI handoff stays
closed; user documents on USB remain untouched. The full OS goal remains active.
