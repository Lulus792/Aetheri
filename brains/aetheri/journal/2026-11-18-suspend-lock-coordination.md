# Suspend and resume lock coordination

The original desktop owner can prepare the lock for a power transition,
query its readiness, and finish the hold when the operation returns or is
cancelled. The compositor replaces any previous cover or pending release
with a fresh generation. It hides the password dialog and confirms the new
black frame on every enrolled output. A queued physical lock request can
precede the sleep acknowledgement on the same channel.

The original dialog clears its draft and pending state. It confirms the
Wayland focus leave after a display sync, then acknowledges the pause on
the ordered credential connection. The owner discards queued submissions
and consumes any outstanding privileged PAM result against its old
generation without acting on the verdict. It checks that the original
session, compositor, dialog and peer sockets remain live before reporting
readiness.

The wake transition requires another set of freshly presented black frames.
The dialog resumes accepting input after that acknowledgement and after
the pre-sleep PAM attempt settles. It displays a blank password field and
still needs the normal successful password and neutral-input checks to
release the original desktop. See the [session contract](../../../spec/session.md#suspend-and-resume-lock-coordination).

## Verification

Release `c1a694200ba6` passes eight selected CTests and the separate
privileged authority test with the installed Sound and lock build options.
The native compositor fixture with 1080p, 1440p and 4K outputs withholds
one real presentation in each power transition, delivers a stale sequence
and verifies that each acknowledgement waits for its own frame. It checks
the blocked pending release, cleared draft, restored window focus, and
physical-lock request crossing sleep preparation. The broker fixture
checks a queued credential before the original dialog's pause reply, a
premature finish request and release fencing after loss of identity.

Carrier case `420878c47851` runs the real compositor, Wayland dialog and
privileged Linux-PAM verifier. The desktop has `NoNewPrivileges=1`. The
controller observes an accepted PAM reply queued from the original parent
before requesting sleep preparation. The three outputs stay black during
the hold. The simulated wake presents a fresh dialog with an empty field.
Only a new successful PAM attempt restores the exact original frame digests
at all three resolutions in the original app. The carrier observes four
finite privileged verifiers in the complete run. A subsequent session-loss
test preserves all three black frames. The foreground actors exit normally.

The first run exposed that the test driver retained its presentation hold
after its first release. The driver now clears the consumed capture and
can retain a later presentation when the cover is superseded. One parallel
build filled the carrier's shared temporary filesystem during a screenshot.
The serial Release run repeated the same checks successfully. One later
real-PAM run failed during the final screenshot. The final accepted-reply
test uses a separate private tmpfs and preserves the original failure.

The native output, actor logs and controllers are retained in
`local/login-review-2026-11-10/suspend-lock-coordination-2026-11-18-evidence.tar.gz`
with SHA-256
`cb1a4f7f686b8e544f2b39c22e0dac20711c593fa5094feaaa1feb5025350800`.

## Installed suspend path

The installed suspend service still needs a privileged caller for the
transaction. The [systemd inhibitor contract](https://systemd.io/INHIBITOR_LOCKS/)
bounds delay locks. The normal [suspend target](https://github.com/systemd/systemd/blob/v259.10/units/suspend.target)
requires [systemd-suspend.service](https://github.com/systemd/systemd/blob/v259.10/units/systemd-suspend.service.in).
Connect preparation as a required condition before the sleep helper and
hold the original transaction through its outcome. Then test the actual
kernel sleep and wake in a freshly signed VM image and on physical target
hardware. The USB gate remains.
