# Verify native input before interpreting the search result

5 November 2026. Resolves the interpretation of the earlier ordinary-desktop
200-percent search capture and strengthens VM test input. This changes the host
development tool and tests; the editor, terminal and compositor binaries are
unchanged. Read the [search record](2026-11-05-editor-search.md) for the original
candidate, attempts and preserved evidence.

## Cause established by the original pixels

The previous review misread the final query as `zero`. Compare the actual four
query glyphs with the unhighlighted `zero cat` document cells in that **same**
original PNG, `local/editor-search-2026-11-05/final-drm/preview-200-contrast-sample-3.png`.
At its observed 19x39 cell grid (left 65, top 109), query row 12 cells 1–4 match
document row 1 glyphs **z, e, r, r**. The last query cell's complete thresholded
glyph mask equals `r` and differs from `o`. The recorded independent file proof
is still `zero cat\nmiddle cat\nlast cat`. Thus the visible query is **`zerr`**:
the literal engine's “No match” is consistent with the actual input.

The glyph masks/hashes and original PNG hash are retained in
`local/editor-input-2026-11-05/original-glyph-comparison.json`. Direct pixel counts
also show the second/third document rows unchanged; the prior claim of unexpected
document rendering was not established. Keep the old raw images and review as
history, with this correction rather than silently replacing their contents.

The old driver intended `zero` but admitted two stable images without checking
the actual query or match. That input/result admission gap is established. These
pixels do **not** identify the component that introduced the repeated `r`, nor
prove a general QEMU, kernel, SDL or compositor input defect. Do not turn the
timing hypothesis into a causal claim.

## Correction and discriminating checks

The host `tap_keys` helper admits one to four distinct qcodes and rechecks the
configured live carrier before a chord. One `input-send-event` request contains
all presses and reverse-order releases. It has no delayed-release timer, fallback
or automatic retry after an unknown acknowledgement. Controlled tests reject
invalid/unverified input before dispatch, preserve a balanced chord and prove
that a lost acknowledgement does not replay a press. The
[QEMU primary reference](https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html#command-input-send-event)
defines the explicit key events; its
[send-key reference](https://www.qemu.org/docs/master/interop/qemu-qmp-ref.html#command-send-key)
defines delayed key-up. Neither is an application text/readiness receipt.

A new actual PTY case types `zerr` into the original editor, requires no match,
clears to `zero`, requires a match, then checks exact saved bytes and shell return.
It passes Release and ASan+UBSan against the original candidate hashes. The source
now contains 24 PTY cases; this targeted run executes the new case only, not a
fresh full 24-case matrix. The earlier 36-group matrix remains attributed to its
original candidate/test scope.

The corrected ordinary-desktop driver additionally waits for each printable
native text commit in the original controller's boot/PID-selected journal. It
observes scaling stages separately and admits the final 200-percent pair only
when query glyphs equal the calibrated original `zero` document glyphs **and all
four document cells are highlighted**. This detects an unintended `zerr` even
if an image pair is stable. These gates supplement manual image inspection;
they do not replace general visual, physical or assistive acceptance.

## Current evidence

Host development contracts pass eight executed cases, with six Linux-only cases
skipped on Windows. All fourteen pass in the enforcing unprivileged Linux carrier
(1.313s). Targeted Release/ASan+UBSan query distinction runs pass in
`/home/aetheri/build/input-query-check-kc674wwo`.
Final host-tool failure checks pass 2/2 on Windows and 2/2 on Linux after separating
the overlength-chord case from the duplicate-key case with five distinct qcodes.

The ordinary desktop passes the exact query/highlight pixel gate and manual
200-percent inspection under the original unchanged Debug editor, SHA-256
`eeb31a91b3e2e1f86770ad4761173c4d1ee16a2f58a70493fc9342a88d945131`.
Original token `03a42b7533964a429617f82d354f7f1c`, editor PID/group 144726,
original shell 136149, controller 136010, invocation
`73506519b53f49228a948444703b099d`, stream `eb1ef0df2fcd4c3ca36a48439599de6b`,
scope `session-2235.scope`, carrier `0a174bc7-f17b-4799-9331-145ba3a5800c`,
boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. All original compositor/desktop/
terminal children exit 0; the original token closes. The exact document remains
`zero cat\nmiddle cat\nlast cat`, and the same shell writes `restored` afterward.

Under `local/editor-input-2026-11-05`, retain **26** stable console files from
26 probes, the calibrated query gate, original glyph comparison, full original
journal, start/stop/actor records and nine inventoried native evidence files.
Read-only final audit matches **320** host/snapshot/guest source files, **28**
frozen SQL/XML and **19** protected old-runtime hashes, without SQLite sidecars.
All 1,072 local Markdown targets resolve. The carrier is enforcing and idle on
tty1, with no known native test/apps, Aetheri service or seat0 session. The old
running/recover-handoff intent stays unchanged and unresolved; none of this
constitutes its retry, recovery or settlement.

## Boundary

This resolves the alleged `zero` no-match counterexample and prevents the test
from accepting intended input as observed input. It does not authenticate all
future injected input, prove a timer race, change production authority/admission,
settle the quarantined old runtime or finish normal session/accessibility/hardware
acceptance. NexaUI needs no new requirement or chat handoff for this tool change.
After this verification, continue document selection and the full planned apps/
OS/image/installer/resettable-desktop scope.
