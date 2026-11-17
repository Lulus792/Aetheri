# Graphical greeter stop ordering

The connection poller originally fetched its descriptors through the same
admission path as command and stream operations. An error in that lookup
returned before the native loop could observe an already readable stop
descriptor. During backend shutdown, a running greeter can therefore report
an admission error before handling the worker's stop notification.

`ae_login_connection_watch` now provides the original descriptors and
interest mask from local state. It reports an existing permanent fence.
The native loop can observe stop and focus before transport work.
Command preparation and pump still recheck the exact peer, deployment and
handoff through their existing gates. A pending operation fences without
sending its queued bytes if the local stop or backend gate denies progress.

The targeted `login.ui_poll` regression fails on the pre-change code
(`354a7bdf148e`), where the native poll is skipped. The corrected Release
(`e579ff85ff0c`) and memory (`e9ed9bf98c92`) builds each pass nine selected
login/session CTests plus `login.ui_poll`. That test exercises local stop
against both `EAGAIN` and `ESTALE` and independently verifies that an
`EAGAIN` backend rejection without local stop still fences at the pump and
sends zero bytes. Existing focus loss and successful-send controls pass.

The original VM idle-stop repro `d533c96bc2dd` reaches the empty account
frame and returns `ESTALE` with greeter exit 1. Its diagnostic parser sees
no populated unit Job on that run. The precise property source for the
historical `EAGAIN` in `58a9b0d7f26d` remains unobserved. Two earlier
wrapper probes (`45fe6b042319`, `04487ccdaf35`) disturbed the D-Bus
read cursor and did not reach stop. Case `204f7bb9a24f` used the wrong
`systemctl show -P Job` empty-value expectation. Preserve all three
failed preparations and their outputs.

The corrected idle-stop case `65600c933018` displays the empty form,
then stops via PID1. The original compositor, greeter, PAM worker and
backend all exit zero and the worker records PAM closure.
The corrected full case `8904a560d0b0` uses the real PAM prompt and
native keyboard input, confirms Start acknowledgement and desktop readiness
independently, ends the user session and reaches a fresh empty greeter.
All five captured frames match the previously inspected frames byte for
byte. The original user entry, PAM workers and replacement display chain
end normally. The replacement greeter reports `start_ack=0` for the
administrative stop. The controller verifies the host account files,
retirement of the isolated credential projection and terminal local seat.
The guest is back at tty1 with the original enforcing policy SHA-256
`a1c128d0e25d64cac9cdc811b5857852ba310b4313813c0762367f3024b1e4f6`.

Both corrected VM cases use the administrative test role. Dedicated
graphical confinement, normal deployment and physical release remain open.
