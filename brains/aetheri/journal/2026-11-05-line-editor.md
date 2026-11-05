# Own grapheme-aware shell line editor

5 November 2026. Original C17 editor/history, generic UTF-8/grapheme module and
Linux input adapter now connect to the actual graphical terminal. The held shell
receives `-i --edit`; batch and plain canonical `-i` remain supported. See the
[contract and controls](../../../spec/line-editor.md).

## Result and limits

Whole-grapheme cursor/delete, start/end, cut/yank, draft-preserving bounded
history and horizontal viewports are implemented. History is in memory only;
there is no new service, thread, runtime library, normal app capability or
production policy. Editing has an explicit whole-draft invalid/overflow guard.
The adapter joins a private nonblocking controlling-terminal OFD and restores
original modes before commands; it never reads beyond accepted LF into program
stdin. Job stop/continue, signal cancellation and shutdown retain original
process/terminal ownership.

This is a single-line editor, not the finished modal editor or multiplexer.
Completion, undo/search, panes, persistent sessions, accessible text, shaping/
font fallback, normal admitted apps/sessions and the complete OS/experiment
desktop remain required. Provisional controls are not final product design.

## Unicode and footprint

The original algorithm implements Unicode 18 default extended-grapheme rules,
including revision 49's Indic linker rule. The pinned official data and complete
Unicode License V3 produce 1,641 combined constant ranges and all 853 official
test vectors. Original code remains MIT; derived data retains Unicode-3.0.
Reproduction and all five cache hashes pass. Exact versioned URLs, sizes,
locally observed SHA-256 values and license provenance are in
[unicode-inputs.json](../../../spec/unicode-inputs.json). Hashes are not upstream
signatures. Generic segmentation has no allocation and does not claim glyph
width/shaping/normalization. The Release shell still links only libc dynamically.

The editor retains a bounded 16,383-byte line, draft/cut buffers and boundary
array; history retains at most 64 entries/131,072 bytes including terminators.
A candidate history copy is allocated before eviction, temporarily adding up
to 16,384 bytes. No new performance superiority is inferred from this footprint.
Complex clusters have explicit display fallbacks in the current terminal cell
profile without changing their original command bytes.

## Executed evidence

Guest root `/home/aetheri/build/line-editor-check-qxwuf9lp`:

- Final Release: **28/28**, 77.02s; Debug ASan/UBSan: **28/28**, 93.16s. The exact
  selected set is `text.*`, `shell.*`, `terminal.*`, `desktop.*` and compositor
  Compose/input/windows/desktop/clipboard groups. Original logs and JUnit XML
  are retained. Third-party libraries keep their existing reviewed profiles.
- `text.graphemes`: all 853 official vectors, strict malformed UTF-8, measurement,
  capacity and unchanged-output failure cases. Separate scratch build
  `grapheme-negative-gl2ucjbx` runs the same test successfully, then omits only
  GB11 from a copied implementation: the unchanged official-vector test exits 1.
  Its positive/negative binaries and source/command/result hashes are retained.
- `shell.editor`: real combining/ZWJ/flag edits, alias insertion, atomic invalid
  rejection, draft/history copy ownership, malloc failure, count/byte eviction
  and full-line overflow. The first test build had an unbraced-loop warning;
  braces fixed the test without weakening strict compiler warnings.
- `shell.editor_native`: nine actual private-PTY cases per profile. Includes
  exact edited UTF-8 file effects, no-prefix rejection, Ctrl+C/status 130,
  oversized input, actual canonical/ECHO/foreground/nonblock=false child
  context, subsequent program stdin, resize, Escape timeout, stop/fg mode
  restoration, exit restoration and no history file.
- Native Compose provider: ten cases per profile, six raw images each. Added
  actual multi-scalar Compose deletion and Home/Delete/End/history effects to
  the existing native focus/device/keymap/preedit checks. Normal development
  owner with and without Compose, clipboard and selection checks also pass.

An actual viewport counterexample in
`session-release/shell-interactive-vvqimdvn` placed the caret at 90 in 80 columns
after a long indivisible Hangul cluster. `viewport-before.log` retains the
unchanged failing <=78 assertion. The fix advances past the entire cluster to
keep the caret within the actual width; the expanded native case passes both
profiles. It does not split the cluster or alter text bytes.

Two expanded graphical-test failures were fixture errors: a keyboard receipt
race (`compose-provider-hsd6uvet`) and nine rather than ten right arrows
(`compose-provider-apvql5md`, actual `axc` file). The fixture now waits for real
terminal input counters and sends the correct sequence; the same expected `abc`
assertion remains. Original logs/file bytes are preserved. An earlier direct
PTY fixture similarly corrected its cursor count. These are not promoted to
production bugs or evidence of a relaxed timeout/assertion.

Final review added another actual counterexample:
`session-release/shell-interactive-xorrpp46` shows that timed-out `ESC [` could
be discarded and `touch partial-escape` then executed. The original created file
and `partial-escape-before.log` are retained. The adapter now distinguishes a
lone expired Escape from an incomplete key sequence: the latter blocks the whole
draft until reset. The same expanded native test now passes in both final2 profiles.
The earlier 69.04/95.75s matrices and DRM run predate this guard and retain their
own identities; final2 matrices/console evidence supersede them.

`local/line-editor-2026-11-05/native-evidence-index.json` verifies **607** copied
files and **186** decompressed native images, including final roots, original
viewport/graphical failures, scratch negative control and both final matrices.

## Ordinary virtual DRM session

Final original Debug desktop+Compose token `98b5cdf92c0740f6ac401ee07d11ae69`:
boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`, PID79587, invocation
`945411d4248949069375b03b66e0627f`, stdout stream
`30fe574b4379427b9458703584179fc5`, session-1439.scope.

Real QMP PS/2 keys through logind/libinput/bochs-drm select the original terminal.
Home/Right/Delete/End produces exact `abc`; moving only that proof file and
Up/Enter produces it again. Compose apostrophe+j then x/Left/Backspace/End leaves
exact `x`, deleting the whole base-plus-accent unit. Preedit/cancel at 100/200
percent restores baseline pixels exactly. Fourteen final PNGs form seven stable pairs. The earlier token
`42a36a85e1c04d5fa5e678dd574c8785` and its fourteen images remain separate
pre-escape-guard evidence; they are not reassigned to the final shell.
The middle-edit and 200-percent preedit images were visually inspected.

QMP remains protected host-local AF_UNIX; actual display is SDL/gl-off with window
close disabled. The original compositor/desktop/terminal all exit 0, and the
controller closes that same token. Native and console terminal/shell digests
must agree in the final audit. This is virtual software/native-protocol evidence,
not physical devices, Windows-keyboard forwarding, assistive human acceptance
or a trusted/admitted session.

## Preservation and continuation

The final read-only audit matches 299 host/snapshot/guest files, 28 frozen SQL/XML
and 19 protected hashes without sidecars. It verifies 607 copied native files, 186
native raw images, 14 final console files and 967 local Markdown targets.
The final read-only audit is `local/line-editor-2026-11-05/final-audit.json`.
It checks host/snapshot/guest sources, frozen SQL/XML, protected old hashes/no
sidecars, idle enforcing tty1, absence of known native apps/tests, Aetheri units
and seat0 sessions, local file links, console files and shared native binaries.
The quarantined old running/recover-handoff attempt remains untouched; this work
cannot settle its original catalogue/lease obligations.

Continue with own editor commands/undo/completion and panes/session behavior,
without losing normal app/session admission, image/installer, remaining apps,
hardware and early full experiment-desktop requirements. NexaUI checks maintain
its existing catalogue only; no further chat message is needed.
