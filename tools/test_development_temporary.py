"""Explicit disposable-carrier test of the development unit's temporary profile.

No normal session start, shared mount change, policy installation or app admission.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='fresh root-owned evidence directory')
    parser.add_argument('--carrier-uuid', required=True, help='independently selected disposable VM UUID')
    args = parser.parse_args()
    if sys.platform != 'linux' or os.getuid() != 0:
        raise RuntimeError('explicit root-only dedicated-carrier test')
    if Path('/sys/class/dmi/id/product_name').read_text().strip() != 'Aetheri P0 disposable VM':
        raise RuntimeError('not the dedicated disposable carrier')
    carrier = str(uuid.UUID(args.carrier_uuid))
    if Path('/sys/class/dmi/id/product_uuid').read_text().strip().lower() != carrier:
        raise RuntimeError('disposable carrier UUID differs')
    if Path('/sys/fs/selinux/enforce').read_text().strip() != '1':
        raise RuntimeError('carrier must remain enforcing')
    output = args.output.absolute()
    if output.resolve() != output: raise RuntimeError('aliased evidence path')
    output.mkdir(mode=0o700)
    source = Path(__file__).resolve().with_name('development_session.py')
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    host_device = Path('/tmp').stat().st_dev
    results = []
    for case, mount, expected in (('private', '/tmp:rw,nosuid,nodev,size=1G,mode=1777', True),
                                  ('shared', None, False),
                                  ('wrong-budget', '/tmp:rw,nosuid,nodev,size=64M,mode=1777', False)):
        unit = 'aetheri-development-tmp-test-' + uuid.uuid4().hex + '.service'
        intent = dict(unit=unit, case=case, source_sha256=source_hash, mount=mount)
        (output/(case+'-intent.json')).write_text(json.dumps(intent, indent=2)+'\n')
        script = '''import importlib.util,json,os
from pathlib import Path
spec=importlib.util.spec_from_file_location('controller', SOURCE)
s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
try:
 result=s.temporary_area();accepted=True
except (RuntimeError,OSError) as e:
 result=dict(error=str(e));accepted=False
assert accepted is EXPECTED,(accepted,result)
if accepted:assert result['device']!=HOST_DEVICE
print('TEMPORARY_TEST '+json.dumps(dict(accepted=accepted,result=result,uid=os.getuid())),flush=True)
'''.replace('SOURCE', repr(str(source))).replace('EXPECTED', repr(expected)).replace('HOST_DEVICE', repr(host_device))
        cmd = ['systemd-run', '--unit='+unit, '--wait', '--collect', '--uid=aetheri', '--gid=aetheri',
               '--property=NoNewPrivileges=yes', '--property=CapabilityBoundingSet=', '--property=StandardOutput=journal']
        if mount: cmd += ['--property=TemporaryFileSystem='+mount]
        cmd += ['/usr/bin/python3', '-c', script]
        # Do not replay an unknown dispatch. The intent remains for inspection.
        run = subprocess.run(cmd, text=True, capture_output=True)
        (output/(case+'-dispatch.log')).write_text(run.stdout+run.stderr)
        journal = subprocess.check_output(['journalctl', '-b', '-u', unit, '--no-pager', '-o', 'cat'], text=True)
        (output/(case+'-journal.log')).write_text(journal)
        assert run.returncode == 0, (case, run.returncode, journal, run.stderr)
        rows = [json.loads(line.removeprefix('TEMPORARY_TEST ')) for line in journal.splitlines() if line.startswith('TEMPORARY_TEST ')]
        assert len(rows) == 1 and rows[0]['accepted'] is expected and rows[0]['uid'] == 1000, rows
        props = subprocess.check_output(['systemctl', 'show', unit, '--property=ActiveState,MainPID,ControlGroup'], text=True)
        assert 'ActiveState=inactive\n' in props and 'MainPID=0\n' in props and 'ControlGroup=\n' in props, props
        assert Path('/tmp').stat().st_dev == host_device
        results.append(dict(case=case, unit=unit, result=rows[0], terminal_unit=props))
        (output/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(dict(result='passed', cases=3, source_sha256=source_hash, evidence=str(output))))


if __name__ == '__main__': main()
