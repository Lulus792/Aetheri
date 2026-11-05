# Executable own modal editor

5 November 2026. The opt-in C17 `aetheri-editor` now joins the original document,
modal, local file and pure cell-view modules. See the [guide/contract](../../../spec/editor.md).
It is a foreground development command inside our existing terminal, not a normal
admitted app, a document grant or the finished OS/editor.

## Behavior and boundaries

The first view has header, numbered/panned text, current mode/location/content
state and keyboard footer. Clusters/caret stay whole; tabs and unsupported or
invisible clusters use the existing terminal-cell profile. Filename bytes cannot
inject controls. The 30x8 minimum has wrapped/scrollable help; smaller views pause
edits/commands. Close choices take footer focus and cancellation restores the
original document. Native Compose and terminal text scaling stay with their owner.

Own /dev/tty OFD, session/foreground ownership and a bounded poll/signalfd loop
avoid changing shell descriptors. Alternate screen/modes restore before stop/
exit. Background resume stops again until foreground returns. Bounded input
bursts do not read beyond a completed quit. Literal bounded UTF-8 commands reject
invalid/control/overflow prefixes until explicit reset. No plugin/shell command
evaluation is performed by the editor.

Local save uses the existing metadata/unknown-outcome contract. Close checks
uncertainty independently of clean content, and explicit discard requires the
literal word DISCARD. Save-as/export are create-only: export retains original
dirty/unknown state and up to eight extra unknown file holders. Untitled save
and close asks for a new path and only exits after acknowledgement. General
artifact inspection, cross-process/durable restart reconciliation and brokered
access remain work. Emergency signal/TTY-failure copies are best effort in the
current owned directory, not periodic autosave or recovery after SIGKILL/power loss.

## Executed evidence

Root `/home/aetheri/build/editor-app-check-i_lnntsm`:

- All **34/34** affected groups run without skips/failures in Release **97.40s**
  and Debug ASan+UBSan **126.33s**. Original shell/terminal/desktop/Compose groups
  remain, with the new file/view/native editor groups.
- **Twelve actual editor PTY cases per profile** cover new/open/untitled files,
  save/save-as/export, dirty and clean-UNKNOWN close decisions, explicit discard,
  Unicode grapheme deletion/endings, invalid/overflow command rejection, compact
  input refusal, minimum-size help, same-buffer stop/bg/fg and emergency SIGTERM
  copy. Exact shell modes and subsequent queued shell input are checked.
- **Sixteen actual native provider cases per profile**, including three new editor
  workflows and fifteen raw images. Actual native keys/Compose/SDL/PTY input joins
  the original running editor image, a dirty-close footer focus/cancel, 200-percent
  high contrast, exact file bytes and return to the same shell. No fixed text
  producer substitutes for the real application.
- Independent terminal-grid projection checks ordinary/minimum/wide/240x100 and
  compact sizes, unsafe filename bytes, long-line caret, UNKNOWN/confirmation and
  command tails, help and bounds.
- A separate source copy first passes the same clean/UNKNOWN-close test. Removing
  only the uncertainty guard from clean close makes the unchanged test fail with
  exit 1. The test-only fsync producer is never linked into the ordinary editor.
  Per-unit feature flags, original Unicode license, sources/binaries/commands and
  both result logs remain retained.

Visual review after that full matrix found that long absolute parent paths hid
the filename in the header. The view now puts the basename first. All **three**
affected view/native/provider groups pass again in **both** profiles, including
the new minimum-width basename assertion. The original matrix/source snapshot
and final header-result identities remain separate; this does not claim all 34
groups were rerun after the header-only change.

## Ordinary virtual DRM

