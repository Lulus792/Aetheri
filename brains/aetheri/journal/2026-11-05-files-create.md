# Create-only Files workflow

5 November 2026. Original ordinary-user development functionality, not normal
app/document admission, durable operation recovery or an installed OS image.

## Result

Files now creates an empty file or folder through a literal-name dialog. Ctrl+N
and Ctrl+Shift+N, pointer buttons, Tab/Shift+Tab, Escape, Enter, Ctrl+U and whole-
grapheme Backspace use the same operation path. Long input shows the editable
tail; rejected input cannot submit a retained valid prefix. The retained parent
descriptor and immutable name keep background navigation from changing the target.

`files/create.h` separates effect-free preparation from one attempted operation.
Empty files reuse the editor's staged no-replace save. Directories use mkdirat,
held/named checks and object/parent flushes. Parent ownership/modes are rechecked.
One SDL worker performs the attempt; the UI stays responsive and close waits for
its original result. Existing names remain unchanged. Success refreshes/selects
the new entry; refresh failure cannot turn confirmed creation into a retry prompt.

UNKNOWN retains the operation and possible staging name. Further creation in that
window is inspection-only. Cancel/close never delete candidates or roll back an
effect. No durable restart journal, hostile-writer exclusion, volume identity,
StorageRef or brokered grant follows. See [contract](../../../spec/files.md).

## Executed verification

The existing independently identified Fedora carrier stayed SELinux Enforcing.
Exact source snapshots were copied with `tools/sync_guest.py`. The commands and
complete results are retained under `local/files-create-2026-11-05`:

```text
python local/files-create-2026-11-05/run.py
python local/files-create-2026-11-05/collect.py
python local/files-create-2026-11-05/exercise_drm.py <final-drm-directory>
python local/files-create-2026-11-05/final_audit.py
```

The run driver configures/builds both existing session profiles and runs:

```text
ctest --test-dir /home/aetheri/build/session-<profile>
  -R ^(text\.|editor\.|files\.|shell\.|terminal\.|settings\.|desktop\.|compositor\.(compose_|input$|clipboard$|wayland$|desktop$))
  --output-on-failure --output-junit <profile.xml>
```

All **43/43 groups** pass in Release (**121.05s**) and ASan+UBSan (**220.79s**).
New core tests cover collisions, private modes, held-parent rename/replacement,
allocation failure, permission drift and lost mkdir/file/directory-flush replies.
The unchanged test passes an original copied source and fails after removing only
the cached-result guard. This is an actual negative control, not a second test
that assumes replay occurred.

Each Files native profile has **30 cases and 30 images**, with zero CPU ticks in
its bounded one-second idle sample. Actual Wayland keyboard/pointer proves modal
target/focus fencing, 640×420 German/contrast/200-percent usability, cancellation,
whole-name rejection, conflicts and empty-file → Editor → exact save. A separate
`test_files_fault` binary performs a real mkdir, waits for independent release and
then loses its acknowledgement. UI remains responsive, inspection cannot submit
another operation, and close waits for the original worker. The ordinary binary
does not contain that fault hook. Debug command evidence confirms instrumentation
of the new source/test paths.

The actual virtual DRM session
`ee28da006f5442c6b1ab2468744eddd9` starts with the original protocol3 owner and seven
bound binaries. QMP/PS2 input creates `notes/first.txt`, saves exact
`Aetheri document` bytes in the original Editor, rejects replacement, cancels an
unsubmitted second name, closes/reopens Files and retains the first file. Three
actual app actors and private 0700/0600 workspace modes are recorded. Eighteen
stable PNG captures accompany the effects. All direct session roles end status0;
the original development record is closed/success.

The first DRM driver stopped before creation because it waited for VIEW lines
that the development supervisor intentionally filters. Journal and screenshot
prove the original Files window and empty workspace. The corrected driver used
that same still-live session, independently verified the empty precondition and
checked actual CREATE records, filesystem effects, actor observations and pixels.
No uncertain creation dispatch was replayed and no replacement session was started.

Final audit: **333** host/snapshot/guest sources match, **28** frozen SQL/XML and
**19** old-runtime hashes remain unchanged, and no old SQLite sidecar exists.
Retained native evidence contains **1,079 files, 306 images and nine special-node
records**. The carrier is idle on tty1 with no known native program, Aetheri unit
or seat0 session. The old running/recover-handoff intent remains quarantined;
these development UI checks do not settle it.

## Remaining scope

Creation recovery across process/session restart, normal writable document grants,
copy/rename/move/removal with previews, richer handlers and the standard graphical
editor remain work. The original full OS/apps/image/installer/experiment-desktop
objective stays active. Keep NexaUI catalogue maintenance incremental; no new
handoff message is pending. The independent NexaCloud update-depot planning is
recorded in that project's own Roadmap; it is not an implemented updater.
