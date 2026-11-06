# Native Terminal tab selection and pending clipboard ownership

The full Aetheri OS/apps/image/installer/update/hardware/experiment-desktop goal
remains active and incomplete. This change advances the existing original-session
Terminal without changing the SDK pin, default SDL profile or installed authority.

## Change

The own view registers a required single-selection tab group with the pinned
NexaUI 0.1.18 public C API. Before projecting panes, the original host resolves
the requested widget ID against the current workspace and applies its existing
focus-transfer path. This retains paste/preedit cancellation, original session
models, process pins, one readiness worker and real PTY sizing. A modal/closing
boundary consumes but ignores a previously admitted request. Disabled tabs retain
their declared SELECT membership and matching focusable capability; disabled
regions and group state reject actions. No second window or daemon is introduced.

The independent libatspi client now checks standard Selection, exact tab roles,
names and direct child indices, one selected child, real first/third shell commands,
modal refusal, keyboard/model agreement and retired tab actions. The driver proves
original child exit before a slot receives a fresh process/cookie. Its client has
no NexaUI linkage. Complete human screenreader and multipane acceptance stay open.

The native multiplex test adds independent Wayland clipboard producers. A producer
holds the original transfer writer while a pane or tab changes inside the same
native window. The reader closes, the producer observes EPIPE and neither shell's
cursor/input changes. A later finite paste targets the selected original pane;
it creates no file until explicit Enter. The existing implementation already
supplied that cancellation; this change adds actual cross-pane evidence.

## Original failures and corrections

- `terminal-tabs-focused-8auyeodo`: all three new Release groups failed. Disabled
  tab actions were zero, removing the required group's selected member. Separately,
  the clipboard test counted the substring FOCUS in READ_FOCUS diagnostics.
  The real original pipe was already closed with window focus unchanged.
- `terminal-tabs-corrected-epmesd0n`: preserving actions exposed the public
  widget contract's matching FOCUS/focusable requirement. The disabled path still
  omitted the flag. It now retains both declared capabilities; region disabled
  state rejects actual interaction. The original failures remain preserved.
- `terminal-tabs-membership-ap2mwfd7`: all three new groups pass in Release
  (9.82 seconds) and ASan/UBSan (16.13 seconds), without skips. The view test also
  queues a live selection, disables the group in the next frame and verifies the
  host's rejection preserves the actual selected member.
- `terminal-tabs-before-negative-b3xtyhh2`: the retained byte-identical previous
  Release producer is rejected by the new independent client specifically because
  its tab list has no standard Selection interface. The candidate's native task
  passes; this is not merely a test of synthetic semantic rows.

- `terminal-tabs-regressions-4i3211_3`: the other 25 affected Terminal/desktop-owner
  groups pass Release (121.64 seconds) and ASan/UBSan (174.42 seconds), without
  skips, against the same unchanged application bytes. Together with the three
  focused groups this covers 28 distinct affected groups per profile, not the
  entire app/UI suite.
- A subsequent test-only extension retains the old tab until a fresh original
  process is actually running in its reused slot, then attempts the old action.
  It passes Release in `terminal-tabs-retained-pgrxugch` (1.83 seconds) and
  ASan/UBSan in `terminal-tabs-retained-san-ucyaemvw` (2.94 seconds). No application
  source or binary changed for that extension; only the runtime-compiled independent
  client and Python driver changed.
- That additional Release run also repeats the existing read-only native test.
  It fails at group acquisition in `bounded-state-stream`, with
  `Terminal AT-SPI 215: group`, under `terminal-atspi-oxk6md_z`. This test launches
  without the PTY helper and does not enter the new tab-group path. Earlier
  positives and the similar historical intermittent failures do not establish
  this occurrence's cause. Preserve it as an open native discovery/query finding;
  do not claim a completely green final native suite or an SDK/host fix.

The final read-only audit joins 387 host/snapshot/guest inputs, 355 retained exports,
156 native images, sixteen selected ELF hashes and C17 command records. Both
eight-shell idle samples have zero frames/CPU ticks over two seconds. Nineteen
protected hashes, 28 frozen SQL/XML inputs, SDK 0.1.18/e71ee30 and the exact
historical CLOSED DRM record are unchanged; no database sidecars exist. The
Enforcing guest is idle on tty1 without native/build/private-registry actors,
running Aetheri units or a seat0 session. Ordinary native tab and large-text/
high-contrast menu captures were inspected. Local evidence/continuation lives in
`local/terminal-native-tabs-2026-11-06`; source publication is recorded separately.

Earlier intermittent native Text/GetState/Close findings remain open. This change
does not diagnose them, implement persisted sessions, settle descendants/leases,
complete removal/recovery or establish normal-session/hardware acceptance.
