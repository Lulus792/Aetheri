# libinput virtual-device tests — 9 November 2026

## Test boundary

Follow the [build/test contract](../../../spec/libinput-build.md). The private
1.32.0/Lua 5.5 candidate now has an explicit root-only upstream-device test in
the separate enforcing audio lab. No normal runtime/image provider changes.
The original P0 carrier remains stopped; USB user documents and foreign UI/
planning files remain untouched. NexaUI handoff stays closed.

The fresh ordinary-user Release build v5 repeats the generic, utility/version
and matching-bytecode positive/counter tests. Its complete 2,465-file export is
checked, SHA256 `606f70ee8b37909f5ce231d7e70b2dc1dc211782591d88ffbb53f3f075c6a70f`.
Its staged library is 637,664 bytes, SHA256
`ca4472b257663239f9356e5ec07fcd1983193311cf755d325c7006c94cc69b9b`.
All 309 exported functions and 25 version tags match the retained 1.31.3 ELF.
This is the exact Release artifact used below, rather than the preceding v4
artifact's different hash. Symbols alone remain insufficient ABI/input evidence.

Unlike the earlier namespace-only build, this test preparation leaves a fresh
global `/run` build tree visible to udev. After the ordinary builder ends, all
entries become root-owned and non-writable. The root test checks the complete
independently selected inventory before executing any helper. No RPM installation,
new package, network opening or broad SELinux permission is needed.

## Failed preparation and actual VM retirement

Two first attempts are retained without replay:

- Device v1 cannot deliver the original log FD to PID1. The actual enforcing AVC
  denies `init_t` write access to that fresh `admin_home_t` file; the private bus
  reports a disconnected transport. The recorded start stays pending. Independent
  inspection finds no current unit/cgroup, mapped test code, extra input devices
  or installed litest rules. Do not turn name absence into a successful test or
  a general lost-start recovery proof. Fix the fresh log's type to the standard
  `var_log_t`, keeping the policy enforced.
- The separate v2 service starts, then aborts before running cases. Upstream's
  Release `builddir_lookup()` returns false, so it assumes installed rules/quirks
  and fails opening `/usr/share/libinput`. Two temporary rules remain. The
  controller also encounters the already removed original cgroup's `ENODEV`;
  retain its original process/event handles and failed service. The corrected
  checker accepts that observation only with the original terminal process/unit
  and absence of its original cgroup path. No artifacts are automatically deleted.

Both complete failure snapshots and retained rule contents are exported and
checked: 17 evidence files, SHA256
`88b1604111445f8f1503895f406baea821b0eace580c9568a059f609fc7157d3`.
The original 80-file GUI input snapshot is separately checked before and after
a protected checkpoint copy (25,241,858 bytes). Preserve all failed inputs/output.

Then request ordinary lab poweroff. The original QEMU43164 exits0 through its
original controller10491; both original test SSH controllers end on that actual
VM exit. Their pending records remain historical, never rewritten as successful
or replayed. This establishes end of that disposable VM, not production recovery.

## Fresh build-directory runner, original Release library

The same registered lab UUID `033ac49e-68bb-47a3-9c9c-26a812dc8300` starts under
QEMU5548/controller15559, boot `8b915c0c-ab36-4cce-a8a3-e84e1c12627c`, enforcing
and still `restrict=on`. New udev manager PID693 has invocation
`1f3734ab799e47ab91ea4d27abbac917`. Initial rules and uinput module state are clean.

Fresh v6 ordinary-user preparation builds the upstream `debugoptimized` runner,
whose own build-directory profile supplies temporary test rules/quirks. Its
complete 2,468-file export is checked, SHA256
`e525ff074ad234d553d7cda506688d158d1b117ef1aa00c09ba9806e91a9b34c`.
The root-sealed tree is `/run/a-li-fa96b3460844`; protected build evidence is
`/root/aetheri-libinput-build-fa96b34608444e5c966554daf9f69d3e`.
Both generic/model and bytecode/counter gates also pass for that build variant.

