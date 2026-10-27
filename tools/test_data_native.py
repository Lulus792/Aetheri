"""Independent protected-config bytes and unconfined native metadata tests."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def identity(last):
    return bytes.fromhex("000000000000400080000000000000"+f"{last:02x}")


def main():
    executable,build=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="data-",dir=build) as temporary:
        root=Path(temporary)
        owner=b"AETHOWN1"+b"".join(identity(n) for n in (1,3,2,4,5,6))
        allocation=b"AETHALC1"+owner+struct.pack(">Q8sQQIQQI",0xef53,b"testfsid",17,18,2,19,20,3)
        config=b"AETHDTA1"+owner+b"".join(identity(n) for n in (20,21,22,23))+struct.pack(">QQQ",2,2,0)+allocation
        assert len(config)==368
        path=root/"config"
        path.write_bytes(config)
        (root/"catalogue").mkdir(mode=0o700)
        (root/"crash").mkdir(mode=0o700)
        subprocess.run([executable,root/"catalogue",path,root/"crash"],check=True,timeout=40)


if __name__=="__main__":
    main()
