# Current private desktop build — 10 November 2026

Continue the full Aetheri goal after the wlroots SDK proof. The preceding status
answer did not implement anything; this continuation takes the next concrete
action against retained native evidence. No USB-targeted build or flash, P0
restart, normal image promotion or NexaUI handoff occurs.

## Genuine app inputs and fresh builds

The [app SDK](../../../spec/app-sdk-inputs.json) supplies original files from
eight signed packages, including OpenSSL Crypto 3.5.9, SQLite 3.51.2 and
libselinux 3.11. It retains 162 regular inputs/10,391,401 bytes and five aliases.
Only the original libsepol/PCRE2 pkg-config files satisfy libselinux's private
metadata requirements; neither introduces another runtime or a static SDK.
Original headers, bodies and notices remain separate from path-only PC edits.
Signature/package reviews are retained under
`/root/aetheri-app-sdk-review-9b51452c9f8246a69f6007a97b09a4cb` and
`/root/aetheri-app-sdk-metadata-review-523acd2a50d5404ead74eec18f4e545f`.
No package or script is installed/executed.

SDK `/home/aetheri/deps/app-sdk-3566ad4252` is checked against its independent
profile, not trusted through its own receipt. Six exact component inventory pins
come from the completely verified NexaUI, SDL/text and wlroots exports. The
memory HarfBuzz body is 144,648,912 bytes; exact independently observed component
limits and streamed hashing replace an inappropriately small generic bound.

Final source `/var/tmp/a-da-60817f4c79/repo`;
Release `/home/aetheri/ad-60817f4c79-r`;
ASan/UBSan `/home/aetheri/ad-60817f4c79-m`.
Original host execution session 44539 finishes with exit 0. Both variants compile all
408 enabled target steps and pass 36 ordinary app/model tests. Nine main
executables have recorded body hashes. Root authority/catalogue acceptance is
not inferred from compilation; only separately executed cases count.

See the [build contract](../../../spec/current-desktop-build.md). CMake receives
exact include/library/package paths and typed `NexaUI_DIR:PATH`. Compose uses
the original privately extracted table/COPYING, not an installed X11 package.
The original ctest ELF retains the app loader environment; its tool-only wrapper
would intentionally replace it. Leak detection remains on without suppression.

## Preserved failures and resource boundary

- v1 prepares the SDK and passes controls, then stops because its source snapshot
  omitted protocol XML. Do not label this an app/compiler failure.
- v2 exhausts the carrier's 780-MiB `/run` quota while staging a new source. The
  incomplete snapshot remains. New snapshots use the disk under `/var/tmp`.
- The separate resource record
  `/var/tmp/aetheri-dev-resource-d2512b8e5e494e76afede3f4f2896f91` binds the
  original mount observations before changing only `/run`'s virtual cap to 1 GiB.
  Mount 49/parent 71/device 0:28 and rw,nosuid,nodev,seclabel,inode64,mode755,
  inode budget and shared propagation remain. This does not preallocate 1 GiB,
  increase VM RAM, restart the VM or alter permissions/policy.
- v3 configures, then Ninja rejects the missing global Compose path. Supplying
  the checked original private input fixes this without a global installation.
- v4 completes Release, then rejects the known large memory component during
  preflight. No memory build is created there. v5 completes the memory build.
  Preserve those successful builds with their original failed controller outcome.
- The first GUI controller passes 13 Release cases and stops before app creation:
  `NexaUI_DIR` is UNINITIALIZED instead of PATH. Fix the builder, not the cache
  validator. The developer validators also explicitly support the current SDK
  directory shape while retaining legacy history and profile/path rejection.
- The first expanded GUI run passes the shared desktop owner and 24 other cases,
  then fails the large-output clipboard prompt expectation. Retained native
  output ends at x=9,y=1. The compiled readline source deliberately inserts LF
  before erasing/drawing the next prompt, preserving the child's final output.
  Update the old y=0 expectation to y=1; keep the finite producer pause, final
  cursor-home marker, complete history and returned-prompt gate. No timer or
  production rendering change is made.

## Native integration

Each GUI variant has an explicit private source copy. Only the selected
`development_builds` field in the NexaUI pin and the separately hash-bound updated
clipboard test differ from the compiled-source snapshot. No upstream version,
artifact hash, normal source pin or installed default is relabelled. The real
developer supervisor retains source/cache/artifact and process ownership checks.
The test-only host selects its actual reviewed font and source instead of old
absolute carrier paths; altered font bytes and duplicate cache entries reject.