A separately selected copy of the exact v5 staged Release ELF lives under its
`release` directory. The test's loader path selects it before the private SDK;
actual original process maps verify Release libinput and Lua 5.5. The runner and
udev helpers use the debugoptimized build profile. No normal binary or ABI alias
changes; this does not claim that debug helpers themselves are Release binaries.

## Executed suites

- **Lua v3:** all50 pass, zero NA/fail/skip. The suite exercises registration,
  restricted functions/GC, timers, virtual-device metadata/frame changes,
  debounce/feature controls and timeout unloading. Original controller18370 exits0.
  Protected result: `/root/aetheri-libinput-devices-7684532cedf648efac8403e5172fc174`.
- **Keyboard v4:** upstream exit0/statusPASS; 293 completed =262 pass +30 not
  applicable +1 skip, zero failures. The skipped `keyboard_keycode_obfuscation`
  case is explicitly compiled out when `EVENT_DEBUGGING` is disabled; do not
  enable diagnostic logging merely to erase the skip. Original host controller
  26594 exits1 because the first report aggregator demanded all cases pass.
  Preserve that verdict: native tests did not fail. The subsequent report model
  separates upstream success from complete coverage and checks count consistency.
  Protected result: `/root/aetheri-libinput-devices-ebd2654ea0784d4aa1e087462eec49d0`.
- **Pointer v5:** upstream exit0/statusPASS; 2,664 completed =2,158 pass +506 not
  applicable, zero failures/skips. Original host controller10288 exits0.
  Protected result: `/root/aetheri-libinput-devices-4096b2d2c1df4fd089ac991aa9a00cd3`.

The three suites therefore complete 3,007 cases: 2,470 pass, 536 not applicable,
one explicitly explained skip, zero failures. They do not provide complete
coverage of all virtual devices or all upstream test collections.

The complete 30-file export is independently checked: 693,464 bytes, SHA256
`250da5f5f15183e7f75beb8ab9e866f9071fa3ada4b25d2e976d5726c7da2af0`,
protected path `/root/aetheri-libinput-evidence-a43ed974a4d54848b6c7f5ce6ee739c9/export.tar.gz`.
Every exported input matches the submitted hash-bound bundle. Original controller
records and logs are retained, including v4's strict report verdict and auxiliary
bytecode caches from root-only inspection. The host independently recomputes all
summary counts, compares each complete baseline/after snapshot and checks the
actual selected Release/Lua map paths. No native test is rerun to repair reporting.

Each completed device fixture verifies original process end and held cgroup
removal, then retires its referenced service. It settles the same original udev
manager, compares input objects/nodes and all rules/quirk directories, checks no
helper/library mapping remains, unloads only its own unused module and removes
only its own original empty rules directory. Unknown effects preserve context.
This is administrative VM evidence, not normal device ownership or physical HP
mouse/touchpad/Bluetooth acceptance.

Independent same-boot end inspection confirms no libinput test unit, mapped
build/helper code, litest directory, rules directory or loaded uinput module.
The original udev manager PID/invocation remains unchanged and enforcing stays1.
QEMU5548/controller15559 remains live for the next fresh fixtures, with restricted
networking. The two failed first-boot fixtures remain historical after the exact
original VM exit; no failed test or transient rule is replayed/adopted.

Local evidence: `local/libinput-review-2026-11-09`. Five source-archive controls
and six report controls pass on the host. The report controls reject native
failure, absent/duplicate/incomplete/inconsistent summaries and empty runs while
preserving visible NA/skip counts. Remaining touchpad/input collections,
sanitizers and actual compositor/input integration remain gates before runtime
promotion. PCRE2, normal device/session ownership, physical hardware and the rest
of the OS/apps/installer/update/experiment goal remain open.

## Touchpad data dependency and corrected full suite

