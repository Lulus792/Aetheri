# Original-session Terminal tabs and split panes

6 November 2026. Final broad suite and retention audit pass.

## Implemented boundary

The own NexaUI presenter now hosts multiple actual original shells in one SDL
window. `workspace.c` owns only a bounded tab/split value tree; `session.c` and
the host retain stable per-session models, PTYs, pins and input. Eight sessions,
eight tabs and fifteen binary partition nodes are limits, not prestarted actors.
One existing worker polls duplicated original readiness handles, while the UI
drains at most 16 KiB across a rotating turn. No model or live session is copied.

New `spawn.c` prepares independently selected held sources and a bounded copied
environment before threads. Later `pidfd_spawn` calls use fixed fd actions and
the own unprivileged `pty_starter.c`, which acquires the original controlling
terminal and execs the shell in that same process. Original pins survive later
observation failures. The retained pidfd path can observe even a failed initial
PID-info query. Mutable-source metadata comparisons reject observed drift;
they do not seal bytes, authenticate a publisher or replace production admission.

Named pane projections give independently created models disjoint native IDs
even when their content tokens coincide. Edits, resize, hidden projections and
retired sessions cannot redirect old targets. Actual tab/pane focus cancels old
clipboard/preedit state and fences queued text. Per-session Close retains the
original child and stream until settlement, then permits a fresh slot identity.
This is not descendant/Root-lease drain or persistent/restorable sessions.

Development-owner v2 additively binds the starter and four new model sources;
v1 and historical closed receipts retain their prior interpretation. The
independent Files editor is deliberately not given the multiplexer helper.
No SDK version/dependency-family/default/policy or catalogue contract changes.

## Executed focused evidence

- `terminal-multiplex-integrated-l2787r04`: six focused groups pass each Release
  and ASan/UBSan before the later native/focus refinements.
- `terminal-multiplex-native-qcpxxyjo`: Release build and six unit groups pass;
  native independent shells, draft preservation, close/reuse, divider drag and
  actual `stty size` pass. Outer pointer resize fails; keep its original logs.
- `terminal-multiplex-resize-diagnostic-qnw83_yy` / native `2p8xodwq`: delayed
  original input with a Wayland trace reaches 480x320. The original injection ACK
  was not client/compositor resize admission. The driver now retains the actual
  button through the original resize request/configure acknowledgement and
  actual resized fallback. No timeout/layout requirement was relaxed.
- `terminal-multiplex-native-resize-sync-j8q1umn7`: enabling the wire trace in
  combined stdout/stderr interleaves a complete workspace diagnostic record.
  The parser rejects that damaged observation. Separate native-wire stderr from
  structured app stdout; do not infer that the displayed prompt was absent.
- `terminal-multiplex-native-resize-sync-2-f47uay2q`: seven focused groups each
  Release/ASan/UBSan pass. Six screenshot cases cover actual shells, small
  viewport, scaled localized menu and eight sessions. Both idle samples have
  zero new frames/CPU ticks over two seconds. An inspected menu screenshot
  reveals a clipped German shortcut; the menu is widened before final testing.
- `terminal-multiplex-owner-v2-cq7oixyf`: four selected owner groups each
  Release/ASan/UBSan pass, including actual owner-started splits/tabs, independent
  shell effects, retained original reuse, source-drift rejection, Files editor
  save/close and ordered retirement. No DRM controller record is changed.
- `terminal-multiplex-native-menu-hefqcdi1`: a new test incorrectly expects the
  unscaled 480x320 minimum at 32 points. The actual original policy requests
  960x640; the assertion is corrected to that existing policy, without changing
  production geometry. Earlier images/failure remain preserved.
- `terminal-multiplex-native-focus-dyh2ml2v`: seven focused groups each
  Release/ASan/UBSan pass with the wider menu and correctly scaled minimum,
  plus real pane-focus preedit cancellation and same-native-batch Ctrl+Tab/old
  commit rejection. Eight image cases per profile cover the expanded viewport
  matrix. Native test-only input does not establish installed IME acceptance.
- `terminal-multiplex-pre-focus-negative-qfyzf2xd`: retained old Release
  producer/shell/starter bytes fail the new pointer/preedit focus assertion.
  The first diagnostic invocation never maps its private compositor with the
  longer nested build path; the corrected separate log uses the ordinary short
  test build root. The real counterexample is the later original-producer failure,
  not that earlier setup error. All original negative inputs remain hash-equal.
