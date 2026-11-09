# Original user-manager observation

The private audio session previously kept the user manager's unique bus name
without joining its bus process to PID 1's original user-manager invocation.
Add a read-only native observer and require it before fixture effects. Existing
audio/session and normal authority contracts remain separate.

## Implementation and current evidence

`src/audio/manager.c` joins bus ProcessFD/EUID/PID, live pidfs identity, current
kernel credentials, PID 1 main/invocation/job observations, the exact manager
`init.scope` and independently selected systemd ELF object. No numeric process
opening or missing-evidence fallback is used. The private PAM fixture retains
the original destination and rechecks before effects and during playback.

Native tests redirect only the included subject's bus/PID1/cgroup calls. They
retain actual bus objects, pidfds, child exit, current executable observations
and FD accounting. They do not establish actual PID 1/credential provenance;
the separate cold-boot image tests the real integration.

Isolated original-carrier build/export attempts are retained under
`local/audio-manager-2026-11-09`:

- v1 rejects a test compile: glibc's pidfd structure is defined twice when the
  included subject and test both include the header. Move the model ioctl body
  after the subject and let the subject include the header once.
- v2 compiles but CTest cannot run absent `dbus-run-session`. Use existing
  read-only system-bus clients for message construction/IDs; all subject
  owner/credential/PID 1 operations stay locally redirected. No package install,
  host bus-name acquisition, unit mutation or policy edit is needed.
- v3 passes four native groups in Release and ASan/UBSan and builds all private
  session images. Its first actual boot fails before audio with ENXIO.
- v4 repeats both native variants and adds fixture-only negative diagnostics.
  Its boot identifies the failure as initial GetNameOwner: the bus is reachable
  before systemd acquires its name. Restore bounded initial discovery, retrying
  only the bus driver's NameHasNoOwner error before capture or any unit effect.
  This never retries a lost admitted identity or a mutation. Both failed boots
  actually power off; their zero QEMU exits do not turn them into accepted runs.

All five exact build/export helpers and both separate probe helpers retire after
independent archive hashing. Failed evidence is retained. No USB, normal desktop,
authority, frozen contract or foreign UI/project file is changed.

The corrected v5 build also passes all four groups in both variants. Its actual
PAM playback boot acquires the initial name on attempt two, then admits the real
manager and completes the reference audio. All six final v5 boots pass, retaining
their original distinct UUIDs, serial/QMP/capture evidence and zero original QEMU
exits. Host acceptance has 85 cases, including missing/late admission and
an explicit rejection that cannot be hidden by a success line.

A separate read-only check against the existing enforcing carrier reaches the
current executable observation and receives EACCES. It sends no service mutation;
the denial is retained rather than expanding policy or granting ptrace. Normal
confined integration is still open. Three local controlled counterexamples omit
owner equality, forget the latched loss, or omit MainPID admission. The current
tests reject all three at their intended assertions; the unchanged baseline
passes. A local harness-only failure handler retires its exact held/unreaped
sleep child before each expected rejection. Neither the production source nor
the ordinary native test is weakened to obtain those counterexamples.

## Final acceptance

`acceptance-final.json` independently verifies every source/archive member and
helper retirement, all 567 current code/pin inputs, the three successful native
build pairs, exact counterexample edits/results, the enforcing read rejection,
the 85-case verifier and all six original image/serial/QMP/PCM records. The four
session roots differ only in their fixed case selector. The runtime remains
86 files, ten aliases and 10,302,437 bytes; no new third-party package is added.

| Final cold boot | Captured frames | Reference start | Result |
| --- | ---: | ---: | --- |
| Playback | 132,544 | 14,688 | Complete reference and original stop/logout |
| VT loss/return | 134,944 | 14,688 | Original actors stop; no automatic restart |
| Graph exit | 134,464 | 16,736 | Full reference and original end |
| Policy exit | 135,424 | 16,736 | Full reference and original end |
| Native regression | 133,984 | 16,736 | Existing direct profile retained |
| No card | — | — | Gate rejects before manager/actors |

All five captures contain the complete 96,000-frame stereo reference with
0.49795693 LSB maximum error against the unchanged two-LSB criterion. No samples
or headers are repaired. These are explicit instruction-clock functional runs,
not physical performance or a fix for the retained default-clock APIC issue.
Preservation again checks 445 original sources, 19 protected hashes, 31 frozen
SQL/XML inputs and 52 normal files, plus the four unchanged foreign work hashes.

## Sources and remaining boundaries

The systemd [credential API](https://man7.org/linux/man-pages/man3/sd_bus_get_name_creds.3.html)
describes bus-client credential ownership and non-negative success. The
[PIDFD lookup implementation](https://raw.githubusercontent.com/systemd/systemd/v259/src/core/dbus-manager.c)
returns the containing unit and invocation after a final process check; it does
not identify the main actor by itself. MainPID, idle job, actual cgroup and image
checks are separate. Existing exact runtime pins are retained.

Normal service/configuration/library provenance, enforcing app MAC, delegated
holders, manager replacement/recovery and physical/real-time acceptance remain
work. The full OS goal stays active.
