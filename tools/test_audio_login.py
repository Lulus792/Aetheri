"""Explicit PAM/Wayland login probe in the separate enforcing audio lab.

Creates one protected temporary PAM profile and one supervised finite test
process. Retain original pidfd, service/scope and cgroup-event observations until
actual end; then remove only the exact owned profile and restore the original VT.
This is administrative test infrastructure, not production login/app admission.
Failure retains the journal and keeper for independent inspection; never rerun
an uncertain attempt or treat a stop/close/timeout as proof of drain.
"""
from pathlib import Path
import hashlib,json,os,select,subprocess,sys,time,uuid

if len(sys.argv)!=1:raise SystemExit('usage: test_audio_login.py (explicit disposable audio lab only)')
if not __debug__:raise ValueError('run the disposable login checks without Python optimization')
if sys.platform!='linux' or os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('separate root-only enforcing audio lab required')
if any(Path('/etc/pam.d').glob('aetheri-audio-*')):raise RuntimeError('foreign or pending PAM fixture; reconcile first')
sys.dont_write_bytecode=True
from audio_mac_bus import Bus
token=uuid.uuid4().hex;root=Path('/root/aetheri-audio-login-'+token);root.mkdir(mode=0o700)
bus=Bus();name='aetheri-audio-login-'+token+'.service';pam=Path('/etc/pam.d')/('aetheri-audio-'+token)
record=dict(token=token,unit=name,pam=str(pam),boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),state='preparing')
def save():
 with (root/'record.json').open('w') as f:json.dump(record,f,indent=2);f.flush();os.fsync(f.fileno())
def run(*args):return subprocess.check_output(args,text=True,timeout=10).strip()
def facts():
 raw=run('systemctl','show',name,'-p','MainPID','-p','InvocationID','-p','ControlGroup','-p','ActiveState','-p','SubState','-p','Job','-p','ExecMainStatus','-p','Result')
 return dict(line.split('=',1) for line in raw.splitlines())
