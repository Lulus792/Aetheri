"""Compare the native P0 install-intent projection with the independent encoder."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile

import runtime_contracts as reference


def main():
    executable, build = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    frozen = json.loads((Path(__file__).resolve().parents[1] / "spec/vectors/runtime-v1.json").read_text())[0]
    def identity(last): return "000000000000400080000000000000" + f"{last:02x}"
    intent = dict(plan=identity(10), authority=identity(1), actor=identity(12), scope=identity(3),
                  kind="install", component=frozen["input"]["component"], install=None, generation=None,
                  data=None, artifact=frozen["sha256"], grant=None, retention="none", action=None,
                  boot=identity(2), expires=8_000_000_000_000_000_000,
                  reads=[dict(epoch=identity(1), kind="scope", ref=identity(3), revision=1)],
                  effects=[dict(number=n, kind=kind, component=frozen["input"]["component"],
                                ref=identity(target), disposition="create", reason="fixture_prepare")
                           for n, (kind, target) in enumerate((("install",14),("generation",15),("storage",16)))])
    expired = copy.deepcopy(intent)
    expired["expires"] = 0
    with tempfile.TemporaryDirectory(prefix="fixture-journal-", dir=build) as temporary:
        root = Path(temporary)
        manifest, expected = root / "manifest", root / "expected"
        manifest.write_bytes(bytes.fromhex(frozen["hex"]))
        activation=b"AETHACT1" + bytes([0x43])*32 + struct.pack(">QQQQ",1,2,6,123457000)
        expected.write_bytes(hashlib.sha256(reference.intent(intent)).digest() + hashlib.sha256(reference.intent(expired)).digest() + hashlib.sha256(activation).digest())
        for scenario in ("lost", "exit-before", "exit-after"):
            catalogue=root / scenario
            catalogue.mkdir(mode=0o700)
            subprocess.run([executable, manifest, expected, catalogue, scenario], check=True, timeout=30)


if __name__ == "__main__":
    main()
