"""Unconfined native catalogue projection fixtures; no runtime/app admission."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable, build = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="projection-", dir=build) as temporary:
        root=Path(temporary)
        assert root.resolve().parent==build
        system,user=root/"system",root/"user"
        system.mkdir(mode=0o700)
        user.mkdir(mode=0o700)
        subprocess.run([executable,system,user],check=True,timeout=40)


if __name__=="__main__":
    main()
