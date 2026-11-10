# Login protocol and backend review

10 November 2026. The new optional C17 login conversation model passes native
Release/memory and independent wire tests. Two signed greetd packages were
reviewed as data. No authentication backend, greeter or policy was installed,
and no new USB-targeted image was built or flashed.

## Selection and source evidence

The post-login entry exists; the next boundary needs maintained PAM authentication
and an original UI. Prefer a separate greetd-compatible backend over another
original privileged PAM/account-switching service. It supplies the maintained
privileged conversation/session worker while allowing an independent C greeter.
The exact runtime is not admitted by this direction choice.

Signed Fedora 44 greetd 0.10.3-6.fc44 packages are pinned in
`spec/login-inputs.json`: binary 465,869 bytes/SHA
`d6f3ab7263462e40f96dff09f17a7246fa1e0c0776ad0f4cab728cf62d3145a8`,
source 84,213 bytes/SHA
`5caf4b69985e4eccce7aa557e2e1de35955926007b27ffef4f14a145ef4ff9e7`.
Full signer `36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6`, header/payload checks pass.
Metadata, scripts, dependency notices, PAM/service/default config and original
worker/codec/conversation sources were inspected. Package code was never executed.
The code uses native-endian framing, separate explicit prompt styles and a
10 KiB worker datagram. Start command elements are shell-joined; source_profile
defaults true. Our client fixes command/environment, and future backend admission
must disable profiles and review its shell/runtime dependencies.

The binary dependency notice lists bytes 1.11.1 and Tokio 1.49.0. The former meets
the reviewed RUSTSEC-2026-0007 fixed range; the latter is in RUSTSEC-2026-0386's
reported version range. Affected API reachability remains unproven; do not infer
an exploit or clear the whole package from this observation. Review a maintained
rebuild/current full dependency set before admission. The pam-bindings advisory
does not identify the separately listed pam-sys as that crate. Original client
code remains MIT; external backend/component licences remain distinct.

Direct Sourcehut pages were unavailable to the browser; the signed source RPM
provides the inspected original source instead. The first preparation rejected
the upstream IPC licence symlink. A fresh preparation checks that exact
`greetd_ipc/LICENSE -> ../LICENSE` alias and leaves it unmaterialized, retaining
the original source archive/regular licence. No failed tree was replayed/adopted.

Final package review source `/var/tmp/a-login-review-2402d9ded16c`; independent
export verifies all 76 regular files. Local `review-evidence-v2.tar.gz`:
1,335,998 bytes, SHA-256
`7ac8500e59550bfae2c3862ae4167aaf4f4467cd1454a0fc6bd817b8ef4bf23f`.

## Implementation

`src/login/protocol.{h,c}` owns bounded native-u32/JSON framing, strict UTF-8 and
JSON response parsing, the ordered prompt/auth/cancel/start conversation and one
fixed Aetheri session command. Its only linked library is existing aetheri_text.
No Rust codec/library is linked. No process, socket, PAM/account action, login
authority or background worker is supplied by this model. A future transport
still owes independent original backend/connection/greeter admission.

Local invalid input preserves phase/prompt; arbitrary commands/environment are
not API inputs. The 1,024-byte response budget also bounds escaped worker messages.
Consumed TX prefixes and interrupted RX/prompt buffers are overwritten; caller/UI
copies, swap, backend memory and durable reconciliation remain outside that claim.
No automatic answer/retry or unknown-start replay occurs. Confirmed start means
scheduling acknowledgement only. Unknown cancel/start cannot permit a new attempt.

## Executed checks

Only the restricted lab VM was used: UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU 5548, boot
`8b915c0c-ab36-4cce-a8a3-e84e1c12627c`, enforcing 1. Original vm/p0 stays stopped.
Final standalone source `/var/tmp/a-login-protocol-2df317545be1`, work
`/home/aetheri/login-protocol-2df317545be1`: Release and memory compile/run pass,
with strict warnings/GCC analyzer and unsuppressed ASan/UBSan/leak checks.
Retain the first misleading-indentation compiler failure and original inputs.

Final pinned CMake Release source `/var/tmp/a-session-cmake-eeb8476bef13`, memory
source `/var/tmp/a-session-cmake-837743d5b9d1`: configure, library/test builds and
both login.protocol/login.wire pass as UID1000, without skips. The latter uses an
independent Python JSON peer over actual pipes; Unicode/escaping, fragmented
headers/responses and the complete fixed command/env compare independently.
All relevant compilation commands and memory flags are checked.

Controlled cases cover early start, secret/visible versus info/error, empty/NULL,
duplicate/unknown fields, malformed UTF-8/surrogates/NUL, bounds, every shorter
success-response prefix, extra bytes, loss and explicit cancel-before-new-attempt.
The separate premature-start mutant compiles then fails
`ae_login_start(s)==-EBUSY`, exit 134. It is never installed; core dumps disabled.
No real credential was supplied or logged. No actual greetd/PAM/session transition
or UI/hardware acceptance follows from these checks.

## Audit and continuation

Independent streaming audit checks all 1,552 exported files and 1,494 submitted
inputs, current C/test/Python/CMake bodies, raw CTest/wire outputs and the mutant.
Local `protocol-evidence-v1.tar.gz`: 5,355,612 bytes, SHA-256
`302f8f4e6c377b5f3eb9569b675ddba148b96ebada93f477a2b7f812a731ce81`.
Guest protected origin:
`/root/aetheri-login-protocol-evidence-0d9279eb563b4b05a70d5ec75039f970`.
Host regressions pass 71/skip 16 (87 cases). Packages, original udev 693/invocation,
enforcing 1, tty1, no fixture PAM/local seat and unmapped selected runtime/review
code are checked. The four foreign UI-memory files remain separate and unchanged.

Next finish the candidate's maintained build/security/runtime review and original
protected connection admission, then real PAM/greeter/session transitions and
the own graphical greeter. This is normal-login work, not a new product scope.
Production app capabilities and old quarantine stay closed. Revalidate the lab
before dispatch; USB release gate and NexaUI catalogue-maintenance rule stand.
