"""Prepare the isolated writable data/runtime boot fixture, never a device."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import uuid

import build_root_probe as root
import test_boot_probe as boot

ROOT = Path(__file__).resolve().parents[1]


def build(base, packages, output, prepare=None, geometry="root-v1", *, data_bytes=256*1024**2):
    if type(data_bytes) is not int or not 256*1024**2<=data_bytes<=32*1024**3 or data_bytes%(1024**2):
        raise ValueError('invalid bounded data extent')
    original = boot.inspect(base)
    pins = json.loads((ROOT/"spec/data-probe-inputs.json").read_text())
    payloads = {}
    for p in pins['files']:
        data = Path(p['source']).read_bytes()
        if len(data)!=p['size'] or hashlib.sha256(data).hexdigest()!=p['sha256']:
            raise ValueError('changed data runtime input: '+p['source'])
        payloads[p['source']] = data
    nonce = original['nonce']
    data_uuid = str(uuid.uuid5(uuid.UUID('54669380-2dd7-4cb2-b827-6c152fe66de2'),nonce+'/data'))
    # virtio-blk GET_ID carries at most 20 bytes.
    serial = 'AETHDATA'+nonce[:12]
    if len(serial)!=20:
        raise ValueError('virtio data serial must fit exactly 20 bytes')

    def prepare_data(tree, selected_nonce):
        if selected_nonce!=nonce:
            raise ValueError('wrong root profile')
        logs=output/'data-logs';logs.mkdir()
        env={**os.environ,'LC_ALL':'C','MKE2FS_CONFIG':str(output/'mke2fs.conf')}

        def run(name,args):
            p=subprocess.run(args,capture_output=True,env=env)
            (logs/(name+'.stdout')).write_bytes(p.stdout);(logs/(name+'.stderr')).write_bytes(p.stderr);p.check_returncode();return p.stdout

        for name,data in payloads.items():
            path=tree/name.lstrip('/');path.parent.mkdir(parents=True,exist_ok=True)
            if path.exists():
                if path.read_bytes()!=data:raise ValueError('conflicting root payload')
            else:path.write_bytes(data);path.chmod(0o644 if '/licenses/' in name else 0o755)
        (tree/'usr/lib/systemd/systemd-udevd').symlink_to('../../bin/udevadm')
        if not (tree/'bin').exists():(tree/'bin').symlink_to('usr/bin')
        for name in ('var','home'):(tree/name).mkdir()
        (tree/'etc/passwd').write_text('root:x:0:0:Root:/root:/nonexistent\naetheri:x:1000:1000:Development user:/home/aetheri:/nonexistent\n')
        (tree/'etc/group').write_text('root:x:0:\naetheri:x:1000:\n')
        (tree/'etc/nsswitch.conf').write_text('passwd: files\ngroup: files\nshadow: files\n')
        (tree/'etc/aetheri-data-proof').write_text(nonce+'\n'+data_uuid+'\n'+serial+'\n')
        rules=tree/'etc/udev/rules.d';rules.mkdir(parents=True)
        (rules/'99-aetheri-data.rules').write_text('SUBSYSTEM=="block", TAG+="systemd"\n')
        compiler=['/usr/bin/gcc','-std=c17','-D_GNU_SOURCE','-O2','-g0','-Wl,--build-id=none','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror','-fanalyzer',str(ROOT/'src/boot/data.c'),'-o',str(tree/'usr/libexec/aetheri-data-probe')]
        run('compile',compiler)
        units=tree/'etc/systemd/system'
        (units/'default.target').unlink()
        (units/'default.target').symlink_to('aetheri-data.target')
        (units/'aetheri-root.target').unlink()
        (units/'aetheri-root.service').write_text('[Unit]\nDescription=Aetheri readonly root preflight\nDefaultDependencies=no\n[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/libexec/aetheri-root-probe\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n')
        definitions={
            'aetheri-udev.service': '[Unit]\nDescription=Aetheri development device events\nDefaultDependencies=no\n[Service]\nType=notify\nExecStart=/usr/lib/systemd/systemd-udevd\nRuntimeDirectory=udev\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n',
            'aetheri-udev-trigger.service':'[Unit]\nDefaultDependencies=no\nRequires=aetheri-udev.service\nAfter=aetheri-udev.service\n[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/bin/udevadm trigger --action=add --subsystem-match=block\nExecStart=/usr/bin/udevadm settle --timeout=20\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n',
            'aetheri-data-check.service':'[Unit]\nDefaultDependencies=no\nRequires=aetheri-root.service aetheri-udev-trigger.service\nAfter=aetheri-root.service aetheri-udev-trigger.service\nFailureAction=poweroff-immediate\n[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/libexec/aetheri-data-probe --check\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n',
            'var.mount':'[Unit]\nDefaultDependencies=no\nRequires=aetheri-data-check.service\nAfter=aetheri-data-check.service\n[Mount]\nWhat=/dev/vdb\nWhere=/var\nType=ext4\nOptions=rw,nosuid,nodev\nTimeoutSec=20\n',
            'home.mount':'[Unit]\nDefaultDependencies=no\nRequires=var.mount\nAfter=var.mount\n[Mount]\nWhat=/var/home\nWhere=/home\nType=none\nOptions=bind,nosuid,nodev\nTimeoutSec=20\n',
            'tmp.mount':'[Unit]\nDefaultDependencies=no\nRequires=aetheri-data-check.service\nAfter=aetheri-data-check.service\n[Mount]\nWhat=tmpfs\nWhere=/tmp\nType=tmpfs\nOptions=mode=1777,size=64M,nosuid,nodev\n',
            'aetheri-data-user.service':'[Unit]\nDefaultDependencies=no\nRequires=home.mount var.mount tmp.mount\nAfter=home.mount var.mount tmp.mount\n[Service]\nType=oneshot\nUser=aetheri\nGroup=aetheri\nUMask=0077\nNoNewPrivileges=yes\nCapabilityBoundingSet=\nRuntimeDirectory=aetheri-data-user\nRuntimeDirectoryMode=0700\nEnvironment=XDG_RUNTIME_DIR=/run/aetheri-data-user\nWorkingDirectory=/home/aetheri\nExecStart=/usr/libexec/aetheri-data-probe --user\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n',
            'aetheri-data-finish.service':'[Unit]\nDefaultDependencies=no\nRequires=aetheri-data-check.service\nAfter=aetheri-data-check.service aetheri-data-user.service\nSuccessAction=poweroff-immediate\n[Service]\nType=oneshot\nExecStart=/usr/libexec/aetheri-data-probe --finish\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n',
            'aetheri-data.target':'[Unit]\nDescription=Aetheri writable data proof\nDefaultDependencies=no\nWants=aetheri-data-user.service aetheri-data-finish.service\nAfter=aetheri-data-finish.service\n',
        }
        for name,data in definitions.items():(units/name).write_text(data)
        data_tree=output/'data-root';data_tree.mkdir()
        for name,mode in [('home',0o755),('home/aetheri',0o700),('home/aetheri/Documents',0o700)]:
            path=data_tree/name;path.mkdir();path.chmod(mode)
        data_file=output/'data.raw'
        with data_file.open('xb') as f:f.truncate(data_bytes)
        run('mkfs',['/usr/bin/mkfs.ext4','-q','-F','-U',data_uuid,'-L','AETHERI_DATA','-E','root_owner=0:0','-d',str(data_tree),str(data_file)])
        batch=output/'data-owners.debugfs'
        selected=[('/',0),('/home',0),('/home/aetheri',1000),('/home/aetheri/Documents',1000)]
        batch.write_text(''.join(f'set_inode_field "{p}" uid {uid}\nset_inode_field "{p}" gid {uid}\n' for p,uid in selected))
        run('owners',['/usr/bin/debugfs','-w','-f',str(batch),str(data_file)])
        batch.write_text(''.join(f'stat "{p}"\n' for p,uid in selected))
        reports=run('verify',['/usr/bin/debugfs','-f',str(batch),str(data_file)]).decode()
        owners=re.findall(r'User:\s*(\d+)\s+Group:\s*(\d+)',reports)
        if owners!=[(str(uid),str(uid)) for p,uid in selected]:raise ValueError('data ownership mismatch')
        run('fsck',['/usr/bin/e2fsck','-f','-n',str(data_file)])
        addition = dict(proof='writable-data',target='aetheri-data.target',data_volume=dict(uuid=data_uuid,serial=serial,size=data_bytes,initial_sha256=boot.sha(data_file)),data_builder_sha256=boot.sha(Path(__file__)),data_source_sha256=boot.sha(ROOT/'src/boot/data.c'),data_pins_sha256=boot.sha(ROOT/'spec/data-probe-inputs.json'))
        if prepare:
            addition.update(prepare(tree, selected_nonce, addition['data_volume']))
        return addition

    root.build(base,packages,output,prepare=prepare_data,geometry=geometry)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--boot-image',type=Path,required=True);p.add_argument('--packages',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();build(a.boot_image.resolve(),a.packages.resolve(),a.output.absolute())
