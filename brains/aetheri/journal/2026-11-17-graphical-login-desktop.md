# Graphical PAM login and desktop in the VM

Case `58a9b0d7f26d` starts the native `aetheri-login-greeter` under the
original greetd service and separately publishes its protected handoff. The
ordinary greeter owns the private DRM/Wayland compositor on seat0/VT8.
The host checks the selected greeter and compositor images before sending
keyboard input. The account form, actual PAM password prompt, authenticated
Start button and 1280×800 desktop were visually checked. The desktop PNG
SHA-256 is `7ba13673a9cf1fdb6f1a6fe995bc202451db03518a559367b3955cd2367d197c`.

The actual `pam_unix` authentication uses a synthetic password in a private
tmpfs `/etc` projection, with the PAM and session-entry files bound inside
the service namespace. The effective policy was enforcing throughout. The
provider used the existing administrative `unconfined_service_t` test role.
Dedicated graphical policy and normal installation still need verification.

The first greeter records `AETHERI_GREETER_END start_ack=1`. Its original
compositor, greeter and PAM worker exit zero, with the greeter PAM close
recorded. The selected user entry reports desktop readiness independently.
After the screenshot, the controller uses the original entry pidfd to end
the session. The original entry and worker exit zero, the journal records
`AETHERI_DESKTOP_END status=ok` and the user PAM session closes. The user
scope goes inactive and an independently observed replacement greeter shows
the empty account form.

The replacement is visible before the administrative stop. Its compositor
and PAM worker exit zero, and the original backend goes inactive with no
main PID, control PID or cgroup. The replacement greeter exits 1 and records
`AETHERI_GREETER_FAIL error=-11`. The current service guard can return
`EAGAIN` when PID1 has a nonempty job. That is a candidate explanation
requiring a stage-specific check of this stop sequence. The live transport
fence stays intact. The controller did not mark the case complete. After
checking the original test unit was terminal and no seat0 session remained,
administrative observation returned the guest to tty1. The loaded enforcing
policy SHA-256 remains
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.

The first case `f1ad6a3e8557` fails in systemd namespace setup on a direct
file bind to `/etc/shadow`. The next case `6177dcdd4dd7` uses the
directory projection, reaches the visible desktop and completes the user
PAM session. Stopping the replacement at first exec records exit 1 for
both its compositor and greeter. This motivated separate verification of the
replacement frame and greeter diagnostics. Keep the original test inputs
and stopped case records in place.

The greeter redirects only its stderr from an attached VT to the local
journal if available. Release `d0102077421a` and memory `9f98934edbbd`
each pass nine selected login/session CTests and `login.ui_poll`, plus
the argument, provider and missing-`NoNewPrivileges` refusals.