Two actual Debug desktop+Compose attempts run on boot
`124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. The initial candidate uses original token
`54f30decb42241b191ee71429942f6ac`; the final basename-first candidate uses
`fab95e7e7a1e49909171699bb7e66881`, controller PID 108148, invocation
`173e3f06851a4b2ebbd4432a31415cb0`, stdout stream
`0c7e26214d254791adf6851369b80e4e`, session-1763.scope.

Actual QMP/PS2 through bochs-drm/logind opens our terminal/editor, verifies absent
file before save, exact close-cancel document/caret pixels and acknowledged file
bytes. At 200-percent high contrast the final header keeps `editor-note` visible.
The editor returns to the same shell, which executes a new proof command. Both
original compositor/desktop/terminal trees exit 0 and their same tokens close.
QMP remains protected host-local AF_UNIX with SDL/gl-off/window-close-false.

The final ordinary editor snapshot is PID/group 108447, parent shell 108283,
current UID1000 and image SHA256
`445daad21aae181c3e60c4927070f21932e50b9be5a95862f0f4bc4cafa00f34`, matching
the final native provider/header result. This snapshot is not an app grant,
retained recovery pin or independent executing-lease drain proof. The actual
PTY/native tests separately retain their original process pins.

The index verifies 899 copied native files and 204 decompressed native images;
six intentional special nodes have lstat records, never content reads. Both DRM
attempts retain 44 console PNGs, including 34 final files in stable pairs and
earlier probes/observation. Actual native and ordinary 100/200-percent images
were opened and inspected; both retired session file proofs match exact bytes.

The read-only audit matches 318 exact host/snapshot/guest sources, 28 frozen
SQL/XML and 19 protected old hashes without SQLite sidecars. All 1,040 local
Markdown file targets resolve. The enforcing guest is idle on tty1, without known
native apps/tests, running Aetheri units or seat0 sessions. The old quarantined
running/recover-handoff state and bytes remain unchanged.

These results do not establish physical input/monitors, human screenreader
acceptance, trusted login/lock/resume or normal admitted apps/documents.

## Earlier failures retained

The first actual PTY group had four failures: repeated raw-frame predicates could
accept old state, the queued next shell command made a mode assertion inspect
the next command's legitimate canonical mode, a fixture used `set` instead of
our actual `export` builtin, and a cancelled discard left a stale message. The
fixture now requires a newer complete frame, checks modes at the next settled
prompt, and uses the actual grammar; Escape clears the stale app message.

A broad native provider run exposed a fixture race: Insert's `i` was not yet in
the accepted-byte count when the next text target was computed. It now waits for
that actual byte before typing. A minimum-size test exposed clipped help: the
view now wraps all help text and clamps its scroll offset. A further test barrier
had searched raw output for the underlying document even while help overlaid it;
it now also requires the help footer to disappear before sending a new command.
One strict compile rejected a shadowed local in wrapping; it was renamed. A
chained first attempt accidentally continued against an old binary after that
compile failure; those results are not final evidence. Later builds stop on errors.

The initial ordinary DRM driver compared newline-terminated `ABSENT` output with
an untrimmed token and stopped before editing. Its continuation first verified
the same live original session/editor and corrected that observation. A further
seed assertion had counted every unstable screenshot as a final pair; it now
retains all probes while selecting the last equal pair. No app/session was
restarted to hide either fixture error. That same original attempt later passed
and retired normally; a separate reviewed attempt checks the final header candidate.
That final driver also observed the original shell's child list before the new
editor was visible. The same original context was re-observed with the correct
running editor and continued without another launch; its final attempt passes.

The initial failed matrices, source/binary identities and transcripts remain
retained. No weakened policy, changed frozen contract or quarantined-runtime
retry was used to pass these checks.

## Remaining scope

Document undo/search/selection, plugins/panes, durable sessions, accessible text/
external IME/bidi/complex shaping, localised/editor light-theme design, ordinary
app/document admission and hardware remain open. The user retains final design
choices. The full OS/apps/image/installer/experiment-desktop goal remains active.
Simultaneous termination/stop signals and broader failure/recovery scheduling
still need explicit fault coverage; the proven SIGTERM copy is not that guarantee.
The existing NexaUI integration case was extended in place, preserving all 103
IDs and thirteen surface groups; no new chat message, SDK or consumer pin change.
