"""Read the pinned libbpf RPM as data into a fresh development image tree.

No package script, host installation, mount or device operation is performed.
The runtime is systemd's dynamic backend; Aetheri accepts no caller BPF objects.
"""
import hashlib
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]


def checked(path,expected):
    if path.is_symlink() or not path.is_file() or type(expected.get('size')) is not int or not 0<expected['size']<=2*1024**2 or not re.fullmatch('[0-9a-f]{64}',expected.get('sha256','')):
        raise ValueError('invalid device runtime source')
    raw=path.read_bytes()
    if len(raw)!=expected['size'] or hashlib.sha256(raw).hexdigest()!=expected['sha256']:
        raise ValueError('changed device runtime bytes')
    return raw


def install(tree,packages,output,run):
    raw=(ROOT/'spec/device-filter-inputs.json').read_bytes();pins=json.loads(raw)
    entry=pins['package'];license=pins['license']
    if pins.get('format')!=1 or entry.get('name')!='libbpf-1.6.3-2.fc44.x86_64.rpm' or entry.get('payload')!='usr/lib64/libbpf.so.1.6.3' or pins.get('soname')!='libbpf.so.1' or license.get('name')!='libbpf-LICENSE.BSD-2-Clause' or entry.get('signer')!='36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6':
        raise ValueError('different reviewed device runtime profile')
    rpm=output/entry['name']
    with rpm.open('xb') as f:f.write(checked(packages/entry['name'],entry))
    signature=run('device-rpm-signature',['/usr/bin/rpmkeys','--checksig','--verbose',str(rpm)]).decode()
    if entry['signer']+': OK' not in signature:raise ValueError('different device runtime signer')
    # rpm2cpio and to-stdout extract only the admitted member. No archive paths
    # select a filesystem destination and package scripts are never executed.
    import subprocess
    p=subprocess.run(['/usr/bin/rpm2cpio',str(rpm)],capture_output=True);p.check_returncode()
    q=subprocess.run(['/usr/bin/cpio','--quiet','--extract','--to-stdout','./'+entry['payload']],input=p.stdout,capture_output=True);q.check_returncode()
    (output/'desktop-logs/device-rpm-cpio.stderr').write_bytes(p.stderr+q.stderr)
    if len(q.stdout)!=entry['payload_size'] or hashlib.sha256(q.stdout).hexdigest()!=entry['payload_sha256']:raise ValueError('different device runtime payload')
    target=tree/entry['payload'];alias=target.parent/pins['soname'];notice=tree/'usr/share/licenses/libbpf/LICENSE.BSD-2-Clause'
    if any(p.exists() or p.is_symlink() for p in (target,alias,notice)):raise ValueError('device runtime collision')
    target.parent.mkdir(parents=True,exist_ok=True)
    with target.open('xb') as f:f.write(q.stdout)
    target.chmod(0o755);alias.symlink_to(target.name)
    notice.parent.mkdir(parents=True,exist_ok=True)
    with notice.open('xb') as f:f.write(checked(packages/license['name'],license))
    notice.chmod(0o644)
    (output/'device-runtime.json').write_bytes(raw)
    return dict(inputs_sha256=hashlib.sha256(raw).hexdigest(),helper_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),package=entry['nevra'],payload_sha256=entry['payload_sha256'],payload_bytes=entry['payload_size'],license_sha256=license['sha256'])