The next exact Release run, device v6, completes 1,372 cases but reports two
failures: `touchpad_left_handed_rotation` for Bamboo2FG and Intuos5. Both logs
first report an empty libwacom database. Original controller11641/native exit1
is retained; its original unit/cgroup/devices/rules/module retire normally.
The complete 10-file failure export is checked, SHA256
`e542ea2dd35b2b66d4e984c1a2b2749d494f091aed289c1dbc6529a8734ca1a6`.

This is a scoped test-runtime omission. The existing desktop image profile
already pins all 829 database/layout files. The 80-file Sound projection includes
the library but omits those data files. Use the already approved HPND
libwacom-data 2.19.0-1.fc44 RPM, SHA256
`5099f8e569baba02dc1343ab49316a3c9876b981e2b51a40636d2b988d946921`.
No package installation or library-source patch follows from the two failures.

The first data constructor rejects five zero-body newc SVG entries before
creating its selected data tree. Their actual bodies belong to other members
of the same sealed RPM-header hardlink classes. The new data helper resolves
only that exact class and independently checks every destination's existing
size/digest. It creates separate regular files; no filesystem links or scripts.
Keep the failed initial staging unchanged. Four pure controls also reject a
different class with equal bytes, changed bytes, links, missing entries and sizes.

The resulting root-owned non-writable tree `/run/a-wacom-ead046f4d61a` has all 829
files, 1,174,091 bytes. Its protected source is
`/root/aetheri-wacom-data-ead046f4d61a4fcbbab4858b4ed092b7`, independently
confirmed by the local intent. Host checks compare all returned pins and the original placement to
the preselected intent and existing image profile. Its complete 833-file
data/input export is checked, SHA256
`e76f9ba420c3a6ce2810a057d127efd115f5dc6cb12fcd7b00f00ec787143838`.

