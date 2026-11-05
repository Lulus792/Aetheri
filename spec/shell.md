# Experimental Aetheri shell

5 November 2026. This is the first implementation profile for the requested own
shell and language. Its spelling is a provisional engineering recommendation;
final language/product review remains open. It is not a POSIX/Bash compatibility
promise, graphical terminal, sandbox, installed login shell or app-launch API.

## Language profile 0

One foreground pipeline per line; scripts contain successive lines. Blank lines
and comments starting a word with `#` are ignored. Pipeline syntax and placement
of state-changing builtins are checked before a process/redirection is created.
Runtime command/file errors can still occur after execution begins. An invalid line
stops the script with status2; an executed nonzero command normally permits the
next line. The script result is its last pipeline result, or explicit `exit`.

```text
export GREETING "Hello from Aetheri"
printf '%s\n' "$GREETING" | tr a-z A-Z
pwd
cd "/a directory with spaces"
printf '%s\n' "$?"
printf '%s\n' "saved text" > note.txt
cat < note.txt
printf '%s\n' "another line" >> note.txt
some-program 2> error.txt
exit 0
```

- Whitespace separates words. Adjacent quoted/unquoted fragments make one word.
  Single quotes are literal. Double quotes permit variable expansion; backslash
  escapes `$`, `"` and `\` there and preserves other backslashes. Outside quotes,
  backslash quotes the next byte. Empty quoted words are retained.
- `$NAME`, `${NAME}` and `$?` expand to one argument fragment. Names use ASCII
  letters/underscore followed by letters/digits/underscore. An unset value is an
  error; an explicitly empty value is valid. There is no word splitting, globbing
  or recursive interpretation of expanded operators. Literal `$` must be quoted
  or escaped. Source bytes below 0x20 other than tab are rejected; CRLF line
  endings are normalized by the executable. Other bytes, including UTF-8, are
  preserved. Expanded values are copied as data and can contain control bytes.
- `|` connects stdout to the next command's stdin. `<`, `>`, `>>`, `2>` and `2>>`
  select file redirections, at most one per standard descriptor per command.
  `>`/`2>` truncate an existing target; append preserves existing bytes. File
  names use the same word rules. Redirections override that stage's pipe end.
- Builtins: `cd [PATH]`, `export NAME VALUE`, `unset NAME`, `exit [0..255]`, `pwd`
  and `help`, plus interactive `jobs`, `fg [ID]` and `bg [ID]`. Parent-state/job
  builtins must be alone without redirection; invalid
  use rejects the whole pipeline before effects. `pwd`/`help` can be piped.
- `;`, `&`, `&&`, `||`, command substitution, here-documents, function/value
  syntax and multiline quoting are unsupported and fail before execution.
  Existing shell scripts require an explicit appropriate interpreter. A script
  with its own kernel shebang can run as an external program; an executable text
  file without one never falls back to `/bin/sh`.

Limits: 16,383 source bytes per line, 16 commands, 64 arguments per command,
256 words including redirection paths, and a 16,384-byte decoded/expanded arena.
Expansion exceeding these budgets fails. These limits are part of this profile,
not final large-script/interactive requirements.

## Execution boundary

The opt-in Linux C17 executable accepts `-i [--edit]`, `-c SOURCE`, `--file FILE` or `--stdin`.
It refuses root or mismatched real/effective IDs and requires open standard
descriptors. This is not a complete capability/privilege admission check.
`--stdin` remains noninteractive; `-c` may execute a
foreground command using its inherited stdin. The parser owns no OS resources;
its successful result owns decoded strings in the caller's program object.
`--stdin` does not read ahead: a foreground program can consume subsequent bytes
from that same stream. `-c`/`--file` keep command source separate from program stdin.

The single-threaded runner creates bounded close-on-exec pipes and retains each
original child PID. Children wait behind one start barrier until all forks
succeed; a pipe/fork failure closes that barrier without running commands or
opening output files. Each child installs only selected stdin/stdout/stderr,
closes extra descriptors and uses explicit `execve`/PATH lookup. There is no
implicit shell interpreter or fallback command. PATH search is inherited; absent
PATH uses `/usr/bin:/bin`, and an empty PATH element permits the current directory.

The batch parent waits every direct child. Interactive stopped jobs retain their
original waitable children across stop/continue until actual end. A pipeline
returns its last stage's status;
the executable resets inherited SIGCHLD ignore/NOCLDWAIT before creating them.
Signal termination becomes 128+signal. Status126 means execution denied/invalid
image, 127 means not found, and 125 means runner infrastructure failure. An open
redirection failure returns1. These are process observations, not Aetheri app
admission, readiness, cgroup drain or lease release. External commands can create
their own descendants; this runner is not their supervisor or isolation boundary.

`cd` updates physical PWD after changing directory. If that environment update
fails, it reports the already-applied directory change and removes stale PWD;
it never claims a rollback. Builtin environment changes last for this shell only.
Files are ordinary user-owned files, not managed installation/private-data state.

The separate [development terminal](terminal.md) now supplies a real PTY, bounded
cell/history model and SDL3 graphical view. Its explicit `-i --edit` profile now
supplies [our own grapheme-aware line editor and bounded history](line-editor.md).
Persistent sessions, completion/undo/search, functions/typed values, terminal compatibility and
panes/plugins remain required work. No production service/profile, normal launcher
or system shell is replaced.

## Interactive terminal and ephemeral jobs

Run `aetheri-shell -i --edit` for our line editor, or plain `-i` for canonical
kernel editing, inside an independently selected foreground process group
with its own group leader and canonical/ISIG Linux N_TTY controlling terminal.
The executable retains that terminal FD and session/group observations; it does
not create a login session, seize another foreground group or choose a fallback
terminal. Private PTY tests establish this environment without the VM's physical
console, normal session manager or a policy change.

| Action | Behavior |
| --- | --- |
| Type a line / Enter | Parse and execute it; syntax errors return to the prompt |
| Ctrl+C | Cancel pending input or signal the actual foreground job; shell remains |
| Ctrl+Z | Stop the foreground job; retain its original members and return to the prompt |
| `jobs` | Show observed running/stopped jobs; continuation requests are distinct from observations |
| `fg [ID]` | Give the terminal to that original job, restore saved modes and continue it |
| `bg [ID]` | Request continuation of a stopped job without terminal ownership; terminal reads can stop it again |
| `exit [STATUS]` / empty Ctrl+D | Exit only with no unresolved jobs; otherwise explain the retained obligation |

Without ID, fg selects the most recent retained job and bg the most recent stopped
one. There are 16 live job slots and at most 16 direct children each. IDs increase
without reuse during this shell; a stale ID never selects a new job. Labels contain
only a bounded first-command name with non-ASCII/control bytes replaced for this
provisional display; actual arguments retain their original bytes. Job records are
in memory, not app RunIds, component metadata or durable session restoration.

Both parent and child establish the job group while the child is behind the start
barrier. The parent hands over its observed terminal before release. A denied
handoff closes the barrier and waits each original child without command/file
effects. Child signal actions are reset before that phase; its mask is cleared
immediately after release, before potentially blocking file opens. Foreground
waits and background notifications use blocked signals plus signalfd/poll, never
a periodic refresh loop. A fresh stop after continuation remains visible even if
the kernel coalesces the intermediate continued event.

Normal completion/stopping requires the expected job/shell foreground before
reclaiming the terminal. Foreign ownership fails with no automatic takeover.
Stopped job termios is retained; shell termios is restored on return. In plain
`-i`, editing belongs to N_TTY. Raw records of 4095 bytes or more are rejected
before trimming delimiters/CR, preventing the observed saturated-read prefix
execution. This does not recover bytes the kernel already discarded and that
were subsequently edited below the limit. The graphical terminal instead selects
`-i --edit`, with its own bounded decoder and whole-draft rejection; see the
[editor contract](line-editor.md). Neither profile implements a full modal editor.

On terminal loss, SIGHUP or SIGTERM, this prototype signals only its still-waitable
original direct children, continues stopped ones and waits for their observed end.
It does not force-kill or adopt a replacement PID/group. A child ignoring hangup
keeps that shell owner waiting until the child actually ends. This is a provisional
conservative policy, not a final product choice about window/logout persistence.
External programs can create descendants; direct-child end is never complete
descendant, cgroup, delegation or Aetheri lease drain.

## Build, use and evidence

Inside the dedicated Linux carrier, with the existing reviewed base development
inputs, configure `AETHERI_SHELL=ON`. It is off by default and requires no SDL,
Wayland or new library. Root CMake configuration still checks its existing core
dependencies; the Release shell itself has only libc in its ELF NEEDED entries.

```sh
cmake -S /home/aetheri/source -B /home/aetheri/build/session-release -DAETHERI_SHELL=ON
cmake --build /home/aetheri/build/session-release --target aetheri-shell test_shell_parse test_shell_failure
ctest --test-dir /home/aetheri/build/session-release -R '^shell[.]' --output-on-failure
/home/aetheri/build/session-release/aetheri-shell -c 'help'
```

| Files | Responsibility |
| --- | --- |
| [parse.h](../src/shell/parse.h) / [parse.c](../src/shell/parse.c) | Bounded parsing, owned argument storage, no OS effects |
| [execute.h](../src/shell/execute.h) / [execute.c](../src/shell/execute.c) | Ordinary-user process/environment/cwd effects and original direct-child waits |
| [main.c](../src/shell/main.c) | Explicit source selection, bounded line reading, diagnostics and command-loop result |
| [jobs.h](../src/shell/jobs.h) / [jobs.c](../src/shell/jobs.c) | Private terminal/process-group ownership, signal events, ephemeral job observations and stop/continue |

The original noninteractive checkpoint passed all three CTest groups
(0.42 s / 0.94 s). These include 16 actual Linux execution/file/descriptor/signal
cases, parser ownership/grammar/limit checks and 20,000 deterministic hostile byte
inputs. Controlled fork/pipe failures prove no command/output-file effects before
complete process creation and no remaining direct children. The unchanged failure
test rejects a build-local mutation that ignores denied barrier EOF, at its actual
unexpected-file assertion. This is a controlled fault test, not installed lifetime
recovery or arbitrary descendant settlement.

The native example uses actual export/printf/tr/cat/false/exit, writes the owned
test file and returns exactly `HELLO FROM AETHERI` and `last=1` in both profiles.
The GCC static analyzer reports no diagnostics for the three implementation files.
Original reports, exact binary hashes, the negative mutation and an initial Python
test-helper module-name collision are retained under `local/shell-2026-11-05` and
the referenced guest build directories. The collision was in the signal fixture;
renaming its helper leaves the shell's signal behavior unchanged.

The interactive extension passes all four groups in Release (3.24 s) and Debug
ASan/UBSan (3.91 s), with 16 batch execution and 13 private PTY cases, existing
parser checks and fault injection. Final PTY harness synchronization/cleanup
checks pass separately in 2.64 s / 3.17 s; no implementation changed between those
runs. Test input uses actual kernel line discipline/signals. Original pidfds and
observed foreground groups replace assumed readiness delays; cleanup enrolls the
fresh private session while its original leader is still unreaped and never
rediscovers a replacement session by an old numeric SID.

The tests prove multi-process Ctrl+C, Ctrl+Z/fg/bg, TTIN stops, idle completion,
mode restoration, exit/EOF refusal, finite slots/nonreused IDs, cooperative hangup
and retained hangup-ignoring children, foreign ownership and denied-handoff effects.
FIFO-open interruption and saturated input regressions failed before their fixes.
The unchanged foreign-owner test rejects a deliberately weakened reclaim guard.
Original evidence is under the referenced `shell-interactive-*` and
`shell-foreground-negative-*` guest build directories and
`local/shell-interactive-2026-11-05`. This is native PTY evidence, not graphics,
physical keyboard/screenreader, production login/session or persistent multiplexer
acceptance. Shell-level raw editing and the original graphical terminal remain next.

## Sources

Linux man-pages, read 5 November 2026: [execve](https://man7.org/linux/man-pages/man2/execve.2.html),
[exec variants](https://man7.org/linux/man-pages/man3/exec.3.html),
[pipes](https://man7.org/linux/man-pages/man2/pipe.2.html) and
[close_range](https://man7.org/linux/man-pages/man2/close_range.2.html).
The explicit exec path avoids execvp's documented interpreter fallback. These
references explain primitives; actual Aetheri tests remain separate evidence.
Interactive references read separately: [signalfd](https://man7.org/linux/man-pages/man2/signalfd.2.html),
[terminal foreground groups](https://man7.org/linux/man-pages/man3/tcsetpgrp.3.html)
and [canonical termios behavior](https://man7.org/linux/man-pages/man3/termios.3.html).
GNU libc job-control pages timed out; they are not claimed as inspected evidence.
