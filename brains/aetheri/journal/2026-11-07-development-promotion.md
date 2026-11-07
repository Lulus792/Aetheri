# Normal NexaUI promotion and private temporary profile

7 November 2026. The regular dedicated-carrier development paths now use the
reviewed NexaUI 0.1.23/97a4264 SDK. This follows the separate
[consumer upgrade](2026-11-07-nexaui-upgrade.md); it does not change SDL defaults,
installed authority capabilities or production app admission.

## Source and build transition

The previous shared sources/programs/CMake caches were exported before replacement:
561 files / 66,985,081 bytes, with every archived member verified on the host.
The inspected source transition changed 58 paths, including previously published
backend source additions; it did not install those backend services or migrate
any catalogue. Subsequent controller/test-only revisions are recorded separately.
All 445 current native source inputs are checked before declaring the builds ready.

The fixed `session-release` and `session-debug` app targets rebuild successfully
against their separate reviewed SDK prefixes. The owner still checks exact source,
model, cache and mapped-artifact bindings. `NexaUI_DIR:PATH` must be supplied with
its CMake type: an untyped command-line replacement left `UNINITIALIZED` and all
six owner guards correctly refused it before process creation. Explicit typed
reconfiguration fixes that input; the checks were not relaxed.

An explicit pending-source marker blocks a fresh Root start before selection or
mutation during promotion. Historical reads/stops remain historical. The marker
was removed under the source writer lock only after source and both selected
profile bindings matched and the final owner tests passed. Interrupted preparation
must retain the marker and evidence for inspection; this developer procedure is
not a production update/power-loss recovery protocol.

## Bounded temporary storage

New starts journal additive `temporary_profile: tmpfs-v1` before dispatch. They
request a separate 1 GiB `/tmp` through systemd's `TemporaryFileSystem`, with
`nosuid,nodev` and mode 1777. This uses the existing reviewed systemd 259.10
facilities; no shared mount, global quota or SELinux policy is changed.
The [systemd 259 reference](https://github.com/systemd/systemd/blob/v259/man/systemd.exec.xml)
describes the per-unit filesystem namespace and explicit mount options.

Before spawning UI, the ordinary supervisor checks actual tmpfs type, budget,
owner/mode, mount flags and absence of shared user/group/project quotas, then
preallocates 4 MiB through an anonymous file. A sparse mapping alone is not backing
storage evidence. Root also rejects a changed unit temporary-filesystem property
before later mutation. Records without the new field retain their original
shared-temp meaning; completed history never queries a current unit.
This is development resource isolation, not Aetheri's production app sandbox or
the full resettable experiment desktop. It avoids the exhausted shared quota;
the SDK's later allocation-failure handling remains separate P04 work.

Capture-heavy owner fixtures can now select a canonical, UID 1000-owned mode 0700
artifact directory separately from the checked binary prefix. The final test
matrix keeps these files on a bounded private tmpfs and exports a verified archive
to F: before retiring the namespace. Aliases and wrong permissions are rejected.
This avoids filling the carrier root with temporary raw display captures while
retaining inspectable evidence. A previous Debug run ran out of space; its three
partial raw files were archived/verified before only those inactive copies were
reclaimed. Failure logs and receipts remain. No foreign NexaUI tree was removed. The real session's shader cache exposed the
remaining small root-space margin. A manifest-limited cleanup then retired 524
unused compiler objects (67,848,176 bytes) from four inspected inactive builds;
programs, libraries, sources, journals and managed documents remain. The observed
writable margin is about 74 MiB. Dedicated build/capture capacity is the next
infrastructure task; repeated broad runs must not consume that margin blindly.

## Executed acceptance

- Both normal app builds pass: Release 10.565 s and Debug 8.496 s, after configuration.
- All six affected actual owner groups pass per profile: Release 32.020 s,
  ASan/UBSan 61.608 s. Their verified export has 866 files / 36,845,631 bytes.
- All 32 Linux controller/ownership/process tests pass without skips. They cover
  historical semantics, pending deployment, temporary-profile drift and early
  backing failure, plus independent artifact/binary locations.
- The explicit Root-only carrier probe passes three real PID1 cases: a fresh
  UID 1000 private 1 GiB tmpfs with real backing allocation, rejection of shared `/tmp`,
  and rejection of a wrong 64 MiB budget. A foreign carrier UUID is rejected before
  creating evidence or units. Temporary probe units retire; no production policy
  or service is installed.
- A fresh original Root-bound DRM session (`5a85fd54c2fb445ebe37e472f1f8afa3`)
  runs all three selected NexaUI views with Compose and the current v2 Terminal
  starter binding. Actual maps, executable hashes, parents, credentials, namespace,
  pixels, text commits and exact file bytes provide independent observations.

The actual QMP/PS2/virtual-DRM workflow creates a folder/file, opens the original
editor, saves exact `hello`, closes/reopens Files, inspects German 200% grid/details,
and runs independent Terminal split/tab shells. Each writes its distinct proof
bytes, then retires individually while the original shell PID remains. Settings,
Files and Terminal coexist on the actual selected SDK. All five supervised roles
exit 0, followed by the original controller stop and historical retry.
The supervisor's live `/tmp` has a different device from the shared mount, exactly
1 GiB capacity, mode 1777, UID 0 ownership, NoNewPrivs and an empty capability bound.

The first exercise paused at an obsolete `FOCUS 1` diagnostic predicate, filtered
by the current supervisor. The app/frame/window and clipboard focus were healthy.
The continuation revalidated the same original Root/app identities, focus and
saved bytes, without replaying earlier mutations or creating another session.
An earlier standalone `--pipe` smoke probe received an unknown connection reset;
its intent is retained and was not replayed. Journal-output probes and the normal
journal-output session work; no cause or policy fix is claimed for that reset.

Three actual desktop captures were visually inspected. The opaque prototype,
selected pane/tab and saved file remain clear. Earlier wide terminal output keeps
its existing grid behavior and can be clipped after narrowing a pane; complete
reflow/final visual design, human assistive and physical GPU/monitor acceptance
remain open. Virtual DRM evidence does not establish those broader requirements.

Working evidence is retained under `local/development-promotion-2026-11-07/`.
The complete OS/apps/image/installer/update/experiment goal remains incomplete.

The final read-only audit preserves 31 frozen SQL/XML contracts, 19 protected
quarantine hashes and 52 normal-P0 files/controller. Both old SDK18/SDL04 and the
original closed session bytes remain, with the earlier session now in history.
Current/earlier historical retries leave the new closed record unchanged. Nine
allocated probe/development units are inactive with no main process/cgroup; no
own normal-build actor remains, tty1 is restored and seat0 is free. The audit
verifies 445 current source inputs and all local documentation links. These
observations do not release any unrelated native lifecycle lease.
