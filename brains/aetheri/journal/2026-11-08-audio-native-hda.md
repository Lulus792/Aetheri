# Native controls on discovered HDA

This extends `c24c5ae`: the C17 management adapter now controls the actual
udev/ACP-discovered output in a separate diskless VM. The finite control client
observes volume/mute, closes from its change callback and exits before playback.
The supervisor joins its output name/ID/serial to independent inspection. Volume
and mute writes use the library, not `wpctl`; profile/link inspection remains a
bounded CLI oracle. Default switching is tested on private graphs, not on HDA.
See the [contract and reproduction](../../../spec/audio-control.md).

No new dependency family, normal service, authority/catalogue capability or USB
image is introduced. The user's HP test documents remain on the unchanged stick.

## Corrections and rejected evidence

The new callback-time close probe exposes a lifetime boundary missed by the
earlier command-driven close test. Preserve the local attempts:

- v1 completes native unity but reports an upstream mixer sync warning at close.
  Disable cache subscriptions and use a final same-connection barrier.
- v2 then crashes after the retirement notification; v3 repeats with ASan and
  an unresolved instruction address. That alone does not prove module unload.
- v4 defers final destruction, but ASan now identifies `on_remote_data` in the
  pinned protocol-native module, reached through `wp_loop_source_dispatch`.
  Disconnecting inside its sync completion invalidates the active read dispatch.
- v5 also defers the disconnect itself to an idle turn. All three sanitized
  close probes pass. Its Release half-volume FIFO capture nevertheless replaces
  source frames 37,888–38,911 with zero. Later samples keep their original phase.
  The strict verifier rejects this despite the producer's local DRAINED report.
- The finite non-RT producer now requests `EARLY_PROCESS`, filling free buffers
  early on its existing main loop. Prepared data, the 1024-frame clock and all
  sample limits remain unchanged. v7 passes both native suites; a separate
  regression passes all four original audio CTest groups in both variants.
  This is no guarantee against future TCG stalls or production underruns.

Close now disables observer work, cancels owned observation and waits at most
five seconds for a final barrier. It disconnects and destroys objects outside
the protocol callback. The caller continues its context until data retirement.
The final suite also closes while the original private server is still paused;
only afterward does the harness resume that server. Neither close nor UNKNOWN
undoes the earlier submitted command or authorizes replay.

v6 builds before running the graph suites, but export exhausts its private 1 GiB
workspace. It produces no verified bundle; none of its results is accepted.
Read-only inspection finds the original helper absent, no matching actors and
two empty root-owned control/workspace directories. Preserve those directories.
Later exports use the already-private `/dev/shm` mount with exclusive creation,
then the same complete hash/member verification and exact acknowledgement.
No global temporary directory is cleaned and no failed dispatch is replayed.

The first HDA half-volume attempt in v7 has correct native 0.5 readback and
changes the actual hardware master from 74 to 56. Its captured amplitude is
about 0.75295 of unity, so the ideal cubic-gain check fails. QEMU 11.1 advertises
dB steps but applies `floor(step * 255 / 74) / 255` linearly; step 56 yields
192/255, matching the observed mismatch. Do not make that capture pass by
changing sample tolerances or silently redefining 50% volume.

The native image instead enables maintained software volume/mute for only the
fixed `alsa_card.pci-0000_00_01.0`. v8 exposes a second fixture prerequisite:
software control leaves the cold-boot hardware master at 0/muted. That guest
passes local control readback but fails independent acceptance with silence.
v9 initializes the independently checked hardware master once to 74/unmuted
before ordinary actors start, then verifies that it stays unchanged. The live
card must expose `api.alsa.soft-mixer = true`. QEMU's mixer remains enabled.
The nested legacy device/manual images retain hardware-mixer behavior.
This rule and initialization are test infrastructure, not physical-card policy.

## Executed native evidence

All builds and private graph tests use ordinary UID/GID1000, empty supplementary
groups and NoNewPrivileges inside fresh private mounts of the existing enforcing
carrier, boot `124d6ce8-6ff6-4ed8-a8c8-51aeaa87f056`. No packages or modules are
installed into that carrier. Only Aetheri code is sanitizer-instrumented; this
does not establish upstream library security or sanitizer coverage.

