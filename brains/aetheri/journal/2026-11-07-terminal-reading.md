# Terminal keyboard reading

7 November 2026. Original Aetheri Terminal, pinned NexaUI 0.1.18 and private SDL
04 on the existing Fedora 44 Enforcing disposable carrier. Guest clock/host date
differences remain unchanged; test deadlines use monotonic time.

## Behavior and ownership

Ctrl+Shift+R enters reading at the original input row, or at the first visible
history row, and toggles it off. Up/Down move across original retained rows;
PgUp/PgDn move a viewport with one overlapping row. Home/End select the visible
ends; Control extends them to the oldest/latest retained row. Escape leaves
reading without entering the PTY. Left/Right are consumed for this whole-row
view. Modified selection, clipboard and workspace commands keep their existing
owners. Accepted ordinary shell input, new composition and pane replacement
explicitly discard reading requests.

The private presenter returns a bounded scroll delta for the host to apply to
its original model. It retains one pending source/target content binding and
composes keys before the next frame. There is no second document, model owner,
timer or process. Before drawing the outline and publishing native focus in
that frame, it checks the original model/pane, source ID/content, target content,
dimensions, screen and viewport. The viewport may retain the requested offset
or the original top-row token. This supports both live rows moving under new
output and a scrolled view retaining its original content as its offset grows.
The exact original target must remain visible. A different current focus owner,
edited/evicted source or target, unrelated scroll, disabled/hidden pane or
dimension/screen change cancels rather than targeting replacement text by row
number. Explicit clear also retires pending intent. Alternate-screen projections
receive fresh semantic identity epochs. Legacy mode now retains its original
model binding, matching the existing one-model-lifetime contract.

These are local UI/content observations. They do not authenticate an account,
file grant, run, lease, volume or privileged lifecycle operation.

## Tests and counterexamples

`terminal.reading` exercises legacy and named panes with original models:
batched row/page/history moves, boundaries, unchanged model bytes across view
requests, same-frame semantic focus and visible outline, edited source/target,
other focus, unrelated scroll, resize, alternate screen, hidden/returned pane,
explicit clear, content movement, retained scrollback under new output and
history eviction. It uses the existing native font fixtures.

`terminal.reading_atspi` runs the actual original shell/PTY in the original SDL
window. Sixty numbered output rows give an independently compiled C17/libatspi
client exact standard Text/FOCUSED targets. Row/page/visible/full-history keys,
Escape, consumed Left/Right and unchanged PTY input counters are checked. The
same original shell subsequently writes exact `intact` bytes to its proof file.
DE/EN, high contrast and larger text remain native presentation checks. The
original pane task additionally checks Up/Down on each of its two original
shells without changing the active pane or input byte counts. Clients link
libatspi/D-Bus, not NexaUI or the Terminal model.

The actual preceding published Terminal ELF is retained under
`terminal-reading-baseline-prcswsct`; the new native task fails at
`read-previous-output` in `terminal-reading-atspi-h3wuhbg5`. Its binary hash is
checked against the preceding pane-access audit, rather than calling a newly
compiled reconstruction the original producer.

Initial code passed 32 affected groups in each profile at
`terminal-reading-bagkd9k3`: Release 7/20.01s + 25/148.25s; ASan/UBSan
7/26.91s + 25/245.87s. Three additional test-only groups then passed in
`terminal-reading-extended-8mhhyh2m` (Release 9.50s, sanitizer 14.13s), with eight
product executable hashes unchanged per profile. These are earlier evidence,
not final acceptance of the later live-output correction.

The additional history-offset case then established a real focus loss despite
unchanged visible content. The first negative harness stopped before executing
the case because its static-archive hash changed during rebuilding; that
observation's cause is unproven. Its original log is retained, without counting
it as the behavioral counterexample. The settled run
`terminal-reading-offset-original-7wqq_p_u` fails the exact anchored-focus
property; executed test/source files, hashes and raw diagnostic are retained.
The first top-token-only correction failed the separate positive live-movement
property (`terminal-reading-final-_pxn5kxi`, Release 6/7). Neither failure was
repeated unchanged or hidden. The joined offset/content observation preserves
both positive cases while retaining unrelated-scroll and stale-content refusal.

Final run: `terminal-reading-joined-67wx4tap`.

| Profile | Focused groups | Other affected groups |
| --- | --- | --- |
| Release | 7/7, 22.24s | 25/25, 150.07s |
| ASan/UBSan | 7/7, 30.60s | 25/25, 211.55s |

The runner checks actual JUnit names/status, rejects skipped cases, retains raw
LastTest/build logs and stops on any failure. Leak detection and halt-on-error
remain enabled. This is the affected 32-group scope, not a claim of a new complete
83-group app/UI-suite run.

## Evidence and remaining work

Local evidence is under `local/terminal-reading-2026-11-07`, including native
raw captures, client logs/maps/proof files, exact C17 compile commands, current
ELF/source hashes and original negative evidence. The final collector verifies
617 exported files and 228 raw native images against their recorded hashes.
Inspected captures show the focused original command with the German larger-text
high-contrast footer and the restored original two-pane workspace. This is a
virtual-output observation, not final design or physical-display acceptance.

Audit: all 395 source inputs match the guest; nineteen protected hashes,
twenty-eight frozen SQL/XML contracts, SDK 0.1.18, private SDL 04 and the exact
historical CLOSED DRM record remain unchanged. No SQLite sidecars. Eight-shell
idle samples have zero frames and 0/1 CPU ticks over two seconds in Release/SAN.
The Enforcing carrier is on tty1, without own build/native/private-registry
actors, an Aetheri service or seat0 session. The quarantined old runtime is
unchanged; these child/test observations do not settle its lease or operation.
`final-audit.json` and `final-exports.json` record the exact boundary. Git history
records publication separately.

Selection/copy tasks tied to the reading row, human/full screenreader use,
physical displays/input, persistent sessions, accepted final materials and normal
production admission/lifecycle remain work. The complete OS/apps/image/installer/
update/hardware/experiment-desktop objective remains active and incomplete.
NexaUI handoff is delivered; no repeated chat message or new SDK requirement is
implied by using the existing focus/text contracts.
