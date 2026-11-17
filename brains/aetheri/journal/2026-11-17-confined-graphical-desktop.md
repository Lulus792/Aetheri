# Confined graphical login through the user desktop

The combined service binds a private POSIX shared-memory directory before
starting the backend. Its Root-owned mode-01733 mount allows name lookup
and creation without listing. Separate SELinux transitions label greeter
pixels, greeter keyboard maps and user-session buffers. Each side is
denied open, read, write and unlink permission for the other side's
files. The greeter retains its separate private `/tmp`. The user entry
creates its own mode-0700 runtime below `/run/user/1000` in the test.
The controller compares the actual process mount view and runtime labels
before accepting the visible desktop.

Case `f5ed8d7d307e` verifies five frames against the previously inspected
account, PAM prompt, authenticated, desktop and replacement frames.
Every frame matches byte for byte. PAM audit shows success in the
dedicated worker. The greeter reports `start_ack=1` and the separate
entry records desktop readiness. The desktop frame hash is
`7ba13673a9cf1fdb6f1a6fe995bc202451db03518a559367b3955cd2367d197c`.

The controller signals the original user entry through its held pidfd.
The entry and original worker exit zero, PAM records session closure and
the logind scope becomes inactive. A new greeter displays an empty account
form. The controller stops the original backend and observes the
replacement compositor, greeter and worker exit zero. The separate
greeter diagnostic reads `start_ack=0`. The temporary credential
projection is removed and the checked account files stay byte-identical.
The guest returns to tty1, with its previously selected tty8 label.

The first combined run `1dc4c2e691a2` reaches the visible user desktop and
records its independent runtime identity. The screenshot marker arrives
after the controller deadline. A separate administrative stop checks the
selected backend invocation and ELF and ends that test service. It does
not restore the original lifetime pins. Preserve that case and its
original artifacts.

The strict pre-check `075b80613daf` rejects the proposed additional
`map` assertion because the base policy includes a dormant generic map
boolean. The active carrier has `domain_can_mmap_files` disabled.
The final source keeps explicit pathname and content access prohibitions.
Build `a3b3b06eafd7` passes strict compilation over 426 inputs, with
13 intentional grants rejected. Effective checks pass all 272 decisions.
The final source loaded as `b3a2fdbf1b71` produces the same policy bytes
as the running test candidate `60783b1f5c1c`, SHA-256
`c93759a68b37e6446376975d1d40a6850130602a572221d6c00268add0caf91f`.
Both candidates are retired and the guest returns to its original
enforcing policy.

After the successful case, the user entry's own `aetheri-dNUZ4h` runtime
and empty `cache` directory remain. Other sessions for UID 1000 keep its
logind runtime parent alive. The entry currently has no removal step for
its own generated directories. Preserve this observation for an
original-object cleanup regression. The test provider remains disposable
and no release image was built.
