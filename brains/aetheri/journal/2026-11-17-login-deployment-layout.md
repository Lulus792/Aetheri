# Fixed graphical login layout

The [deployment layout](../../../spec/login-deployment.md) fixes the system
paths and account, unit, PAM, runtime and policy inputs for graphical login.
The stager takes eleven explicitly selected artifacts and produces 24 files
and a receipt. The startup and greeter observers read `bin_t` directories
because an installed path can descend through `/usr/libexec`. That permission
is limited to directories. The greeter's execution of general `bin_t` files
remains forbidden.

The three packaging tests pass on Windows and as the ordinary Linux VM user.
They verify repeatable bytes and modes, exact layout, and refusal before output
creation for altered content, invalid artifact formats, and ambiguous or
incomplete selection. The isolated Linux case `33b887fe4e0e` uses inert ELF
fixtures and the reviewed font. It checks the packaged units with
`systemd-analyze verify`; enables the display-manager alias inside the isolated
root; exercises the real `systemd-sysusers` and `systemd-tmpfiles` recipes; and
checks all 17 file-context paths against libselinux. The staged bytes and the
VM's own account database are unchanged by those checks.

Strict candidate case `9360a4d7940b` compiles the positive 426-input policy and
rejects all 13 assertion violations. Loaded candidate `022652a61780` has the
same complete effective policy digest `c93759a6…caf91f` as the previous
confined test on this carrier. The loaded checker passes 228 core and 272
graphical decisions.

The separate live case `994e02d124b3` uses the existing VM provider, the
original staged session runtime, `ProtectSystem=full`, and two Root-owned
mode-1733 scratch mounts. The test sees distinct `/tmp` and `/dev/shm`
directory identities. The actual confined user and greeter use the selected
mounts, with SELinux separating their file types. Synthetic local password
authentication passes through PAM. The keyboard drives Start, the visible
desktop and logout. The original entry and worker finish, PAM closes, and
the original user runtime disappears with its parent and control sibling
preserved. The replacement displays an empty login form and its original
chain stops at zero. This test uses the fixture's unit names, provider, file
placement and private PAM configuration. The packaged unit and PAM paths
still need a full boot test with the normal provider.

The exact candidate module is retired with no typed tasks, enforcing mode,
the original carrier policy digest `a1c128d0…4b1e4f6`, and VT1 restored.
Normal image assembly, fixed-path provider selection and the first release's
USB acceptance gate remain open.
