# Privileged unlock handoff

The original PAM session worker now owns the privileged end of a private
unlock connection in the opt-in backend profile. It passes the other end
at descriptor 3 to the exact authenticated desktop child. The desktop
keeps its ordinary UID and `NoNewPrivileges`. The socket is tied to the
original parent and child pidfds, and the privileged side verifies the
original cgroup and all kernel UID/GID values of the desktop before
accepting a request or sending its result.

The session worker uses its existing wait loop. A password attempt starts
one finite privileged child for the reviewed PAM transaction. The parent
matches the result to its original request and checks process exit. The
desktop still checks the cover, session and neutral-input requirements.
See the [wire contract](../../../spec/session.md#privileged-password-verification)
and the [backend build](../../../spec/login-backend.md#privileged-unlock-handoff).

## Build and runtime

The final Fedora Release workflow `f3a76847f6da` passes five selected
CTest cases and the separate privileged authority test. The latter
uses a wrapped PAM verdict to verify root execution of the finite child,
ordinary UID and `NoNewPrivileges` on the caller, refusal of the greeter
account, denial and acceptance, and a copied-endpoint sender rejection.

The graphical real-PAM case `a070ee80f14d` uses the original compositor,
dialog, broker and both sides of the C unlock channel. With
`NoNewPrivileges=1`, a wrong password yields a visible refusal. The
correct password restores exactly the same 1080p, 1440p and 4K frame
digests after Enter is released. The same application stays alive.
The simulated session loss discards the third queued privileged
result, keeps all outputs black, and closes the root connection.

The offline backend build `3f13d256c94b` checks all 2,612 prepared
files, builds and links the native archive, and passes 25 Rust unit
tests and one documentation test. The direct-exec, startup-only and
default profiles compile independently. The preparer passes 13 tests.
The exact Release backend (SHA-256 starts with `5b6373f85c77`) also
runs as the original PAM session worker in carrier fixture
`reauth-worker-xrrv0gxy`. A real private login selects the fixed desktop
path, which receives descriptor 3 as UID 1000, sets
`NoNewPrivileges=1`, and witnesses both wrong and correct password
outcomes. After SIGHUP, the original desktop becomes terminal and
the worker closes PAM and exits at zero. This fixture uses an ordinary
native probe at the private desktop path.

The same backend also passes `reauth-worker-pu6_ndc5` with an ordinary
fixture home. After two completed attempts, the controller holds the
original root verifier's pidfd during a third attempt and sends SIGHUP
to the original worker. Both the live verifier and the desktop stop.
The worker exits at zero with an empty diagnostic stream.

## Earlier attempts

The first C compilation caught a missing standard boolean header.
Two graphical attempts restored the original desktop but failed to
observe the delayed-result case. Their fixture had used the retained
parent pidfd as a readiness indicator for a password attempt. The
final test waits for the broker's active attempt deadline and then
for readability on the result socket.

The VM root filesystem was full during the first Rust staging attempt.
A private tmpfs in a separate mount namespace let the later build
export its evidence before unmounting. Existing VM artifacts stayed
in place. The first actual greetd probe used a deliberately absent home
and reported that it could not change directory.

Signed assembly still requires verified PAM and executable inputs,
dedicated policy decisions and a VM boot of the combined desktop and
lock path. The integrated suspend and wake test and the first-version
USB gate remain.

The source, build and test evidence is in
`local/login-review-2026-11-10/unlock-authority-2026-11-18-evidence.tar.gz`
with SHA-256
`b844422217b335847c5267453e187ec57604b074244072f53d930f05f94c55a2`.
