"""Independent SHA-256 vectors; invoke the native verifier only in the Linux build."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def main():
    executable, build = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    fixture = json.loads((Path(__file__).resolve().parents[1] / "spec/vectors/runtime-v1.json").read_text())[0]
    vectors = [
        (b"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
        (b"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
        (b"a" * 1_000_000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"),
    ]
    vectors += [(data, hashlib.sha256(data).hexdigest()) for data in (
        b"synthetic-not-an-executable\n", b"hello\n", bytes(range(256)) * 257,
        b"x" * (65536 - 1), b"x" * 65536, b"x" * (65536 + 1), b"x" * (16 * 1024**2))]
    with tempfile.TemporaryDirectory(prefix="digest-", dir=build) as temporary:
        root = Path(temporary)
        archive = root / "vectors.bin"
        with archive.open("xb") as stream:
            stream.write(struct.pack(">I", len(vectors)))
            for data, digest in vectors + [(bytes.fromhex(fixture["hex"]), fixture["sha256"])]:
                stream.write(struct.pack(">I", len(data)) + bytes.fromhex(digest) + data)
        # Invalid provider search/config locations must not select application
        # plugins or modify this fixed private-context SHA-256 calculation.
        environment = {**os.environ, "OPENSSL_MODULES": str(root / "absent-modules"),
                       "OPENSSL_CONF": str(root / "absent-config")}
        subprocess.run([executable, archive, root / "payload"], check=True, env=environment, timeout=40)


if __name__ == "__main__":
    main()