Evidence stays in `local/audio-native-hda-2026-11-08`:

| Verified run | Original helper | Bundle SHA256 |
| --- | ---: | --- |
| v7 probes/control graphs | 1246294 | `1e7939c22f6aef4631cce42fa8c278f1c28c8b902431526f0bf9cf01062b3151` |
| Transport/parser regression | 1248101 | `63ddc8dbf142e700a61665f7c7036099ac11b864c94f632ed4b46e311e0cfafd` |
| Final v9 images/parser controls | 1250134 | `5d9e04f57b20ec7ce42c5ebb803d81f38ad769a1a584dedb6a9fcb3982f080aa` |
| Paused-server close regression | 1251617 | `df7e61d5e3b834ea40f088ab5719dec2911e46e2c4e6207a46750ef9c3cdc3b7` |

Release and ASan/UBSan each pass three finite callback-close probes with complete
48,000-frame FIFO captures at unity/cubic 50% gain or actual zero mute. The
existing native-control suite also passes all three graphs in both variants:
default selection, invalid/stale/foreign references, unsafe names, mute-preserving
volume, missing explicit endpoints, immediate close, wrong-thread rejection,
UNKNOWN on timeout, close while paused, fresh-observer separation, server loss
and original-handle cleanup after injected failures. Native stderr stays empty.
All four original audio CTest groups and twelve pure device-parser controls pass
in both variants. The final HDA contract suite has 48 passing cases, including
wrong gain, nonzero/short mute, foreign output identity, absent close, wrong mixer
metadata/live property and missing/late hardware baseline. Parser tests never
call the included privileged fixture entry.

## Final cold boots

Each v9 attempt has fresh NVRAM, an original QEMU process and one host-local
AF_UNIX QMP connection. All original processes exit 0 after actual guest poweroff
and explicit capture retirement. No guest has a NIC, disk, shared directory or
host-speaker output. No capture header or rejected sample is repaired.

| Attempt | Captured frames | Signal start | Result |
| --- | ---: | ---: | --- |
| Native 50% | 182,161 | 62,952 | All 96,000 frames at cubic gain 0.125 |
| Native unity | 187,592 | 66,560 | All 96,000 frames |
| Native mute | 185,391 | — | Every sample zero |
| Native unity, second cold boot | 194,315 | 74,697 | All 96,000 frames |
| Legacy discovered-device control | 223,694 | 104,639 | All 96,000 frames |
| Manual HDA control | 135,103 | 16,384 | All 96,000 frames |
| No card | — | — | Device gate rejects before actor startup |

All non-muted captures have maximum error below 0.5 LSB, within the unchanged
two-LSB limit. Mute proves captured silence with sufficient duration and checked
linked/local playback evidence; silence alone cannot identify a signal's phase.

The independent final audit checks 555 current code/pin inputs and all 2,078
members across the four final evidence bundles, twelve controller actor snapshots,
six probe captures, six management captures and every final HDA capture. All ten
acknowledged native helpers have retired through their original export protocol;
the failed v6 export retains its separate empty-directory audit. Read-only
preservation checks 445 original carrier source inputs, 19 protected hashes,
31 frozen SQL/XML and 52 normal P0 files. The original authority and CLOSED DRM
history remain unchanged, as do the foreign PROJECT/QUESTIONS/START/UI files.

## Boundaries and next work

The fixed direct UID1000 udev ownership rule retains its upstream deprecation.
The RAM guest has no production SELinux policy or login/session admission.
Neither this device test nor the observer barrier proves application/device
drain, production revocation, physical audibility or normal lifecycle recovery.

Next implement per-user supervision and mediated device access, then Settings
and HP headphone output. Review/update the legacy Lua runtime before normal
rollout. Bluetooth, recording, hotplug, physical multi-device switching, suspend
and real-time scheduling need separate acceptance. The full OS goal stays active;
NexaUI handoff remains closed.
