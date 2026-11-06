# Truthful Terminal startup output

6 November 2026. Implemented and scoped final validation complete; intermittent
native-query investigation remains open.

The NexaUI Terminal's normal shell profile now selects the fixed
`-i --edit --welcome` argv. The original shell emits the accepted Aetheri-shaped
ASCII mark and a one-time kernel-information snapshot before its first prompt.
It is ordinary PTY output, retained by the original cell/history/selection and
native read-only text paths. Repaint, zoom, text size and language changes do
not repeat it. Direct-editor windows and the earlier quiet PTY profile stay
quiet. No substitute shell, second model, helper process, new network lookup,
timer, configuration file or runtime library is introduced.

The report identifies **Aetheri development**, the numeric current UID and
sanitized kernel hostname, kernel release, architecture, online kernel CPU
count, usable kernel RAM total in GiB, uptime and the experimental Aetheri shell
profile. These are separate observations at startup, not a machine attestation
or a promise of process-affinity/cgroup budgets. RAM is explicitly total, not
invented used/available memory. There is no copied 1.0 release, display/GPU data,
mock username or assumption about physical versus virtual hardware. Account
name resolution is avoided so NSS/network configuration cannot delay startup.

`welcome.c` separates capture from formatting. Missing values remain unavailable;
checked memory-unit multiplication refuses overflow. Kernel strings become
bounded printable ASCII, so they cannot supply terminal controls. Only authored
blue/reset SGR escapes color the mark. Formatting uses a private 4096-byte buffer
and commits only a complete result; undersized/invalid requests leave caller
output untouched. Fields wrap within columns minus one, avoiding implicit soft
wrap. At 72 columns the information is beside the mark; narrower views stack it,
with a compact mark below 26 columns. Output uses the original signal-aware
nonblocking terminal writer and confirmed editor modes. Interrupted decoration
is not replayed; original shutdown and child/stream rules remain.

## Evidence so far

The new `shell.welcome` group checks real kernel observations, injected failure/
overflow/control-string cases, every width 16–240 plus 4096, exact capacity and
atomic rejection. The PTY test selects the new fixed argv, sees actual output,
executes a command in the same original shell and observes its normal exit.
Quiet-profile evidence remains separate. Native selection tests find the actual
prompt row rather than assuming it is the first line. The independent libatspi
client checks the displayed report against its own kernel observations, ASCII
mark and real prompt/caret before typing commands. It is not linked to our
formatter or NexaUI.

The first build at `terminal-startup-probe-s23tvkt_` exposes the second shell-main
consumer `test_shell_editor_allocation` missing the new static library. Its link
is corrected without altering the fault hook or original allocation semantics.
The six-group corrected run at
`/home/aetheri/build/terminal-startup-probe-267hwp4e` passes Release 38.73s and
ASan/UBSan 52.79s, without skips (runner 38.76s/52.81s).

Separate compiled negatives at
`/home/aetheri/build/terminal-startup-negative-79uzibzh` leave shared products,
sources and linker dependency outputs untouched. Removing control filtering
fails the defanged-field assertion; removing the multiplication guard fails the
unknown-memory assertion. Replacing the real Terminal's welcome profile with
the quiet profile fails the independent native startup-information check at
`seen==511u`; its ordinary prompt remains visible. Native failure artifacts are
`terminal-atspi-9m7zyi0f`. All original commands/logs are retained.

The first full suite at
`/home/aetheri/build/terminal-startup-final-nausx6vn` passes all 75 Release
groups in 307.03s, but ASan/UBSan ends 74/75 in 537.03s. The independent native
peer fails acquiring a row's Text interface after keyboard reading focus
(`terminal-atspi-cdpoasyb`). The actual report, remaining terminal workflows and
other 74 instrumented groups pass. This is not a complete passing suite.

A subsequent diagnostic-only peer adds direct native object/interface details
when Text acquisition fails, without relaxing its assertion, time limit or
target identity. The focused instrumented run `terminal-startup-focus-dgrlky3v`
passes once, then fails earlier with a direct GetState `NoReply` and a defunct
frame (`terminal-atspi-elmx96ol`). No SDK or host root cause is established.
The separate live-failure probe also records a ten-second Close-client timeout
(`terminal-atspi-8d3ntpqv`), before it could capture live task state. All failed
fixtures retire through the driver's original cleanup; no original failure is
reclassified as a pass.

