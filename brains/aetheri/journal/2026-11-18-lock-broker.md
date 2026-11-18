# Desktop owner lock broker

The candidate session owner creates the managed compositor channel,
the private display stream and the credential channel. It binds the
broker to the two original children and to the account resolved from
the authenticated UID. The broker runs in the existing session event
loop and starts one finite PAM worker per submitted password.

Physical Super+L starts the cover immediately. The compositor first
notifies the owner of the new generation, then separately reports the
complete presentation of all covered outputs. The broker accepts the
password only from the original view in that generation. It samples
the current login context again before treating PAM success as a
release. The compositor retains its own presentation and neutral-input
checks. See the [session contract](../../../spec/session.md#desktop-owner-lock-broker).

## Native tests

The Release build `8587f85d6b5d` passes all 12 selected tests.
The sanitizer profile passes all four selected tests. The committed
broker regression pins two distinct children, consumes the genuine
cover messages and rejects a valid-shaped credential from a foreign
sender with the copied peer descriptor. Both channels then close.
The view regression verifies that masking and cover still work when
initiated through physical Super+L, and that the original application
receives none of the shortcut's L events.

Private carrier cases `6e32253de414` and `bb346b38dbb9` passed the
complete graphical flow with the C broker and Linux-PAM. They exercised a
rejected password, accepted reauthentication, the held Enter gate, and
identical restored frames on three outputs. In the final case the
fixture invalidated its session guard after the original worker became
terminal, before the broker consumed the result. All three screens
remained black. The ordinary application survived both covers.

The first final-source graphical attempts hit the ordinary fixture
client's 10-second poll timeout while the VM was rendering captures.
The ordinary and NNP attempts exited before reporting the planned
outcomes. The original CTest binaries and logs are preserved. A later
process could see the old stage directories but found the executable
missing, so a new private build recreates the graphical case from the
same sources. The controller now uses fixture input-sync messages
after the application has mapped to keep it live during long captures.
An earlier attempt sent a sync before the application received its
initial configure, leading to an xdg-shell error at startup.

## Installed rights boundary

The candidate code is opt-in. The installed session already sets
`NoNewPrivileges`, and the exact carrier `unix_chkpwd` is root-owned
and mode `04755`. That helper cannot gain UID 0 from an ordinary
session with the flag set. An installed unlock needs an independently
admitted verifier that already holds the narrow account-checking
authority, with the original session and account authenticated before
forwarding any attempt. The desktop retains its existing privilege
limit. Counter case `ee12beb0f6f2` sets and observes
`NoNewPrivs: 1` in the fixture. The user, private PAM profile and worker
are selected as in the ordinary case. The correct synthetic password
is refused, the dialog shows the retry state, and all three outputs
remain covered after the session fence. The build rejects enabling the
candidate in the signed-image profile pending the installed verifier
and its signed inputs. A Linux CMake configure with both options
explicitly selected reaches that guard.

The installed authority and jointly tested sleep and wake lifecycle
remain open. The first-version USB gate applies.

The build, VM and counter evidence is in
`local/login-review-2026-11-10/lock-broker-2026-11-18-evidence.tar.gz`
with SHA-256
`6ba29fab7b63101fc27d6639cec79d9aef04ad117f7bed76a015abdd419e9f31`.
