# Login backend candidate build

The original greeter uses the [C conversation model](login.md). A separate greetd
candidate supplies PAM handling and account switching. It is now built and tested
with pinned Rust dependencies; it is not installed or admitted as a login service.

## Inputs and footprint

[login-backend-inputs.json](login-backend-inputs.json) records the source-archive
digest, complete 34-crate graph, archive sizes/checksums, licences and notices,
toolchain packages and dated advisory query. [login-backend.Cargo.lock](login-backend.Cargo.lock)
is Cargo-generated. Both are required: an upstream lockfile does not prove which
crates a distribution binary actually contains.

The source is `greetd-0.10.3.tar.gz` from the independently reviewed signed Fedora
44 source RPM in [login-inputs.json](login-inputs.json). The newer signed Fedora
45 `0.10.3-12.fc45` package was also read as data. Its notice lists Tokio 1.53.1;
the older Fedora 44 binary lists 1.49.0. Both precede the reviewed advisory's
1.53.3 fix. Neither distribution binary is selected for execution. This is a
specific dependency finding, not evidence that greetd reaches the affected API.

The candidate keeps the original daemon source, limits workspace members to
`inish`, `greetd_ipc` and `greetd`, and pins Tokio to `=1.53.3`. Other crate versions
are fixed by the lock. The sole source licence alias is materialized with the
original `LICENSE` bytes; licences and notices remain with the vendored sources.
The daemon is GPL-3.0-only; our independent C code remains MIT. Packaging must
retain the exact source, build recipe, lock and third-party notices.

Rust/Cargo 1.99.1-2.fc44 and their supporting libraries are private development
tools. They do not become OS services or application dependencies. The tested
880,048-byte ELF directly needs only `libpam.so.0`, `libpam_misc.so.0`,
`libgcc_s.so.1` and `libc.so.6`; the PAM configuration can introduce additional
runtime modules and must be reviewed separately. Cargo's Git/SSH/HTTP libraries
and LLVM are not direct dependencies of that ELF.

## Offline preparation

Use Python 3.11 or later and a cache containing the exact `.crate` archives named
by the manifest. Obtain the source archive from the reviewed source RPM. The
preparer does not download or execute package contents:

```sh
python3 tools/prepare_login_backend.py \
  --source-archive CACHE/greetd-0.10.3.tar.gz \
  --crate-cache CACHE/crates \
  --output NEW_WORK/prepared
python3 -m unittest discover -s tools -p test_prepare_login_backend.py -v
```

It snapshots bounded archive bytes, checks the independent frozen digests, admits
canonical regular members and the exact source licence alias, checks crate
identities against the lock, and builds an entirely local Cargo directory source.
All admission precedes output creation. Existing/partial output refuses reuse;
write failures retain the partial tree. There is no automatic cleanup or resume.
The generated Cargo checksums are consistency metadata, not publisher trust.
Preparation is development tooling, not a privileged package importer or app API.

The native build uses an empty private Cargo home, an explicit toolchain path,
`CARGO_BUILD_JOBS=2` and `RUSTFLAGS='-C target-cpu=x86-64 -L native=LINK_DIR'`.
`LINK_DIR` supplies development linker names for the carrier's existing PAM
libraries, without changing `/usr/lib64`. From `NEW_WORK/prepared/source`, run:

```sh
cargo build --frozen --offline --release -p greetd --verbose
cargo test --frozen --offline --release --workspace --verbose
```

Run as an ordinary user in a disposable build carrier. The tested system service
has a read-only system, only its work directory writable, a private network,
private temporary/device views, no new privileges/capabilities and bounded memory,
tasks and runtime. Bind the private SDK read-only into that temporary view when
the SDK lives under `/var/tmp`. Preserve service and original process evidence on
an uncertain dispatch; do not repeat a build merely because observation failed.

## Verified boundary and next step

The source/vendor projection contains 2,607 files. All match the independently
prepared host input; the exported evidence has 2,641 regular files including its
manifest. The Release build and ten upstream tests plus one documentation test
pass. All thirteen preparer regressions pass on Windows and ordinary Linux; a
digest-bypass mutant fails the targeted rejection. The compiled daemon's
ordinary-user `--help` succeeds. Its ELF advertises x86-64-baseline; this is not
target-hardware acceptance or proof about every compiler/runtime object.

