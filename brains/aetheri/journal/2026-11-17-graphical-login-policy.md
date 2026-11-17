# Dedicated graphical greeter policy

`policy/login-graphical.cil` splits the display-owning child from the
credential view. The greeter can execute and observe the selected
compositor and terminate its original child. The compositor receives
DRM and input descriptors through logind. The policy denies direct opens
of those device types and keeps the login socket, protected handoff, shadow
and ptrace outside the compositor's access.

Case `78d550754bf1` shows the native account form, real PAM password
prompt and authenticated Start action with the dedicated backend, worker,
greeter, display and startup roles. While that greeter was visible, its
original child held `/dev/dri/card0` and three `/dev/input/event*`
identities. The greeter held none of those device endpoints. The
startup journal confirms publication and the worker audit records
`PAM:authentication` success. The original controller expired before
the stop marker arrived. After checking the same backend invocation and
executable, a separate administrative stop ended the service. That stop
does not restore the lost controller pins.

Fresh case `cd81c2dc1d7e` reruns protected publication and the graphical
authentication with the same final policy. The account, prompt and
authenticated frames match the inspected images byte for byte. The
administrative stop before Start observes the original compositor,
greeter, internal PAM worker and backend exit zero. The internal PAM
session closes, the greeter records `start_ack=0` and the local seat
clears. The controller checks the host account bytes and restores the
original tty8 label after returning to tty1.

The test selects private typed tmpfs directories for `/dev/shm` and
`/tmp`. wlroots creates keyboard maps in the first directory. The pinned
NexaUI software path creates its pixel file under the second. Case
`f160e518ddf0` identifies the received pixel FD denial at
`wl_shm.create_pool`. libwayland 1.26.0 maps a passed client buffer shared
read/write. The final policy permits that mapping only for the greeter's
pixel type and fences direct opens, creations and unlinks for the compositor.
The NexaUI P04 catalogue now records the backing-placement requirement.
[Wayland shared-memory server](https://raw.githubusercontent.com/wayland-mirror/wayland/1.26.0/src/wayland-shm.c#L334)

Earlier cases record a stale tty8 label (`0187b53f884c`), a runfile
outside the writable runtime directory (`967653714b82`) and failed startup
readiness or backend loss (`01315fc3d3be`, `e5f6d01341ae`, `16232c35035b`).
Case `907ce34b2cdf` reaches DRM modesetting, then reports missing
keyboard-map backing. Case `7a14a4794fe6` adds the private keymap mount
and still cannot produce a frame. Case `de5cd022d04c` hits a concurrent
policy-file read before launch. Retain the previous failed holders and
original outputs.

SELinux replaces the greeter's inherited TTY stderr with the null device
under this profile. The native entry opens the existing journal stream
in that case. Case `16232c35035b` shows that the carrier's early receiver
still has the kernel domain. The narrow compatibility rule stays in
the disposable carrier supplement. The final greeter and compositor
diagnostics reach the journal without granting direct TTY access.

Release `259185c38d35` and memory `9ac41472e0c2` each pass nine
login/session tests plus `login.ui_poll`. The read-only checker observes
258 effective decisions for candidate `fe06776a787a`. The strict build
`be7af98d33fb` compiles the production candidate over 426 inputs and
rejects ten forbidden permission additions. A wrong loaded digest is
rejected. Candidate modules were retired only after their typed tasks
and seats ended. The final removal restored the baseline enforcing bytes
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.
The dedicated graphical test uses a disposable provider and
stops before Start. The full user-session transition under this graphical
policy, normal deployment and physical release remain open.
