# Login image account data

The image stages an exact NSS file with files-based passwd, shadow, group,
gshadow and initgroups lookup plus files/DNS host lookup. The existing sysusers
recipe enrolls the locked `aetheri-greeter` in a fresh image root. The tmpfiles
recipe creates its Root-owned empty home and typed runtime directory inputs.
The account verifier compares independently selected UID and GID, locked
records, exclusive group, empty home, and pinned shell. It emits no database
contents. Selection preparation accepts `--account-root` to verify the same
identity and exact signing groups before encoding the existing wire format.

The supplement adds util-linux `nologin` 2.41.3-12.fc44 and two upstream
license notices. The exact executable requires only the selected glibc and
loader. The ordinary staging fixture `ba986613c360` copies 21 files and eight
aliases and verifies 14 supplemental ELF objects. The separate composed-root
verification checks 31 ELF objects and 18 library aliases. The password helper
continues to use ordinary mode in staging and the final reviewed image metadata.
[Util-linux source and notices](https://github.com/util-linux/util-linux/tree/v2.41.3)

Final private VM run `70ad60d89fa6` applies the shipped recipes through the
carrier's 259.5 tools. Its dry run leaves all four synthetic account databases
unchanged. Actual enrollment chooses the independently expected fixture
UID/GID 999 and preserves the other entries byte-for-byte. Image libc resolves
the actual NSS entries, confirms the locked/expired shadow record, and
`initgroups` plus identity drop yields exactly kernel group 999. The installed
shell refuses a command with exit 1. The exact image PAM modules accept the
greeter for internal authentication/account handling, reject the ordinary
fixture account at the greeter gate, and reject the greeter at human
authentication. The earlier five default-stack denials still pass.

The account verifier rejects a duplicate name, shared primary group, added
privileged membership with or without surrounding whitespace, a changed
lock and a changed NSS configuration. The signing selection with the verified
group encodes the same bytes; an extra selected group fails before encoding.
Release `219310fa54e0` and ASan/UBSan `4b67459cbee0` each pass the 11 selected
Linux tests with 834 inputs. The last verifier-only whitespace change passes
the final composed-root run. All four Windows packaging tests pass.

Preserve the local Windows command-length failure `b2debb33dfac` and
private tmpfiles input-path failure `5f082ba9eaa6`. The final fixture streams
its driver over SSH and supplies the staged sysusers/tmpfiles recipes on
standard input. The fixture alters neither the archived root nor the
carrier account files. The next test must boot the image and exercise the
installed login providers, selected devices and normal session creation.
Candidate policy module `a621c2756d3a` was retired to the original policy
bytes with enforcing mode and zero remaining candidate tasks.
