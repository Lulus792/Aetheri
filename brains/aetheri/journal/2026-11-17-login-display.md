# Private Wayland display channel

The combined native login and DRM build adds a one-client mode to the
compositor. It creates the Unix stream after the compositor has started
the DRM backend and has an output. The compositor hands the client endpoint
through an inherited unnamed seqpacket channel. The Wayland server exposes
no filesystem socket or clipboard global in this mode. The client closing
its endpoint ends the compositor. Capture paths and the desktop-shell
channel are disabled.

The client guard receives one descriptor with close-on-exec. It compares
the kernel peer pidfd object to the original launch pin, then verifies
current direct-child identity, the selected SELinux labels and the original
ELF. A wrong peer, malformed offer, extra descriptor or original child loss
fences the guard. The image test admits a Root-owned protected copy under the
carrier's ordinary user and verifies the actual running ELF. A foreign
stream from the right child's control channel still rejects.
[Linux Unix socket semantics](https://man7.org/linux/man-pages/man7/unix.7.html)

Release `d3bb907799e8` and memory `f48283d72de1` pass the original nine
login/session CTests and the separately selected `login.display` and
`compositor.drm_selection` tests. The protected copy in case
`5725b9cfc293` passes the same four descriptor cases with the real ELF
observer. The independent DRM probe in that case runs the selected
compositor as an ordinary user on a temporary systemd/PAM seat0/VT8
session. It compares the returned kernel peer pidfd and the live executable
to its own launched process, observes the absence of a pathname listener,
and completes two Wayland round trips. The original compositor holds
`/dev/dri/card0` and `/dev/input/event0` through `event2`. Closing the
sole client ends that child with status 0. The controller holds the original
entry's pidfd to exit status 0 and observes the inactive session-417 scope.
The guest returns to tty1 and the enforcing baseline digest is unchanged.

The earlier protected probe `ee5e4f7c4f09` found a test-lifetime race.
Its foreign-offer child could close its own endpoint and exit before the
test verified the expected signal. The fixture now keeps that connection
open until the original child is signaled. The compositor link initially
lacked the libinput dependency search paths; the guest build uses
`-rpath-link` to the existing protected runtime. Cases `40377ec7e529` and
`9d059e11b610` preserve the first probe script's syntax failure.
`796974350797` completed its Wayland requests and child end but the outer
observer tried to recover an already cleared MainPID. Separate administrative
observation verified terminal processes/scope and restored tty1. The final
probe pins the entry before allowing it to start the compositor.

The real DRM mode and guard are available for the next native greeter host.
The final probe used an ordinary administrative carrier session and sent no
credentials. Active greeter-seat policy, focus and rendered login view still
need to be joined to the existing protected backend before human login.