The exact-version OSV query returns no findings for all 34 lock entries on
10 November 2026. That database lookup does not review source, unsafe/FFI code,
PAM modules, configuration, account switching, SELinux policy or the protocol's
privileged transport. It must be refreshed before release.

Next admit the protected daemon image/configuration and original local Unix peer,
join the C conversation to bounded transport, then test real PAM rejection,
cancellation and greeter-to-user session transitions. Disable `source_profile`;
keep the fixed command/environment and UNKNOWN fences from the conversation
contract. No real password, login service, PAM/account configuration or production
policy was installed in this build work. Read the
[execution record](../brains/aetheri/journal/2026-11-10-login-backend-build.md).

## Optional startup notice

The Aetheri integration build adds GPL-3.0-only modules to the pinned
greetd source. Prepare a fresh tree with `--startup-notice`, then use
`cargo build --frozen --offline --release -p greetd --features aetheri-startup`.
The preparer checks the original archive and each integration site. The same
Cargo lock and vendor tree apply. The feature links to the reviewed libcrypto
3.5 LTS, with a build-time native linker name and the 3.5.9 runtime floor.
The original candidate remains available without this feature.

The backend takes the exact command `greetd --config PATH`. It reads the
absolute path once, rejects a final symlink, and checks for stable regular
UID0/GID0 file metadata, one hard link and exact mode 0444 or 0644. The read is
limited to 64 KiB and requires nonempty UTF-8 without NUL. The caller supplies
the independently protected parent, file labels and immutable deployment.
The integration requires `source_profile = false` and rejects an initial
automatic login. The upstream `--help` and internal session-worker paths retain
their separate behavior.

The same byte string feeds the existing greetd parser and the SHA-256 operation
in a private OpenSSL context. After validation, the backend sends one status
datagram to the systemd notification socket. `StatusText` then contains
`AETHERI_CONFIG_V1 INVOCATION_ID SHA256`, with canonical lowercase hex. The
deployment must verify `NotifyAccess=main`, the original live service invocation,
the expected config digest and the admitted running backend before relying on
the status. A failed send aborts this startup path. The status reports parsed
configuration before the backend enters PAM or the server loop. Login readiness,
effective policy and the original greeter remain separately checked milestones.

The candidate passed an offline native build and its workspace tests. The
actual backend sent the expected byte digest to systemd in an ordinary-user
preflight that stopped at the intentionally absent PAM service. See the
[native record](../brains/aetheri/journal/2026-11-15-login-startup-notice.md).