The [upstream constructor](https://raw.githubusercontent.com/linuxwacom/libwacom/libwacom-2.19.0/libwacom/libwacom-database.c)
accepts the fixture's selected `XDG_CONFIG_HOME/libwacom` database. Set that only
on the test service and refuse unselected system data fallbacks. The fixture
checks the separately selected data tree before and after the native run.

Focused device v7 passes 21/22, with one NA and no failure. Full device v8 then
passes 1,177/1,372, with 195 NA, zero skips/failures, against the unchanged Release
ELF. Original controllers91511/8018 exit0 and both fixtures fully retire. The
complete 19-file positive export is checked, SHA256
`0a3a1717e6956eeeb844fc0bfdbfd6b9fee79e8b06006dece10505aa89c5ef14`.
The four full functional groups now total 3,647 pass, 731 NA, one explained skip,
zero failures in their accepted configuration. Do not double-count focused runs
or erase the original missing-data result.

## Dual-library memory instrumentation

The separate sanitizer recipe selects the previously verified patched Lua5.5
artifact (6,082,112 bytes, SHA256
`51bcd64207349f63774de451d734be8cac0f22ed30f5902ac341f1fac3c5c52e`).
Its six SDK files are independently pinned. Existing compiler-tool runtimes
libasan/libubsan 16.2.1-2 and carrier libstdc++ 16.0.1-0.10 are package/version/hash
checked before and after use. They are test tooling, not new production files.

The larger fresh builds use disk-backed input directories beneath protected
build evidence, bound to new `/run/a-li-*` paths with nosuid/nodev. This avoids
the lab's 798 MiB `/run` limit while preserving exact compiled helper paths.
The input mounts remain deliberately retained; no process end is described as
mount retirement. Failed inputs are never rebuilt/adopted. All source/archive/
package checks precede ordinary UID1000 compilation.

- Build v7 stops under UBSan in upstream test registration: linker-section
  boundaries were declared as individual structs. Its complete 1,677-file export
  is checked, SHA256 `aaac6f9c1304745fb39b37c92fc278a449518adbe20deaf6480a23bdd757200b`.
- The pinned test-only declaration patch changes those two boundaries to arrays
  and preserves exact pre/postimages. It changes no library source and disables
  no sanitizer. Build v8 passes generic/utility/version and matching-bytecode
  checks, but its address counter reaches UBSan's object-size diagnostic before
  the expected ASan diagnostic. Keep that rejected report and complete checked
  1,814-file export, SHA256
  `b7dda4e4db50425d1c688c493cb9b6eb52af9808ef27f11b8c827e960856f754`.
- A volatile pointer load in the test-only address control preserves the fault
  while making its allocation size opaque to that earlier static check. Fresh
  build v9/controller96113 exits0. Both sanitizers remain enabled everywhere;
  ASan heap overflow and UBSan signed overflow now fail precisely as intended.

V9's 2,486-file export is fully checked, SHA256
`e628fb9e2b58575fbe437619ce5093d2e2fa6bf66f38a322bc73018fd5141849`.
Protected evidence is `/root/aetheri-libinput-build-5aaa7f22470a4745b2cb764555af1c99`;
ready input view `/run/a-li-5aaa7f22470a`. Instrumented library: 10,045,816 bytes,
SHA256 `f807ebd6ca205c8391b9030461e1420e7a2c205db7da7efb124584abd76f34a8`.
This is never the normal Release provider. Actual positive process maps bind
both instrumented libraries and the three selected tooling runtimes. Dynamic
symbols verify instrumentation in both C libraries. 44 deviceless/64 utility
cases, version, two clean context controls and bytecode admission/counter checks
pass; both memory-fault controls fail with their precise expected diagnostics.
Leak checking stays enabled without suppressions. No whole-stack leak-free or
memory-safety claim follows from this coverage.

Independent same-boot inspection finds no mapped code under the three memory
builds or functional runner, no test unit/device/rule/quirk/module remnants and
no system libwacom installation. Original udev PID693/invocation remains unchanged,
enforcing1. Input bind mounts and their failed/ready artifacts stay retained.
QEMU5548/controller15559 remains live. The instrumented virtual-device profile
now has the scoped evidence below. Next join actual compositor/input regressions,
then review an independently
attributed normal runtime. Other touchpad/gesture collections, physical devices,
PCRE2, normal ownership and the rest of Aetheri remain open.

## Instrumented virtual-device Lua run

Device v9 is distinct from memory build v9. It explicitly selects the completed,
sealed `/run/a-li-5aaa7f22470a` inventory and `candidate/build` library through
the new instrumented profile. Release selection remains unchanged. Counter-build
directories, unknown profiles and mismatched profile/directory pairs are refused.
Eight host result/profile controls pass. The retained sanitizer-tool packages and
exact canonical files are checked before dispatch and after retirement; actual
main-process maps bind both instrumented libraries and all three tooling runtimes.
Leak checking and halt-on-error remain enabled, without suppression or policy changes.

All50 Lua virtual-device cases pass: zero NA, skip or failure. Original controller
16083 exits0. Unit `aetheri-libinput-97b3afe926a34505b6a6669048831524.service`,
invocation `f38c4eb9a0564a29b7c3a2407359b31f`, reaches retired state with no
pending effect after original process/cgroup/unit and device/rule/module checks.
The complete10-file export is independently checked:236,788 bytes, SHA256
`a06ce121e25c14b61ad5906690110812e2ea91a33ad3500995fb525fa8d46dd3`, protected at
`/root/aetheri-libinput-evidence-838b6c0a30fb40f18409681c3aff5076/export.tar.gz`.
Inputs still match the original submitted bundle. No udev journal diagnostics
occur during the run.

Final independent same-boot inspection again finds no selected-library mapping,
litest process, test unit, private rule directory, uinput module or global Wacom
installation. Udev remains PID693 with its original invocation; SELinux stays
enforcing. The three input bind mounts intentionally remain. The QMP-verified
audio lab remains QEMU5548/controller15559 on its original boot. Four foreign UI
note hashes remain unchanged. Neither this Lua-only instrumented coverage nor
the earlier functional groups establish actual compositor/physical-input or
whole-stack memory acceptance. Normal runtime/image/USB inputs remain unchanged.