- `terminal-multiplex-final-exlqkwv1`: first broad Release run ends 77/79 in
  339.36s; both clipboard presenters fail the same premature large-copy fixture
  predicate. It selects after history merely exceeds 100 rows, exporting about
  49 KiB while subsequent frames continue receiving the producer's output. The
  shared read turn remains bounded; no production budget is relaxed to make the
  fixture pass. A finite two-burst producer now emits a final cursor-home marker;
  the fixture waits for full history and the original shell's returned prompt
  at that final position before copying. The >64 KiB immutable-copy, eight
  blocked sends, deadlines, replacement and exact original lifetime checks stay.
- `terminal-clipboard-ready-negative-7hwqx7gi`: restoring only the old partial-
  history readiness predicate against the paced producer and unchanged native
  programs fails the original large immutable snapshot assertion. This is a
  fixture synchronization counterexample, not a production clipboard fix.
- `terminal-multiplex-clipboard-ready-e475tc5o`: both clipboard presenter groups
  pass Release (27.05s total) and ASan/UBSan with the final-marker synchronization.
  The earlier whole-source result remains 77/79; it is not retroactively green.
- `terminal-multiplex-final-ready-7wyhkhcl`: corrected Clipboard tests pass, but
  the broad Release run ends 78/79 in 338.56s and exposes a separate `terminal.view_native` fresh Close
  click timeout in `terminal-ui-jtnzdvhs`. The retained producer shows no closing
  frame, but its old diagnostics do not identify the pointer hover/press state.
  This is not established as a clipboard or SDK failure, nor is the previously
  observed AT-SPI query issue automatically its cause. Diagnostic chrome state
  is now extended with hovered/pressed IDs; the native task will require actual
  presented target hover and original press before release. This is stronger
  input evidence, not a claimed root-cause fix for the old timeout.
- `terminal-multiplex-close-observation-34_rr_z0`: the new multiplex task passes
  Release, but a strict older chrome-format assertion rejects the added fields
  before the fresh Close case. Retain that harness failure; preserve the existing
  CHROME line and publish the extra IDs in a separate CHROME_POINTER line. This
  compatibility correction changes no production click handling or timeout.
- `terminal-multiplex-close-observation-2-38w5qd2q`: both native task groups
  pass Release and ASan/UBSan. In the instrumented build the multiplex task
  takes 12.20s and the legacy native task 19.99s. The new Close proof joins the
  actual presented target hover, original down state, release and child/PTY end.
  The earlier timeout's exact cause remains unproven; these checks are not an
  application-policy fix or full human/physical acceptance.
- `terminal-multiplex-final-observed-z25o2fba`: the earlier candidate passes
  79/79 Release in 321.24s; ASan/UBSan ends 78/79 in 535.40s on `desktop.owner`.
  Its driver reads the latest VIEW before that frame's CONTROL lines arrive,
  raising IndexError. The original `desktop-owner-t0r5ygtv` later contains the
  requested Files control in that same frame. A retained-prefix replay reproduces
  the old exception and the new wait. Shared test helpers now require same-frame
  LAYOUT and native DRAWN before input, retain one read snapshot and never invent
  a newline for a partial owner-log record. Missing/duplicate controls in a
  completed frame still fail. Windows unit checks pass with six genuinely
  Linux-only skips; native owner acceptance requires the new carrier run.

## Canonical PTY review correction

Review finds that post-thread PTYs retained kernel defaults instead of the
existing original Terminal's canonical baseline. This affects external cooked-
mode programs even though the own shell editor works. In the isolated retained
producer `terminal-canonical-negative-3-hwymqdpz`, IUTF8 is actually disabled:
typing UTF-8 e-acute, Delete, newline and EOF to an actual `cat` job writes
`c3 0a` instead of the expected lone newline. This is an observed input defect.

The existing `pty.c` baseline is extracted without changing its flags or control
characters. Both the pre-thread opener and post-thread spawner apply it to their
fresh private slave before any child uses it. It is not a live-session reconfigure
API. `test_terminal_spawn` now owns a separate fresh original shell for the
external-program case and waits for the original foreground group to return
before inspecting its held output file. The first two draft fixtures used the
previous shell's unfinished printf/prompt and observed an empty file; preserve
them as fixture-readiness errors, not as evidence of the UTF-8 defect.

