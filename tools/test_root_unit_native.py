"""Retained synthetic owner journals joined to inert administrative PID1 custody."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile
import time
import runtime_contracts as reference
from test_launch_native import identity
from root_issuer_fixture import run as supervised


def main():
    executable, build = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip().replace('-', '')
    plan = dict(plan=identity(60), authority=identity(50), actor=identity(62), scope=identity(2),
                kind='launch', component='aetheri.probe', install=identity(10), generation=identity(14),
                data=identity(12), artifact=None, grant=None, retention='none', action='run', boot=boot,
                expires=2**63-1, reads=[dict(epoch=identity(epoch), kind=kind, ref=identity(ref), revision=revision)
                    for epoch, kind, ref, revision in ((1, 'gate', 10, 3), (1, 'install', 10, 2), (50, 'data', 12, 1), (50, 'gate', 12, 3))],
                effects=[dict(number=0, kind='data', component='aetheri.probe', ref=identity(12),
                              disposition='retain', reason='launch_data_reservation')])
    root = Path(tempfile.mkdtemp(prefix='root-unit-', dir=build))
    print('ROOT_UNIT_PENDING '+str(root), flush=True)
    expected = root/'expected'
    expected.write_bytes(hashlib.sha256(reference.intent(plan)).digest())
    vector=reference.load_json((Path(__file__).resolve().parents[1]/'spec/vectors/unit-selection-v1.json').read_bytes())
    codec=root/'codec.bin'
    codec.write_bytes(bytes.fromhex(vector['profile_hex']+vector['unit_sha256']+vector['selection_sha256']+vector['placement_sha256']))
    subprocess.run([executable,'unit-codec',codec],check=True,timeout=15)
    reports = []
    modes=('create-before','create-after','create-lost','create-fence','create-recipe','create-placement','normal', 'copy', 'fork', 'fenced', 'end-deny', 'precommit', 'commit-lost', 'last-gate', 'image', 'guard-image',
                 'guard-deny', 'late-fence', 'subscribe-before', 'subscribe-after', 'ref-before', 'ref-after',
                 'unref-before', 'unref-after', 'unsubscribe-before', 'unsubscribe-after',
                 'journal-origin-lost','journal-intent-before','journal-intent-lost','journal-ack-before','journal-ack-lost','start-ack-lost')
    assert len(sys.argv) in (3,4) and (len(sys.argv)==3 or sys.argv[3] in modes)
    for mode in modes:
        if len(sys.argv)==4 and mode!=sys.argv[3]: continue
        case = root/mode
        case.mkdir(mode=0o700)
        system, user = case/'system', case/'user'
        system.mkdir(mode=0o700)
        user.mkdir(mode=0o700)
        started = time.monotonic()
        supervised(executable, system, user, expected, 'exec-root-unit-'+mode)
        reports.append(dict(case=mode, exit=0, seconds=time.monotonic()-started))
        (root/'results.json').write_text(json.dumps(reports, indent=2)+'\n')
    # Native REQUEST/user preparation and reserved leases remain unsettled.
    # Keep both catalogues/receipts, even on success; no release/rollback inferred.
    print('ROOT_UNIT_RECEIPTS '+str(root), flush=True)


if __name__ == '__main__':
    main()
