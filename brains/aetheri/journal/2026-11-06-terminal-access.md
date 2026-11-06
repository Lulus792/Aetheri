# Native Terminal accessibility on the original SDL host

6 November 2026. Scoped native Terminal transport checkpoint.

## Boundary

The optional NexaUI Terminal uses the existing pinned 0.1.18
`linux_accessibility_host.h` interface. The original SDL window, Wayland
connection/event dispatch, PTY/session, shell/editor and clipboard/IME remain.
The UI thread publishes completed context snapshots with native focus and
window visibility. Screen position is unavailable rather than guessed from the
test compositor. No second window or permanent worker is created.

The original readiness worker additionally polls the native bridge descriptor
and its deadlines. The UI thread alone pumps/routes/publishes/destroys the
bridge. A mutex protects duplicated CLOEXEC readiness descriptors: reconnect
cannot close/reuse an object while the worker is still polling it. Worker
copies close before acknowledgement; the worker joins before host retirement.
This is readiness, not permission, app identity or process-drain evidence.

Visible row snapshots retain their existing original content/caret boundaries.
They expose read-only text, never shell editing or file permissions. A native
press-only localized Close invokes the existing `user_close` path; it does not
replace editor save/drop/back or original child waits. Native focus is observed,
not inferred from an accessibility request. Human screenreader tasks, complete
terminal selection/caret/event/geometry, review/IME semantics and normal session
integration remain separate work.

## Checks so far

The independent C17/libatspi client links neither NexaUI nor the terminal model.
It uses the existing verified private bus/registry fixture and actual compositor.
It reads real Unicode shell output, checks character counts/read-only state and
window-relative bounds, rejects text/caret mutation, observes native focus loss
and return, reconnects to the same original app after private bus replacement,
invokes localized Close and observes a held text object become defunct.
A disabled-bridge native window is rejected by the same client.

Initial test failures are retained: the first client queried the application
name rather than the synthetic FRAME that owns native active state. The second
expected status zero from a registry deliberately terminated by SIGTERM; its
actual status is -15. Those are corrected test expectations, not SDK defects.
The focused Release case then passed in 7.76 seconds.

The first full suite is `/home/aetheri/build/terminal-access-final-i5l0yd_0`.
Its ordinary SDL `terminal.clipboard` case currently exposes an existing
test-driver freshness assumption: `paste_review()` waits for a VIEW log but
does not await the matching native frame callback before Ctrl+Enter. The app
retains the review without dispatching the command. The exact failing event's
timestamp/modifiers were not traced, so the precise rejection cause is not
established. Preserve the existing input fence and first failure; require a
matching current-or-later DRAWN observation before subsequent confirmation.
The first full Release suite ends 73/74 in 298.86 seconds and does not start
its sanitizer phase. It is not reclassified as passing.

The subsequent source also refreshes native focus/visibility metadata from the
completed context without requiring a repaint. The extra ordinary SDL focus-
gain redraw was removed; its prior behavior is retained. The native test now
awaits the actual returned shell prompt after Unicode output before querying
that text. The final suite is `/home/aetheri/build/terminal-access-verified-dvsqanhi`.
Release passes all 74 groups in 310.01 seconds, with no skips. Its sanitizer
suite ends 73/74 in 539.71 seconds: the new native case discovers a FRAME after
bus replacement but its held reference does not return ACTIVE. The exact first
failure's wire reply was not captured; a stale reference versus a live wrong
state is not established by that historical result.

That failure also exposes test cleanup closing the compositor control socket
before retiring the Terminal. Its fatal Wayland disconnect precedes an actual
288-byte/three-proxy LeakSanitizer report in SDL wake/hide calls. The original
log is retained without suppression. The fixture now retires/waits its children
before closing that socket, and tests the held original shell pidfd directly
rather than the last peer's pidfd. The diagnostic sanitizer case passes 9.17s.

A separate wrong-focus control then requires rejection while the actual live
window is unfocused: diagnostic DEFUNCT=0, visible/showing=1, exit1. The full
native sequence passes 12.08s with that control. The reader now permits bounded
rediscovery only after an explicitly defunct FRAME, matching the SDK's renewed
native identities after reconnect. The same original deadline and exact live
ACTIVE requirement remain; this is not a repair claim for the first untraced
wire reply or a reason to accept wrong focus. The scoped case passes 11.99s.
The final whole-source suite is
`/home/aetheri/build/terminal-access-adoption-y3qo6o3l`: all 74 app/UI groups pass
Release in 321.31s and ASan/UBSan in 507.07s, without skips. The user paused the
goal while this existing run was active; it continued to completion. On resume,
the same original host handle was observed terminal, never restarted.

The first post-run audit joins 375 source inputs, 210 exports/90 native images,
28 unchanged frozen SQL/XML contracts and nineteen protected hashes without
sidecars. Both actual new native samples record zero CPU ticks/new frames across
two idle seconds. The enforcing guest is idle on tty1 without build/registry
actors, Aetheri units or seat0 session; the old quarantined runtime and exact
closed development record remain unchanged. No new DRM session was started.

Final review removed a test-only diagnostic's dereference of optional retired
object metadata. It changes no application/SDK source or acceptance condition;
the independent native client is rechecked separately in Release and ASan/UBSan
at `/home/aetheri/build/terminal-access-diagnostic-final-drus6xzj`. That local
follow-up wrapper skips the app rebuild (its generic build-success label is a
no-op); each native case still compiles and runs the actual independent C peer.
Both final native cases pass, Release in 10.97s and ASan/UBSan in 12.01s. Each
records the same application bytes as its corresponding complete-suite run.
The updated audit exports 270 records, retaining 90 full-suite native images
plus final-client logs/results and the original failed clipboard/reconnect/leak
logs. All 375 host/snapshot/guest inputs match; protected/frozen/closed state and
the enforcing idle carrier remain unchanged. The SDK and default choices stay.

The existing NexaUI catalogue receives eleven native-transport evidence lines
only, preserving its IDs/surface groups and closed handoff. No new chat message,
SDK implementation, pin change or other-project publication is implied.

The two Release native captures were visually inspected: real command/output,
returned prompt and German footer are present. The selected font still draws a
missing-glyph box for the CJK character even though the native text read is exact.
This does not close font coverage or final visual/human assistive acceptance.