The [configuration reader](login.md#configuration-read-by-the-backend) also
matches the notice of a live greetd after an inert test greeter starts. The
private test configuration supplies no ordinary-user authentication path.

## Configuring-worker cancellation

The optional `aetheri-startup` integration also fixes cancellation after a PAM
authentication failure. Upstream keeps a configuring session whose worker can
already have exited. Sending Cancel to that socket can fail, leaving the C
client correctly fenced UNKNOWN. The integration preserves the client's strict
protocol behavior and makes the backend confirm its original worker's end.

`aetheri_worker::Worker` belongs to one original direct child returned by the
backend's fork. Creation/enrollment, cancellation and the sole backend child
reaper share `Context::inner`. The reaper now takes that lock before waitpid,
and records a matching terminal result in configuring/scheduled sessions before
releasing it. While the original child is unreaped, cancellation can observe
its terminal state with `waitid(P_PID, WEXITED|WNOHANG|WNOWAIT)` under that same
lock. This original-child/exclusive-reaper invariant prevents PID reuse from
substituting another process. It is not an interface accepting a caller PID.
An unrecorded `ECHILD` is lost evidence and permanently fences the worker.

Cancellation observes first, attempts at most one Cancel send and waits for
the original worker's end. The send and wait share one five-second deadline,
retained across a dropped future. The parent datagram is explicitly nonblocking
before fork; the worker endpoint keeps its existing blocking behavior. A send
error alone neither proves completion nor permits replay. A timeout or lost
observation retains the configuring slot and its failure. Reaping later cannot
clear that failure. Successful confirmed cancellation clears the original slot,
so the same admitted greeter can explicitly begin another authentication.
No new daemon, thread, external library or crate is added.

This observer applies before target Start. Worker exit is not descendant drain,
lease settlement or proof of an executed user's session ending. General PAM
deadlines, startup/shutdown recovery, successful authentication, PAM/logind
session transitions and production account/MAC/device policy remain separate.
The original build without the feature remains available. The old failing
candidate and its C client's UNKNOWN evidence remain preserved.

## Original session-worker shutdown

The optional integration now includes
[`aetheri_close.rs`](../patches/greetd/aetheri_close.rs), GPL-3.0-only.
The crate graph and toolchain stay unchanged. It addresses an actual carrier
failure: the daemon could exit after signaling the greeter child, causing the
PAM worker's parent-death signal to terminate it before session teardown.

The fresh external worker blocks TERM/USR1/CHLD before opening PAM and forking.
Its sole child checks its original parent after arming its own parent-death
signal, then restores the original signal mask before ordinary exec. The worker
checks its original daemon parent around its late parent-death setup. While its
original child is unreaped, it handles TERM by sending one TERM to that child;
USR1 requests one forced child termination. Only the worker signals this
still-unreaped child. The daemon no longer stores/signals the child's numeric PID
in this profile and never uses that force path to kill the Root PAM worker.

The waiting worker uses synchronous signal waiting, with no signal handler,
idle polling timer or added thread. After it reaps the original child, TERM and
USR1 remain blocked through PAM close, credential deletion and PAM end; the old
CHLD mask is restored for PAM teardown helpers. Lost child/signal evidence
retains the worker with one diagnostic instead of reporting closure.
The privileged worker-control socket is marked close-on-exec after the first
external worker exec, preventing inheritance into the later ordinary session.

The daemon keeps scheduled start/current-slot enrollment and its sole reaper
under the original Context lock. Shutdown signals the original direct worker at
most once and waits for its normal zero exit under that lock, observing without
reaping. The shutdown-only wait checks at 10 ms intervals; there is no idle
session polling. Dropping the future does not authorize another signal. Abnormal
or lost results retain the original slot, lock and daemon. The ordinary child
reaper likewise requires the exact current worker's zero exit before starting
a scheduled session or replacement greeter; ECHILD with a current slot fences
it. A reaped worker's numeric PID is never signaled.

The feature build also returns a nonzero process status after a worker/main
error. Upstream logs that error and then returns zero; zero would otherwise be
insufficient evidence for the new completion check. An ordinary-user actual
counterexample with the same closed worker descriptor returns zero from the
preceding candidate and one from the corrected candidate, without opening PAM.
This does not turn exit status alone into proof of all session or descendant
cleanup; the protected source/PAM configuration and independent observations
remain required.

[The reviewed service drop-in](../data/systemd/aetheri-login.service.d/20-session-shutdown.conf)
uses `KillMode=process`, `SendSIGKILL=no`, `TimeoutStopSec=infinity` and
`Restart=no`. It is a required deployment companion to the integration, not an
automatically installed service. Pending teardown must remain visible for
diagnosis and explicit reconciliation. No elapsed timeout, successful signal,
EOF, worker exit or inactive scope proves descendant drain, lease release,
durable recovery or complete application removal.

## Installed image execution

Build the image backend with `--features aetheri-direct-exec`. This
enables the startup integration and uses Linux `execve` for the final
session process. The greeter command from the protected configuration
is one absolute executable pathname. Authenticated Start supplies a
vector whose first element is an absolute pathname. The worker
converts this vector into literal arguments and rejects an empty
vector, a relative executable or an embedded NUL before acknowledging
the arguments. The signed deployment and the existing peer, PAM and
policy checks still govern the selected programs.

Only the final forked child executes the program, after dropping to
the selected account and applying the existing parent-death checks.
A failed exec writes its errno to the existing session diagnostics
and terminates that child. The original worker still owns PAM
teardown, with Start acknowledgement distinct from exec or
graphical readiness. The startup-only carrier profile retains
greetd's upstream shell execution.