`terminal-canonical-fixed-rxou2u16` passes with isolated final C sources.
`terminal-canonical-controls-3xpiwbdi` passes the same complete native spawn test
under ASan/UBSan; removing only IUTF8 from those private sources again produces
`c3 0a` and fails the exact assertion. All counterexample builds stay outside
shared products/dependency files. The earlier whole-source run tests the earlier
candidate; it cannot establish acceptance of this later production correction.

The earlier sporadic native AT-SPI Text/GetState/Close problem remains open.
`terminal-multiplex-panes-view-0yhj7zd2` preserves another independent native
child query failure; later passing diagnostics do not establish a cause or fix.
Full multipane assistive use, paste races beyond the existing adapter suite,
physical performance, final materials and normal admitted sessions remain work.

## Final candidate verification

`/home/aetheri/build/terminal-multiplex-final-canonical-6girjfpb` passes all
79 app/UI groups in Release (318.29s) and ASan/UBSan (547.56s), without skipped
groups. This includes the actual canonical-input correction, complete-frame
owner observers, named pane projections and original-session Terminal/Files
workflows. Earlier failed runs keep their original outcomes. The old untraced
native fresh-Close and AT-SPI observations remain unresolved; passing current
tasks are not evidence of their root causes or full human/physical acceptance.

The final read-only audit joins 387 host/snapshot/guest source inputs and 601
retained exports, including 252 native images with validated raw-pixel hashes.
Selected ordinary/large-text/high-contrast/owner images were inspected. Sixteen
native executable hashes and the retained Ninja C17 command records identify
the selected core/host/test products. The first collector expects a missing
compile_commands.json; preserve its failure and query Ninja's existing command
database read-only instead. No reconfigure, relink or test rerun is needed.

Both eight-session idle samples have zero new frames and CPU ticks over two
seconds. All build/native/private-registry actors are gone; the carrier is
Enforcing on tty1 with no running Aetheri unit or seat0 session. The nineteen
protected hashes, 28 frozen SQL/XML contracts, unchanged SDK0.1.18 library hashes,
absence of database sidecars and exact historical closed DRM record are checked.
The quarantined old runtime remains unchanged/running/recover-handoff metadata;
no lifecycle settlement is inferred. See local/terminal-multiplex-2026-11-06
for final-audit.json, final-exports.json, visual-review.json and original failures.

## Continuation

Inspect the running full-suite handle and its actual results before retrying.
The original broad runner is `local/terminal-multiplex-2026-11-06/run_final.py`;
its first candidate evidence is `/home/aetheri/build/terminal-multiplex-final-exlqkwv1`.
The corrected Clipboard-focused runner is `probe_clipboard_ready.py`, evidence
`/home/aetheri/build/terminal-multiplex-clipboard-ready-e475tc5o`, now completed.
The next whole-source runner is `run_final_ready.py`, actual evidence
`/home/aetheri/build/terminal-multiplex-final-ready-7wyhkhcl`; inspect its original
live handle/results before retrying. That runner has now ended after its failed
Release gate; Debug was not started. The diagnostic `probe_close_observation.py`
ran at `terminal-multiplex-close-observation-34_rr_z0` and ended at the earlier
format assertion. The corrected `probe_close_observation_2.py` completed at
`terminal-multiplex-close-observation-2-38w5qd2q`, with the separate pointer line,
actual presented hover/press checks and no production click-policy change.
The whole-source runner `run_final_observed.py` has ended at
`/home/aetheri/build/terminal-multiplex-final-observed-z25o2fba`, with the results
above. The integrated fifteen-group runner `probe_canonical_owner.py` at
`/home/aetheri/build/terminal-multiplex-canonical-owner-yorjyycd` passes every
selected group in Release and ASan/UBSan (Release76.93s). It includes the
canonical correction and complete-frame owner observers. The current whole-
source runner `run_final_canonical.py` completed at
`/home/aetheri/build/terminal-multiplex-final-canonical-6girjfpb`, with the final
results above. Its original host handle is terminal; no unchanged rerun is needed.
The final source/protected/SDK/actor audit is complete. Preserve foreign project
documentation; publication must include only the explicitly reviewed own files.
Next: native per-pane clipboard cancellation and complete tab semantics/assistive
tasks on these original sessions, then continue the accepted desktop/app work.
The full Aetheri OS/apps/image/installer/update/hardware/experiment-desktop
goal remains active and incomplete.
