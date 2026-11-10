# Pinned offline login backend build

10 November 2026. The greetd candidate now builds offline as UID1000 and passes
upstream tests. The independent C greeter model remains unchanged. No daemon,
PAM/account configuration or production policy is installed or admitted.

## Selection and preparation

The preceding goal turn answered status only; that did not advance implementation.
This continuation revalidated the original restricted audio lab: UUID
`033ac49e-68bb-47a3-9c9c-26a812dc8300`, QEMU5548, boot
`8b915c0c-ab36-4cce-a8a3-e84e1c12627c`, enforcing1. The original p0 VM remains closed.

Fedora44 has no newer greetd binary than the reviewed 0.10.3-6.fc44. The signed
Fedora45 0.10.3-12.fc45 binary/source are independently read as data, using the
official full Fedora45 fingerprint `4F50A6114CD5C6976A7F1179655A4B02F577861E`
and a private filesystem keyring. No global import or cross-release installation.
The binary's dependency notice lists Tokio1.53.1 and bytes1.12.1; Tokio still
precedes the reviewed 1.53.3 fix. Preserve the failed private-RPMDB preparation
and its actual SELinux denial; that is not a package-signature failure.

Use the signed Fedora44 original source archive, a Cargo-generated lock and
Tokio=1.53.3. A temporary loopback-only read-only registry relay supplied fixed
HTTPS index/crate paths through the original SSH connection; the VM retained
restricted networking and host-local Unix QMP. All crate hashes matched the
registry metadata and lock. Both the relay and original SSH tunnel exited.
No credentials were supplied. The complete34-entry OSV exact-version query has
no returned findings; this is not comprehensive security or PAM acceptance.

The private signed Rust/Cargo1.99.1-2.fc44 SDK contains126 regular files and six
aliases (305,013,981 regular bytes); streaming preparation verifies RPM header
digests as well as signatures. The first incomplete Cargo etc-path preparation
is retained. Read-only recursive ELF inspection, rather than another blind
loader retry, identified libssh2 and llhttp in addition to libgit2. Their signed
Fedora44 inputs and existing OpenSSL3.5.9 are separately projected. The package
metadata's historical OpenSSL4 changelog is not the actual ELF dependency: the
selected binaries require soname3. Rust version/sysroot, Cargo with actual
OpenSSL3.5.9, an x86-64 test compile and ordinary-user execution all pass.
Preserve both initial missing-library probes and the separate Python path-variable
preparation error. No host/global compiler or RPM database was changed.

## Reusable source preparation and native results

`tools/prepare_login_backend.py` admits bounded original archives and the pinned
lock before creating a new tree. Its exact source licence alias becomes a regular
copy of the original bytes. Workspace-only and Tokio dependency changes are
explicit; no daemon Rust source is patched. Vendored notices remain present.
Invalid hashes/graph/names/types/budgets and existing output reject. Partial write
failure retains its tree and blocks reuse. This tool neither executes build
scripts nor installs anything. `*.lock` now has explicit LF checkout semantics.

Final native working input `/home/aetheri/login-build-7bd6ff44319a` prepares2,607
source/vendor files. Its submitted manifest has the same source/lock/crate recipe
as the final tracked manifest; final metadata additionally names the supporting
build-library inputs. Final preparation additionally rejects trailing-slash file
aliases and canonical duplicate names. A fresh full-cache preparation produces
the identical 2,607-file map; final source/tests were separately exercised below.

Build service `aetheri-login-build-2e02d431b725` exits0. It runs ordinary UID/GID1000,
with private network/temp/devices, read-only system/SDK, one writable build tree,
NNP/empty capabilities, two build jobs and bounded memory/tasks/runtime. Retain
the earlier nonexecuting dispatch failures: DBus rejects inherited SSH pipe FDs;
a subsequent stdout destination rejects; private temporary views hide the SDK
until an explicit read-only bind is added. Each original unit/process is inspected
before a new service attempt. No policy weakening or automatic retry follows.

Test service `aetheri-login-tests-bfd67d0feecb` exits0: seven greetd config tests,
three inish parser tests and one greetd_ipc documentation test pass. The latter
crate has no separate unit tests. Three upstream warnings remain (including
lifetime spelling); they are not silently fixed in third-party source.

The compiled candidate is880,048bytes, SHA256
`464c907e69b8a2e7b74998839525c6985ed32bda05a0bcea44de583b9e6c9c9a`,
build-ID `8fae222d4a29254b5bc7048de979e460890bce29`. ELF notes advertise
x86-64-baseline, dynamic flags NOW/PIE. Direct dependencies are PAM/PAM-misc,
libgcc and glibc. Ordinary-user `--help` exits0, with no daemon authentication.
This is not physical CPU acceptance or a complete runtime-module inventory.

Final source tests `/home/aetheri/login-input-tests-eadadb4d1a0f/good` pass all 13
cases as UID1000; the same 13 pass on Windows. The deliberately digest-blind
mutation fails exactly the valid-archive/wrong-frozen-digest case, exit1. It is
test-only. The earlier submitted ten-case suite also passed under both systems.
Preserve the intervening failed test that expected Python's tar reader to expose
a directory's raw trailing slashes; the reader normalizes those. The final case
checks the observable canonical duplicate instead. Contract validation separately
passes 374 assertions/14 API methods; no SQL/XML contract was changed.

## Export and remaining work

Protected evidence `/var/tmp/a-login-backend-evidence-450d1d9f4866` exports
7,576,012 bytes, SHA256
`df69e2c8401dd943f96d5ab4d535ec04c9cbd796227cd7a900f6117c2f5f70c5`.
Host `local/login-review-2026-11-10/backend-export-audit-v4.json` independently
verifies all2,641 regular files, including all2,607 prepared input bodies against
the host preparation. The126 SDK bodies are rehashed in the guest against the
signed-header receipt; the full SDK is not independently re-exported here.
Keep earlier export preparations: raw JSON newline differences and atime-changing
reads were corrected in the evidence checker; the completed v3 archive's separate
host audit succeeds after a missing local import; final v4 adds the stricter
preparer tests. None required replaying build
or test effects. These receipts are evidence, not runtime admission.

RPM list and original udev693/invocation remain unchanged, enforcing1, tty1; no
selected SDK/build code remains mapped. No real password was used. The four
foreign UI-memory files remain outside this change. Next admit original protected
daemon/configuration/socket peers, join bounded C transport and test real
PAM/greeter/session transitions, then own greeter UI and system packaging.
Trusted lock/suspend, device/app integration and the full first version remain
open. No next USB-targeted image or flash; no NexaUI handoff.
