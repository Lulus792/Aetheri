"""Two desktop boots without a UART, with explicit native capture review gates.

Engineering capture review is not user design approval. Final acceptance also
requires an independent read-only ext4/proof-file inspection after both boots.
Unknown attempts remain preserved; no restart, reset, TCP or physical disk.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import uuid

import image_qmp
import image_io
import image_state
import run_vm as vm
import test_boot_probe as boot
import test_data_probe as data_test
import test_usb_desktop as usb
import uefi_probe_image as media


def require_headroom(free,allocation):
    # The lower floor is only for a kernel-confirmed bounded sparse NTFS source.
    # It still leaves >7 times the maximum possible source data before guest use.
    if type(free) is not int or free<4*1024**3 or (free<8*1024**3 and (type(allocation) is not int or not 0<=allocation<=512*1024**2)):
        raise OSError(28,'image test needs 8 GiB, or 4 GiB with <=512 MiB proven sparse source')


def retire_failed(process,monitor,current,save):
    """Recording failure must not skip retirement through the ORIGINAL handle."""
    issues=[]
    try:save()
    except BaseException as error:issues.append('record: '+str(error)[:512])
    try:
        if process.poll() is None:process.terminate();process.wait(timeout=30)
    except BaseException as error:issues.append('retire: '+str(error)[:512])
    finally:
        try:current['exit']=process.poll()
        except BaseException as error:current['exit']=None;issues.append('poll: '+str(error)[:512])
        try:save()
        except BaseException as error:issues.append('terminal-record: '+str(error)[:512])
        if monitor:
            try:monitor.close()
            except BaseException as error:issues.append('close-record: '+str(error)[:512])
    return issues


def command(image,attempt,medium,identifier):
    args=usb.command(image,attempt,medium,identifier);result=[];at=0
    while at<len(args):
        if args[at]=='-serial':at+=2;continue
        result.append(args[at]);at+=1
    return result+['-serial','none','-parallel','none']


def run(image,output):
    output=vm.owned(output)
    if len(str(output/'boot-1/qmp.sock').encode())>=108 or any(',' in str(p) for p in (image,output,boot.TOOLS)):raise ValueError('unsupported QEMU path')
    free=shutil.disk_usage(output.parent).free
    with (image/'boot.raw').open('rb') as source:
        ranges=image_io.allocated_ranges(source,0,os.fstat(source.fileno()).st_size)
    allocation=None if ranges is None else sum(end-start for start,end in ranges)
    require_headroom(free,allocation)
    manifest=media.inspect(image);volume=usb.admit(manifest)
    if manifest.get('usb_console_fallback') is not True or manifest.get('laptop_drm_selection')!='session-primary-auto':raise ValueError('use the explicit laptop console/DRM profile')
    tools=boot.host_tools();output.mkdir(mode=0o700);vm.private_path(output,directory=True)
    state=dict(state='copy_pending',pending='medium',manifest_sha256=boot.sha(image/'manifest.json'),controller_sha256=boot.sha(Path(__file__)),qmp_sha256=boot.sha(Path(image_qmp.__file__)),host_tools=tools,attempts=[],free_before=free,source_allocation=allocation)
    def save():image_state.write(output/'sequence.json',state)
    save();medium=output/'medium.raw'
    with medium.open('x+b') as held:
        usb.copy(image/'boot.raw',held,volume);held.flush();os.fsync(held.fileno())
        expected=data_test.identity(os.fstat(held.fileno()));state['data_object']=expected
        for number in (1,2):
            attempt=output/('boot-'+str(number));attempt.mkdir(mode=0o700);vm.private_path(attempt,directory=True)
            shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd',attempt/'nvram.fd')
            identifier=str(uuid.uuid4());args=command(image,attempt,medium,identifier)
            current=dict(number=number,state='dispatch_pending',uuid=identifier,command=args,qmp={},captures={});state['attempts'].append(current);state['pending']=number;save()
            with (attempt/'qemu.log').open('xb') as log:
                process=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
                monitor=None
                try:
                    current.update(state='running',pid=process.pid);save()
                    start=time.monotonic()
                    while not (attempt/'qmp.sock').exists():
                        if process.poll() is not None or time.monotonic()-start>30:raise RuntimeError('original QMP did not appear')
                        time.sleep(.1)
                    monitor=image_qmp.Connection(attempt/'qmp.sock',identifier,process,current['qmp'],save)
                    def pause(seconds):
                        until=time.monotonic()+seconds
                        while time.monotonic()<until:
                            if process.poll() is not None:raise RuntimeError('original VM ended before poweroff')
                            monitor.poll(min(.2,max(0,until-time.monotonic())))
                    def chord(*keys):
                        down=[dict(type='key',data=dict(down=True,key=dict(type='qcode',data=k))) for k in keys]
                        up=[dict(type='key',data=dict(down=False,key=dict(type='qcode',data=k))) for k in reversed(keys)]
                        monitor.execute('input-send-event',dict(events=down));pause(.015)
                        monitor.execute('input-send-event',dict(events=up));pause(.015)
                    def text(value):
                        simple={' ':'spc','-':'minus','.':'dot','/':'slash',"'":'apostrophe','\\':'backslash','=':'equal',',':'comma'}
                        for c in value:
                            if c=='>':chord('shift','dot')
                            elif c=='_':chord('shift','minus')
                            elif c in simple:chord(simple[c])
                            elif 'A'<=c<='Z':chord('shift',c.lower())
                            elif 'a'<=c<='z' or '0'<=c<='9':chord(c)
                            else:raise ValueError('unsupported fixed input')
                    def review(name):
                        path=attempt/(name+'.png')
                        if path.exists() or path.is_symlink():raise ValueError('capture must be new')
                        monitor.execute('screendump',dict(filename=str(path),format='png'))
                        raw=path.read_bytes()
                        if not 1024<=len(raw)<=8*1024**2 or raw[:8]!=b'\x89PNG\r\n\x1a\n' or struct.unpack('>II',raw[16:24])!=(1280,800):raise ValueError('unexpected native capture')
                        digest=hashlib.sha256(raw).hexdigest();ack=attempt/(name+'.review.json')
                        current['captures'][name]=dict(sha256=digest,dimensions=[1280,800]);current['phase']='review:'+name;save()
                        print(json.dumps(dict(event='capture_review',boot=number,name=name,path=str(path),sha256=digest)),flush=True)
                        deadline=time.monotonic()+240
                        while not ack.exists():
                            if time.monotonic()>deadline:raise TimeoutError('native capture review timed out')
                            pause(.2)
                        if ack.is_symlink() or ack.stat().st_size>4096 or json.loads(ack.read_text())!={'observed':name,'capture_sha256':digest}:raise ValueError('foreign capture review')
                        current['captures'][name]['review_sha256']=boot.sha(ack);current['phase']='reviewed:'+name;save()
                    pause(60);review('desktop')
                    chord('meta_l','spc');pause(.3);text('terminal');chord('ret');pause(5);review('terminal')
                    commands=["printf 'aetheri-image\\n' > image-note.txt" if number==1 else 'cat image-note.txt',
                        f'cat /proc/sys/kernel/random/boot_id > boot-id-{number}.txt',f'cat /proc/self/status > process-{number}.txt',
                        f'cat /proc/mounts > mounts-{number}.txt',f'cat /sys/class/tty/console/active > console-{number}.txt',
                        f'systemctl show aetheri-usb-data-ready.service aetheri-device-baseline.service aetheri-device-denied.service --property=Id,ActiveState,Result,ExecMainStatus > services-{number}.txt',
                        'cat image-note.txt']
                    for value in commands:text(value);chord('ret');pause(.5)
                    review('document');chord('ctrl','shift','q');pause(2)
                    chord('meta_l','spc');pause(.2);chord('ctrl','q');pause(3)
                    current['phase']='powerdown_pending';save();monitor.execute('system_powerdown')
                    deadline=time.monotonic()+180
                    while process.poll() is None and time.monotonic()<deadline:
                        try:monitor.poll(.2)
                        except (EOFError,ConnectionResetError):
                            process.wait(timeout=10);break
                    if process.poll() is None:raise TimeoutError('original VM did not power off')
                    # The original process can exit before the final queued event
                    # is read; drain this same connection, never reconnect.
                    final_deadline=time.monotonic()+2
                    while not image_qmp.guest_poweroff(current['qmp']['events']) and time.monotonic()<final_deadline:
                        try:monitor.poll(.1)
                        except (EOFError,ConnectionResetError):break
                    if process.returncode!=0 or not image_qmp.guest_poweroff(current['qmp']['events']):raise RuntimeError('guest poweroff cause not proven')
                    data_test.check_held(held,medium,expected);usb.check_immutable(medium,manifest)
                    current.update(state='observed',exit=process.returncode,phase='offline_proof_required',**usb.final_hashes(medium,manifest));state['pending']=None;save()
                except BaseException as error:
                    current.update(state='failed_preserved',error=str(error));state['state']='failed_preserved'
                    issues=retire_failed(process,monitor,current,save);monitor=None
                    failure=dict(state='failed_preserved',error=str(error)[:1024],exit=current['exit'],secondary=issues)
                    try:
                        with (attempt/'failure.json').open('x') as record:
                            json.dump(failure,record,indent=2);record.flush();os.fsync(record.fileno())
                    except BaseException as recording_error:
                        print(json.dumps(dict(failure=failure,recording_error=str(recording_error)[:512])),file=sys.stderr,flush=True)
                    raise
                finally:
                    if monitor:monitor.close()
        if media.inspect(image)!=manifest:raise ValueError('source image changed')
        state.update(state='observed_requires_offline_verification',pending=None,**usb.final_hashes(medium,manifest));save()
    return state


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.image.resolve(),a.output.absolute())
