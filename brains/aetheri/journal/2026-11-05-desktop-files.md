# Files in the development desktop

5 November 2026. Files now has an independent dock/search launch slot beside
Settings and Terminal. This joins the [Files workflow](../../../spec/files.md)
to the [development desktop](../../../spec/desktop-ui.md); it is not normal app
admission, document-grant transport or a production session manager.

## Behavior and ownership

The C17 desktop model now holds three tagged app choices, independent pending/
process/error state and a correctly bounded result list. The SDL adapter supplies
four dock buttons and keyboard search at normal and narrow widths. A new private
version3 packet profile adds the fixed Files selector; old/malformed/FD-bearing
wire packets are refused. The root controller still reads historical format1
protocol1/protocol2 records without rewriting/rebinding them. New desktop records
bind seven fixed executable hashes plus the existing font/license inputs.

The ordinary-user supervisor selects Files' program, editor/Terminal inputs and
the independently prepared 0700 session `documents` workspace. It initially has
no sample/user files. Closing/reopening Files reuses it; session close retains it.
The test files below are explicit fixtures, not installed default content.

The direct document window now has the native title `Aetheri Editor` and app ID
`org.aetheri.Editor`. A normal Terminal keeps its own title/ID. The visible terminal
header remains its own presentation. Descriptive title/app-ID grouping does not
authorize a process, but distinct values avoid selecting the document when the
normal Terminal slot is requested.

Shutdown fences new launches, then waits for Files and its original document
owner before Terminal, Settings, desktop and compositor. Existing search and
window activation remain usable while closing, so the user can finish a dirty
document. Each original process and output pipe must end before the next role
retires. There is no forced production kill, inferred descendant drain or lease
settlement. The owner emits a single bounded STOPPING diagnostic for the applied
fence; the record is not a readiness/drain capability.

## Executed verification

Final Release and ASan+UBSan builds pass all **42 affected groups** (105.57s and
168.65s). They include the expanded actual owner tests with and without Compose,
plus editor/Files/Terminal/shell/Settings/compositor regressions. Debug command
inventory confirms instrumentation on the changed desktop/Terminal adapters and
the underlying file/PTY paths.

The native owner fixtures launch/reuse/close/reopen Files, preserve its workspace,
open a dirty document before the normal Terminal, prove distinct window selection,
and keep all later roles alive after the real shutdown fence. They activate the
existing editor through the closing search, save exact `hello alpha\n` bytes and
close it. The existing hangup-ignoring job and retained stdout writer then keep
the remaining retirement steps honest. Independent original pidfds and actual
process/pipe end are retained throughout the test.

A copied owner reverses Files/Terminal retirement. The unchanged integration test
rejects it at the premature Terminal-close check, exit1. The unchanged selected-
inode negative control also still rejects its omitted check. Copies, logs and
original failed-fixture evidence remain local. Test-only pinned force cleanup is
not adopted as session policy.

The actual DRM/logind session `0aa26febd7bc4d7b8be69f1213b0519c` uses protocol3
and all seven selected Release binaries. QMP/PS2 keys select Files from search,
reuse it, open/edit a document, run the normal shell alongside it and launch
Settings. Six independently observed app actors match the selected Release
hashes. After Ctrl+Q applies the original owner fence, the still-open search
activates the editor, saves exact bytes and closes it. All five direct owner
roles end status0 in order; the original unit is closed and the workspace file
remains. Rendered captures were inspected. This proves virtual guest behavior,
not physical GPU/monitor, Windows input forwarding or assistive acceptance.

Evidence: `local/desktop-files-2026-11-05`, including final native source/binary/
JUnit/byte/image inventories, wrong-order source/control, and `final-drm` actor,
input, file, frame and original-unit records. Earlier selector/search fixture
failures remain available: one assumed a result index, another queried the drawn
shell header rather than the native window title, and another toggled an already
open search. The final fixture uses the exact current window and actual fence;
it does not weaken deadlines or infer success from historical activation text.

Final read-only audit: 329 host/snapshot/guest source hashes agree; all 28 frozen
SQL/XML and nineteen protected old-runtime hashes are unchanged. No old SQLite
sidecars appeared. Native evidence retains 1,035 files, 282 raw-image hashes and
ten special-node records. The final DRM record has twelve stable captures and
six app-actor observations. Both owner fixtures in each profile retain the exact
saved document and selected binary/source hashes. The enforcing carrier is idle
on tty1, with no known native test/app, Aetheri service or seat0 session. The old
running/recover-handoff intent remains separate, unchanged and unresolved.

## Next work

Add useful file creation/management and document handling while preserving the
ownership, conflict and removal-preview requirements. Regular admitted sessions,
document grants/revocation, other app families, the system image/installer and
the full resettable experiment desktop remain part of the original OS goal.
The frozen management contracts, protected old runtime and public capabilities
are outside this change.
