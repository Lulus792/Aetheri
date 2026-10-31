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
import runtime_contracts as reference


def main():
    if len(sys.argv) not in (3,4) or (len(sys.argv)==4 and sys.argv[3] not in ('reidentify','handoff','handoff-retention')):
        raise ValueError('binary build-directory [reidentify|handoff|handoff-retention] required')
    if sys.platform!='linux' or os.getuid()!=0 or os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri P0 disposable VM':
        raise RuntimeError('requires the dedicated root-only carrier test')
    binary,build=(Path(p).resolve(strict=True) for p in sys.argv[1:3])
    reidentify=len(sys.argv)==4 and sys.argv[3]=='reidentify'
    handoff=len(sys.argv)==4 and sys.argv[3] in ('handoff','handoff-retention')
    retention=len(sys.argv)==4 and sys.argv[3]=='handoff-retention'
    # Capability-empty root cannot traverse the developer's UID-1000 home. Keep
    # this explicit temporary component service in its own root-owned /run tree.
    root=Path(tempfile.mkdtemp(prefix='aetheri-lifetime-',dir=Path('/run') if handoff else build))
    try:
        if handoff:
            probe=root/'probe'; probe.write_bytes(binary.read_bytes()); probe.chmod(0o700); binary=probe
        if reidentify:
            vector=reference.load_json((Path(__file__).resolve().parents[1]/'spec/vectors/process-identity-v1.json').read_bytes())
            encoded=root/'process.vector'; encoded.write_bytes(bytes.fromhex(vector['hex']))
            subprocess.run([str(binary),'--codec',str(encoded)],check=True,timeout=10)
        with private_bus(root) as environment:
            subprocess.run([str(binary)]+([str(root)] if reidentify or handoff else [])+(['--retain-units'] if retention else []),env=environment,check=True,timeout=85)
        expected={'bus.conf','bus.log','bus'}|({'process.vector'} if reidentify else set())|({'probe'} if handoff else set())
        if {p.name for p in root.iterdir()}!=expected:
            raise RuntimeError('unexplained lifetime test inputs; preserve')
        for name in expected: (root/name).unlink()
        root.rmdir()
    except BaseException:
        print(f'Preserved lifetime test evidence: {root}; inspect exact printed services before retry.',file=sys.stderr)
        raise


if __name__=='__main__': main()
