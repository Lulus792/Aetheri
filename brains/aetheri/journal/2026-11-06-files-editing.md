# Editing Files names and filters

6 November 2026. The own NexaUI Files view now edits at a visible cursor and
replaces selected text. It retains the existing controller, filesystem operations,
SDK 0.1.18 and SDL defaults.

## Behavior and ownership

Left/Right, Home/End, Shift selection, Ctrl+A, Backspace/Delete and mouse click,
Shift-click or drag operate at app-owned Unicode grapheme boundaries. Typing
replaces the selection. The complete candidate must fit the 255-byte limit and
the original name/filter validation before the model changes. A rejected edit
retains the old bytes and blocks subsequent text/deletion until Ctrl+U clears
the entire field. An insertion that merges adjacent clusters re-snaps the caret
to a resulting whole-cluster boundary.

NexaUI supplies the text layout, pointer hit test, selection rectangles and
glyph painting. The view holds only bounded selections and the last observed
model bytes; it creates no competing editor/undo history. Horizontal viewing
keeps the caret visible. Scale/layout changes retain the selection; native focus
loss cancels drag, hides the caret and refuses late untagged text. There is no
continuous idle repaint or extra worker.

Filter contents now retire visible-action scopes even when the selected ordinal
remains zero. Naming-dialog lifetimes reset their own selection. These safeguards
retain original-action binding; text selection is no filesystem permission.

## Verification

The new middle-editing case rejects the unchanged earlier view at the expected
`aXd` assertion. The final C tests check middle insertion, selection replacement,
Backspace/Delete, combining and ZWJ clusters, mouse click/drag, full-length
replacement and retained rejection. Both raster-density variants retain their
existing layout, stale-action and modal tests.

Actual Wayland input corrects `wrok` to `work` before creating that exact folder,
replaces an 80-byte naming draft with `note.txt`, and replaces a mouse-selected
`seed` query before the original rename/move/document-save workflow. The native
AT-SPI client and held/lost-syscall tests retain their previous scope. Both real
development-owner variants still bind the current image and preserve original
document close/save obligations.

The first seven-group runs pass Release (32.47s) and ASan/UBSan (67.37s), without
skips. No unchanged shell/desktop/core suite is relabelled as newly executed.
The first implementation hit a duplicate semantic child ID; its failed XML is
retained and the field now uses the original single child-ID slot.

The first long-name image was captured while the app was still processing input:
the server socket reply acknowledges the compositor, not a drawn app state.
The test-only refinement waits for actual focus on Cancel and then a Shift-Tab
return before capturing the name selection/end/beginning. The original run and
images remain preserved; the final native runs are recorded separately.
Those two groups pass again in Release (21.96s) and ASan/UBSan (38.11s). The
snapshot comparison confirms that only the Python capture driver changed after
the seven-group runs; product and C test bytes are identical.

The subsequent visual review also found an old rejection notice surviving a
new valid draft. Successful recovery now clears only that exact English/German
notice, without discarding other status messages. The explicit-clear regression
checks this too. All seven groups pass again on these final bytes: Release
32.51s and ASan/UBSan 65.57s, without skips. Final execution root:
`/home/aetheri/build/files-edit-clean-aepw7erk`; earlier runs/images remain separate.

Local original evidence is under `local/files-edit-2026-11-06/`, with guest root
`/home/aetheri/build/files-edit-e58j8xx9`. Source, native image and protected-state
audit results remain with that execution record.
The final collector checks 366 host/snapshot/guest inputs, 183 exports including
114 native images, unchanged 28 frozen SQL/XML and nineteen protected hashes
without sidecars. All build/private-registry actors ended. The enforcing carrier
is idle on tty1 without an Aetheri service or seat0 session. Existing closed DRM
records and the foreign files are unchanged; no new DRM session was started.
The corrected selection, long-name end/beginning and mouse-query images were
inspected; the final naming image also confirms the obsolete notice is gone.

## Remaining work

Native accessible Text/EditableText/caret/selection transport, external IMEs,
field undo/redo and clipboard/word shortcuts remain open. The view is not a
finished file manager, production document broker or recovery/removal system.
Final material/human/physical acceptance, normal session/admission and the full
OS/apps/image/hardware/experiment-desktop objective remain required.