Three runs with a privately compiled UI/wait/poll timing observer, three with
an additional private-bus monitor and three with a failure-only live task probe
pass. The ordinary shared programs/libraries and linker dependency metadata
remain unchanged by these private experiments. Timing/monitoring can perturb
scheduling: these positives do not explain or fix the uninstrumented failures.
The timing binary is not the final producer. No strace is installed; a checked
attempt stops before dispatch rather than installing anything. Original suite,
native and diagnostic artifacts are exported as 357 indexed records under
`local/terminal-startup-2026-11-06/preserved-native/`. Do not replace them with
later passing results. The earlier untraced state-query issue remains open too.

The final suite with the diagnostic peer and ordinary application at
`/home/aetheri/build/terminal-startup-final-diagnostic-hm7pr6dg` passes all 75
groups in Release (316.68s) and ASan/UBSan (559.35s), with no skips. Runner wall
times are 316.70s and 559.38s. The actual native samples are
`terminal-atspi-a9ml580r` and `terminal-atspi-k249mfsx`, each with seventeen peers
including two intended negatives. Both record zero idle CPU ticks/new frames
over two seconds. Full terminal workflows record zero idle frames and 0.00s/
0.01s CPU respectively. These passing samples do not resolve the failures above.

The audit joins 378 host/snapshot/guest inputs and 246 exported records, including
96 native images, unchanged nineteen protected hashes/28 frozen contracts and
no sidecars. Both SDK libraries remain 0.1.18 with their original hashes. The
ordinary application/shell/unit-test hashes still match the counterexample
baseline. The carrier is enforcing and idle on tty1 without test/build/private-
registry actors, running Aetheri services or seat0 session. The historical closed
DRM record is unchanged; no new DRM run or installed authority change occurs.
The collector initially expected transient link.d files after Ninja had consumed
them. Read-only `ninja -t deps` reports valid original-object/library dependencies
for both targets. The corrected collector exports these actual stored records;
no relink, metadata recreation or product modification was needed.

Actual final native startup/zoom images and the large-text/contrast workflow
were inspected. The opaque compact header, blue mark, real fields and normal
prompt remain legible. This is scoped fallback acceptance, not final glass,
arbitrary-script fonts or physical/human acceptance. Local records are in
`local/terminal-startup-2026-11-06/`. Final studio typography/material, external
IME/human reading/physical acceptance, normal admission and actual tab/pane
lifecycle remain open. The older untraced native-state query is unresolved.
The foreign UI design note changed externally during this task to add accepted
account/recovery designs. Its Terminal scope remains compatible. The original
hash manifest is retained, and separate reviewed-current manifests record the
account/recovery and later Files visual deltas; the other three foreign files
retain their original bytes. The unreviewed move-picker proposal is not adopted.
Do not stage or overwrite any of these files. Preserve the old quarantined runtime.
The full Aetheri goal
remains active and incomplete.

## Next-session mechanism probe

The current PTY API must remain pre-thread-only. An isolated C17/Linux/glibc 2.43
probe at `/home/aetheri/build/terminal-pty-spawn-mechanism-et8ulna8` instead uses
`posix_spawn` file actions and a held `/proc/self/fd/4` bootstrap image. After its
own exec, that single-threaded helper obtains the controlling PTY, closes its
image descriptor and execs the held original shell in the same child. No
permanent helper or extra authority is introduced. The local source/result and
compiler commands are under `local/terminal-startup-2026-11-06/`.

With an already-running allocation worker, the probe creates two actual PTYs,
checks each original child/session and current shell image, exchanges distinct
commands, waits the first original child/stream and runs another command in the
survivor before waiting its own child/stream. Deliberately non-CLOEXEC parent
pipe descriptors are absent from the bootstrap. Strict C17 warnings pass; the
probe exits zero. This is a tested implementation direction, not a shipped
spawn API, original-image admission, tab/pane UI or general recovery proof.
Source/profile binding, bounded multi-session host ownership and the actual
view/input/close integration still require implementation and tests. Reference:
[posix_spawn contract](https://man7.org/linux/man-pages/man3/posix_spawn.3.html).
The post-probe audit repeats the nineteen protected hashes and exact closed
DRM record, retains enforcing/idle tty1 and finds no build/test actors. The
prototype does not change product sources, installed inputs or the tested SDK.
