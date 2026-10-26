"""Independent persistent-allocation bytes and native Linux publication checks."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

import runtime_contracts as reference


def allocation(value):
    data = b"AETHALC1" + reference.owner(value["owner"])
    data += struct.pack(">Q", value["filesystem_type"]) + bytes.fromhex(value["filesystem_id"])
    for kind in ("anchor", "root"):
        row = value[kind]
        data += struct.pack(">QQI", row["inode"], row["birth_seconds"], row["birth_nanoseconds"])
    return data


def main():
    executable = Path(sys.argv[1]).resolve(strict=True)
    build = Path(sys.argv[2]).resolve(strict=True)
    if executable.parent != build:
        raise ValueError("test binary outside build")
    vector = json.loads((Path(__file__).resolve().parents[1] / "spec/vectors/allocation-v1.json").read_text())
    encoded = allocation(vector["input"])
    if len(encoded) != 168 or encoded.hex() != vector["hex"] or hashlib.sha256(encoded).hexdigest() != vector["sha256"]:
        raise AssertionError("independent allocation vector drift")
    with tempfile.TemporaryDirectory(prefix="allocation-vector-", dir=build) as temporary:
        directory = Path(temporary)
        if directory.resolve().parent != build:
            raise ValueError("test vector outside build")
        path = directory / "golden.bin"
        path.write_bytes(encoded)
        subprocess.run([str(executable), "codec", str(path)], check=True, timeout=10)
    subprocess.run([str(executable), "basic", str(build)], check=True, timeout=30)


if __name__ == "__main__":
    main()
