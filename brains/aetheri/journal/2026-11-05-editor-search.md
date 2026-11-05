# Reversible literal document search

**Interpretation corrected later on 5 November:** the alleged 200-percent `zero`
counterexample below actually displays `zerr`; exact glyph comparison and a new
observed-input desktop run resolve it. See the
[native-input record](2026-11-05-native-key-input.md). Keep the raw original
attempt/review, but do not repeat its earlier visual interpretation as a current
search or renderer defect.

5 November 2026. Adds original C17 read-only search and actual terminal-editor
query/repeat controls. See the [contract](../../../spec/editor.md#literal-document-search)
and [independent reference test](../../../tests/test_search.c).

## Scope and ownership

Case-sensitive literal UTF-8 within one line, at most 256 query bytes. Whole
document-grapheme endpoints, overlapping matches, forward/backward lookup and
inclusive/exclusive origins. No regex, normalization, automatic wrap, cross-line
query, persistent index, allocation or new runtime dependency. Prefix/KMP byte
scanning and two monotonic boundary cursors segment each relevant line once.
The fixed line/query scratch and synchronous whole-query rescan remain bounded;
no speed superiority or universal maximum-file responsiveness is claimed.

Normal `/`/`?` saves the original cursor; valid query changes show an inverse
match and place keyboard focus on the query. Enter chooses a valid current match;
Escape restores the original position. Empty/missing/rejected queries cannot
accept a stale prefix or replace the previous accepted query. Ctrl+U clears and
Backspace deletes a whole query grapheme. Escape also cancels after incomplete
UTF-8. n/N recomputes from the actual current document after edits/undo without
wrapping. Save/close/suspension cancels preview before the ordinary action.

The model never changes text/cursor/revision/clean/history. The modal/app owner
performs explicit cursor preview/jumps and resets vertical goals; closing a typing
group does not change retained history entries. No search operation writes a file,
acknowledges UNKNOWN or executes a shell/editor command. Existing file fences and
retained uncertain export holders remain independent.

## Verification

The first focused run passed six of seven affected groups; editor.native's
help test still expected the now-obsolete “Search/panes”
line. Updated that exact expectation to “Selection/panes” and additionally require
the new search help to be reachable. The subsequent eight-group Release run
passes (16.59s), including all 22 PTY cases and actual native Compose input.
This intermediate run precedes the final inverse-highlight/short-label candidate.

The final compiled candidate passes **36/36** affected groups in Release
**84.41s** and Debug **ASan+UBSan 123.95s**, no skips/failures. Root:
`/home/aetheri/build/editor-search-check-qg2j7v9y`. There are **22** actual editor
PTY cases and **18** actual provider cases/**15** images per profile. Strengthened
view/native tests pass 2/2 in both (6.19s/6.69s), with unchanged editor hashes.
A subsequent exact repeat/clear/resize case checks complete document rows and
the `?zero` match in both real PTYs; each targeted run passes. The source now
registers that additional twenty-third PTY case; do not describe the earlier
full suite as having executed all 23. Positive copied-source search tests pass;
removing only whole-grapheme endpoint admission fails the unchanged test (exit 1).
The retained earlier branch/signal omission controls also still reject.

Editor SHA-256: Release
`8a8da1959f4af15fbab82429e27fb4f3dadde25137369c2c3c60d5d8e289154a`, Debug
`eeb31a91b3e2e1f86770ad4761173c4d1ee16a2f58a70493fc9342a88d945131`.

## Ordinary desktop evidence — historical finding, later corrected

The first original attempt `509f2d736d8a42bc9a4a89a04d305fb0` reached the saved
file proof, then its driver incorrectly used a relative `shell-proof` path in an
unwritable working directory. The actual shell visibly reported Permission denied.
The same original shell writes the expected proof using an absolute path; the
attempt shuts down and retires with its evidence preserved. This is a driver
path error, not evidence of an editor or shell permission defect.

The corrected full attempt `04ed872a4fff4b24b092300932f0649c` passes its original
file/process/capture assertions. Actual editor PID/group **135029**, original
shell **134868**, controller **134734**, invocation
`664e64c0de45420abba25f874aaa58ac`, stream `696fe3ca93ec4c7f9edfcfb937b97c31`,
scope `session-2195.scope`; same carrier `0a174bc7-f17b-4799-9331-145ba3a5800c`
and boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. The recorded actual image hash
matches the Debug candidate. All three desktop children exit 0 and the original
token closes. Both retired sessions retain exact `zero cat\nmiddle cat\nlast cat`
document bytes and a subsequent same-shell `restored` proof.

Manual 100-percent image inspection confirms the whole match, query focus and
accepted cursor. Cancel returns the exact original pixels. **The final
200-percent stable pair did not establish visual acceptance:** the original
review misread `final-drm/preview-200-contrast-sample-3.png` as no match for
`?zero` and suspected unexpected document rendering, whereas an earlier probe and first attempt
show a highlighted prefix/full match. File bytes remain correct. Preserve this
counterexample and `visual-review.json`; do not relabel a successful automated
capture/file test as a successful semantic image review. The exact PTY counterpart
passes in both profiles, but that does not explain or close the desktop finding.
Input/frame/geometry timing and graphics/integration were hypotheses, not proven
causes. The subsequent input record corrects this interpretation and supplies
fresh calibrated 200-percent evidence. No unavailable physical/assistive or
normal-session acceptance is implied.

Read-only final audit matches **320** host/snapshot/guest sources, **28** frozen
SQL/XML and **19** protected old-runtime hashes without SQLite sidecars. The old
controller's running/recover-handoff bookkeeping stays unchanged and unresolved.
The current inventory retains **1,090** native files, **204** raw images and six
special-node metadata records. The final desktop has twenty stable image files
out of 23 probes; this count explicitly does not close its manual visual finding.
All 1,068 local Markdown file targets resolve. The carrier is enforcing and idle
on tty1, without known native test/apps, Aetheri services or a seat0 session.

The new independent flat-byte reference covers 12,800 random query/origin/direction
cases over 800 documents, including overlapping/absent queries. Explicit cases
cover the gap, CRLF, combined characters/flags, invalid/limit admission, unchanged
history/revision/cursor and allocation-free actual 8MiB scans. View/PTY cases cover
reversible query focus at 30x8, rejection/clear, prior-query retention, edit-time
recomputation and unchanged save uncertainty. A copied-source omission control
must fail when partial document graphemes become admissible.

## Remaining scope

Rich selection/search, replacement, plugins/panes, durable sessions, accessible
document APIs/external IME, final UI/localisation, normal app/document admission,
remaining apps, original OS image/installer, hardware and the complete resettable
experiment desktop remain work. No old-runtime reconciliation or frozen SQL/XML,
installed policy/profile/capability change. The existing NexaUI case receives
only an incremental ownership update, with no new chat message, IDs or SDK change.
