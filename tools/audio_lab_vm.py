"""Separate enforcing Fedora audio test carrier. Never selects hardware disks.

The original vm/p0 controller/config/disk remain unchanged. This is development
infrastructure, not the requested Aetheri desktop experiment environment.
"""
import argparse
import hashlib
import http.server
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import threading
import time
import uuid
from functools import partial

import image_state
import prepare_vm
import run_vm

ROOT=Path(__file__).resolve().parents[1]
DIRECTORY=ROOT/'vm/audio-lab'
SERIAL='AETH_AUDIO_LAB'
DISK=32*1024**3
PRODUCT='Aetheri audio lab VM'


def runtime():
    name='_aetheri_audio_lab_runtime_'+uuid.uuid4().hex
    spec=importlib.util.spec_from_file_location(name,run_vm.__file__)
    module=importlib.util.module_from_spec(spec);sys.modules[name]=module;spec.loader.exec_module(module)
    module.VM=DIRECTORY;module.SSH_PORT=19223;module.HTTP_PORT=19447
    module.config=lambda:load(module)
    return module


def identity(path):
    s=path.lstat()
    if path.is_symlink() or not stat.S_ISREG(s.st_mode) or s.st_nlink!=1 or not s.st_ino:raise ValueError('different lab file object')
    return dict(device=s.st_dev,inode=s.st_ino)


def validate(value):
    if not isinstance(value,dict) or type(value.get('format')) is not int or value.get('format')!=1 or value.get('profile')!='audio-lab' or type(value.get('disk_bytes')) is not int or value.get('disk_bytes')!=DISK or value.get('disk_serial')!=SERIAL or value.get('stage') not in ('preparing','ready'):
        raise ValueError('not an audio-lab registration')
    if str(uuid.UUID(value['uuid']))!=value['uuid'] or uuid.UUID(value['uuid']).version!=4:raise ValueError('invalid lab UUID')
    if value['stage']=='ready':
        if set(value.get('objects',{}))!={'system.qcow2','nvram.fd','id_ed25519','id_ed25519.pub','http/carrier.ks'}:raise ValueError('incomplete lab objects')
        for item in value['objects'].values():
            if set(item)!={'device','inode'} or any(type(n) is not int or n<0 for n in item.values()) or not item['inode']:raise ValueError('invalid lab object witness')
    return value


def load(module):
    value=validate(image_state.read(module.owned(module.VM/'config.json')))
    if value['stage']!='ready':raise ValueError('unfinished lab preparation; inspect without replay')
    for name,expected in value['objects'].items():
        if identity(module.owned(module.VM/name))!=expected:raise ValueError('registered lab object replaced: '+name)
    template=(ROOT/'spec/audio-lab.ks.in').read_bytes().replace(b'\r\n',b'\n')
    if hashlib.sha256(template).hexdigest()!=value['template_sha256']:raise ValueError('lab template changed; existing guest is not reconfigured')
    if hashlib.sha256((module.VM/'http/carrier.ks').read_bytes()).hexdigest()!=value['kickstart_sha256']:raise ValueError('registered lab kickstart changed')
    return value


def verify_inputs():
    pins=json.loads((ROOT/'spec/vm-inputs.json').read_text())
    iso=next(e for e in pins['inputs'] if e['name']=='Fedora-Server-netinst-x86_64-44-1.7.iso')
    prepare_vm.verify(ROOT/'downloads'/iso['name'],iso)
    receipt=json.loads((ROOT/'local/verification/host-tools.json').read_text())
    for name,digest in receipt['files'].items():
        if name.startswith(('local/tools/qemu-11.1.0/','local/tools/7zip-26.05/')):
            p=ROOT/name
            if p.is_symlink() or not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=digest:raise ValueError('changed reviewed host tool: '+name)


