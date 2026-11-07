"""Pass independent published bytes to the native bounded codec."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
vector = json.loads((root/'spec/vectors/unit-issuer-v1.json').read_text())
with tempfile.TemporaryDirectory(prefix='issuer-codec-', dir=sys.argv[2]) as directory:
    frame = Path(directory)/'issuer.bin'; frame.write_bytes(bytes.fromhex(vector['hex']))
    subprocess.run([sys.argv[1], str(frame)], check=True, timeout=15)
