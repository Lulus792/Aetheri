# Confirm configuring-worker cancellation — 15 November 2026

The optional pinned greetd integration now permits an explicit new login attempt
after authentication failure. It confirms the original configuring worker's end
before acknowledging cancellation. The C conversation/connection UNKNOWN fences
are unchanged. See [the contract](../../../spec/login-backend.md#configuring-worker-cancellation).

## Implementation boundary

`patches/greetd/aetheri_worker.rs` owns one original fork-returned direct-child
identity. Creation and enrollment hold the same Context lock as cancellation
and the backend's sole waitpid reaper. The configuring slot is recorded before
the first fallible asynchronous initiation send. A second create cannot replace
an existing configuring/scheduled slot. The reaper acquires the lock before
reaping and records actual matching terminal results before releasing it.

Cancellation first uses that recorded result or `waitid(P_PID)` with
WEXITED/WNOHANG/WNOWAIT while the original child is still unreaped. Under the
exclusive-reaper invariant, this PID cannot refer to a replacement child.
No peer/caller PID is reopened, no socket error implies exit and ECHILD without
a recorded original end fences permanently. A terminal observation is only
for this pre-Start worker, never a descendant/app/lease drain proof.

One Cancel send at most and the original-worker wait share a five-second
deadline. The send marker/deadline survive a dropped future. Timeout and lost
evidence retain the configuring slot and failure; a later reaper observation
cannot clear that failure. The parent datagram is set nonblocking before fork,
so its async send cannot silently become a blocking syscall outside the timer.
The worker endpoint retains its existing blocking behavior. This adds no
daemon, thread, external dependency or crate. It is GPL-3.0-only integration
with the existing greetd candidate, not an MIT relicensing of that backend.

## Native evidence

The same enforcing carrier remains on boot
`fdc84399-7306-4760-8605-a9fd638cb9a3`, QEMU PID16628. Prepared Root-owned source
`/var/tmp/a-worker-build-432dce063f45` independently matches all2609 planned
files. Only two modules are added and five existing source/manifests differ
from the base preparation; the pinned34-crate graph and Cargo lock are unchanged.
Ordinary UID1000 builds offline in the existing isolated toolchain profile.
Build and tests exit0. The17 unit tests and one doc test include six new cases
using actual child processes: failed send plus original end, live-child timeout,
foreign-child rejection, unrecorded reap, dropped wait without replay and bounded
blocked send. Each test cleans up/reaps only its own known children.

The resulting ELF is886472 bytes, SHA256
`109b9ff31401c0842d2a699bf33b8cac5c188d46a95a0c40fecd77826efce55e`.
Root stages the host-selected/hash-bound body and already protected reviewed
crypto at `/opt/aetheri-worker-test-b63921609efe`. Its direct dependencies remain
PAM/PAM-misc, libgcc, glibc and libcrypto; no RPATH/RUNPATH or user SDK loader
path is installed in the privileged backend/publisher. The daemon is1288 bytes
larger than the preceding startup-notice candidate, not the Rust build toolchain.

The projected tree also passes `cargo check --release --frozen --offline
--workspace --no-default-features`, recorded at
`/var/tmp/a-worker-default-a2c59dfd67da`. Three pre-existing upstream warnings
about unreachable exec follow-up and a hidden lifetime remain unsuppressed.
No new warning was introduced by the worker module.

The C test greeter's explicit `retry` mode first reproduces the failure with
the original backend in `/var/tmp/a-greeter-live-decbc73693e0`. It reaches a
real pam_unix prompt, acknowledges the first cancellation, receives AUTH_ERR
for an empty answer and then fails the expected IDLE assertion after Cancel.
This is retained negative evidence; its C protocol fence still works.

The same C client passes with the corrected backend in
`/var/tmp/a-greeter-live-3dcde125a07b` (Release) and
`/var/tmp/a-greeter-live-ea3bed726dfe` (ASan/UBSan). During a real secret prompt,
the client waits at a test-only checkpoint. Root independently selects the
actual backend child, checks its UID/GID/parent/current executable, holds a
kernel pin and releases the checkpoint. At the Cancel ACK that original pin
is terminal. After the empty-answer AUTH_ERR, another Cancel ACK permits two
further real prompt/cancel attempts. Each case makes four create requests,
one empty answer and no real-password submission. By the final marker only
the original greeter PAM worker remains among the backend's children.

Both cases retain the actual greeter/source/image/ancestry, protected handoff,
original peer and loaded-config checks. Own-program mode drift still fences
after restoration. The memory client explicitly checks LSan before DONE;
there is no ASan/UBSan/leak diagnostic. The Root backend is the ordinary tested
Rust build, not sanitizer-instrumented. PAM remains unit-private: pam_unix auth
and deny account/password/session entries for the existing locked test account.
No authenticated user session was started or claimed.

Final C source snapshots `/var/tmp/a-login-cmake-d0cbc63b3d07` and
`/var/tmp/a-login-cmake-62cccdf1bb3b` match all767 submitted files each. The
actual selected current C inputs and sanitizer flags are independently checked.
Both pass the three login CTests. Host preparation tests pass13 cases; contract
validation passes374 assertions/14 methods. The independent worker audit also
matches the actual Rust input graph, ELF, native receipts and old/new outcomes.
All16 retained live login fixtures and the three final build/test/check units
are terminal; cgroups are empty/removed, sockets absent and no selected code
remains mapped. RPM inventory, global PAM absence, enforcing1 and tty1 match
the carrier baseline. No USB build or flash occurred.

## Preserved failures and limits

The first build exposed a Rust2018 closure borrow of `self` while mutating its
deadline field. Copying the duration into a local resolves it; the failed
`a0199c0ae16c` preparation and terminal build remain. A local checkpoint-driver
string matched two sites and stopped before dispatch; an anchored selection
fixed that fixture. The first no-feature helper tried to recreate existing
native linker symlinks and stopped before a check unit existed. The corrected
helper validates those existing targets and records a fresh check output.
No failed source preparation or live fixture was replayed/reused.

Receipts, source projections and failed evidence are retained under
`local/login-review-2026-11-10` with `worker-` prefixes. The final audit is
`worker-audit-v1`. The four foreign UI-memory files remain unchanged. Preserve
the old execution quarantine and NexaUI catalogue-maintenance-only status.

This fixes the tested configuring-worker retry path. It is not an installed
display manager, complete PAM timeout/recovery strategy or production account/
SELinux/logind/device policy. Successful password authentication, actual user
PAM/session transitions, protected startup coordination and graphical greeter
remain work. Advance those next while keeping the full OS objective active
and the agreed first-version USB gate closed.
