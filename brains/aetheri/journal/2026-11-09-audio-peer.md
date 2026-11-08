# Bind audio clients to the original listener

Work extends `535d860`. Local evidence is in `local/audio-peer-2026-11-09`.
Normal desktop/audio admission remains closed; USB and the original carrier's
services, inputs and quarantined history stay protected.

## Change and boundary

The original connector checks a same-UID peer as a sanity condition. A real
same-UID impostor can satisfy it. The new strict alternative receives an
independently admitted original server pidfd and duplicates it for the bounded
call. It joins `SO_PEERPIDFD` from the connected socket to that original pidfs
object, with mandatory PIDFD_GET_INFO, UID/GID/cgroup snapshots and live polls.
No PID is used to select/open a replacement process. No protocol bytes precede
the join; unsupported/missing observations close the connection without fallback.

The C control adapter has an explicit strict open. Endpoint failure produces an
empty DISCONNECTED/ENDPOINT observation. The private PAM client passes its
original server pin through fixed inherited FD3 to both native control and finite
playback. Other nonstandard FDs are closed; each child closes its inherited pin
after the synchronous join. The parent keeps the original session/lifetime
observer. This is not normal app-FD transport or an authority/grant API.

Caller admission of current image, unit/configuration, exec/delegation exclusion
and ongoing lifetime remains necessary. Socket activation identifies the original
listener creator, which need not be its accepting service. Such a different
creator is rejected by this profile rather than adopted. An established FD stays
at its original endpoint across path replacement. Neither that FD nor a pidfd
proves every later holder, authorizes a run or settles a lease.

## Evidence so far

The new native test uses real AF_UNIX listeners, child processes and kernel
pidfds. It verifies the original live listener, repeated same-UID pathname
takeover rejection without application bytes, continued use of the original
connected FD, stale original refusal, invalid-FD refusal, explicit separately
selected peer admission, and inherited-listener rejection. A same-UID-only
countercheck actually reaches both impostor and inherited listeners. A controlled
ENOPROTOOPT countercheck fails closed. Final FD count and exact original child
retirement are checked; no broad kernel/SELinux permission is added.

Build v1 passes this group plus the existing session guard and barrier groups
in Release and ASan/UBSan. Helper 1277557, token
`494de13a814d42a5b5b0b3c86c5bf24e`, exports 459 independently verified members
and retires with exit 0. Bundle SHA256
`c22704357f42d8a93afa017b4e953a794bda02f8b72891cac2cb0f0f92ef9163`.

The 81 host contract cases now require both native-client peer-join markers
before control/playback acceptance. Build v2 additionally exercises the C adapter
against a real server with a deliberately wrong self pin: it must retire with
ENDPOINT and never fall back. The original four audio CTests, management/probe
graphs and parser groups repeat in both modes. Final runtime results are pending.

## Final evidence

Build v2 passes all three native groups, the four original audio CTests and
twelve parser controls in both Release and ASan/UBSan. All fourteen private
management/probe graphs retain complete 48,000-frame PCM or actual zero mute.
The two new adapter-rejection graphs report exactly CLOSED followed by
ORIGINAL_REJECTED, while their actual server and independent reference playback
remain healthy. The actual peer test also verifies CLOEXEC/nonblocking socket
flags and no leaked temporary handles. No new policy grant is installed.

Helper 1279175, token `ccec0135fc6146c4ae38c52fe85cc11d`, exports 1,011 verified
members and retires with exit 0. Bundle SHA256
`a55f4b84a007b667777492cb72a8365f14f1ce99c4e2077e97fd0f5a181b78e8`.
The independent audit joins both exports (1,470 members), both exact helper
retirements, the sealed v2 source and 562 unchanged code/pin inputs plus the two
explicit host-only review revisions. Actual privileged-group rejection remains
before connection. Preservation checks 445 original sources, 19 protected
hashes, 31 frozen SQL/XML, 52 normal P0 files and all four foreign files.

All six original VM processes exit 0 after observed poweroff. They keep fresh
NVRAM/UUIDs, host-local AF_UNIX QMP, two virtual CPUs and the explicit functional
instruction-clock profile. No network, shared directory, hardware audio, disk
or USB is used. Both checked native-client markers precede acceptance/playback.

| Cold boot | Captured frames | Signal start | Result |
| --- | ---: | ---: | --- |
| Checked user-session playback | 133,504 | 15,712 | All 96,000 frames at cubic 50% gain |
| VT loss and return | 135,904 | 15,712 | Full reference, original end, no restart |
| Original graph exit | 134,464 | 16,736 | Full reference and original end |
| Original policy exit | 134,464 | 16,736 | Full reference and original end |
| Legacy native control | 132,064 | 14,688 | Full reference with unchanged old profile |
| No sound card | — | — | Gate rejects before clients |

Maximum error is 0.49795693 LSB in all five complete captures; unchanged
acceptance is two LSB. No samples or headers are repaired.

Final review finds two incomplete-trace cases where the new marker ordering
could index a missing control actor or native result. Their counterexamples
raise under the executed verifier. Check those prerequisites first; both now
return false. All six VM attempts finish before the two host files change.
The executed 81-case source copies/hashes and receipts are retained. The
83-case reviewed verifier revalidates their complete original serial/QMP/PCM
evidence in `acceptance-final.json`, without rewriting execution receipts or
claiming the reviewed predicate executed inside the original controller.

Original manager authentication/lifetime, complete current image/configuration/
unit and endpoint policy, exec/FD delegation fencing, app MAC, general holder
drain, normal desktop/Settings and HP/Bluetooth audio remain work. Default-clock
APIC failure and real-time/physical performance are not cleared by instruction
counting. The full OS goal stays active; USB and normal authority remain unchanged.

## References

- Linux [Unix credentials](https://man7.org/linux/man-pages/man7/unix.7.html):
  credentials are captured at connect/listen/socketpair, not inferred from the
  current pathname or accepting process.
- Linux 6.19 [socket implementation](https://raw.githubusercontent.com/torvalds/linux/v6.19/net/core/sock.c)
  returns a pidfd from the socket's retained peer pid; an AF_UNIX stale handle
  is possible. This profile separately rejects exited processes.
- [PIDFD_GET_INFO ABI](https://raw.githubusercontent.com/torvalds/linux/v6.19/include/uapi/linux/pidfd.h):
  requested information needs returned-mask checks and can become stale after
  an observation. The accepted x86-64 pidfs kernel is a prerequisite, not a
  durable volume/process authorization claim.
