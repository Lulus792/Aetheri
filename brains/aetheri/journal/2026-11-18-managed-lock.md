# Restore the original desktop after PAM

The optional managed compositor channel checks every command against the
original parent process descriptor. The compositor numbers covers within
that channel. A release requires the current covered generation and
completed presentation on every output. It waits for physical keyboard and
pointer release before restoring app focus. A closed or invalid channel
leaves the current cover in place.
See the [wire contract](../../../spec/session.md#managed-compositor-lock).

## Process, input and image checks

The final Release build `aa018293d4` passes all ten selected compositor
and unlock tests. The native test holds the third output's first cover
report, attempts an early release and then supplies the original report.
It rejects a stale generation and waits while either a key or a pointer
button is held. When both are up, a new key reaches the original window.
During another cover a process with a copied descriptor tries to send a
release while Enter is held. The channel closes and all three outputs stay
black even after releasing Enter and the pointer button. The test also
covers ordinary input and display operation before the lock.

The ASan/UBSan build `95df69f669` passes four selected tests, including
both compositor input profiles and both unlock tests. The nested
Wayland test remains outside this run following its recorded baseline
failure. Earlier Release runs each passed nine of ten selected tests.
The first timed out during ordinary desktop focus restoration. The
next passed that check, then its clipboard server exited before readiness.
The final workflow uses shorter temporary paths and captures evidence
before the SSH session ends. The initial remote directories had already
vanished at recheck. The two failure causes remain unobserved.

## Same application after real PAM

Fixture `b5b2e34b6d57` uses the Release compositor, Wayland client and
native unlock worker inside the development VM. The headless compositor
uses three virtual outputs. The unprivileged fixture controller has a
private PAM and account view with synthetic credentials. Each attempt
uses a new worker process without publishing a socket.

An incorrect password leaves every byte of all three captured frames black.
The next, correct password produces an accepted result and the controller
sends release for generation 1. Full-frame SHA-256 matches before and
after on 1920×1080, 2560×1440 and 3840×2160. App PID 27857 and compositor
PID 27856 remain original. The app maps each window once and both actors
exit normally. A replay of generation 1 during generation 2 is refused.
Each frame remains identical to the earlier full-black capture.

The fixture controls credential input and the lock broker. The signed
desktop needs its admitted owner, protected password dialog and combined
sleep and wake test. Its physical hardware and USB release gates remain.

Build and integration evidence is in
`local/login-review-2026-11-10/managed-lock-2026-11-18-evidence.tar.gz`
with SHA-256
`06870300fc5623806a73ba5d8a4b2ddbeefe34e621a81f70a3754b1c3d775a41`.
