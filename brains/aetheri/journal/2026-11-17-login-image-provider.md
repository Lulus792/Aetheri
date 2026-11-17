# Installed login image provider

CMake now has an explicit installed image mode. Both executables compile
the same selected Ed25519 public key and release identifier and keep their
existing native installed adapters. Image mode requires exact lowercase
hex values and refuses simultaneous custom provider sources. The startup
translation unit stays independent of the UI headers.

The provider requires enforcing SELinux, its role's exact process context,
and no-new-privileges. In the calling mount view it checks the ext4 root
and selected account, PAM, runtime and application directories. The unique
mount ID comes from a held original directory. The kernel verifies the
superblock flags, mount flags, and origin path. Canonical same-path service
binds are allowed. The underlying ext4 superblock must itself be read-only.
Procfs and the `/run` tmpfs are checked separately. Boot and image assembly
still supply authenticated image provenance, the actual dependency closure,
account files, and device policy.

Fixture `4da837ded1e8` uses a fresh regular ext4 image in a private VM
namespace. The check passes as Root and UID1000 and with an original-path
bind over `/etc`. It rejects a different subtree bind, a writable root,
and a read-only bind onto a writable superblock. The glibc loader finds a
fixture shared library through the generated image cache. The probe uses
the exact staged `ld.so.conf`, with no environment path or executable
RPATH. The assembled image must generate its own cache after staging.

Release `26ef074c659a` and ASan/UBSan `182336f5ccb5` build both actual image
entry executables and each pass 11 selected CTests with 839 inputs. Four
configure controls reject missing or uppercase keys, an all-zero release,
and mixed image/custom selection. The packaging suite passes four methods.
The explicit custom-provider Release configuration `3323ae2277a8` also passes.
Native service fixture `2bd0da2d4e63` uses the selected Release startup
binary. The unconfined invocation rejects its context. The actual SELinux
startup service with `ProtectSystem=strict` rejects the writable carrier
superblock, then ends inactive.
Candidate policy `f336f9cafe7e` was retired to the exact original enforcing
policy bytes with no remaining candidate tasks.

The first build exposed the UI include dependency in the startup
translation unit. The first analyzer driver also lacked the existing
core's GNU feature definition. Preserve both analyzer preparations
`74d437c24f23` and `41555d7b4f0e`. In `676a07c5553c`, the newer build
headers exposed the by-FD request flag, while the selected Linux 6.19
kernel returned EINVAL for that flag. The final code uses the unique-ID
request supported by the target kernel.
[Linux 6.19 statmount implementation](https://github.com/torvalds/linux/blob/v6.19/fs/namespace.c)

Continue with a standalone VM boot of the composed root and the fixed
startup/backend/greeter units. The native gate test and service refusal
do not establish a successful graphical login.
