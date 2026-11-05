# Native Files views and inline names

5 November 2026. This connects the selected Files presentation hierarchy to
actual data and input, using the approved opaque material fallback. Glass and
complete desktop/app/accessibility acceptance remain required.

## Result

The SDL view and hit testing share the original C17 layout: compact toolbar,
list/cards, real held locations, optional information and a single status bar.
Ctrl+1/2 and toolbar controls select the view; grid arrows follow columns and
reveal complete rows. F4 toggles the sidebar; F9 shows information as an embedded
pane or an owned read-only overlay when space is scarce. Close and native
fullscreen/restore controls issue real requests. Original line icons add no new
assets or runtime dependency. No fictional Recent/Trash/bookmark/content data
is introduced. Information reports snapshot metadata, not document previews.

New-file/folder and same-folder rename use an inline field and keyboard/pointer
Cancel/Confirm. A new placeholder is presentation only, never a model item or
authority. The original parent/source descriptors remain held while navigation
is blocked. Valid field blur inside Files submits once and consumes the original
pointer interaction; invalid/composing/rejected input cannot submit a prefix.
Window blur retains the draft. Interrupted composition rejects the name; this
is not complete native preedit/provider acceptance. Unchanged rename ends editing
without an operation. Unknown/stale inspection and moves retain their separate
review surface. Original workers and unknown results are never replayed or
settled by closing a field/window. See [Files](../../../spec/files.md).

Optional panes collapse by width and height. At 640×420 with 200-percent text,
one whole entry or naming field remains usable. Information still opens there.
Pointer targets come from the same geometry; diagnostics describe the rendered
frame before the matching view record. They are not grants or stable identities.

## Executed verification

Evidence: `local/files-view-2026-11-05`; final guest run
`/home/aetheri/build/files-view-check-ypp4cktq`.

```text
python tools/run_vm.py status
python tools/sync_guest.py
python local/files-view-2026-11-05/run.py --focus
python local/files-view-2026-11-05/run.py
python tools/development_session.py start --desktop --compose
python local/files-view-2026-11-05/exercise_drm.py <evidence-directory>
python local/files-view-2026-11-05/audit.py
python local/files-view-2026-11-05/collect.py
git diff --check
```

All **46/46 affected groups** pass Release (**130.12s**) and ASan+UBSan
(**186.07s**); changed implementation/test compile commands retain both
sanitizers. Each native Files profile has **61 workflow cases and 48 images**,
with zero ticks in the one-second idle sample. Native assertions cover current
control geometry, spatial grid selection, held sidebar navigation, optional
panes/overlay ownership, minimum-size naming, real blur confirmation, unchanged
rename/cancel, rejected prefixes, fullscreen/restore and all existing create/
rename/move/unknown/document/close paths. Layout properties separately check
**113,344** size/scale/pane/naming combinations. A shadowed local name failed the
first inline build and was corrected; the failed log remains, not a test pass.

Actual virtual DRM session `c624e6ab6c6443948e05679ce7078572` creates a folder
and document through the original desktop owner. Its actual Editor saves exactly
`Aetheri native files`. Grid/inline rename preserves directory inode/device/owner/
mode; list/details, German 200-percent reflow, information overlay and native
fullscreen/return have sixteen stable PNG captures. Files reopens the saved
workspace, then the original three direct roles end status0 and the controller
closes successfully. Three actual actor ELF hashes match tested Release binaries.

Audit binds **340** host/snapshot/guest inputs; **28** frozen SQL/XML and
**19** old-runtime hashes are unchanged, with no old SQLite sidecars. The carrier
is enforcing and idle on tty1 without known native actors, an Aetheri unit or
seat0 session. The old running/recover-handoff intent remains quarantined.
The bounded native export retains **102 files**, including **96 verified raw
image hashes**; it does not recursively duplicate the full test workspace.

## Visual findings and next work

Actual native images were compared with the selected compact hierarchy. The
opaque list/grid, inline fields, information and minimum-size naming are visible.
The DRM fullscreen image also proves a remaining compositor defect: the dock is
drawn above the fullscreen app. Native size/restoration tests pass but do not
establish the required hiding of desktop UI. Preserve
`local/files-view-2026-11-05/drm/native-fullscreen-sample-1.png` as the counterexample.
Correct fullscreen/dock/search ordering with native input/output evidence next;
do not quietly count this as complete visual acceptance.

Full glass, final controls/motion, complete IME and assistive workflows, richer
name/content information, copy/removal and normal grants/recovery remain work.
NexaUI can be assessed per view; its handoff remains delivered, with no new chat
text pending. Other apps, installable OS, hardware and the complete resettable
experiment desktop remain required. Keep the full project goal active.
