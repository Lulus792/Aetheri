# Same-folder rename previews

5 November 2026. This extends the ordinary-user development Files app, not normal
document grants, app admission, durable recovery or managed removal.

## Result and boundary

F2 and the pointer Rename control open a preview bound to the independently
selected parent and O_PATH regular-file/directory witness. The original escaped
label and retained folder label stay visible above the new-name input. New names
reuse creation's whole-intent validation, grapheme Backspace and editable tail.
Cancel is effect-free. An unchanged name cannot submit. A Files-owned document
must end before rename admission.

The separate C17 `files/rename.h` operation owns descriptors and immutable names.
It rechecks the private parent and original source, refuses an observed existing
target, then attempts `renameat2(RENAME_NOREPLACE)` once. Successful parent flush
and both-name observations precede DONE. Source replacement invalidates the
preview instead of adopting the replacement. Destination races remain protected
by the kernel flag; every error after dispatch is UNKNOWN. No replace/copy/unlink
fallback, rollback, automatic replay or candidate cleanup exists.

Destination nonreplacement is atomic; source identity matching is not a kernel
compare-and-swap. This assumes ordinary cooperative local use, not hostile-writer
exclusion, volume authentication or a production mutation fence. Existing open
external writers are not stopped. New-name aliases do not rewrite content or
other hard links. UNKNOWN retains the original operation/names, blocks creation
and rename in this window and opens read-only inspection. Unknown rename also
blocks new document starts. Close waits for the original worker and only releases
memory/FDs. Reopening Files is not durable recovery. See
[contract](../../../spec/files.md#renaming-a-selected-entry) and the reviewed
[Linux rename documentation](https://man7.org/linux/man-pages/man2/rename.2.html).

## Executed evidence

Evidence is retained under `local/files-rename-2026-11-05`. The independently
selected Fedora carrier remains enforcing; sources use the existing snapshot copy.

```text
python local/files-rename-2026-11-05/run.py
python local/files-rename-2026-11-05/finalize.py
python local/files-rename-2026-11-05/collect.py
python local/files-rename-2026-11-05/exercise_drm.py <final-drm-directory>
python local/files-rename-2026-11-05/final_audit.py
```

The complete affected **44/44 groups** pass Release (**115.35s**) and ASan+UBSan
(**224.08s**). `files.rename` uses real syscalls with controlled destination races,
lost successful-rename/flush replies and allocation failure. Repeated operation
calls preserve results without another effect, including after old-name reuse.
Directory content, held-parent rename/replacement, private modes and raw old names
are covered. The selected-object adapter accepts directories without widening the
editor's regular-file-only adapter.

Unchanged copied-source tests pass the original and fail separately when either
the cached-result guard or original-source recheck is removed. The original
copied implementation and test hash match the source snapshot. No test is changed
to manufacture the negative outcome.

After visual review, the sole final source change adds the original folder label
to the preview. The before/after snapshots differ only in `src/files/main.c`.
The unchanged cores are not retested wholesale: Files and both desktop-owner modes
are rebuilt/rechecked, **3/3 groups per profile**, Release **48.23s** and Debug
**60.32s**. Only Files and its separate fault binary change hashes; other session
binaries remain identical. Both full-suite and final revalidation XML/logs/hashes
are retained. New source/test paths have ASan/UBSan compile-command evidence.

Each final native Files profile has **41 cases and 39 images**, with zero CPU ticks
in its bounded one-second idle observation. Actual input proves preview/cancel,
non-drag pointer Rename, German contrast/200-percent minimum-size use, source
replacement invalidation, destination conflicts, renamed-directory navigation,
own-document admission fencing and Editor save under the new name. Separate fault
instances hold a real rename, keep the UI responsive, lose the reply and prove
cross-operation blocking/read-only inspection and original-worker-owned close.
The ordinary executable has no fault hook. The first expanded native run exposed
a capture assumption: its sixth client was on output2, not output0. The driver
now checks the actual selected output; the successful unchanged feature assertions
and failed calibration artifacts are kept separately in the carrier.

The actual protocol3 virtual DRM session
`6fd8ca6fa74840ae8aca29fc7c2a8a4f` uses the final bound binaries and QMP/PS2 input.
It creates/renames a file, opens/edits/saves it, creates/renames/enters a directory,
creates a nested file, rejects an existing-directory destination and cancels an
unsubmitted name. Before/after metadata proves the rename retains inode/device/
owner/mode; saved file bytes are exactly `Aetheri renamed note`. Reopening Files
and retiring the session retain both entries. Three actual app actors, fourteen
stable PNG captures and the original journal are recorded. All direct roles end
status0 and the original session record closes successfully.

Final audit: **336** host/snapshot/guest sources match; **28** frozen SQL/XML and
**19** protected old-runtime hashes remain unchanged, with no old SQLite sidecars.
The retained native inventory has **1,123 files, 324 images and nine special-node
records**. The carrier is idle on tty1 without a known native actor, Aetheri unit
or seat0 session. The old running/recover-handoff intent remains quarantined and
unresolved; these tests do not settle its obligations.

## Next work

Copy/cross-folder move, removal previews and recoverable document management remain.
Normal writable grants/mutation fencing, durable restart recovery, full app removal,
additional apps, OS image/installer, hardware acceptance and the full resettable
experiment desktop remain part of the active objective.
