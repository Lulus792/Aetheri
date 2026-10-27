"""Owned child-only exec/credential tests in the dedicated carrier; no app admission."""
from pathlib import Path
import os
import stat
import subprocess
import sys
import tempfile

CASES=("formats","success","wrong-observation","environment","image","object","context","descriptors","caps","ambient","bounds","securebits","nnp","groups","gid","uid","signals","exec-error","before-exit","fast-exit","broken-reader")


def fixture(root,payload):
    root.mkdir(mode=0o700)
    (root/"code").mkdir(mode=0o755)
    (root/"code/probe").write_bytes(payload)
    (root/"code/probe").chmod(0o555)
    (root/"code").chmod(0o555)
    (root/"payload").mkdir(mode=0o700); os.chown(root/"payload",1000,1000)
    (root/"setup").mkdir(mode=0o700)
    for name in ("app","data"): (root/"setup"/name).mkdir(mode=0o700)


def verify_fixture(root,payload):
    expected={"code","code/probe","payload","setup","setup/app","setup/data"}
    if root.is_symlink() or {p.relative_to(root).as_posix() for p in root.rglob("*")}!=expected:
        raise RuntimeError("unexpected exec fixture contents; preserve")
    for name in (".","code","code/probe","payload","setup","setup/app","setup/data"):
        path=root/name; info=path.lstat()
        file=name=="code/probe"
        owner=1000 if name=="payload" else 0
        mode=0o555 if name in ("code","code/probe") else 0o700
        if path.is_symlink() or not (stat.S_ISREG(info.st_mode) if file else stat.S_ISDIR(info.st_mode)) or info.st_uid!=owner or info.st_gid!=owner or stat.S_IMODE(info.st_mode)!=mode or (file and info.st_nlink!=1):
            raise RuntimeError("exec fixture identity changed; preserve")
    if (root/"code/probe").read_bytes()!=payload: raise RuntimeError("exec fixture image changed")


def cleanup(root,payload):
    verify_fixture(root,payload)
    (root/"code/probe").unlink()
    for name in ("setup/app","setup/data","setup","payload","code"): (root/name).rmdir()
    root.rmdir()


def main():
    if sys.platform!="linux" or os.geteuid()!=0 or Path("/sys/class/dmi/id/product_name").read_text().strip()!="Aetheri P0 disposable VM":
        raise RuntimeError("requires dedicated root-only carrier test")
    binary,payload_path,build=map(lambda p:Path(p).resolve(strict=True),sys.argv[1:])
    payload=payload_path.read_bytes()
    root=Path(tempfile.mkdtemp(prefix="execute-",dir=build))
    try:
        for case in CASES:
            work=root/case; fixture(work,payload)
            subprocess.run([binary,work,case,"current"],check=True,timeout=15)
            cleanup(work,payload)
        root.rmdir()
    except BaseException:
        print(f"Preserved owned exec fixture: {root}",file=sys.stderr)
        raise
    print("Native exec boundary: credential/FD/environment sealing, exact image observation, faults and unknown EOF cases passed.")


if __name__=="__main__": main()
