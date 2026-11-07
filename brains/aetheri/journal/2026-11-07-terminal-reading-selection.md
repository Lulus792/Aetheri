# Terminal reading selection and clipboard

7 November 2026. Original Terminal, existing NexaUI 0.1.18 and private SDL 04 in
the Fedora 44 Enforcing disposable carrier. No new dependency or installed
capability. The complete Aetheri objective remains active and incomplete.

## Behavior

Shift with reading Up/Down, page or visible-end keys selects whole original rows.
Further moves keep a valid matching endpoint's anchor; reversing contracts the
range and can cross the anchor. A different source starts a new range. Existing
Ctrl+Shift cell selection retains its own path. Reading navigation retains an
existing selection; Escape leaves reading, and ordinary selection Escape can
then clear the range.

Ctrl+Shift+C prefers explicit selection. Otherwise it can copy the checked
original current/pending reading row without changing selection, cursor or
scroll. Empty text leaves the old clipboard. Both exports share the existing
UTF-8, combining/wide-cell, trailing-space and soft-wrap rules. Copy publication
is not delivery proof; already transferred bytes are independent owned copies.
Later output does not revoke them. New requests recheck their original context.

The model validates a whole-row range before replacing selection atomically.
Its local selection-gesture serial survives clear, saturates without wrapping
and is projected with selection state. This detects a range created and lost
before the next frame as well as a vanished displayed range. Such a copy cannot
silently become copying a different reading row. A saturated serial disables
implicit row fallback. Source/target and model/pane/dimension/screen/viewport/
current-focus checks reuse the existing reading context. These are local UI
observations, never account/run/document/volume authority or durable identity.
The original clipboard owner still handles bounded publication and native IO;
no additional process, thread, document model or timer is introduced.

## Executed evidence

Model cases cover forward/reverse range creation, original-anchor contraction
and crossing, new-source reanchoring, atomic bad-token/short-buffer refusal,
unchanged selection during single-row export, history, eviction, soft wraps and
combining/wide Unicode. Legacy and named view cases verify original pending
targets and selection fences, including Select All and clear before presentation.

The native task runs the original shell and PTY in the original SDL window.
Independent libatspi clients verify exact read-only focus targets. A separate
libwayland receiver checks seven actual transfers: a focused row without
selection, reverse/extended/contracted ranges, a new anchor across the history
edge, and exact UTF-8 `copy-é中`. Each transfer uses actual focus and Wayland IO;
neither client links NexaUI/model code. The original shell subsequently writes
its existing proof and emits the Unicode row. Native Text editing/selection
writes remain refused; this does not claim a new native Text setter or complete
screenreader range integration.

The actual preceding published producer is retained at
`terminal-reading-selection-baseline-2hm5ol6w`, with its ELF hash checked against
the preceding reading audit. It reaches the independently verified reading row
but fails the five-second copy-publication requirement with no successful copy.
The initial baseline `2u46m3dt` instead hit the independent receiver's own idle
limit while the driver waited the same ten seconds. That original failure is
retained, not counted as the behavioral counterexample. The new publication
check is bounded to five seconds; the receiver and overall test limits are not
extended. The settled negative isolates the missing copy behavior.

The first positive campaign, `terminal-reading-selection-9wfwz9_s`, passed seven
of eight Release focused groups. The selection fixture had edited the cursor to
the first row before a bounded newline loop, so it had not actually evicted the
oldest row. The corrected fixture explicitly places the cursor at the original
bottom before that loop. The production selection algorithm was not changed for
this fixture correction. The native task was also extended with the history-edge
and Unicode transfers; its earlier results remain attributed to their own scope.

Final run: `terminal-reading-selection-final-nrzhx9il`.

| Profile | Focused groups | Other affected groups |
| --- | --- | --- |
| Release | 8/8, 21.85s | 24/24, 148.47s |
| ASan/UBSan | 8/8, 31.06s | 24/24, 213.55s |

Each run checks actual JUnit names/status and rejects skips. Leak detection,
halt-on-error and original regression assertions remain enabled. The 32 distinct
affected Terminal/desktop-owner/Files groups per profile are not a complete new
83-group app/UI-suite claim. Source/build logs, independent client output,
original failures, native images, runtime maps and exact C17/ELF hashes are kept
under `local/terminal-reading-selection-2026-11-07`. The final collector verifies
502 exported files and 216 raw native images by recorded hash. Inspected captures
show the original three-row selection and the larger German/high-contrast copy
guide. The selected Mono font displays the CJK character as a missing-glyph box;
the independently read text/clipboard still retains exact `中` bytes. Broader font
coverage remains a separate presentation task, not a Unicode-rendering acceptance
claim from these captures.

Audit: all 395 source inputs match the guest; nineteen protected hashes,
twenty-eight frozen SQL/XML contracts, SDK 0.1.18, private SDL 04 and the exact
historical CLOSED DRM record are unchanged. No SQLite sidecars. Both eight-shell
idle samples have zero frames and zero CPU ticks over two seconds. The Enforcing
carrier is on tty1 without own build/native/private-registry actors, Aetheri
services or seat0 session. The quarantined old runtime is unchanged.
`final-audit.json`/`final-exports.json` record this scope. Git history records
publication separately.

## Remaining work

Human/full screenreader use, physical input/displays, final accepted materials,
persistent sessions and normal production admission/lifecycle remain open.
The development carrier and successful child/test cleanup do not settle the
quarantined old runtime's operations or leases. The whole OS/apps/image/installer/
update/hardware/full experiment-desktop objective remains active. NexaUI handoff
is delivered; existing SDK contracts suffice here and no new chat message is due.