try:
 save();old=Path('/sys/class/tty/tty0/active').read_text().strip();assert old.startswith('tty') and old[3:].isdigit();record['old_vt']=old;save()
 content=b'auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so type=wayland class=user\n'
 with pam.open('xb') as f:f.write(content);f.flush();os.fsync(f.fileno())
 pam.chmod(0o444);run('restorecon',str(pam));st=pam.stat();record['pam_object']=[st.st_dev,st.st_ino];record['pam_sha256']=hashlib.sha256(content).hexdigest();save()
 run('chvt','8')
 program="import json,os,subprocess,time;print(json.dumps(dict(pid=os.getpid(),uid=os.getuid(),gid=os.getgid(),context=open('/proc/self/attr/current').read().strip(),login=subprocess.check_output(['loginctl','show-session','self','-p','Id','-p','User','-p','Active','-p','Remote','-p','Seat','-p','Type','-p','Class','-p','Scope','-p','Service','-p','TTY'],text=True))),flush=True);time.sleep(10)"
 properties=[('Description','s','Aetheri explicit local Wayland login probe'),('Type','s','exec'),('User','s','aetheri'),('Group','s','aetheri'),('PAMName','s',pam.name),('TTYPath','s','/dev/tty8'),('StandardInput','s','tty'),('NoNewPrivileges','b',True),('CapabilityBoundingSet','t',0),('AmbientCapabilities','t',0),('KillMode','s','control-group'),('TimeoutStopUSec','t',5000000),('Environment','as',['XDG_SESSION_TYPE=wayland','XDG_SESSION_CLASS=user','XDG_SEAT=seat0','XDG_VTNR=8']),('ExecStart','a(sasb)',[('/usr/bin/python3',['/usr/bin/python3','-c',program],False)])]
 if True:
  properties.extend([('StandardOutput','s','journal'),('StandardError','s','journal'),('SyslogIdentifier','s',name)])
  record['state']='dispatch-pending';save();job=bus.create(name,properties);record.update(state='dispatch-acknowledged',job=job);save()
  bus.unit('RefUnit',name);record['ref_acknowledged']=True;save()
  deadline=time.monotonic()+5
  raw=''
  while not raw:
   log=run('journalctl','-b','-t',name,'-o','cat','--no-pager')
   rows=[line for line in log.splitlines() if line.startswith('{')]
   if rows:raw=rows[-1];break
   assert time.monotonic()<deadline,(facts(),run('journalctl','-b','-u',name,'-n','15','--no-pager'));time.sleep(.025)
  value=json.loads(raw);observed=facts();assert value['uid']==value['gid']==1000 and observed['MainPID']==str(value['pid']),observed
  pin=os.pidfd_open(value['pid']);assert not select.select([pin],[],[],0)[0]
  record.update(actor=observed,process=value,pidfd_inode=os.fstat(pin).st_ino)
  status={k:v.strip() for k,v in (line.split(':',1) for line in Path('/proc/'+str(value['pid'])+'/status').read_text().splitlines() if ':' in line)}
  assert all(status[k].split()==['1000']*4 for k in ('Uid','Gid'))
  assert all(int(status[k],16)==0 for k in ('CapEff','CapPrm','CapInh','CapAmb')) and status['NoNewPrivs']=='1'
  record['ordinary_context']={k:status[k] for k in ('Uid','Gid','CapEff','CapPrm','CapInh','CapAmb','NoNewPrivs')};save()
  login=dict(line.split('=',1) for line in value['login'].splitlines())
  assert all(login[k]==v for k,v in {'User':'1000','Active':'yes','Remote':'no','Seat':'seat0','Type':'wayland','Class':'user','TTY':'tty8'}.items()),login
  assert observed['ControlGroup'].startswith('/system.slice/'+name)
  groups={name:observed['ControlGroup']}
  scope=login['Scope'];assert scope.startswith('session-') and scope.endswith('.scope')
  scope_facts=dict(line.split('=',1) for line in run('systemctl','show',scope,'-p','ControlGroup','-p','InvocationID','-p','ActiveState','-p','Job').splitlines())
  assert scope_facts['ActiveState']=='active' and scope_facts['ControlGroup'].startswith('/user.slice/user-1000.slice/')
  bus.unit('RefUnit',scope);groups[scope]=scope_facts['ControlGroup'];record['scope_facts']=scope_facts
  held={}
  for unit,path in groups.items():
   group=Path('/sys/fs/cgroup'+path);groupfd=os.open(group,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC)
   events=os.open('cgroup.events',os.O_RDONLY|os.O_CLOEXEC,dir_fd=groupfd);st=os.fstat(groupfd);held[unit]=(group,groupfd,events,[st.st_dev,st.st_ino])
  record['cgroups']={name:dict(path=str(v[0]),object=v[3]) for name,v in held.items()};save()
  deadline=time.monotonic()+15
  while not select.select([pin],[],[],0)[0] or facts()['ActiveState']!='inactive' or facts()['Job']:
   assert time.monotonic()<deadline,facts();time.sleep(.025)
  final=facts();assert final['InvocationID']==observed['InvocationID'] and final['ExecMainStatus']=='0' and final['Result']=='success' and final['MainPID']=='0',final
  for unit,(group,groupfd,events,identity) in held.items():
   assert [os.fstat(groupfd).st_dev,os.fstat(groupfd).st_ino]==identity
   try:
    data=os.pread(events,4096,0).decode();assert group.is_dir() and [group.stat().st_dev,group.stat().st_ino]==identity and 'populated 0' in data
    record['cgroups'][unit]['end']='same-original-empty'
   except OSError as error:
    import errno
    assert error.errno==errno.ENODEV and not group.exists()
    record['cgroups'][unit]['end']='missing-name-and-original-reader-ENODEV'
   os.close(events);os.close(groupfd)
  save()
  deadline=time.monotonic()+15
  while subprocess.run(['loginctl','show-session',login['Id']],capture_output=True).returncode==0:
   assert time.monotonic()<deadline,'original local session remains';time.sleep(.025)
  os.close(pin);record.update(state='original-ended',final=final);save();bus.unit('UnrefUnit',scope);bus.unit('UnrefUnit',name);bus.close()
  assert pam.stat().st_uid==0 and [pam.stat().st_dev,pam.stat().st_ino]==record['pam_object'] and hashlib.sha256(pam.read_bytes()).hexdigest()==record['pam_sha256'];pam.unlink()
  run('chvt',old[3:]);record['state']='retired';save()
  print(json.dumps(dict(root=str(root),record=record)),flush=True)

except BaseException as error:
 record['inspection_required']=True;record['error']=repr(error);record['keeper_pid']=os.getpid();save()
 print(json.dumps(dict(root=str(root),record=record)),flush=True)
 # Keep original process/cgroup/bus handles after an uncertain test effect.
 # Independent root inspection or an observed original-VM shutdown is required;
 # no replay, automatic stop, identity adoption or cleanup follows this wait.
 import signal
 while True:signal.pause()
