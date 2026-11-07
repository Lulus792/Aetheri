# NexaUI consumer upgrade — 7 November 2026

## Reason and selected input

The user asked why Aetheri still selected NexaUI 0.1.18. That was the last
whole-consumer test pin, not a policy of remaining behind NexaUI development.
The selected replacement is 0.1.23 at immutable commit
`97a42647480bb05167cee5e4ae08cedd9147d80d`; it is not a floating `main` dependency.
The archive, inventory, MIT notice and private Release/Debug prefixes are pinned
in [nexaui-inputs.json](../../../spec/nexaui-inputs.json).

The C17 Wayland/software/AT-SPI profile retains the existing D-Bus runtime and
header-only RPM inputs. X11/Vulkan remain disabled in this SDK profile. Original
app models, SDL desktop/defaults and system authority contracts do not change.
The SDK source is unmodified. New public fullscreen, styled-field and larger
read-only snapshot APIs do not automatically enable new Aetheri product features.
This is a scoped compatibility review, not a comprehensive security audit.

## Consumer/build changes

CMake reads the exact version and revision from the manifest and rejects another
package version or exported source revision. Runtime Terminal diagnostics query
the linked library instead of printing a stale version literal. Native tests
continue to check actual mappings, selected library hashes and original owner
bindings. Two owner fixtures now consistently use their passed private build.

Git archive bytes remain unchanged. Private extraction uses a fixed zero mtime:
the carrier clock precedes the Git commit time, which previously made Ninja
regenerate its manifest 100 times. The original Settings example uses libm
without declaring its direct consumer link; an Aetheri-owned CMake wrapper adds
that link without editing the upstream example or SDK source.

## Executed evidence and failures

Both private SDK builds and complete original Aetheri app builds succeed in
Release and ASan/UBSan. Original long-path Release checks passed 65/73, with no
skips. Five failures occurred before app execution because diagnostic socket
paths exceeded Linux's AF_UNIX pathname budget. Fresh shorter build paths fix
that fixture constraint without changing test deadlines or assertions.

Three Settings processes received SIGBUS in the native Wayland buffer path.
An independent 4 MiB posix_fallocate probe returned EDQUOT on the shared /tmp,
while statvfs still reported about 831 MB free. The tmpfs has per-user quotas.
A repetition sampled that free space and still crashed at initial buffer fill.
A private mount namespace with a fresh bounded tmpfs, followed by dropping back
to UID/GID1000 before tests, passes all three original Settings workflows.
It changes no shared mount, quota, foreign files or SELinux policy. This is an
administrative carrier test arrangement, not Aetheri's app sandbox product.
The SDK's sparse ftruncate/mmap allocation still needs controlled quota-error
handling; passing on a fresh tmpfs does not fix that separate robustness gap.

One subsequent test pass ran out of writable carrier space during captures;
its missing JUnit report is incomplete evidence. Another complete Release run
passed 67/73; six owner guards correctly rejected executable mode0700 produced
by this work's private build umask. Exact private build executables were changed
to their required0755 and cache to0644, preserving every content hash. Guards
were not relaxed. Earlier results and corrections remain attributed separately.

The 27 development-owner contract/process tests pass under UID1000 Linux without
skips. Negative real CMake configurations reject 0.1.18 and a copied 0.1.23 package
with an incorrect exported revision; positive original packages build normally.

The six corrected Release owner groups pass (35.693 s), covering the remaining
cases from the 67-case successful subset of the earlier 295.951 s run. This is
split Release evidence, not a claimed single all-green 73-case invocation.
The complete 73-group ASan/UBSan matrix passes in 537.672 s without skips.

Source-v4 changes only the two Settings/Files peer-header paths relative to v3;
compiled app/SDK inputs are unchanged. Both peers now use the selected SDK's
D-Bus headers rather than requiring the old prefix. All eighteen header hashes
match the previous reviewed SDK, and the two actual native AT-SPI workflows pass
again in both profiles (four checks, 116.898 s total). Tests retain the unmodified
v3 app binaries and independently recorded v4 script/source hashes.

The verified evidence export contains 2,530 files / 49,513,350 bytes. Four native
captures were visually inspected: ordinary Settings, German 200% high-contrast
minimum viewport, Files and Terminal. Ordinary text, hierarchy, selections and
chrome remain recognizable and readable; the minimum viewport deliberately shows
scrolling/partial rows rather than fitting the whole page. The existing opaque
prototype and small-window/final-design limitations remain. Automated 1x/2x
layout checks are separate from this selected native 1x visual inspection; no
physical mixed-DPI or human screenreader acceptance is claimed.

## Storage and preservation

Old own raw native captures were losslessly archived to the Windows workspace,
all member hashes verified, then only the exact inactive guest copies reclaimed.
Unused compiler objects from recorded own completed/candidate builds, including
the completed private app builds, were removed;
programs, libraries, sources, logs, owner journals and receipts remain. Two failed
SDK extraction trees were checked byte-for-byte against the retained exact source
archive before removal. Their failed build logs and outputs remain. No foreign
NexaUI temporary tree was removed, and disk/reserve/clock settings were unchanged.

The read-only preservation audit verifies 444 current source inputs, 31 frozen
published SQL/XML contracts, 19 quarantined-object hashes, all 52 normal-P0 files
and controller, the original CLOSED DRM record, old SDK 0.1.18 binaries and SDL04.
Normal owners/services, public capabilities and the old executing quarantine are
unchanged. Foreign planning/design edits are preserved and excluded.

## Remaining boundary

The shared normal development source/build paths have not been promoted.
Successful private consumer tests do not switch an existing session or establish
normal launch, hardware, human assistive, final design or performance acceptance.
Native image review, evidence export and final audit are complete. The audit
finds no surviving upgrade actor, verifies every selected SDK artifact and checks
all local documentation links. Shared normal source/build promotion remains
separate and needs sufficient test storage without disturbing concurrent SDK work.
The complete Aetheri OS/apps/image/installer/update/experiment goal remains active
and incomplete. No new NexaUI chat handoff is pending.

Working evidence: `local/nexaui-upgrade-2026-11-07/`; SDK build inputs and logs are
retained in the separate guest prefixes. Read RESUME.md before further carrier work.
