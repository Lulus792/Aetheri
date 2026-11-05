# Document undo/redo and queued signal exit

5 November 2026. Extends our original document/modal editor with bounded copied
byte deltas, original cursor/logical-state restoration and separate monotonic
revisions. See the [contract](../../../spec/editor.md#undo-redo-and-saved-content)
and [history/reference tests](../../../tests/test_history.c).

## Scope

Retain up to 1,024 whole undo/redo steps and 32MiB requested entry storage.
Same-line adjacent typing coalesces with geometric entry capacity; semantic
control/cursor/newline/save boundaries separate groups. No whole-file copy for
every keystroke, disk history, new service or runtime library. Destructive/large
replacements still copy their delta bytes; these are not constant-time operations.
Preflight growth precedes mutation/redo removal/eviction. Undo/redo reuses retained
capacity and advances revision while restoring the exact logical history state.
Returning to the acknowledged saved state becomes clean; old save revisions still
fail. Manually matching bytes on a different branch need not become clean.

The internal inverse path can cross a newly joined grapheme, bound to its original
history state. Ordinary replacement still requires public grapheme boundaries.
Combining, Prepend, flags and CRLF/joined lines restore original bytes. Normal
`u`, Ctrl+R and existing terminal Ctrl+Shift+Z/Y chords delegate to the same owner;
changed traversal returns to Normal. Save uncertainty/export holders remain
independent and cannot be acknowledged or rearmed by history.

## Original failures and corrections

The first held-process PTY test queued two termination signals while the original
editor was stopped, then resumed it. The old code made **two** emergency copies.
That case cannot prove late stop delivery: continuing can discard a pending stop.
A separate test-only first-fsync gate then holds an actual emergency copy, injects
SIGTSTP without another continue, and releases it. The original editor suspends
again instead of ending; its retained process pin is not readable as ended.

The signal reader now drains already queued managed signals without another
copy, cancellation, suspension or exit-result rewrite once exit is decided.
Both unchanged tests pass. The fsync gate is only in the explicitly separate
test producer; the ordinary editor has no such hook. This is not storage/crash
recovery or a proof over every later signal schedule.

The first ordinary desktop undo/redo run passed its file/pixel assertions, but
image review showed that Ctrl+R immediately after Escape had no visible message.
A new packed `ialpha<Escape>u` PTY case failed against the actual preceding binary:
the decoder swallowed the next key. The correction cancels/enters Normal, then
processes a non-CSI/SS3 next byte. The same case passes and also checks immediate
Escape→Ctrl+S saves exact bytes. Arrow sequences retain their separate decoder.
The earlier matrix/desktop evidence is retained separately from the final rerun.

## Final evidence

After the Escape correction, all **35/35** affected groups pass Release
**77.86s** and Debug **ASan+UBSan 107.45s**, with no skips/failures. Final root:
`/home/aetheri/build/editor-history-check-prqgqq52`; local logs, original full
CTest logs, JUnit and inventory are retained in `local/editor-history-2026-11-05`.
The editor PTY group runs eighteen cases and the actual Compose provider runs
seventeen cases/fifteen images in each profile. Copied-source omission controls
remove only redo-branch discard or the post-exit signal guard: the unchanged
positive tests pass and each broken producer fails with exit 1.

Final editor SHA-256: Release
`c0dbce51ab0c71c8f6bb3db963f7ce82559fdc2705d2944ad28aadfaced7dea8`, Debug
`d49829b8edbdbf3ca58d02dfd4230bb662661343b36b0e58f22df21c4597612f`.
The final ordinary QMP/PS2/bochs-drm desktop attempt
`efa80a318de143189e0fac877e559b50` runs that exact Debug editor as PID 124410
under the original shell PID 124193. Controller PID 123992, invocation
`413ce28a34f84d678d5ed044683ddd96`, stream `9e73f1dc2b6249a3a950817d71977af5`,
scope `session-2027.scope`; carrier `0a174bc7-f17b-4799-9331-145ba3a5800c`,
boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. Eighteen stable console files
show grouped typing removal, exact redo pixels, saved-state return, 200-percent
contrast and a new branch visibly reporting “No later edit”. The final file is
exactly `alpha gamma`; the same shell subsequently writes `restored`. The original
token retires with all three desktop children exiting 0. Earlier pre-correction
desktop and counterexample evidence remains separately attributed, not final proof.

Read-only preservation audit: **319** host/snapshot/guest source files match;
**28** frozen SQL/XML and **19** protected old-runtime hashes remain unchanged,
without SQLite sidecars. The old controller remains running/recover-handoff
bookkeeping and was neither retried nor settled. **888** native files/**204**
raw images and **six** special-node records are inventoried; special files were
never opened as regular inputs. There are **36** stable console files across
the historical and final attempts, **18** for the final candidate. Local links
resolve. The carrier stays enforcing, idle on tty1, with no live known native
test/app, Aetheri service or seat0 session. This is virtual development evidence,
not physical hardware, assistive technology or normal admitted-session acceptance.

The new tests include 5,000 independent snapshot-reference transitions with
substantial edit/undo/redo/cursor/save coverage; actual count/byte eviction,
allocation-free 8MiB traversal and saved-state/branch/OOM cases. Original document
text/index tests now cover all three required allocation failures. Native PTYs
exercise real save/close/uncertain-export, Unicode, stop/continue, signal and
fast-key workflows, not a substitute command producer.

## Remaining scope

Document search/selection, richer grouping/undo-tree/product choices, plugins/
panes, durable sessions, accessible text/external IME, final designs/localisation,
normal app/document admission, other apps, original image/installer, hardware
and the full resettable experiment desktop remain work. No frozen SQL/XML or
installed policy/profile/capability changes; no quarantined old-runtime retry.
The existing NexaUI integration case was incrementally extended to preserve the
app-owned history and saved-state/revision semantics, without another chat text,
new requirement IDs, SDK edits or consumer-pin updates.
