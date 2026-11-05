# Model-owned selection and private editor register

5 November 2026. Adds original C17 character/whole-line selection, copy/cut/change
and before/after paste to the existing editor. See the
[contract](../../../spec/editor.md#selection-and-private-register) and
[reference/failure tests](../../../tests/test_selection.c).

## Ownership and behavior

The modal owner retains the original anchor and a current grapheme endpoint.
Character ranges include both endpoint graphemes; whole-line ranges include
existing endings. Reverse ranges, empty lines, CRLF/mixed/missing final endings
remain document byte operations. One private copied register is bounded to
8MiB and wiped on replacement/destruction. It is neither the native terminal
clipboard nor a file/grant/save capability. Empty selection retains its old
register. Normal x/dd keep their prior delete behavior.

Yank changes no content/revision/clean/history. Cut/change/paste stage register/
assembly and document/index/history allocations before mutation. Failed admission
preserves the selected range, cursor, old register, bytes and redo. Each transfer
edit is one delta; later typing retains separate existing grouping. Linewise cut
of the final unterminated line also removes its preceding separator. Linewise
paste adds only required captured separators. Change leaves one empty selected
line, with its cursor included in the same admitted document edit/history entry.
Returned range metadata is borrowed UI data, not storage/permission proof.

The view paints selected glyphs and an ending/empty-line placeholder, names the
Visual/Lines mode and keeps its keyboard hints at 30x8. Placeholder bytes never
enter the document. Escape leaves selection at its current endpoint; search
first leaves that mode. Unknown file outcomes and retained export holders remain
independent through selection, transfer and undo. Block/mouse selection, named/
platform registers and accessible document semantics remain separate work.

## Initial finding and checks

The initial selection test failed when linewise change left its Insert cursor
after the retained ending, on the next line. The correction adds a bounded
line-start result-cursor variant to the existing document edit preflight. It
requires an original line start even for an empty edit and records that cursor
in the same undo delta; no after-the-fact history rewrite. The revised six-group
Release model/view run passes (3.25s). Targeted view/native/provider checks also
pass (14.79s), with actual visual commands through the original SDL/Wayland/PTY
path. Earlier failing and intermediate logs remain in the carrier.

The new independent flat-string reference exercises 1,000 forward/reverse
inclusive selections, copied paste or cut and saved-state undo. Explicit cases
cover combined characters/flags, mixed/CRLF/EOF, blank lines, original redo
branch/old-register preservation on copy/history failure, scratch/text/index
growth failures and real 8MiB register/cut/paste/limit/undo. The copied-source
cursor-placement omission must fail the unchanged selection test.

## Final evidence

All **37/37** affected groups pass Release **90.34s** and Debug **ASan+UBSan
132.12s**, without skips/failures. Root:
`/home/aetheri/build/editor-selection-check-sdj7ba2t`. Each profile executes
**27** actual editor PTY cases and **19** provider cases/**18** raw images.
The paired copied-source test passes, then fails (exit 1) after only replacing
the line-start cursor admission call with ordinary replacement. Its selection
test is unchanged. A subsequent explicit Visual Delete/register-paste case passes
in the final selection target in both profiles (3.68s/16.96s), with unchanged
editor hashes; this is a targeted follow-up, not a repeated full matrix.
Current compile commands independently retain ASan+UBSan on document/modal/main
and selection-test objects.

Editor SHA-256: Release
`2ff841b5708f974e96bd27545b20be268f5719f9c94dd1eddb5bed13f71189d0`, Debug
`d4d2dfe0abdf433d367262d06817c2b4ebdf3ea4ef8a2abd86c230a10d3838be`.
The ordinary native QMP/PS2/bochs-drm attempt
`f7a97e4b55154cfeb654dd9cd7d74e6f` runs that exact Debug image as PID/group
169221 under original shell 160351. Controller 160207, invocation
`eaf9098464b4489da572618e20ed96d0`, stream `15db9d3d0b564c1b89688f457e727464`,
scope `session-2865.scope`, carrier `0a174bc7-f17b-4799-9331-145ba3a5800c`,
boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`.

Actual character selection/copy/paste, separate undo/redo/saved state and whole-line
cut/register-paste/change pass. Manual 100/200-percent image inspection confirms
the selected cells, mode and keyboard hints. The 200-percent gate requires all
eight `replaced` cells plus the ending placeholder highlighted; cancel returns
the exact original pixels. Earlier character cancel plus Home returns its exact
baseline too. The final file is exactly `alpha beta\nreplaced\nlast`; the same
shell subsequently writes `restored`. The original token closes and all three
desktop children exit 0. Physical/assistive/normal-session acceptance is not implied.

Evidence lives in `local/editor-selection-2026-11-05`: **904** native files,
**210** raw images, six special-node metadata records and **34** stable console
files from 34 probes. Read-only preservation audit matches **321** host/snapshot/
guest sources, **28** frozen SQL/XML and **19** protected old-runtime hashes,
without SQLite sidecars. All **1,084** local Markdown file targets resolve. The
carrier is enforcing, idle on tty1, without known native test/apps, Aetheri
services or a seat0 session. The quarantined running/recover-handoff intent
remains unchanged and unresolved.

## Remaining scope

Richer selection/search, grouping, clipboard and plugins/panes/session behavior,
normal app/document/session admission, accessible text/external IME and final UI
remain. Other apps, the original OS image/installer, updates, hardware and the
complete resettable experiment desktop remain part of the goal. No frozen SQL/
XML, installed policy/profile/capability or quarantined-old-runtime action.
NexaUI receives only an incremental app-ownership case update, without another
chat message, requirement-ID reset, SDK implementation or consumer-pin change.
