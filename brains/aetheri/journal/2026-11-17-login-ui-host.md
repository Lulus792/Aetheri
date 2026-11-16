# Native greeter window and input admission

The host consumes the private compositor offer once and creates the NexaUI
Wayland window from that exact descriptor. The pinned Wayland adapter
consumes `WAYLAND_SOCKET`. The host removes inherited named display and
Wayland debug choices, uses its own wiping native allocator and routes a
bounded set of events on the owner thread. The local session observer adds
an active `greeter` class and a named seat/VT check using its original self
pidfd. The ordinary post-login session observer keeps the `user` class.
[Wayland descriptor ownership](https://github.com/wayland-mirror/wayland/blob/main/src/wayland-client.c)

The host first submits a passive frame and observes keyboard focus. It binds
the interactive view only after the backend connection owner admits the
original handoff. The guarded readiness loop then drains native events
between the window poll and protocol pump. Focus, seat or display loss
interrupts the conversation and ends the host. A request deadline stays
fixed across partial I/O. A Start acknowledgement retires the local UI
and reports scheduling separately. The host owns its native window and
input storage. The deployment supervisor still owes a deadline for the
SDK's synchronous bootstrap calls.

VM cases `91305d9c9368` and `ba8e5bfce7ad` start the actual greetd/PAM
internal greeter account on seat0/VT8 with the approved DRM compositor. The
native fixture uses the production host and display guard. It substitutes
only the connection owner's backend admission to send real wire requests
to an isolated synthetic peer. The first case visibly focuses the account
field, submits the fixture username and password, and receives an explicit
Start ACK. The second shows a masked unsubmitted draft. An administrative
VT switch ends that session with `ESTALE` and the synthetic peer records
zero answers. Both original fixture chains retire and the VM returns to
tty1 with the exact enforcing baseline.

The first direct systemd PAM attempt `ad35e9a129a2` failed in its PAM step.
The first greetd attempt `150f66b2a32d` failed its locked-memory startup
before a greeter appeared. The final fixture uses the prior 64 MiB
LimitMEMLOCK setting and the private PAM projection in greetd's namespace.
The loss case uses the administrative switch after the injected chord did
not change the active VT. The retained screenshots show the account field
and masked password field in the test VM. The dedicated backend provider,
trusted installation and end-to-end PAM-to-desktop run remain pending.

Release `9843a0c93ccb` and memory `aba8515f96f7` pass nine selected
login/session tests, including the greeter-specific VT latch in
`linux.session`. Both pass `login.ui_poll` separately. The latter observes
zero bytes sent when native focus rejects the pending request, with a
successful-send control. The original five view/connection checks pass
against the integrated Release build in `117a1e171371`.
