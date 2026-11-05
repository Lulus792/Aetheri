# Owned terminal composition and native text input

5 November 2026. Concrete progress on the active full Aetheri objective; no OS,
complete terminal, installed IME or production session completion is implied.

Added bounded `src/terminal/composition.{h,c}` and its pure regression. The app
owns copied preedit text/selection/cursor, horizontal view and logical candidate
placement, independently of PTY cell/history contents and SDL object lifetimes.
Complete validation precedes replacement or committed input. Editing/confirmation
keys are held while composing; controls/multiline commit are rejected. Drafts
end on Escape/context loss/close, and native input is inactive during blur/end/
paste review. Local queued-event fencing prevents old batched text from reviving
a cancelled draft. Provider identity, transaction serial admission and future
messages remain separate trust obligations.

Read pinned SDL3 3.4.20 keyboard/video/Wayland sources and current official editing,
ClearComposition and input-area documentation. The Wayland ClearComposition
callback is absent while SDL's generic call returns true, so use actual native
Stop/Start transitions. Read the already reviewed wlroots 0.20.2 text-input source:
enable/disable have distinct signals instead of always emitting commit. An initial
native fixture timed out waiting for the wrong signal; all three are now observed.
That was a fixture failure, not absent SDL activation or an accepted test result.

`tests/compositor_text_input.{h,c}` is a controlled native v3 producer with eight
slots and fixed messages. It is linked solely into the inherited-socket test
server and explicitly enabled with I on. Production compositor globals/sources,
normal provisioning, policy and dependency families are unchanged. The actual
production IME route remains required; client staging is not a substitute for it.

Final fourteen-group Release passes 52.85 s and Debug ASan/UBSan 66.15 s. Earlier
53.03/61.83 s runs predate the final native Escape/commit batch and activation
gating. Preserve both as scoped history. Current logs/JUnit and original failure:
`/home/aetheri/build/terminal-composition-check-kzd0d93p`.

The IME group has seven cases/nine images per profile. It checks real v3 -> SDL ->
original shell/PTY Unicode bytes, transient scalar selection, held editing keys,
actual disable/enable on Escape, empty/combining drafts, queued stale commit,
1000-byte/200% DE/high-contrast viewing, controls/multiline rejection, independent
client focus loss and original end. The mutant in composition-negative-cowwnzfn
removes only the preedit editing-key guard; the unchanged native assertion rejects
it after actual speculative key input. Its exact source/compile/link/binary hashes
and log are retained. This is not hostile-provider or general IME compatibility.

Final native roots under `/home/aetheri/build/session-<profile>`:

- Release: terminal-compose-og_8csae, terminal-clipboard-lboh59vd,
  terminal-ui-5395r_fn, desktop-ui-abzlafrq, desktop-owner-dk4g96ps.
- Debug: terminal-compose-e8_uebb9, terminal-clipboard-az5bugpq,
  terminal-ui-qc2e66w9, desktop-ui-1m7oyx2s, desktop-owner-82pl5tom.

All 162 decompressed image hashes and 274 copied files verify in
`local/terminal-composition-2026-11-05/native-evidence-index.json`. Native selected
German preedit and the long 200% high-contrast draft were visually inspected;
controls and candidate geometry remain bounded. Human assistive, arbitrary-script
shaping/fonts and physical-device acceptance are not established.

The separate actual Debug DRM/QMP run exercises the normal desktop, shell and
clipboard at 100/200% with the final terminal; it cannot demonstrate production
IME because that compositor route is absent. Token eed8b9b32a0b4279b8741a60c076d87a,
original PID 47442 and invocation 7336891bb040482bb3a8c257c446ff5b, ends all three children
with status0 and is closed. Twelve console PNGs form six stable pairs and both
original pixel baselines restore exactly. Stdout stream 960440efdfe741fc9364f00d9982ffa9
is bound to session-893.scope. The terminal matches the actual native test binary.
Final audit passes 276 host/snapshot/guest source hashes, 28 frozen SQL/XML,
nineteen protected old catalogue/static/profile hashes without SQLite sidecars
and 886 local Markdown targets. The guest is enforcing on tty1 with no known
native app/test process, running Aetheri service or seat0 session. See
`local/terminal-composition-2026-11-05/final-audit.json`. The old running/
recover-handoff attempt remains unchanged and quarantined.

Next: implement the reviewed production text-input/provider route and own rich
editing/panes, alongside admitted session/app and full OS/image/app/experiment
work. Retain the complete original objective and the quarantined old lifecycle
attempt; no UI state or test result settles its ownership obligations.
