# Held cross-folder moves

5 November 2026. This extends the ordinary-user Files development workflow, not
normal grants, durable recovery, a finished visual design or an installed OS image.

## Result

Window-local Cut/Paste retains one selected source parent/O_PATH object and its
literal/escaped names. Navigation and a cancelled preview cannot retarget it.
Ctrl+X/ Ctrl+V with list focus, F10, the … control and right-click menus expose
the same actions. A preview binds the independently held destination and shows
both folders. Ordinary Escape clears an unsubmitted selection; menu Escape closes
only the menu; preview Escape keeps the selection for another target. A source
replacement invalidates the preview and drops the selection. Menus retain pointer
ownership through dispatch/close and skip disabled actions during keyboard traversal.

`ae_files_move_prepare` reuses the no-replace operation with a separate destination.
Both parents must remain owned/private/writable. Directory moves inspect at most
128 destination ancestors at preparation and again before dispatch; an observed
self/descendant target or unresolved ancestry limit cannot dispatch. A moved
directory has its own independently opened readable metadata FD. After rename,
its metadata, destination parent and distinct source parent are flushed, then both
names are checked. Ordinary file contents are neither copied nor read. Original
source matching is not kernel compare-and-swap or hostile namespace exclusion.

Every attempt/result stays single-use. UNKNOWN retains the operation's independent
parents/object even after the UI selection is cleared; it blocks new mutations
and document starts. Close waits for the worker, never rolls back/deletes a candidate
or releases a managed lease. There is no restart journal or EXDEV copy/delete
fallback. Copy/content progress/cancellation and recoverable cross-filesystem move
need a separate protocol. See [contract](../../../spec/files.md#moving-between-folders)
and the reviewed Linux [rename](https://man7.org/linux/man-pages/man2/rename.2.html),
[fsync](https://man7.org/linux/man-pages/man2/fsync.2.html) and
[open](https://man7.org/linux/man-pages/man2/open.2.html) contracts.

## Executed verification

Evidence is retained under `local/files-move-2026-11-05`. The existing independently
identified Fedora carrier remained enforcing. Commands:

```text
python local/files-move-2026-11-05/run.py
python local/files-move-2026-11-05/finalize.py
python local/files-move-2026-11-05/collect.py
python local/files-move-2026-11-05/exercise_drm.py <final-drm-directory>
python local/files-move-2026-11-05/final_audit.py
```

All **45/45 affected groups** pass Release (**125.12s**) and ASan+UBSan (**221.55s**).
`files.move` covers unchanged-leaf file/directory moves, both held-parent name
replacements, source replacement, conflicts/private-mode drift, required flushes,
self/descendant/late-overlap and ancestry limits, and lost rename/flush replies.
A historical unknown attempt does not repeat even after its original object is
moved back. The EXDEV adapter is controlled no-fallback evidence, not physical
cross-filesystem acceptance.

The unchanged copied-source test passes the original and rejects five separate
omissions: cached-result guard, original-source check, source-parent flush,
moved-directory flush and pre-dispatch ancestry recheck. Original implementation
and copied test hashes match the source snapshot. Real syscalls run before the
lost-reply/flush wrappers report failure.

After visual review, one final source change suppresses the empty-entry label when
no row fits, fixing overlap with status at 640×420/200-percent text. Before/after
snapshots differ only in `src/files/main.c`. Files and both owner modes pass
targeted **3/3 groups per profile** (**50.89s Release, 59.60s Debug**); unchanged
cores are not rerun wholesale. Only Files and the separate fault binary change
hashes. Final native profiles have **52 cases and 48 images** each, with zero CPU
ticks in the bounded one-second idle sample. Menu, both-folder preview, cancel/
clear, file/directory round trips, self-target refusal and original-worker-owned
unknown/close paths are exercised through actual Wayland input. The expanded
driver initially exceeded the existing 16-capture limit; one new redundant image
was removed while keeping its behavior assertion. Production limits are unchanged.

The actual final-binary DRM session
`6a195c46aa3b40b59192196203e17c49` proves selection across navigation/target creation,
preview cancellation, menu submission, unchanged inode/device/owner/mode, moved
document editing/save, self-target refusal and moving its containing directory.
Exact saved bytes are `Aetheri moved note`. Files and the document reopen under the
new nested path, then the original three direct session roles end status0. Three
actual app actors, twelve stable PNG captures and the original journal are retained;
the original controller record closes successfully.

Final audit: **337** host/snapshot/guest sources match; **28** frozen SQL/XML and
**19** protected old-runtime hashes remain unchanged, with no old SQLite sidecars.
The retained native inventory has **1,171 files, 342 images and twelve special-node
records**. The carrier is idle on tty1 without known native actors, an Aetheri unit
or seat0 session. The old running/recover-handoff intent remains quarantined and
unresolved. No production admission/recovery/removal follows from this UI milestone.

## Next and coordination

The accepted design-studio baseline was recorded concurrently in the workspace's
`knowledge/ui-design.md` and was read when discovered. Future native presentation
must follow that selected reference while retaining these real operation boundaries.
This provisional view is not acceptance of the final Files appearance.

Copy/content publication, recoverable removal, normal document grants/mutation
fencing, durable restart recovery, the other apps, OS image/installer, hardware
acceptance and the complete resettable experiment desktop remain part of the
active objective. NexaUI's existing L08/L09 and Files contracts already cover the
menu/modal presentation needs; no new handoff message is needed.
