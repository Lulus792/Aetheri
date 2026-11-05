# Files layout foundation and held-root navigation

5 November 2026. This is groundwork for the accepted native Files presentation,
plus one working keyboard improvement. The selected studio renderer is not yet
implemented. No normal app/document admission or lifecycle acceptance follows.

## Result

An SDL-independent C17 layout describes compact chrome, toolbar, sidebar,
list/card content, optional inspector and status. It computes complete cells and
grid-aligned reveal without changing model selection. Optional panes collapse
under space pressure. Geometry retains at least one complete cell at 640×420 with
200-percent text, including the planned naming budget. This does not establish
text fit, native focus, glass or accessibility: the current renderer still uses
the earlier layout and can show zero rows at that minimum size.

The directory model now exposes borrowed held-ancestor labels and complete
snapshot entries independently of filtering. Direct ancestor navigation scans
before releasing deeper descriptors. Failure retains the original stack/view;
success clears query/selection. The current depth can clear query/selection
without a scan. Renamed ancestors remain bound to their original descriptors;
the model never reconstructs or adopts their old pathname. These observations
are not current path authentication, writable grants or a sandbox.

Alt+Home is connected to this held-root operation in the native Files app, after
the existing dialog/menu input guards. Display escaping is available as a bounded
helper and now also escapes Unicode line/paragraph separators. Original raw
names remain separate from display labels and filesystem operations.

## Executed verification

Evidence: `local/files-layout-2026-11-05`, guest
`/home/aetheri/build/files-layout-check-y1wdqofj`. Commands:

```text
python tools/run_vm.py status
python tools/sync_guest.py
python local/files-layout-2026-11-05/run.py
python local/files-layout-2026-11-05/audit.py
git diff --check
```

All **46/46 affected groups** pass Release (**116.35s**) and ASan+UBSan
(**184.19s**). Compile commands confirm both sanitizers on the changed C
implementation and test units. Each profile checks **113,344** combinations of
size, scale, view, optional panes and naming budget. Independent properties check
region containment, nonoverlapping controls/cells, complete-cell capacity,
selection visibility/alignment and invalid-input rejection. These are geometry
tests, not rendered screenshots or physical monitor/DPI acceptance.

Model tests cover two-level navigation, failed allocation without stack/snapshot/
query replacement, ancestor rename plus replacement at the old name, actual held
inode continuity, filtered versus complete snapshots and descriptor balance.
Formatter tests cover invalid arguments, maximum escaped length, valid UTF-8,
invalid bytes, literal escape collisions and line/paragraph separators.

The actual Wayland input fixture now enters a directory, filters it and returns
through Alt+Home. Both native profiles pass **53 workflow cases**, retaining
**48 images** each and zero ticks in the one-second idle sample. Existing create,
rename, move, unknown-outcome and owned-document checks remain in the same run.
No new visual acceptance or actual DRM/hardware claim is made for this change.

Audit: **340** matching host/snapshot/guest source inputs; **28** frozen SQL/XML
and **19** protected old-runtime hashes unchanged, with no old SQLite sidecars.
The enforcing carrier is idle on tty1 without known native actors, an Aetheri
unit or seat0 session. The old running/recover-handoff intent remains quarantined.

## Next

Use this geometry for native drawing and hit testing together. Add the selected
list/grid toolbar, actual held locations and optional information inspector,
then inline naming while retaining each original operation/worker and unknown
result. Prove real text, pointer/keyboard/focus and opaque-fallback appearance at
the selected scales before claiming the studio presentation. Full glass, recent/
trash providers, document previews, copy/removal, normal grants/recovery, the
other apps, installable image and resettable experiment desktop remain work.
The full project goal stays active. NexaUI remains catalogue maintenance only.
