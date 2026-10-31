"""Explicit administrative carrier test; private component bus, actual PID1 facts.

No installed bus-policy edits, app launch, lease release or production admission.
On failure retain private bus inputs/logs for inspection; service test identities
are printed before dispatch. Never infer drain from the subprocess timeout.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
from test_launch_native import private_bus


def main():
    if sys.platform!='linux' or os.getuid()!=0 or os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri P0 disposable VM':
        raise RuntimeError('requires the dedicated root-only carrier test')
    binary,build=(Path(p).resolve(strict=True) for p in sys.argv[1:])
    root=Path(tempfile.mkdtemp(prefix='lifetime-',dir=build))
    try:
        with private_bus(root) as environment:
            subprocess.run([str(binary)],env=environment,check=True,timeout=85)
        if {p.name for p in root.iterdir()}!={'bus.conf','bus.log','bus'}:
            raise RuntimeError('unexplained lifetime test inputs; preserve')
        for name in ('bus.conf','bus.log','bus'): (root/name).unlink()
        root.rmdir()
    except BaseException:
        print(f'Preserved lifetime test evidence: {root}; inspect exact printed services before retry.',file=sys.stderr)
        raise


if __name__=='__main__': main()
