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
