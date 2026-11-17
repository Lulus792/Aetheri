# Graphical login deployment

The login layout is a staging input for an Aetheri system image. It gives the
backend, startup verifier, greeter, and desktop entry stable paths so the
deployment provider can bind them to independently selected release inputs.

| Path | Purpose |
| --- | --- |
| `/usr/libexec/aetheri/` | Backend, startup verifier, greeter, its display process, session entry, compositor and desktop |
| `/usr/share/aetheri/login/` | Signed release selection, backend configuration, greeter font and SELinux policy inputs |
| `/etc/pam.d/aetheri-login` | Local human authentication and session setup through PAM |
| `/etc/pam.d/aetheri-greeter` | Internal PAM session for the locked display account |
| `/etc/pam.d/other` | Deny authentication, account, password and session requests for unknown PAM services |
| `/etc/nsswitch.conf` | Local account and group lookup, with DNS available for host names |
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

Both installed adapters include all three PAM files in the common protected
source set. The signed selection supplies the two named profiles. The installed
adapter supplies a fixed digest for the standalone image's deny-only fallback.
The startup helper checks the files in the original backend's mount view
before publishing the handoff. The greeter checks the same named and held files
before credential transport and keeps them in its ongoing admission checks.
The image platform gate also covers PAM's loader, module files, account data and
configuration search paths. Linux-PAM loads the default service alongside a
named profile. [Linux-PAM configuration loader](https://raw.githubusercontent.com/linux-pam/linux-pam/v1.7.2/libpam/pam_handlers.c)

The [session entry](session.md) can supply the runtime's graphics and input
data paths when building against a protected runtime projection. The
packaged human PAM profile contains the account, authentication and
session steps. Image integration selects and verifies the actual runtime.

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

### PAM runtime supplement

```sh
python tools/prepare_login_pam_runtime.py --output login-pam-stage
```

Run this as an ordinary Linux build user with the exact source files listed
in [the PAM runtime inventory](login-pam-runtime-inputs.json) and `readelf`
available. It stages three additional PAM modules, the password helper,
the locked account's `nologin` shell, and their additional libraries and
notices. It verifies hashes during copying
and checks the staged ELF references against the supplement and the selected
desktop and boot inventories. The receipt records the required base paths
and the final image metadata for the password helper. The staged helper
has mode 0755. The image assembler still needs to apply and verify the final
Root ownership, mode 04755 and SELinux context on the assembled image only.

Inspect the assembled root after applying final ownership, modes and labels.
Use a stable offline root and a reader that can see its SELinux file contexts.

```sh
python tools/verify_login_pam_root.py --root /mnt/aetheri-image
```

The verifier checks the three PAM configuration files and follows the selected
module files' ELF dependencies through exact aliases to pinned file bytes in
the image. It checks the loader and the password helper's path, mode and label.
The JSON result lists the checked objects and references. Image boot tests still
need to exercise loader configuration, account data, the installed platform
gates and full PAM session creation. The
[composed PAM check](../brains/aetheri/journal/2026-11-17-login-pam-root.md)
loads the selected modules with the image's own libraries and checks the
default service inside a private test root.

### Local account data

During assembly of a fresh image, select the greeter UID and GID against its
base account inventory. Preview and apply the staged sysusers recipe to that
root. Verify the result against the independently selected IDs before signing.
Keep an already enrolled greeter reserved across updates. [systemd-sysusers](https://www.freedesktop.org/software/systemd/man/259/systemd-sysusers.html)

```sh
systemd-sysusers --root="$IMAGE_ROOT" --dry-run - < "$IMAGE_ROOT/usr/lib/sysusers.d/aetheri-login.conf"
systemd-sysusers --root="$IMAGE_ROOT" - < "$IMAGE_ROOT/usr/lib/sysusers.d/aetheri-login.conf"
systemd-tmpfiles --root="$IMAGE_ROOT" --create - < "$IMAGE_ROOT/usr/lib/tmpfiles.d/aetheri-login.conf"
python tools/verify_login_accounts.py --root "$IMAGE_ROOT" \
  --greeter-uid "$LOGIN_UID" --greeter-gid "$LOGIN_GID"
```

The verifier checks the local NSS policy, the exact enrolled identity and
fully locked password and account, and rejects shared IDs and extra group
memberships. It checks the Root-owned empty home and pinned `nologin` shell.
For this recipe, `initgroups` sets the primary GID as the sole kernel
supplementary group. Pass that GID once as `--group` when preparing the
normal signed selection. The tool reports IDs and groups only.

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
`--group` argument per group in ascending order. With `--account-root`,
selection preparation verifies the offline account against those expectations.

```sh
python tools/prepare_login_selection.py --stage login-stage \
  --account-root "$IMAGE_ROOT" --group "$LOGIN_GID" \
  --release "$LOGIN_RELEASE_ID" --greeter-uid "$LOGIN_UID" --greeter-gid "$LOGIN_GID" \
  --systemd-dropin selected-timeout-abort.conf --effective-policy selected-policy.bin \
  --output login.body
```

On the signing workstation, review the selected inputs and sign the exact
`login.body` bytes with plain Ed25519. For the OpenSSL CLI, use
`openssl pkeyutl -sign -rawin -inkey login-release.pem -in login.body -out login.sig`.
Run the preparation command again with the same selection inputs, adding
`--signature login.sig` and setting the output to
`login-stage/root/usr/share/aetheri/login/selection.bin`. It rebuilds the body
and appends exactly 64 raw signature bytes. It creates the output once and
sets mode 0444. During image assembly use Root ownership and the supplied
SELinux file context. Signing and verification cover the final bytes
even if two preparation runs use different inputs.
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
Both login executables require an explicit deployment provider.

The [fixed-path reader](../src/login/installed.h) authenticates the record and
retains a protected source observation of those authenticated bytes. It
checks Root ownership, mode 0444, a single link, the exact SELinux file label,
and the original path and file identities. It projects shared expectations
for the backend, loaded configuration, ordered drop-ins and greeter, plus
the separate display executable. It reports the signed account and policy
expectations for the providers' live comparisons. When a source check
observes a replacement, later reads of that observation stay fenced.
[Linux path resolution](https://man7.org/linux/man-pages/man2/openat2.2.html)

The startup helper enters the original backend's mount namespace. Each
view needs its own source observation against the same signed release
selection. Source observations retain mount identities from their original
view.

### Startup executable

The startup executable can now select the installed adapter from its existing
build-time deployment source. Set `ae_login_run_profile.installed_release`
to the compiled public key and release identifier, and provide
`startup.check` and its context for the image's platform gate. The adapter
sets the fixed unit, selected source order and handoff location. The platform
gate still admits the protected installation, host namespace, account, PAM,
libraries and devices.

The adapter holds the first signed selection through the last original-view
check. The native startup coordinator enters the original backend's namespace
and verifies it before calling the one-time view hook. That hook opens a
second protected selection and requires the same authenticated record digest.
It keeps the second selection pinned during backend source capture, readiness
and publication. The adapter also compares the signed policy size and SHA-256
with the actual policy read from selinuxfs in each gate invocation. Changes
or unreadable selections cause the startup attempt to fail.

The [startup adapter verification](../brains/aetheri/journal/2026-11-17-login-installed-startup.md)
covers the native entry, real private mount views and effective policy checks.

### Greeter executable

The greeter also selects its installed adapter from its build-time provider.
Set `ae_login_greeter_profile.installed_release` to the compiled trust anchor
and supply the mandatory `backend.check` platform callback and its context.
The deployment can supply reviewed library and input data directories for
the display child.

The adapter derives its UID, GID, complete supplementary group set and
display image from the signed selection. Actual process IDs and groups
must agree before it enters the native greeter runner. The runner keeps
its existing original local session, peer, process and display checks.
The font comes from its original protected file at
`/usr/share/aetheri/login/greeter.ttf`. The resident font copy must match
the signed digest. The original selection and font file are checked on
subsequent host gate calls. An observed replacement fences the attempt.
The ordinary greeter continues to rely on the platform callback for live
policy and protected host admission.

Both installed adapters include `selection.bin` in the common source set
used by the protected handoff. That binds the handoff to the signed selection
observed in the backend mount view, including its release, account, policy
and asset rows.

The installed adapters use the `AETHLGH2` handoff profile. The startup helper
pins the enforcing kernel's status sequence and brackets its checks of the
signed policy digest with that original sequence. The ordinary greeter maps
the read-only status page and uses its own observed sequence to verify the
handoff. Policy reload or an enforcing-mode transition changes that sequence.
An existing attempt stays fenced after a change. The custom provider profile
can select the original handoff contract. The two formats are selected
explicitly and use separate context hash domains.
[Linux selinuxfs](https://raw.githubusercontent.com/torvalds/linux/master/security/selinux/selinuxfs.c)

The greeter has read-only access to the status page. The full loaded policy
read and digest comparison remain in the Root startup domain. The platform
gate still admits the protected boot and host kernel view, account, PAM,
runtime and devices. See the
[policy handoff verification](../brains/aetheri/journal/2026-11-17-login-policy-handoff.md).

See the [installed greeter check](../brains/aetheri/journal/2026-11-17-login-installed-greeter.md)
for the native entry and protected-font tests.

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
the scratch namespace. The [installed selection verification](../brains/aetheri/journal/2026-11-17-login-installed-selection.md)
covers the reader at the exact fixed path in a private mount namespace.
A login from the final fixed paths still requires the selected platform
gates and full image integration.
