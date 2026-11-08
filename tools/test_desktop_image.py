"""Exercise a NEW private desktop VM with local Unix QMP and owned data only.

Every input is dispatched once and journaled before submission. Failed/unknown
attempts retain their data and are never restarted by this controller. This is
an explicit development-image test, not physical-media installation or recovery.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import uuid

import run_vm as vm
import test_boot_probe as boot
import test_data_probe as data_test
import uefi_probe_image as media


def validate_volume(manifest):
    nonce=manifest.get('nonce');volume=manifest.get('data_volume')
    if not isinstance(nonce,str) or not re.fullmatch('[0-9a-f]{32}',nonce) or not isinstance(volume,dict):
        raise ValueError('invalid fixed desktop/data profile')
    if type(volume.get('size')) is not int or volume['size']!=256*1024**2 or volume.get('serial')!='AETHDATA'+nonce[:12]:
        raise ValueError('invalid fixed data extent/serial')
    if not isinstance(volume.get('initial_sha256'),str) or not re.fullmatch('[0-9a-f]{64}',volume['initial_sha256']):
        raise ValueError('invalid initial data digest')
    if not isinstance(volume.get('uuid'),str) or str(uuid.UUID(volume['uuid']))!=volume['uuid']:
        raise ValueError('noncanonical data UUID')
    return volume


def check_device_filter(serial,manifest,boot_id):
    nonce=re.escape(manifest['nonce']);identifier=re.escape(boot_id)
    lines=[]
    for mode,read in [('baseline',1),('denied',0)]:
        # ForwardToConsole adds the journal timestamp/identifier before messages.
        line=r'\bAETHERI_DEVICE_PASS nonce='+nonce+' mode='+mode+' block_read='+str(read)+r' null_rw=1 cap_zero=1 nnp=1 boot='+identifier+r'\s*$'
        matches=list(re.finditer(line,serial,re.MULTILINE))
        if len(matches)!=1:raise ValueError('missing/duplicate device-policy observation')
        lines.append(matches[0].start())
    all_results=re.findall(r'\bAETHERI_DEVICE_.*$',serial,re.MULTILINE)
    if len(all_results)!=2 or lines[0]>=lines[1] or 'cgroup BPF features disabled' in serial:
        raise ValueError('unproven device-filter sequence')
    return True


def query(path,expected_uuid,process,command,arguments=None):
    if process.poll() is not None:raise RuntimeError('original VM is terminal')
    with vm.qmp_socket_at(path) as sock:
        stream=sock.makefile('rwb',buffering=0)
        def read():
            for _ in range(128):
                raw=stream.readline(1024**2)
                if not raw or not raw.endswith(b'\n'):raise RuntimeError('incomplete QMP reply')
                value=json.loads(raw)
                if 'event' not in value:return value
            raise RuntimeError('QMP event bound')
        if 'QMP' not in read():raise ValueError('not a QMP endpoint')
        def send(name,args=None):
            body=dict(execute=name)
            if args is not None:body['arguments']=args
            stream.write(json.dumps(body).encode()+b'\r\n')
            response=read()
            if 'error' in response:raise RuntimeError(str(response['error']))
            if 'return' not in response:raise RuntimeError('unrecognized QMP response')
            return response['return']
        send('qmp_capabilities')
        identity=send('query-uuid')
        if identity.get('UUID','').lower()!=expected_uuid.lower():raise ValueError('foreign QMP UUID')
        return send(command,arguments)


def command(image,attempt,data,serial,identifier):
    return [str(boot.TOOLS/'qemu-system-x86_64.exe'),'-name','aetheri-desktop-image',
            '-uuid',identifier,'-machine','pc-q35-11.1,accel=whpx','-cpu','host,vmx=off,svm=off',
            '-smp','2','-m','1536M','-no-user-config','-nodefaults','-display','none',
            '-monitor','none','-nic','none','-no-reboot','-device','VGA',
            '-drive','if=pflash,format=raw,readonly=on,file='+str(boot.TOOLS/'share/edk2-x86_64-code.fd'),
            '-drive','if=pflash,format=raw,file='+str(attempt/'nvram.fd'),
            '-drive','if=none,id=bootmedia,format=raw,readonly=on,file='+str(image/'boot.raw'),
            '-device','virtio-blk-pci,drive=bootmedia,serial=AETHERI_ROOT_PROBE,bootindex=1',
            '-drive','if=none,id=userdata,format=raw,file='+str(data),
            '-device','virtio-blk-pci,drive=userdata,serial='+serial,
            '-serial','file:'+str(attempt/'serial.log'),
            '-qmp','unix:'+str(attempt/'qmp.sock')+',server=on,wait=off']


def run(image,output,timeout=180):
    output=vm.owned(output)
    if any(',' in str(p) for p in (image,output,boot.TOOLS)) or len(str(output/'boot-1/qmp.sock').encode())>=108:
        raise ValueError('unsupported QEMU path')
    manifest=media.inspect(image)
    if manifest.get('proof')!='desktop-image':raise ValueError('expected graphical image profile')
    volume=validate_volume(manifest);original=image/'data.raw'
    if original.is_symlink() or not original.is_file() or original.stat().st_size!=volume['size'] or boot.sha(original)!=volume['initial_sha256']:
        raise ValueError('changed initial data input')
    hashes=boot.host_tools();output.mkdir(mode=0o700);vm.private_path(output,directory=True)
    state=dict(state='preparing',manifest_sha256=boot.sha(image/'manifest.json'),host_tools=hashes,controller_sha256=boot.sha(Path(__file__)),qmp_adapter_sha256=boot.sha(Path(vm.__file__)),attempts=[])
    def record():vm.write_state(output/'sequence.json',state)
    record();data=output/'data.raw'
    with data.open('x+b') as held:
        with original.open('rb') as source:shutil.copyfileobj(source,held,1024**2)
        held.flush();os.fsync(held.fileno())
        expected=data_test.identity(os.fstat(held.fileno()))
        if boot.sha(data)!=volume['initial_sha256']:raise ValueError('changed data copy')
        state['data_object']=expected;record()
        for number in (1,2):
            data_test.check_held(held,data,expected)
            attempt=output/('boot-'+str(number));attempt.mkdir(mode=0o700)
            shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd',attempt/'nvram.fd')
            identifier=str(uuid.uuid4());args=command(image,attempt,data,volume['serial'],identifier)
            current=dict(number=number,uuid=identifier,command=args,state='dispatch_pending',actions=[])
            state['attempts'].append(current);state.update(state='dispatch_pending',pending=number);record()
            started=time.monotonic()
            with (attempt/'qemu.log').open('xb') as log:
                process=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=log,stderr=log,creationflags=vm.FLAGS)
                current.update(state='running',pid=process.pid);record()
                print(json.dumps(dict(event='started',number=number,pid=process.pid,attempt=str(attempt))),flush=True)
                def serial():
                    path=attempt/'serial.log'
                    if not path.exists():return ''
                    if path.stat().st_size>8*1024**2:raise RuntimeError('serial output bound')
                    return path.read_text(errors='replace')
                def wait_for(value):
                    deadline=time.monotonic()+timeout
                    while time.monotonic()<deadline:
                        body=serial()
                        if value in body:return body
                        if process.poll() is not None:raise RuntimeError('VM ended before '+value)
                        if 'AETHERI_DEVICE_FAIL' in body or 'AETHERI_DESKTOP_FAIL' in body or 'AETHERI_DESKTOP_END status=failed' in body:raise RuntimeError('native desktop/device check rejected startup/use')
                        time.sleep(.2)
                    raise TimeoutError('not observed: '+value)
                def action(name,qmp,args=None):
                    entry=dict(name=name,command=qmp,state='pending');current['actions'].append(entry);record()
                    result=query(attempt/'qmp.sock',identifier,process,qmp,args)
                    entry['state']='acknowledged';record();return result
                def chord(*keys):
                    events=[dict(type='key',data=dict(down=down,key=dict(type='qcode',data=key))) for down,values in ((True,keys),(False,reversed(keys))) for key in values]
                    action('keys:'+','.join(keys),'input-send-event',dict(events=events));time.sleep(.08)
                def text(value):
                    plain={' ':'spc','-':'minus','.':'dot','/':'slash',"'":'apostrophe','\\':'backslash'}
                    for c in value:
                        if c=='>':chord('shift','dot')
                        elif c in plain:chord(plain[c])
                        elif re.fullmatch('[a-z0-9]',c):chord(c)
                        else:raise ValueError('unsupported fixed test text')
                def capture(name):
                    path=attempt/(name+'.png');action(name,'screendump',dict(filename=str(path),format='png'))
                    raw=path.read_bytes()
                    if len(raw)<1024 or raw[:8]!=b'\x89PNG\r\n\x1a\n':raise RuntimeError('invalid native capture')
                    current.setdefault('captures',{})[path.name]=dict(sha256=hashlib.sha256(raw).hexdigest(),dimensions=struct.unpack('>II',raw[16:24]));record()
                def launch(name):
                    chord('meta_l','spc');time.sleep(.3);text(name);chord('ret')
                    # Settings is the native NexaUI client; its actual frame
                    # contract differs from the SDL Terminal/Files diagnostics.
                    wait_for('settings: SETTLED ' if name=='settings' else name+': READY')
                    time.sleep(.4);capture(name)
                try:
                    body=wait_for('AETHERI_DESKTOP_READY uid=1000 cap_zero=1 seat=seat0')
                    root=re.findall(r'^AETHERI_ROOT_PASS nonce='+manifest['nonce']+r' .*ext4_ro=1 write_erofs=1 cgroup2=1 boot=([0-9a-f-]{36})\s*$',body,re.MULTILINE)
                    if len(root)!=1 or (number==2 and root==state['attempts'][0].get('boot_ids')):raise RuntimeError('missing/distorted root or cold-boot context')
                    check_device_filter(body,manifest,root[0])
                    current['boot_ids']=root;capture('desktop')
                    launch('terminal')
                    text("printf 'aetheri-image\\n' > image-note.txt" if number==1 else 'cat image-note.txt');chord('ret');time.sleep(1);capture('terminal-document')
                    chord('ctrl','shift','q');wait_for('terminal: CLOSED');wait_for('desktop: LAUNCH_REPLY X T')
                    launch('files');chord('ctrl','q');wait_for('files: CLOSED');wait_for('desktop: LAUNCH_REPLY X F')
                    launch('settings');chord('ctrl','q');wait_for('desktop: LAUNCH_REPLY X S')
                    chord('meta_l','spc');time.sleep(.2);chord('ctrl','q');wait_for('AETHERI_DESKTOP_END status=ok')
                    action('powerdown','system_powerdown')
                    while process.poll() is None and time.monotonic()-started<timeout*3:time.sleep(.2)
                    if process.poll() is None:raise TimeoutError('original guest did not power off')
                    body=serial()
                    if process.returncode!=0 or 'reboot: Power down' not in body or 'EXT4-fs (vdb): unmounting filesystem' not in body or 'Failed unmounting' in body:
                        raise RuntimeError('clean graphical shutdown not proven')
                    check_device_filter(body,manifest,root[0])
                    data_test.check_held(held,data,expected)
                    current.update(state='accepted',exit=process.returncode,seconds=time.monotonic()-started,serial_sha256=boot.sha(attempt/'serial.log'),data_sha256=boot.sha(data))
                    state.update(state='boot_accepted',pending=None);record();print(json.dumps(dict(event='accepted',number=number,boot_ids=root)),flush=True)
                except BaseException as error:
                    current.update(state='failed_preserved',error=str(error),unknown_effects=True)
                    if process.poll() is None:process.terminate();process.wait(timeout=30)
                    current.update(exit=process.poll(),seconds=time.monotonic()-started);state['state']='failed_preserved';record()
                    raise
        if media.inspect(image)!=manifest:raise ValueError('read-only system artifact changed')
        state.update(state='accepted',data_final_sha256=boot.sha(data));record()
    return state


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.image.resolve(),a.output.absolute())
