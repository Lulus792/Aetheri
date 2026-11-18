# Original process and attempt for an unlock result

The native worker and its broker library are built with
`AETHERI_UNLOCK_PAM=ON`. They exchange one request and one outcome on a
fresh inherited Unix socket pair. The worker has a fixed account selected
at launch and runs under that account's UID. Each side verifies the
packet's process descriptor. A random ticket ties the reply to the request.
The broker verifies child exit, orderly EOF and the original covered
generation before it returns an outcome.
See the [channel contract](../../../spec/session.md#one-worker-per-unlock-attempt).

## Native build and channel cases

Build `6fd5a55a1ea3` final Release and ASan/UBSan profiles pass both unlock
CTests. The native fixture covers a matching original response, bad ticket,
foreign process, duplicate packet, excessive ancillary descriptors, empty
datagram, missing result, nonzero exit, delayed exit, retained peer and changed
generation. Each case compares its descriptor counts. The final Release
channel test passes 128 repeated runs and GCC analyzer passes.

The first build used a fixture name taken by a C library macro. The first
live fixture then failed unnamed endpoint admission despite matching parent
credentials. Linux assigned an abstract name when the broker sent with
`SO_PASSPIDFD` already enabled on its sending socket. The final pair
creation arms the worker's receive end before spawn. The broker arms its
own receive end after its sole send. The worker's receive option ensures
the request carries the original sender identity, and its reply also carries
the sender identity. The earlier failures and diagnostics are retained.

## Exec and real PAM in the VM

Private carrier case `1a00753c08f6` launches the exact Release worker
through exec under an ordinary UID1000 process. The worker SHA-256 is
`8e311243d25be23edbf7f98d83aefba7146c311d8cb8e1a941a2ae79ca2018c6`.
The fixture uses the frozen PAM library, module and password helper with
isolated synthetic account data.

The proper password reaches accepted through both original process checks.
Wrong password, a foreign account, a locked account, expiry and absent
profile return denied. The owner rejects empty and embedded-NUL requests.
A separate process using a duplicate broker descriptor cannot submit the
request. A valid authentication that completes after the supplied covered
generation changes stays rejected when the old generation is presented
again. The fixture's synthetic PAM and account mounts end with its process.

This validates the worker and result admission in the private carrier.
Trusted credential rendering, compositor generation and release, signed
deployment admission and the combined sleep flow remain work. The
first-version USB gate applies.

Evidence and controller inputs are in
`local/login-review-2026-11-10/unlock-channel-2026-11-18-evidence.tar.gz`
with SHA-256
`da06857cd9c685506d1f052b00d1ef792df48d640b25008d36965f9cf6251761`.