def prepare(module):
    verify_inputs();directory=module.owned(module.VM)
    if directory.resolve()!=directory:raise ValueError('aliased lab directory')
    directory.mkdir(parents=True,exist_ok=True);module.private_path(directory,directory=True)
    if (directory/'config.json').exists():
        value=load(module)
    else:
        if {p.name for p in directory.iterdir()}-{'launch.lock'}:raise ValueError('unregistered lab contents; do not adopt')
        value=dict(format=1,profile='audio-lab',uuid=str(uuid.uuid4()),disk_serial=SERIAL,disk_bytes=DISK,stage='preparing',objects={})
        image_state.write(directory/'config.json',value)
        key=directory/'id_ed25519'
        subprocess.run(['ssh-keygen.exe','-q','-t','ed25519','-N','','-C','aetheri-audio-lab','-f',str(key)],check=True,creationflags=module.FLAGS)
        module.private_path(key)
        public=(directory/'id_ed25519.pub').read_text().strip()
        if not re.fullmatch(r'ssh-ed25519 [A-Za-z0-9+/=]+ aetheri-audio-lab',public):raise ValueError('lab key format')
        (directory/'http').mkdir()
        template=(ROOT/'spec/audio-lab.ks.in').read_bytes().replace(b'\r\n',b'\n')
        body=template.decode().replace('@SSH_PUBLIC_KEY@',public).encode()
        with (directory/'http/carrier.ks').open('xb') as stream:stream.write(body);stream.flush();os.fsync(stream.fileno())
        subprocess.run([str(module.IMAGE),'create','-f','qcow2',str(directory/'system.qcow2'),'32G'],check=True,creationflags=module.FLAGS)
        with (directory/'nvram.fd').open('xb') as stream:stream.write((module.TOOLS/'share/edk2-i386-vars.fd').read_bytes());stream.flush();os.fsync(stream.fileno())
        value.update(objects={name:identity(directory/name) for name in ('system.qcow2','nvram.fd','id_ed25519','id_ed25519.pub','http/carrier.ks')},template_sha256=hashlib.sha256(template).hexdigest(),kickstart_sha256=hashlib.sha256(body).hexdigest(),stage='ready')
        image_state.write(directory/'config.json',value)
    info=json.loads(subprocess.check_output([str(module.IMAGE),'info','--output=json',str(directory/'system.qcow2')],creationflags=module.FLAGS))
    if info['format']!='qcow2' or info['virtual-size']!=DISK or info.get('backing-filename') or info.get('snapshots') or info.get('encrypted') or info.get('format-specific',{}).get('data',{}).get('data-file'):
        raise ValueError('unexpected lab disk dependencies')
    return value


def command(module,install):
    value=load(module);directory=module.VM;tools=module.TOOLS
    args=[str(module.QEMU),'-name','aetheri-audio-lab','-uuid',value['uuid'],'-machine','pc-q35-11.1,accel=whpx','-cpu','host,vmx=off,svm=off','-smp','2','-m','4096M',
        '-smbios','type=1,product='+PRODUCT,'-no-user-config',
        '-drive',f'if=pflash,format=raw,readonly=on,file={tools / "share/edk2-x86_64-code.fd"}',
        '-drive',f'if=pflash,format=raw,file={directory / "nvram.fd"}',
        '-drive',f'if=none,id=labdisk,format=qcow2,file={directory / "system.qcow2"}',
        '-device','virtio-blk-pci,drive=labdisk,serial='+SERIAL,
        '-netdev',f'user,id=net0,{"" if install else "restrict=on,"}hostfwd=tcp:127.0.0.1:{module.SSH_PORT}-:22','-device','virtio-net-pci,netdev=net0',
        '-display','none','-serial',f'file:{directory / ("install-serial.log" if install else "serial.log")}',
        '-qmp',f'unix:{directory / "qmp.sock"},server=on,wait=off','-no-reboot']
    if install:
        kernel=directory/'installer/images/pxeboot/vmlinuz'
        if not kernel.exists():
            subprocess.run([str(module.ARCHIVER),'x','-y',f'-o{directory / "installer"}',str(ROOT/'downloads/Fedora-Server-netinst-x86_64-44-1.7.iso'),'images/pxeboot/*'],check=True,creationflags=module.FLAGS,stdout=subprocess.DEVNULL)
        args+=['-drive',f'file={ROOT / "downloads/Fedora-Server-netinst-x86_64-44-1.7.iso"},media=cdrom,readonly=on,format=raw','-kernel',str(kernel),'-initrd',str(kernel.with_name('initrd.img')),'-append',f'inst.stage2=hd:LABEL=Fedora-S-dvd-x86_64-44 inst.ks=http://10.0.2.2:{module.HTTP_PORT}/carrier.ks ip=dhcp console=ttyS0,115200n8 inst.text inst.noninteractive']
    return args


def installation_complete(record,serial,identifier):
    """Installer termination permits a first boot, not runtime acceptance.

    Kickstart redirects %post output into the guest's private post log. Its
    application marker is therefore checked over SSH after boot, never assumed
    to appear on the installer console.
    """
    text=re.sub(r'\x1b\[[0-9;?]*[A-Za-z]','',serial).replace('\r','')
    return (record.get('uuid')==identifier and record.get('install') is True
        and record.get('state')=='exited' and type(record.get('exit')) is int
        and record['exit']==0 and 'Running post-installation scripts' in text
        and '\nComplete!\n' in text and 'reboot: Power down' in text)


