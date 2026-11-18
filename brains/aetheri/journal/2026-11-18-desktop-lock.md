# Compositor cover before sleep

The compositor now has an opt-in cover channel on an inherited unnamed
Unix socket. It puts application windows, dock, popups and desktop
background under one scene branch. Covering disables that branch and
retires the ordinary seat focus, active move/resize grab and cursor.
The compositor waits for matching presentation on every enrolled output
before it sends its one response. A lost controller leaves the cover up.
The normal installed entry still needs the lock client and unlock verifier.
See the [session contract](../../../spec/session.md#compositor-cover).

## Three-output Wayland test

Release build `f54319119195` uses the reviewed wlroots 0.20.2 SDK. Seven
compositor CTests pass. The fixture holds the original third output's
presentation and sees the channel stay silent. It delivers an older commit
sequence and sees the same result. Delivering the matching report releases
the acknowledgement. The client receives keyboard and pointer leave.
Releasing a held pointer button, moving or clicking, scrolling, typing and
replacing the physical-device fixtures produce no ordinary input delivery.
A new ordinary toplevel maps behind the cover after the channel closes.
Captures contain only black pixels at 1920×1080, 2560×1440 and 3840×2160.
The existing login display channel also compiles against the shared
unnamed-socket validator.

The first build lacked the selected libwacom and mtdev runtime libraries.
The corrected link uses the existing protected runtime. The nested
Wayland test fails at the client registry assertion on both the candidate
and unchanged `dceaf9e` source with the same dependency selection. The
first test build with the withheld report also caught an incorrect
assumption about the type of the timestamp in the wlroots presentation
event. Its fixture now copies the actual event by value.

## Virtual DRM

Carrier case `bf56b0c437ac` uses the Release compositor with SHA-256
`99f2c08a22fd7e607cafe447778bbbddc9d08cda8ed2d071c4fda723e1cbae4a`.
The fixture opens Wayland session `c3` on seat0/VT8. The original app
receives actual virtual keyboard input before the cover. QMP shows its
1280×800 content, then an all-black scanout after the response and again
after another virtual key. The original client sees keyboard leave and
completes a round trip with no post-cover key delivery.

The same compositor and client exit zero. The original unit waiter
reports success, the three pinned processes end, the original logind
session disappears and VT1 is restored. The carrier remains enforcing.
The first DRM controller waited for the wrong view's frame. The second
obtained both dark scanouts and clean actor exits, then queried the
transient unit after systemd had collected it. The final test uses the
unit waiter for exit evidence.

The VM proof covers device input and the visible cover. The password
prompt, authentication after wake, sleep integration and physical
display or device recovery remain work. The first-version USB gate applies.

Receipts, logs, controller sources and captures are in
`local/login-review-2026-11-10/desktop-lock-2026-11-18-evidence.tar.gz`
with SHA-256
`cb8475e6af2fe6a928714142f0adbdac20a3238b2b47fbdf0864da9386eeb5eb`.
