# Graphical login deployment

The login layout is a staging input for an Aetheri system image. It gives the
backend, startup verifier, greeter, and desktop entry stable paths so the
deployment provider can bind them to independently selected release inputs.

| Path | Purpose |
| --- | --- |
| `/usr/libexec/aetheri/` | Backend, startup verifier, greeter, its display process, session entry, compositor and desktop |
| `/usr/share/aetheri/login/` | Fixed backend configuration, greeter font, SELinux policy and file-context inputs |
| `/etc/pam.d/aetheri-login` | Local human authentication and session setup through PAM |
| `/etc/pam.d/aetheri-greeter` | Internal PAM session for the locked display account |
| `/usr/lib/systemd/system/` | Backend service, ordered startup verifier and worker shutdown drop-in |
| `/usr/lib/sysusers.d/` and `/usr/lib/tmpfiles.d/` | Display account and per-boot directory recipes |
| `/usr/share/wayland-sessions/aetheri.desktop` | Entry from an existing local Wayland login |

The backend takes VT8 and starts its own greeter. Explicit image activation
can select it as `display-manager.service`. It pulls in the finite startup
verifier, which runs once after the original backend starts and publishes the
protected observation it verified. The [startup contract](login.md#finite-startup-executable)
defines the observation, ordering, and original process checks. The worker
shutdown drop-in preserves the backend's original worker and PAM close path.

The human PAM profile checks the local password and account, excludes UID 0
and `aetheri-greeter` from password authentication, and opens a logind session.
An expired password requires a separate account-management workflow. Image
assembly must enroll and independently verify the locked greeter account and
its groups. The carrier's choice of numeric UID/GID is local to that VM.

Root owns `/run/aetheri/login` and its create-only handoff. The greeter owns
the mode-0700 render directory `/run/aetheri/login-greeter`. The backend and
its descendants share their own `/tmp` and `/dev/shm` mounts backed by distinct
Root-owned mode-1733 directories under `/run/aetheri`. SELinux uses distinct
types for greeter and user objects. `ProtectSystem=full` makes the system paths
read-only in that namespace and leaves the user desktop's home available.
Image assembly applies the chosen file contexts and must verify that creation
at boot keeps those contexts, including on both scratch directories.

## Stage selected inputs

```sh
python tools/prepare_login_deployment.py --inputs login-inputs.json --output login-stage
```

Use a fresh destination with an existing parent. The JSON object has `format`
set to `1` and an `artifacts` object with exactly these keys.

- `aetheri-login-backend`, `aetheri-login-startup`, `aetheri-login-greeter`,
  `aetheri-login-display`, `aetheri-session`, `aetheri-compositor`, `aetheri-desktop`
- `greeter-font`, `greeter-font-license`, `backend-license`, `backend-dependency-notices`

Each value contains `path`, `size` in bytes, and `sha256` as 64 lowercase hex
digits. Relative paths resolve against the manifest directory. Select and hash
the built artifacts independently of the stager. It checks the selected size,
digest and basic file format before creating the destination. `root/` holds
the package tree. `receipt.json` lists the target paths, file sizes, intended
POSIX modes in decimal, and SHA-256 digests in path order. A partial write has
no completion receipt until all files are written. Use a fresh tree for a new
attempt.

## Image integration

### Signed login selection

The startup verifier and greeter need the same independently selected
deployment inputs. The selection can be signed after both executables are
built, so the greeter's own digest can be included. Build the eventual
providers with an explicitly selected Ed25519 public key and a nonzero,
unique 32-byte identifier for the selected image release. Keep the release
signing key on the signing workstation. The providers' trusted image
placement must establish the public key and release identifier before they
read a selection.

Prepare an unsigned record from an already reviewed staging receipt. Supply
the actual distribution-wide systemd service drop-in and a separately
reviewed snapshot of the effective kernel policy bytes as ordinary files.
Use the image's reserved greeter UID/GID and exact supplementary groups, one
`--group` argument per group in ascending order. An empty group set uses no
`--group` arguments.

```sh
python tools/prepare_login_selection.py --stage login-stage \
  --release "$LOGIN_RELEASE_ID" --greeter-uid "$LOGIN_UID" --greeter-gid "$LOGIN_GID" \
  --systemd-dropin selected-timeout-abort.conf --effective-policy selected-policy.bin \
  --output login.body
```

On the signing workstation, review the selected inputs and sign the exact
`login.body` bytes with plain Ed25519. For the OpenSSL CLI, use
`openssl pkeyutl -sign -rawin -inkey login-release.pem -in login.body -out login.sig`.
Append the 64 raw signature bytes to the body as `selection.bin`.
[OpenSSL Ed25519 signing and verification](https://docs.openssl.org/3.5/man7/EVP_SIGNATURE-ED25519/)

The 755-byte version-1 record uses big-endian integers. It contains the
eight-byte magic `AETHLS01`, the 32-byte release identifier, the 32-bit UID
and GID, the one-byte supplementary group count, two zero reserved bytes,
and sixteen 32-bit group slots, padded with zeros. Each file row contains a
32-bit nonzero size and its 32-byte SHA-256 digest. The Ed25519 signature
covers the whole 691-byte body. File rows have this fixed order.

1. Backend unit, startup unit, distribution service drop-in, worker shutdown drop-in
2. Backend executable, backend configuration, greeter executable, greeter display executable
3. Greeter font, human PAM profile, greeter PAM profile
4. Session entry, compositor, desktop, startup executable, effective kernel policy

The native verifier checks the exact record size, release identifier,
signature, account IDs, group ordering, reserved fields, and the size bounds
of each role. A valid record supplies expectations for the subsequent live
checks. Reusing a release identifier would allow multiple selections signed
with that identifier. Boot/update policy still owns rollback between images.
The shipped login executables still require an explicit deployment provider.

### Installed system

The stager copies the selected files and produces the receipt. Image assembly
must supply the linked libraries, PAM modules, full desktop runtime, protected
placement and ownership, and the verified distribution and Aetheri SELinux
policy. The normal compiled providers also need to bind the original loaded
unit, fixed source bytes, executable identities, enrolled account, session and
effective access rules. Select the final executable digests independently of
what the live greeter and backend report. The receipt is an inventory for that
work. The carrier-only policy supplement stays outside the package.

The [deployment verification record](../brains/aetheri/journal/2026-11-17-login-deployment-layout.md)
covers the isolated systemd layout and a separate full graphical VM test for
the scratch namespace. A login from the final fixed paths still requires the
normal providers and image integration.
