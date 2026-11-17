# Graphical greeter executable

`aetheri-login-greeter` is an opt-in Linux executable with an explicit
compiled provider. The provider retains authority for MAC, namespace, source
and runtime selection. The executable checks its account shape, stdio and
`NoNewPrivileges`, drops inherited extra descriptors and calls the runner.

The runner checks the active greeter session and selected compositor ELF,
pins the original child before allowing exec, and hands its private display
guard to the native NexaUI host. It uses the backend socket path only as a
locator. The connection owner still verifies the original live backend and
protected handoff. On return the runner closes the display guard and observes
the original child using its pidfd and direct child status. Its diagnostic
reports a Start acknowledgement separately from desktop readiness.

Release `570d292bde4c` and memory `be86760f57d4` each verify 806 pinned
source inputs, build the entry with the same deliberately denying provider
and pass nine selected login/session CTests plus `login.ui_poll`. In both
builds the entry rejects arguments, Root, missing `NoNewPrivileges` and the
provider denial. Its ELF has no RPATH or RUNPATH. Native Linux CMake
rejects an enabled entry with a missing provider. The initial Windows CMake
probe reached the existing Linux-only guard before the provider check.

The registered VM stayed on boot `342ed7a6-bdd5-46e2-a047-fe6f2797ad8d`,
tty1, with enforcing policy SHA-256
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.
Credential transport and the compositor child are beyond the denial path.
The next VM case needs the real admitted backend, startup handoff and
disposable PAM account, followed by the visible session and original
entry/worker shutdown. No USB image was built.