def require_installation(module):
    receipt=module.VM/'installation.json'
    serial=(module.VM/'install-serial.log').read_bytes()
    digest=hashlib.sha256(serial).hexdigest();identifier=load(module)['uuid']
    if receipt.exists():
        value=image_state.read(receipt)
        if value.get('serial_sha256')!=digest or not installation_complete(value.get('process',{}),serial.decode(errors='replace'),identifier):
            raise ValueError('different original installation evidence')
        return
    record=image_state.read(module.VM/'process.json')
    if not installation_complete(record,serial.decode(errors='replace'),identifier):raise ValueError('no completed original lab installer')
    if image_state.read(module.VM/'last-command.json')!={'arguments':command(module,True)}:raise ValueError('different original installer command')
    image_state.write(receipt,dict(process=record,serial_sha256=digest,runtime_accepted=False))


def start(module,install):
    with module.launch_guard():
        record=module.VM/'process.json'
        if record.exists():
            old=image_state.read(record)
            if 'pid' not in old or module.pid_alive(old['pid']):raise ValueError('original lab start unresolved/live; do not retry')
            if install:raise ValueError('lab installation is never automatically repeated')
        if (module.VM/'config.json').exists() and module.running():raise ValueError('original lab is running')
        if (module.VM/'qmp.sock').exists():raise ValueError('original lab socket needs reconciliation')
        module.free_ports(module.SSH_PORT,*([module.HTTP_PORT] if install else []))
        prepare(module)
        if not install:require_installation(module)
        args=command(module,install);image_state.write(module.VM/'last-command.json',dict(arguments=args))
        server=None
        if install:
            server=http.server.ThreadingHTTPServer(('127.0.0.1',module.HTTP_PORT),partial(http.server.SimpleHTTPRequestHandler,directory=str(module.VM/'http')))
            threading.Thread(target=server.serve_forever,daemon=True).start()
        try:
            with (module.VM/('install-qemu.log' if install else 'qemu.log')).open('ab') as log:
                value=dict(uuid=load(module)['uuid'],state='starting',install=install,started_unix=time.time())
                image_state.write(record,value)
                process=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=log,stderr=log,creationflags=module.FLAGS)
                value.update(pid=process.pid,state='spawned');image_state.write(record,value)
                print(json.dumps(value),flush=True)
                result=process.wait();value.update(state='exited',exit=result);image_state.write(record,value)
                print(json.dumps(value),flush=True);return result
        finally:
            if server:server.shutdown();server.server_close()


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('prepare','install','boot','status','ssh','poweroff'));parser.add_argument('remote',nargs=argparse.REMAINDER);args=parser.parse_args()
    module=runtime()
    if args.action=='prepare':
        with module.launch_guard():print(json.dumps(prepare(module),indent=2))
        return 0
    if args.action in ('install','boot'):return start(module,args.action=='install')
    if not (module.VM/'config.json').exists():
        if args.action=='status':print(json.dumps(dict(registered=False)));return 0
        raise ValueError('lab is not registered')
    value=load(module);active=module.running()
    if args.action=='status':
        observed=image_state.read(module.VM/'process.json') if (module.VM/'process.json').exists() else {}
        print(json.dumps(dict(registered=True,uuid=value['uuid'],qmp_verified=active,process=observed,status=module.qmp('query-status') if active else None)));return 0
    if not active:raise ValueError('original lab identity not verified')
    if args.action=='poweroff':print(json.dumps(module.qmp('system_powerdown')));return 0
    return subprocess.call(['ssh.exe','-F','NUL' if os.name=='nt' else '/dev/null','-T','-p',str(module.SSH_PORT),'-i',str(module.VM/'id_ed25519'),'-o','IdentityAgent=none','-o','ForwardAgent=no','-o','ClearAllForwardings=yes','-o','IdentitiesOnly=yes','-o','BatchMode=yes','-o','ConnectTimeout=5','-o','StrictHostKeyChecking=accept-new','-o',f'UserKnownHostsFile={module.VM / "known_hosts"}','aetheri@127.0.0.1',*args.remote],creationflags=module.FLAGS,stdin=sys.stdin,stdout=sys.stdout,stderr=sys.stderr)


if __name__=='__main__':raise SystemExit(main())
