# Experimental Aetheri shell

5 November 2026. This is the first implementation profile for the requested own
shell and language. Its spelling is a provisional engineering recommendation;
final language/product review remains open. It is not a POSIX/Bash compatibility
promise, interactive terminal, sandbox, installed login shell or app-launch API.

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
  and `help`. State-changing builtins must be alone without redirection; invalid
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

The opt-in Linux C17 executable accepts `-c SOURCE`, `--file FILE` or `--stdin`.
It refuses root or mismatched real/effective IDs and requires open standard
descriptors. This is not a complete capability/privilege admission check.
Interactive terminal input is not accepted by `--stdin` yet; `-c` may execute a
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

The parent reaps every direct child. A pipeline returns its last stage's status;
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

The interactive prompt/editor, foreground terminal ownership, Ctrl+C/stop/job
control, persistent sessions, completion/history, functions/typed values, terminal
emulation, panes/plugins and graphical SDL/NexaUI presentation remain required
work. No production service/profile, normal launcher or system shell is replaced.

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

The final native Release and Debug ASan/UBSan runs pass all three CTest groups
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

## Sources

Linux man-pages, read 5 November 2026: [execve](https://man7.org/linux/man-pages/man2/execve.2.html),
[exec variants](https://man7.org/linux/man-pages/man3/exec.3.html),
[pipes](https://man7.org/linux/man-pages/man2/pipe.2.html) and
[close_range](https://man7.org/linux/man-pages/man2/close_range.2.html).
The explicit exec path avoids execvp's documented interpreter fallback. These
references explain primitives; actual Aetheri tests remain separate evidence.