Final host execution session 44763 exits 0: all 29 selected cases pass in each
variant, 58 total. These include the real shared desktop owner, document editing,
source-drift refusal, independent terminal split/tab children, orderly close and
reopen, keyboard/Compose, native PTY/editor and clipboard workflows. The separately
executed SHA-256 vectors and SQLite cross-language migration checks also pass.
The script's input is `apps-gui-intent-v3.json` under the local evidence directory.
Native GUI records are at
`/var/tmp/aetheri-app-gui-5f86e272a00441e0b4e87390ca3d0c9b`.

Host execution session 29196 also exits 0 for six original graphical Editor
window/clipboard/controlled-save suites, three per variant. Each runs with a fresh
private mount namespace and bounded 1-GiB `/tmp`, retaining the shared carrier's
temporary files. This is the existing foreign-quota workaround, not an SDK
allocation-failure fix. Normal `aetheri-text-editor` contains no fsync pause hook;
the separately selected test target does. Records:
`/var/tmp/aetheri-app-editors-5b1eedf4330248eaa5e134c7653960ca`.

The current tests exercise private headless seats and software-rendered clients.
They do not establish DRM presentation, normal device/service/session admission,
hardware performance or first-version readiness.

## Independent evidence and review

All 4,211 exported regular files and 721 submitted inputs match their original
inventories. All 162 app SDK bodies/PC derivations and five aliases match the
independent profile. Both actual compilation databases have 247 commands, with
ASan/UBSan flags throughout the memory build and absent from Release. All nine
main ELF symbol tables retain the corresponding instrumentation distinction.
The current checkout matches 720 submitted inputs; its one later test-only
clipboard correction separately matches the exact final GUI input hash.

Loader inspection for all nine main executables and 17 actual process-map records
yield 80 unique filesystem code/dependency files. Every body matches independently
selected profiles/previous fully verified exports or the current source-bound
build. The compositor selects the intended libinput 1.32/Lua 5.5, without old
Lua 5.4. This does not inspect every anonymous/JIT mapping or grant runtime
authority. The origin audit rechecks all four earlier export archive hashes.

Eight selected, decompressed frame inputs are bound to their native raw-image
hashes. The plain graphical Editor, dock, Files and Terminal frames are actually
viewed. Release/memory Editor, output-0 dock and output-2 Files frames are pixel
identical; the Terminal greeting shows its different capture uptime. Files can
cover Settings on their shared assigned output; process coexistence is not a
claim that every window is visible in that final capture. No final design,
physical-density or accessibility acceptance is inferred.

Host controls: 96 cases, 88 pass and 8 platform skips. Native controls:
48 executed, no skips/failures. The four pre-existing foreign UI-memory files
remain byte-identical and are not staged. The exact carrier RPM inventory,
original udev PID 693/invocation `1f3734ab799e47ab91ea4d27abbac917` and enforcing 1
remain unchanged. No selected current app/SDK/wlroots/NexaUI code remains mapped
after original executions finish; this is not general lease/cgroup settlement.

Export:
`/root/aetheri-app-evidence-bdc8e7e47f9242b99c0d73042e8316bd/export.tar.gz`;
host `local/full-desktop-build-2026-11-09/apps-evidence-v1.tar.gz`;
163,571,221 bytes; SHA256
`55900ccb3c901bbfbf2b50083f389b3bce7f07bcf86adb681838897483f74268`.
Local receipts: `apps-evidence-verified-v1.json`,
`apps-map-origin-audit-v1.json`, `apps-visual-inputs-v1.json` and
`apps-host-controls-v1.json`. Export recovery never replays native tests.

The same restricted audio VM remains live: UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 5548,
boot `8b915c0c-ab36-4cce-a8a3-e84e1c12627c`. Revalidate it before dispatch.

## Next boundary

Keep all failed sources and original receipts. Continue from verified current
apps toward a protected current-runtime projection and actual DRM development
session, then normal service/device ownership. The old normal installed runtime
and its source/build defaults remain unchanged. Root administrative authority
suites and full image closure are not covered by these ordinary-app tests.
WLAN, Bluetooth, laptop audio/suspend/multi-monitor
acceptance, browser/project apps, general app lifecycle/removal, installer,
recoverable updates/backups and the resettable experiment desktop remain open.
The full goal stays active and the next USB build remains gated.
