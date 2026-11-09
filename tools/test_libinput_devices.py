"""Explicit root-only upstream libinput test in the disposable audio lab.

Requires a fresh, independently selected, completed and root-sealed build.
No P0 carrier, normal installation, policy changes or automatic failure cleanup.
Unknown effects keep the original controller/bus/handles alive for inspection.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import re
import stat
import subprocess
import time
import uuid

from audio_mac_bus import Bus
import image_state


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT, timeout=30).strip()


def facts(unit):
    keys='Id,LoadState,ActiveState,SubState,MainPID,ControlPID,InvocationID,ControlGroup,Job,ExecMainStatus'
    return dict(line.split('=',1) for line in run('systemctl','show',unit,'--property='+keys).splitlines())


def identity(path):
    s=Path(path).lstat()
    return [s.st_dev,s.st_ino,s.st_mode,s.st_uid,s.st_gid,s.st_rdev]


def devices():
    return {str(p):dict(identity=identity(p),target=str(p.resolve()),
                       name=(p/'device/name').read_text().strip() if (p/'device/name').exists() else None)
            for p in sorted(Path('/sys/class/input').iterdir())}


def input_nodes():
    return {str(p):identity(p) for p in sorted(Path('/dev/input').rglob('*'))}


def rules():
    p=Path('/run/udev/rules.d')
    if p.exists():
        for q in p.iterdir():
            s=q.lstat()
            if not stat.S_ISREG(s.st_mode) or s.st_nlink!=1 or s.st_size>1024**2:raise ValueError('foreign/special rule entry')
    return {str(q):dict(identity=identity(q),sha256=hashlib.sha256(q.read_bytes()).hexdigest())
            for q in sorted(p.iterdir())} if p.exists() else {}


def verify_build(root, expected):
    """Caller-selected hashes precede privileged execution; no self-adoption."""
    if root.is_symlink() or not re.fullmatch(r'a-li-[0-9a-f]{12}',root.name) or root.parent!=Path('/run'):
        raise ValueError('fresh audio-lab build root required')
    found=set()
    for p in [root,*root.rglob('*')]:
        s=p.lstat()
        if s.st_uid or s.st_gid or (not p.is_symlink() and s.st_mode&0o222):
            raise ValueError('unsealed build entry: '+str(p))
        if p.is_symlink():
            if not p.resolve(strict=True).is_relative_to(root):raise ValueError('external build link')
        elif p.is_file():
            n=p.relative_to(root).as_posix();found.add(n)
            if n not in expected:raise ValueError('extra build file: '+n)
            e=expected[n]
            with p.open('rb') as f:
                if os.fstat(f.fileno()).st_nlink!=1 or s.st_size!=e['size'] or hashlib.file_digest(f,'sha256').hexdigest()!=e['sha256']:
                    raise ValueError('different selected build file: '+n)
        elif not p.is_dir():raise ValueError('special build entry')
    if found!=set(expected):raise ValueError('incomplete build selection')


def parse_summary(text,status):
    summary=text.rsplit('\nsummary:\n',1)[-1] if '\nsummary:\n' in text else ''
    pairs=re.findall(r'^  (completed|pass|na|fail|skip): (\d+)$',summary,re.M)
    counts={k:int(v) for k,v in pairs}
    valid=(len(pairs)==5 and set(counts)=={'completed','pass','na','fail','skip'}
        and counts['completed']>0 and sum(counts[k] for k in ('pass','na','fail','skip'))==counts['completed'])
    passed=valid and status==0 and counts['fail']==0 and re.search(r'^  status: PASS$',summary,re.M) is not None
    return dict(counts=counts,coverage_complete=passed and counts['pass']==counts['completed'],upstream_passed=passed)


class Fixture:
    def __init__(self,selection,output,group):
        if os.name!='posix' or os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM':
            raise ValueError('disposable audio lab root only')
        if output.parent!=Path('/root') or not re.fullmatch(r'aetheri-libinput-devices-[0-9a-f]{32}',output.name):
            raise ValueError('fresh protected test output required')
        if Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('enforcing required')
        self.selection=selection;self.root=Path(selection['root']);self.output=output
        if Path('/proc/sys/kernel/random/boot_id').read_text().strip()!=selection['boot']:raise ValueError('different original boot')
        if group not in ('lua','keyboard','pointer','touchpad'):raise ValueError('unsupported test group')
        verify_build(self.root,selection['files'])
        if selection.get('library_directory')!='release':raise ValueError('separately selected Release library required; never select a counter build')
        self.library=self.root/'release'
        if not self.library.resolve().is_relative_to(self.root):raise ValueError('external runtime library directory')
        configuration=(self.root/'candidate/build/config.h').read_text()
        if '#define IS_DEBUG_BUILD 1' not in configuration:raise ValueError('upstream device runner requires its build-directory profile')
        output.mkdir(mode=0o700);self.bus=Bus();self.private=None
        self.unit='aetheri-libinput-'+uuid.uuid4().hex+'.service';self.group=group
        self.held=[];self.record=dict(format=1,boot=selection['boot'],root=str(self.root),group=group,unit=self.unit,state='preparing',pending=None)
        self.save()

    def save(self):image_state.write(self.output/'controller.json',self.record)

    def effect(self,kind,call):
        if self.record['pending'] is not None:raise RuntimeError('unsettled effect; never replay')
        self.record['pending']=kind;self.save();result=call()
        self.record['pending']=None;self.save();return result

    def pin(self,path,flags=None):
        if flags is None:flags=os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC
        fd=os.open(path,flags);self.held.append(fd);return fd

    def check_udev(self):
        now=facts('systemd-udevd.service')
        for key in ('MainPID','InvocationID','ControlGroup'):
            if now[key]!=self.udev[key]:raise RuntimeError('original udev manager changed')
        if select.select([self.udev_pin],[],[],0)[0]:raise RuntimeError('original udev manager exited')

    def execute(self):
        self.udev=facts('systemd-udevd.service')
        if self.udev['ActiveState']!='active' or self.udev['MainPID']=='0':raise RuntimeError('udev unavailable')
        self.udev_pin=os.pidfd_open(int(self.udev['MainPID']));self.held.append(self.udev_pin);self.check_udev()
        run('udevadm','settle','--timeout=20')
        self.record['baseline']=dict(devices=devices(),input_nodes=input_nodes(),rules=rules(),litest=sorted(str(p) for p in Path('/run').glob('litest-*')),
            uinput_loaded=Path('/sys/module/uinput').exists(),udev=self.udev)
        self.save()
        if self.record['baseline']['rules'] or self.record['baseline']['litest']:
            raise RuntimeError('foreign litest/rule state; refuse shared fixture')
        parent=self.pin('/run/udev');parent_id=os.fstat(parent)
        self.own_rules=not Path('/run/udev/rules.d').exists()
        self.record['created_rules_directory']=self.own_rules;self.save()
        if self.own_rules:self.effect('mkdir-rules',lambda:os.mkdir('rules.d',0o755,dir_fd=parent))
        rulesfd=self.pin('/run/udev/rules.d');rules_id=os.fstat(rulesfd)
        self.record['rules_directory']=[rules_id.st_dev,rules_id.st_ino];self.save()
        if not self.record['baseline']['uinput_loaded']:
            self.effect('load-uinput',lambda:run('modprobe','uinput'))
        module=self.pin('/sys/module/uinput');module_id=os.fstat(module)
        # Standard existing labels only, on our own sealed inputs. No policy module.
        build=self.root/'candidate/build'
        helpers=[build/n for n in ('libinput-test-suite','libinput-device-group','libinput-fuzz-extract','libinput-fuzz-to-zero')]
        libraries=list(dict.fromkeys([build/'libinput.so.10.13.0',self.library/'libinput.so.10.13.0',*sorted((self.root/'candidate/sdk/usr/lib64').glob('*.so*'))]))
        for p,kind in [(p,'bin_t') for p in helpers]+[(p,'lib_t') for p in libraries if not p.is_symlink()]:
            if p.read_bytes()[:4]!=b'\x7fELF':raise ValueError('different helper/library ELF')
            self.effect('label-owned-input:'+str(p),lambda p=p,kind=kind:run('chcon','system_u:object_r:'+kind+':s0',str(p)))
        executable=self.root/'candidate/build/libinput-test-suite'
        args=[str(executable),'--jobs=1','--verbose','--filter-group='+self.group]
        env=['LC_ALL=C','PATH=/usr/bin:/bin','LD_LIBRARY_PATH='+str(self.library)+':'+str(self.root/'candidate/sdk/usr/lib64')]
        if facts(self.unit)['LoadState']!='not-found':raise RuntimeError('foreign unit')
        with (self.output/'test.log').open('xb') as stream:
            # PID1 must be allowed to receive/write this original FD. Keep the
            # restrictive policy; use its existing log type on our fresh file.
            self.effect('label-owned-log',lambda:run('chcon','system_u:object_r:var_log_t:s0',str(self.output/'test.log')))
            props=[('Type','s','exec'),('RemainAfterExit','b',True),('Restart','s','no'),('KillMode','s','control-group'),
                ('StandardInput','s','null'),('StandardOutputFileDescriptor','h',stream.fileno()),('StandardErrorFileDescriptor','h',stream.fileno()),
                ('WorkingDirectory','s',str(self.root/'candidate/source')),('Environment','as',env),('ExecStart','a(sasb)',[(str(executable),args,False)])]
            self.record['arguments']=args;self.save()
            self.private=Bus(private=True)
            self.record['job']=self.effect('start-test',lambda:self.private.create(self.unit,props))
        self.effect('ref-test',lambda:self.bus.unit('RefUnit',self.unit))
        value=facts(self.unit)
        if not value['InvocationID'] or value['MainPID']=='0':raise RuntimeError('test ended before original observation')
        pid=int(value['MainPID']);pin=os.pidfd_open(pid);self.held.append(pin)
        directory=self.pin('/sys/fs/cgroup'+value['ControlGroup']);events=os.open('cgroup.events',os.O_RDONLY|os.O_CLOEXEC,dir_fd=directory);self.held.append(events)
        again=facts(self.unit)
        if any(again[k]!=value[k] for k in ('MainPID','InvocationID','ControlGroup')):raise RuntimeError('test changed while pinning')
        self.record.update(state='testing',original=value,pid=pid,label=Path('/proc/'+str(pid)+'/attr/current').read_text().rstrip('\0\n'));self.save()
        deadline=time.monotonic()+1200
        while not select.select([pin],[],[],0)[0]:
            self.check_udev()
            maps=Path('/proc/'+str(pid)+'/maps')
            try:raw=maps.read_text()
            except FileNotFoundError:raw=''
            if str(self.root/'candidate/sdk/usr/lib64/liblua-5.5.so') in raw and str(self.library/'libinput.so') in raw:
                self.record['maps']=raw
            if time.monotonic()>deadline:raise TimeoutError('original test still running; preserve')
            time.sleep(.05)
        deadline=time.monotonic()+10
        while True:
            value=facts(self.unit)
            if value['MainPID']==value['ControlPID']=='0' and not value['Job']:break
            if time.monotonic()>deadline:raise TimeoutError('original test supervisor exit')
            time.sleep(.02)
        self.record['terminal']=value;self.save()
        if value['InvocationID']!=self.record['original']['InvocationID'] or value['MainPID']!=value['ControlPID'] or value['MainPID']!='0' or value['Job']:
            raise RuntimeError('test supervisor not terminal')
        try:
            if 'populated 0' not in os.pread(events,4096,0).decode():raise RuntimeError('original test descendants remain')
        except OSError as e:
            if e.errno!=errno.ENODEV or Path('/sys/fs/cgroup'+self.record['original']['ControlGroup']).exists():raise
        # Retire only an independently observed empty original unit, including a
        # known failed test. Failure is recorded; no test rerun follows.
        self.effect('stop-empty-test',lambda:self.bus.stop(self.unit))
        deadline=time.monotonic()+10
        while True:
            try:os.pread(events,4096,0);gone=False
            except OSError as e:
                if e.errno!=errno.ENODEV:raise
                gone=True
            if gone and not Path('/sys/fs/cgroup'+self.record['original']['ControlGroup']).exists():break
            if time.monotonic()>deadline:raise TimeoutError('original cgroup remains')
            time.sleep(.02)
        if facts(self.unit)['ActiveState']=='failed':self.effect('reset-known-test',lambda:self.bus.unit('ResetFailedUnit',self.unit))
        self.effect('unref-test',lambda:self.bus.unit('UnrefUnit',self.unit))
        deadline=time.monotonic()+5
        while facts(self.unit)['LoadState']!='not-found':
            if time.monotonic()>deadline:raise TimeoutError('test unit GC')
            time.sleep(.02)
        run('udevadm','settle','--timeout=20');self.check_udev()
        after=dict(devices=devices(),input_nodes=input_nodes(),rules=rules(),litest=sorted(str(p) for p in Path('/run').glob('litest-*')))
        self.record['after']=after;self.save()
        for key in after:
            if after[key]!=self.record['baseline'][key]:raise RuntimeError('test left changed '+key)
        for p in Path('/proc').glob('[0-9]*/maps'):
            try:raw=p.read_text()
            except (FileNotFoundError,ProcessLookupError):continue
            if str(self.root)+'/' in raw:raise RuntimeError('remaining build/helper mapping: '+str(p))
        if not self.record['baseline']['uinput_loaded']:
            current=Path('/sys/module/uinput').stat()
            if (current.st_dev,current.st_ino)!=(module_id.st_dev,module_id.st_ino) or Path('/sys/module/uinput/refcnt').read_text().strip()!='0' or list(Path('/sys/module/uinput/holders').iterdir()):
                raise RuntimeError('original uinput module has holders or changed')
            self.effect('unload-owned-uinput',lambda:run('modprobe','-r','uinput'))
            if Path('/sys/module/uinput').exists():raise RuntimeError('uinput remains')
        if self.own_rules:
            current=os.stat('rules.d',dir_fd=parent)
            if (current.st_dev,current.st_ino)!=(rules_id.st_dev,rules_id.st_ino) or list(Path('/run/udev/rules.d').iterdir()):raise RuntimeError('original rules directory changed')
            if (Path('/run/udev').stat().st_dev,Path('/run/udev').stat().st_ino)!=(parent_id.st_dev,parent_id.st_ino):raise RuntimeError('udev parent changed')
            self.effect('remove-owned-empty-rules',lambda:os.rmdir('rules.d',dir_fd=parent))
        verify_build(self.root,self.selection['files'])
        self.record.update(parse_summary((self.output/'test.log').read_text(),int(value['ExecMainStatus'])))
        self.record['state']='retired';self.record['passed']=self.record['upstream_passed'] and 'maps' in self.record;self.save()
        for fd in self.held:os.close(fd)
        self.held=[];self.private.close();self.bus.close()
        return self.record['passed']


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('selection',type=Path);p.add_argument('output',type=Path);p.add_argument('--group',default='lua')
    a=p.parse_args();fixture=Fixture(json.loads(a.selection.read_bytes()),a.output,a.group)
    try:passed=fixture.execute()
    except BaseException as error:
        fixture.record.update(state='inspection-required',error=repr(error));fixture.save()
        print(json.dumps(dict(output=str(a.output),error=repr(error),preserved=True)),flush=True)
        while True:signal.pause()
    print(json.dumps(dict(output=str(a.output),passed=passed)),flush=True)
    raise SystemExit(0 if passed else 1)
