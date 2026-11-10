"""Finite, explicit protected-runtime KMS desktop test on the disposable carrier.

Uses the real development supervisor and native apps, PAM/logind and actual host
hardware input. No normal service promotion, public launch API or USB writer.
Unknown effects retain the original Root keeper, pidfds and cgroup observations.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import stat
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))  # Root-copied test source under -I.

import development_runtime
import development_session
import image_state
from audio_mac_bus import Bus
from test_compositor_libinput import Fixture, current_mappings, run
from test_libinput_devices import devices, input_nodes, rules, facts, verify_build


class SessionBus(Bus):
    def value(self, message, signature, value):
        if signature != 'a(ss)':
            return super().value(message, signature, value)
        self.open(message, 'a', '(ss)')
        for path, options in value:
            self.open(message, 'r', 'ss')
            self.append(message, 's', path); self.append(message, 's', options)
            self.end(message)
        self.end(message)


def session(selection, user_root, token):
    value = json.loads(Path(selection).read_bytes())
    if os.getuid() != 1000 or os.geteuid() != 1000 or os.getgid() != 1000 or os.getegid() != 1000:
        raise ValueError('ordinary selected desktop session required')
    development_session.USER_ROOT = Path(user_root)
    # Test diagnostics only; the compositor remains the normal no-injection app.
    development_session.emit_child_line = lambda role, line: print(role+': '+line.decode(errors='replace'), flush=True)
    print('SESSION_CONTEXT '+json.dumps(dict(pid=os.getpid(), login=run('loginctl', 'show-session', 'self',
        '-p','Id','-p','User','-p','Active','-p','Remote','-p','Seat','-p','Type','-p','Class','-p','Scope','-p','TTY'))), flush=True)
    # Test-only start gate: Root retains the actual session/scope before any
    # compositor effect, including a startup failure before READY.
    os.kill(os.getpid(), signal.SIGSTOP)
    return development_session.supervise('release', token, desktop=True,
        settings_ui='nexaui-owned', settings_binding=value['role_bindings']['settings'],
        files_ui='nexaui-owned', files_binding=value['role_bindings']['files'],
        terminal_ui='nexaui-owned', terminal_binding=value['role_bindings']['terminal'],
        temporary_profile=development_session.TEMPORARY_PROFILE, keyboard_layout='us',
        keyboard_binding=development_session.keyboard_fingerprint('us'), current_runtime=value['runtime_binding'])


class DesktopFixture(Fixture):
    def __init__(self, selection, output):
        # Validate the existing protected runtime/input/Wacom/seat prerequisites
        # and retain their original strict shapes before creating this test.
        super().__init__(selection, output)
        value = self.value
        if value.get('input_profile') != 'current-desktop':
            raise ValueError('current runtime selection required')
        selected = development_runtime.Runtime(value['runtime_binding'], 'release', True, *('nexaui-owned',)*3)
        if selected.root != Path(value['gui']) or selected.value != self.profile:
            raise ValueError('different desktop and input runtime')
        bindings = {r:development_session.profile_fingerprint(selected.selection(r)) for r in ('settings','files','terminal')}
        if value['role_bindings'] != bindings:
            raise ValueError('different independently selected app bindings')
        self.selected = selected
        self.private.close(); self.private = SessionBus(private=True)

    def actors(self, text):
        """Enroll actual original children, once, before their later end."""
        expected = dict(compositor=self.selected.build/'aetheri-compositor', desktop=self.selected.build/'aetheri-desktop',
                        **{r:self.selected.build/n for r,n in development_runtime.PROGRAMS.items()})
        for role, number in re.findall(r'^SPAWN (compositor|desktop|files|settings|terminal) (\d+)$', text, re.M):
            name = role+'-'+number
            if name in self.record['actors']: continue
            pid = int(number)
            parent = int(Path('/proc',number,'stat').read_text().rsplit(')',1)[1].split()[1])
            if parent != self.record['actors']['session']['pid']:
                raise ValueError('selected app is not an original supervisor child')
            self.record['actors'][name] = self.pin(name, pid, expected[role])
        for number in re.findall(r'^files: OPEN pid=(\d+) pin=1$', text, re.M):
            name = 'document-terminal-'+number
            if name in self.record['actors']: continue
            parent = int(Path('/proc',number,'stat').read_text().rsplit(')',1)[1].split()[1])
            if not any(n.startswith('files-') and a['pid']==parent for n,a in self.record['actors'].items()):
                raise ValueError('document Terminal lacks its original Files parent')
            self.record['actors'][name] = self.pin(name, int(number), expected['terminal'])
        for owner, kind, number in re.findall(r'^(files|terminal): READY wayland opengles2 (editor|shell)=(\d+)$', text, re.M):
            name = kind+'-'+number
            if name in self.record['actors']: continue
            parent = int(Path('/proc',number,'stat').read_text().rsplit(')',1)[1].split()[1])
            prefix = 'document-terminal-' if owner=='files' else 'terminal-'
            if not any(n.startswith(prefix) and a['pid']==parent for n,a in self.record['actors'].items()):
                raise ValueError('PTY child lacks its original Terminal parent')
            self.record['actors'][name] = self.pin(name, int(number), self.selected.build/('aetheri-editor' if kind=='editor' else 'aetheri-shell'))

    def current(self):
        original = facts(self.unit); scope = facts(self.scope)
        if any(original[k] != self.record['original'][k] for k in ('MainPID','InvocationID','ControlGroup')):
            raise ValueError('original desktop service changed')
        if any(scope[k] != self.record['scope_original'][k] for k in ('InvocationID','ControlGroup')) or scope['ActiveState']!='active':
            raise ValueError('original desktop scope changed')
        for name, actor in self.record['actors'].items():
            if select.select([self.pins[name]],[],[],0)[0]:
                raise ValueError('an original workflow actor ended before the finish request')
            if Path('/proc',str(actor['pid']),'cgroup').read_text().strip()!='0::'+scope['ControlGroup']:
                raise ValueError('original workflow actor left the scope')

    def execute(self):
        self.record['baseline'] = dict(devices=devices(), nodes=input_nodes(), rules=rules(),
            udev=run('systemctl','show','systemd-udevd.service','-p','MainPID','-p','InvocationID'))
        self.old_vt = Path('/sys/class/tty/tty0/active').read_text().strip()
        if not re.fullmatch('tty[0-9]+',self.old_vt): raise ValueError('unknown original VT')
        # Leave room for the session token and Wayland's 108-byte socket path.
        self.runtime = Path('/run/a-cd-'+self.token[:12])
        self.effect('create-runtime', lambda:self.runtime.mkdir(mode=0o700)); os.chown(self.runtime,1000,1000)
        user_root = self.runtime/'sessions'; user_root.mkdir(mode=0o700); os.chown(user_root,1000,1000)
        base = user_root/self.token
        for p in (base,base/'runtime',base/'cache',base/'documents'):
            p.mkdir(mode=0o700); os.chown(p,1000,1000)
        document = base/'documents/alpha.txt'
        with document.open('xb') as f: f.write(b'alpha\n')
        document.chmod(0o600); os.chown(document,1000,1000)
        self.pam = Path('/etc/pam.d')/('aetheri-input-'+self.token)
        content = b'auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so type=wayland class=user\n'
        def create_pam():
            with self.pam.open('xb') as f: f.write(content); f.flush(); os.fsync(f.fileno())
            self.pam.chmod(0o444); run('restorecon',str(self.pam))
        self.effect('create-pam', create_pam); s=self.pam.stat(); self.pam_identity=(s.st_dev,s.st_ino)
        self.record.update(runtime=str(self.runtime),base=str(base),old_vt=self.old_vt,pam=str(self.pam),pam_object=list(self.pam_identity));self.save()
        self.effect('activate-test-vt',lambda:run('chvt','8'))
        args=['/usr/bin/python3','-I','-B',str(Path(__file__).resolve()),'--session',str(self.selection),str(user_root),self.token]
        env=['XDG_SESSION_TYPE=wayland','XDG_SESSION_CLASS=user','XDG_SEAT=seat0','XDG_VTNR=8',
             'XDG_CONFIG_HOME='+self.value['wacom']['root']+'/usr/share','PATH=/usr/bin:/bin','PYTHONDONTWRITEBYTECODE=1']
        props=[('Description','s','Aetheri explicit current desktop workflow fixture'),('Type','s','notify'),('NotifyAccess','s','main'),
            ('User','s','1000'),('Group','s','1000'),('PAMName','s',self.pam.name),('TTYPath','s','/dev/tty8'),('StandardInput','s','tty'),
            ('NoNewPrivileges','b',True),('CapabilityBoundingSet','t',0),('AmbientCapabilities','t',0),('Restart','s','no'),
            ('KillMode','s','control-group'),('StandardOutput','s','journal'),('StandardError','s','journal'),('SyslogIdentifier','s',self.unit),
            ('WorkingDirectory','s',str(base)),('Environment','as',env),
            ('TemporaryFileSystem','a(ss)',[('/tmp','rw,nosuid,nodev,size=1G,mode=1777')]),('ExecStart','a(sasb)',[('/usr/bin/python3',args,False)])]
        if facts(self.unit)['LoadState']!='not-found':raise ValueError('foreign service name')
        self.effect('start-session',lambda:self.private.create(self.unit,props))
        self.effect('ref-service',lambda:self.bus.unit('RefUnit',self.unit));self.refs.append(self.unit)
        self.wait(lambda:'SESSION_CONTEXT ' in self.journal() and facts(self.unit)['MainPID']!='0','original stopped session context',15)
        text=self.journal(); rows=re.findall(r'^SESSION_CONTEXT (.+)$',text,re.M)
        if len(rows)!=1:raise ValueError('ambiguous original session context')
        info=json.loads(rows[0]);original=facts(self.unit)
        if original['MainPID']!=str(info['pid']) or original['ActiveState']!='activating':raise ValueError('different original starting desktop service')
        self.record['original']=original
        login=dict(line.split('=',1) for line in info['login'].splitlines())
        if any(login[k]!=v for k,v in dict(User='1000',Active='yes',Remote='no',Seat='seat0',Type='wayland',Class='user',TTY='tty8').items()):
            raise ValueError('different actual desktop login context')
        self.scope=login['Scope'];self.effect('ref-scope',lambda:self.bus.unit('RefUnit',self.scope));self.refs.append(self.scope)
        self.record.update(login=login,scope_original=facts(self.scope),actors={})
        self.record['actors']['session']=self.pin('session',info['pid'],'/usr/bin/python3')
        for unit in (self.unit,self.scope):
            observed=facts(unit);path=Path('/sys/fs/cgroup'+observed['ControlGroup'])
            fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC);events=os.open('cgroup.events',os.O_RDONLY|os.O_CLOEXEC,dir_fd=fd)
            self.groups[unit]=(path,fd,events,(os.fstat(fd).st_dev,os.fstat(fd).st_ino))
        self.record['groups']={n:dict(path=str(a[0]),object=list(a[3])) for n,a in self.groups.items()}
        self.save()
        if Path('/proc',str(info['pid']),'stat').read_text().rsplit(')',1)[1].split()[0] not in ('T','t'):
            raise ValueError('original session did not retain its test-only start gate')
        self.effect('continue-original-session',lambda:signal.pidfd_send_signal(self.pins['session'],signal.SIGCONT))
        self.wait(lambda:facts(self.unit)['ActiveState']=='active' and not facts(self.unit)['Job'],'actual full desktop readiness',30)
        self.actors(self.journal())
        self.record['state']='awaiting-desktop-workflow';self.save();self.current()
        print(json.dumps(dict(output=str(self.output),base=str(base),unit=self.unit,state=self.record['state'])),flush=True)
        deadline=time.monotonic()+600
        while not (self.output/'finish').exists():
            self.actors(self.journal());self.current();self.save()
            if time.monotonic()>deadline:raise TimeoutError('actual desktop workflow not supplied; preserve originals')
            time.sleep(.1)
        self.actors(self.journal());self.current()
        marker=self.output/'finish';s=marker.lstat()
        if not stat.S_ISREG(s.st_mode) or s.st_uid or s.st_nlink!=1 or marker.read_bytes()!=b'finish\n':raise ValueError('invalid protected finish request')
        if document.read_bytes()!=b'hello alpha\n' or (base/'owner-proof').read_bytes()!=b'desktop':raise ValueError('actual workflow contents incomplete')
        required=('files-','terminal-','settings-','document-terminal-','editor-','shell-')
        if any(not any(n.startswith(prefix) for n in self.record['actors']) for prefix in required):raise ValueError('actual workflow actors incomplete')
        self.record['code_mappings']={}
        for name,actor in self.record['actors'].items():
            if name=='session':continue
            maps=Path('/proc',str(actor['pid']),'maps').read_text()
            (self.output/(name+'-maps.txt')).write_text(maps)
            self.record['code_mappings'][name]=current_mappings(self.value,self.profile,maps)
        self.save()
        self.effect('finish-original-supervisor',lambda:signal.pidfd_send_signal(self.pins['session'],signal.SIGTERM))
        self.wait(lambda:all(select.select([p],[],[],0)[0] for p in self.pins.values()) and facts(self.unit)['ActiveState']=='inactive' and not facts(self.unit)['Job'],'all original workflow actors end',30)
        final=facts(self.unit);text=self.journal()
        rows=re.findall(r'^SESSION_END (.+)$',text,re.M)
        if final['InvocationID']!=original['InvocationID'] or final['ExecMainStatus']!='0' or len(rows)!=1:raise ValueError('original desktop supervisor failed')
        result=json.loads(rows[0])
        if any(code!=0 for code in result['children'].values()) or re.search(r'AddressSanitizer|LeakSanitizer|runtime error:',text):raise ValueError('workflow cleanup failed')
        for name,(path,fd,events,identity) in self.groups.items():
            if (os.fstat(fd).st_dev,os.fstat(fd).st_ino)!=identity:raise ValueError('original group handle changed')
            try:
                raw=os.pread(events,4096,0).decode();s=path.stat()
                if (s.st_dev,s.st_ino)!=identity or 'populated 0' not in raw:raise ValueError('original group not empty')
                end='same-original-empty'
            except OSError as error:
                if error.errno!=errno.ENODEV or path.exists():raise
                end='original-reader-ENODEV-and-name-absent'
            self.record['groups'][name]['end']=end
        self.wait(lambda:subprocess.run(['loginctl','show-session',login['Id']],capture_output=True).returncode!=0,'original local session removed')
        run('udevadm','settle','--timeout=10')
        after=dict(devices=devices(),nodes=input_nodes(),rules=rules(),udev=run('systemctl','show','systemd-udevd.service','-p','MainPID','-p','InvocationID'))
        if after!=self.record['baseline']:raise ValueError('original input/udev baseline changed')
        (self.output/'session.log').write_text(text);self.record.update(state='original-ended',final=final,after=after,result=result);self.save()
        for unit in reversed(self.refs):self.effect('unref-'+unit,lambda unit=unit:self.bus.unit('UnrefUnit',unit))
        s=self.pam.stat()
        if (s.st_dev,s.st_ino)!=self.pam_identity or s.st_uid or self.pam.read_bytes()!=content:raise ValueError('original PAM fixture changed')
        self.effect('remove-owned-pam',self.pam.unlink);self.effect('restore-original-vt',lambda:run('chvt',self.old_vt[3:]))
        self.selected.recheck();verify_build(Path(self.value['wacom']['root']),self.value['wacom']['files'],'a-wacom-')
        for fd in self.pins.values():os.close(fd)
        for path,fd,events,identity in self.groups.values():os.close(events);os.close(fd)
        self.bus.close();self.private.close();self.record['state']='retired';self.save()
        print(json.dumps(dict(output=str(self.output),state='retired')),flush=True)


def main():
    if len(sys.argv)==5 and sys.argv[1]=='--session':
        return session(*sys.argv[2:])
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('selection',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
    fixture=DesktopFixture(args.selection,args.output)
    try:fixture.execute()
    except BaseException as error:
        fixture.record.update(inspection_required=True,error=repr(error),keeper_pid=os.getpid());fixture.save()
        print(json.dumps(dict(output=str(fixture.output),error=repr(error))),flush=True)
        while True:signal.pause()
    return 0


if __name__=='__main__':raise SystemExit(main())
