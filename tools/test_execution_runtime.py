"""Explicit isolated live native producer/starter probe; never a normal launch.

Only the registered disposable enforcing carrier. Actual separate Root/User
catalogues, registered ProcessFD/unit admission, private-data checks, views/exec
and fresh post-exec app connection. Package/allocation/scaffold are trusted fixed
administrative inputs, not normal session provisioning/readiness. Unknown dispatch
preserves the complete fixture and policy for causal reconciliation.
"""
import bz2
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import sqlite3
import stat
import subprocess
import sys
import uuid
import p0_services as normal
import runtime_contracts as reference
from test_execution_profile import prepare_fixture,identity,label,snapshot

ROOT=Path('/var/lib/aetheri-p0-execution-runtime-test')
MODULE='aetheri_p0_execution_runtime'
CONFIG=Path('/etc/dbus-1/system.d/91-aetheri-p0-execution-runtime.conf')
ROOT_UNIT='aetheri-p0-execution-root.service'
USER_UNIT='aetheri-p0-execution-user.service'
REJECT_UNIT='aetheri-p0-execution-user-reject.service'
SOURCE=Path(__file__).resolve().parents[1]

def run(*args,timeout=60):
    return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT,timeout=timeout).strip()
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def quiet_normal(record):
    normal.verified(record)
    if record['pending'] is not None: raise RuntimeError('normal carrier request pending')
    for role in ('system','user'):
        path=normal.state_path(record,role)/'catalogue.sqlite3'
        with closing(sqlite3.connect('file:'+str(path)+'?mode=ro',uri=True)) as db:
            if db.execute("SELECT count(*) FROM admission_gates WHERE state!='closed'").fetchone()[0]: raise RuntimeError('normal gates must stay closed')
            if role=='system' and (db.execute("SELECT count(*) FROM launch_dispatches WHERE state!='terminal'").fetchone()[0] or db.execute("SELECT count(*) FROM generation_leases WHERE state!='released'").fetchone()[0]):
                raise RuntimeError('normal launch/lease is unsettled')
def save(record):
    content=json.dumps(record,indent=2)
    path=ROOT/'controller.next'
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_CLOEXEC,0o600)
    with os.fdopen(fd,'w') as out: out.write(content); out.flush(); os.fsync(out.fileno())
    os.replace(path,ROOT/'controller.json')
    fd=os.open(ROOT,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC); os.fsync(fd); os.close(fd)
def flush_tree(path):
    for parent,dirs,files in os.walk(path,topdown=False,followlinks=False):
        for name in files:
            fd=os.open(Path(parent)/name,os.O_RDONLY|os.O_NOFOLLOW|os.O_CLOEXEC); os.fsync(fd); os.close(fd)
        fd=os.open(parent,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW|os.O_CLOEXEC); os.fsync(fd); os.close(fd)
def installed():
    value=(Path('/var/lib/selinux/targeted/active/modules/400')/MODULE/'cil').read_bytes()
    return bz2.decompress(value) if value.startswith(b'BZh') else value
def capture(path):
    return {name:[*value[:8],value[8].hex(),value[9].hex() if value[9] is not None else None] for name,value in snapshot(path).items()}
def drained(unit):
    facts=dict(x.split('=',1) for x in run('systemctl','show',unit,'--property=ActiveState,MainPID,ControlGroup').splitlines())
    jobs=json.loads(run('busctl','--system','--json=short','call','org.freedesktop.systemd1','/org/freedesktop/systemd1','org.freedesktop.systemd1.Manager','ListJobs'))
    if facts['ActiveState'] not in ('inactive','failed') or facts['MainPID']!='0' or facts['ControlGroup'] or any(x[1]==unit for x in jobs['data'][0]):
        raise RuntimeError('owned process/unit/cgroup/job not drained: '+unit)
    return facts['ActiveState']
def unit_file(unit):
    if not unit.endswith('.service') or '/' in unit or unit.endswith('.service.service'): raise ValueError('fixed service name required')
    return Path('/run/systemd/system')/unit

def require_empty_capabilities(pid):
    fields={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in Path('/proc',str(pid),'status').read_text().splitlines() if ':' in line}
    selected={key:fields[key] for key in ('CapInh','CapPrm','CapEff','CapBnd','CapAmb')}
    if any(int(value,16)!=0 for value in selected.values()): raise RuntimeError('fixture authority has unexpected capabilities')
    return selected
def write_unit(unit,content,record):
    path=unit_file(unit)
    with path.open('x',encoding='utf-8') as out: out.write(content); out.flush(); os.fsync(out.fileno())
    path.chmod(0o644); label(path,'aetheri_execution_unit_test_t'); record['unit_hashes'][unit]=sha(path); save(record)

def acknowledge_user_start(record,work):
    # Only the explicitly requested protected User unit may create its fresh
    # native catalogue in the independently inspected empty control slot.
    current=capture(work); before=record['tree']; database=record['user_catalogue_rel']
    expected={database,record['user_control_rel']+'/writer.lock'}
    if set(current)-set(before)!=expected or set(before)-set(current): raise RuntimeError('unexpected User bootstrap entries')
    for name,value in current.items():
        if name in expected:
            if value[1:5]!=[1000,1000,stat.S_IFREG|0o600,1] or bytes.fromhex(value[8]) not in (
                b'system_u:object_r:aetheri_user_state_t:s0',b'system_u:object_r:aetheri_user_state_t:s0\0'):
                raise RuntimeError('unexpected fresh User file ownership')
        elif name in ('system/catalogue.sqlite3','system',record['user_control_rel']):
            if value[:5]!=before[name][:5] or value[8]!=before[name][8]: raise RuntimeError('bootstrap object replaced')
        elif value!=before[name]: raise RuntimeError('bootstrap changed another object: '+name)
    profile=(work/'registry/execution.conf').read_bytes()
    with closing(sqlite3.connect('file:'+str(work/database)+'?mode=ro',uri=True)) as db:
        if db.execute('SELECT kind,authority_epoch,owner_scope_ref,runtime_incarnation,current_boot_id,catalogue_revision FROM authority_meta').fetchall()!=[
            ('user',profile[40:56],profile[88:104],profile[56:72],profile[72:88],2)]: raise RuntimeError('User startup context differs')
        if any(db.execute('SELECT count(*) FROM '+table).fetchone()[0] for table in ('install_bindings','data_epochs','launches','operations')):
            raise RuntimeError('User bootstrap performed unrequested lifecycle work')
    record['tree']=current; save(record)
def seed(work,record):
    profile=(work/'registry/execution.conf').read_bytes()
    identifiers=[profile[8+16*n:24+16*n] for n in range(14)]
    account,install,generation=identifiers[5],identifiers[6],identifiers[7]
    with closing(sqlite3.connect(work/'system/catalogue.sqlite3')) as db, db:
        db.execute('PRAGMA foreign_keys=ON')
        db.execute("INSERT INTO components VALUES(?,?,'app','Live inert probe',0,1)",('aetheri.p0.exec',identity(21)))
        db.execute("INSERT INTO artifacts VALUES(?,'aetheri.p0.exec','p0-exec','x86_64-linux-gnu','verified')",(profile[256:288],))
        db.execute("INSERT INTO installs(install_id,component_id,scope_id,state) VALUES(?,'aetheri.p0.exec',?,'preparing')",(install,account))
        db.execute("INSERT INTO generations VALUES(?,?,'aetheri.p0.exec',?,'committed',0,1)",(generation,install,profile[256:288]))
        db.execute("UPDATE installs SET state='active',active_generation_id=?,revision=2 WHERE install_id=?",(generation,install))
    record['seeded']=True; save(record)
def verify_static(record):
    for name,expected in record['static_hashes'].items():
        path=ROOT/name
        if path.is_symlink() or not path.is_file() or sha(path)!=expected: raise RuntimeError('static owned input changed: '+name)
    for unit,expected in record['unit_hashes'].items():
        path=unit_file(unit)
        if path.is_symlink() or sha(path)!=expected: raise RuntimeError('owned unit input changed')
    if sha(CONFIG)!=record['config_sha256'] or hashlib.sha256(installed()).hexdigest()!=record['policy_sha256']:
        raise RuntimeError('temporary bus/policy input changed')
def retire(record):
    info=ROOT.lstat(); marker=(ROOT/'controller.json').lstat()
    if ROOT.is_symlink() or not stat.S_ISDIR(info.st_mode) or info.st_uid!=0 or stat.S_IMODE(info.st_mode)!=0o700 or not stat.S_ISREG(marker.st_mode) or marker.st_uid!=0 or stat.S_IMODE(marker.st_mode)!=0o600 or marker.st_nlink!=1:
        raise RuntimeError('untrusted fixture/controller metadata')
    if record['pending'] is not None or record['state']!='passed': raise RuntimeError('unsettled fixture; manual reconciliation required')
    verify_static(record)
    for unit in record['unit_hashes']:
        state=drained(unit)
        if state=='failed': run('systemctl','reset-failed',unit)
    # Compare the complete owned tree before deleting any entry. Only the isolated
    # native catalogue is allowed to mutate; all other objects retain evidence.
    current=capture(ROOT/'root')
    if set(current)!=set(record['tree']): raise RuntimeError('unknown/missing fixture entry')
    for name,value in current.items():
        previous=record['tree'][name]
        if name in ('system/catalogue.sqlite3',record.get('user_catalogue_rel')):
            if value[:5]!=previous[:5] or value[8]!=previous[8]: raise RuntimeError('catalogue object replaced')
        elif name in ('system',record['payload_rel'],record.get('user_control_rel')):
            if value[:5]!=previous[:5] or value[8]!=previous[8]: raise RuntimeError('mutable directory replaced')
        elif value!=previous: raise RuntimeError('fixture object changed: '+name)
    if {x.name for x in ROOT.iterdir()}!={'controller.json','root','profile','driver',MODULE+'.cil'}: raise RuntimeError('unknown controller entry')
    CONFIG.unlink(); run('busctl','--system','call','org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','ReloadConfig')
    for unit in record['unit_hashes']: unit_file(unit).unlink()
    run('systemctl','daemon-reload')
    run('semodule','-X','400','-r',MODULE)
    for parent,dirs,files in os.walk(ROOT,followlinks=False):
        if not Path(parent).is_symlink(): Path(parent).chmod(0o700)
    shutil.rmtree(ROOT)
def main():
    import fcntl
    if sys.platform!='linux' or os.geteuid()!=0 or not Path('/sys/firmware/efi').is_dir() or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri P0 disposable VM' or run('getenforce')!='Enforcing':
        raise RuntimeError('requires registered enforcing disposable carrier')
    lock=os.open('/etc/aetheri-p0-carrier',os.O_RDONLY|os.O_NOFOLLOW|os.O_CLOEXEC); fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    if sys.argv[1:]==['--retire']:
        record=json.loads((ROOT/'controller.json').read_text()); retire(record); return
    driver,starter,payload,observer=map(lambda x:Path(x).resolve(strict=True),sys.argv[1:])
    for path in (driver,starter,payload):
        if b'libasan.so' in path.read_bytes(): raise RuntimeError('use Release binaries for the confined probe')
    ordinary=normal.load(); quiet_normal(ordinary)
    if ordinary['pending'] is not None or ROOT.exists() or ROOT.is_symlink() or CONFIG.exists() or MODULE in run('semodule','-l').split():
        raise RuntimeError('existing/pending state; no replacement or redispatch')
    if run('busctl','--system','call','org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','NameHasOwner','s','org.aetheri.System1')!='b false':
        raise RuntimeError('normal System1 must be unowned')
    app_unit='aetheri-app-'+identity(31).hex()+'.service'
    for unit in (ROOT_UNIT,USER_UNIT,REJECT_UNIT,app_unit):
        if run('systemctl','show',unit,'--property=LoadState','--value')!='not-found' or unit_file(unit).exists(): raise RuntimeError('foreign unit slot')
    ROOT.mkdir(mode=0o700); run('restorecon',str(ROOT))
    record=dict(token=uuid.uuid4().hex,state='preparing',pending=None,static_hashes={},unit_hashes={},seeded=False)
    save(record)
    try:
        policy=ROOT/(MODULE+'.cil'); policy.write_bytes((SOURCE/'policy/p0-execution-runtime.cil').read_bytes()); policy.chmod(0o600)
        record['policy_sha256']=sha(policy); save(record); run('semodule','-X','400','-i',str(policy))
        def observe(path):
            a,b,c,d,e=run(str(observer),'observe',str(path)).split(); return int(a),bytes.fromhex(b),int(c),int(d),int(e)
        replacements={n:uuid.uuid4().bytes for n in (1,5,50,51)}; replacements[4]=uuid.UUID(Path('/proc/sys/kernel/random/boot_id').read_text().strip()).bytes
        def ids(n): return replacements[n] if n in replacements else identity(n)
        work=ROOT/'root'; fixture=prepare_fixture(work,ROOT/'profile',payload.read_bytes(),observe,arguments=(),identity_fn=ids,starter_digest=bytes.fromhex(sha(starter)),data_baseline=3)
        for path,source,kind in ((ROOT/'driver',driver,'aetheri_system_exec_t'),(work/'execution-starter',starter,'aetheri_starter_exec_t'),(work/'user-broker',driver,'aetheri_user_exec_t')):
            path.write_bytes(source.read_bytes()); path.chmod(0o555); label(path,kind)
        parent=work/'launches'; parent.mkdir(mode=0o700); label(parent,'aetheri_setup_root_t')
        workspace=parent/identity(31).hex(); workspace.mkdir(mode=0o700); label(workspace,'aetheri_setup_root_t')
        for slot in ('app','data'): (workspace/slot).mkdir(mode=0o700); label(workspace/slot,'aetheri_setup_root_t')
        objects=[]
        for path in (parent,workspace,workspace/'app',workspace/'data'):
            kind,fsid,inode,seconds,ns=observe(path); objects.append(dict(filesystem_type=kind,filesystem_id=fsid.hex(),inode=inode,birth_seconds=seconds,birth_nanoseconds=ns))
        value=reference.execution_views(dict(profile_sha256=sha(ROOT/'profile'),objects=objects))
        path=work/'registry/execution-views.conf'; path.write_bytes(value); path.chmod(0o444); label(path,'aetheri_registry_t')
        (work/'system').mkdir(mode=0o700); label(work/'system','aetheri_system_state_t')
        run(str(ROOT/'driver'),'init',str(work)); seed(work,record)
        for path in (work/'system').iterdir(): label(path,'aetheri_system_state_t')
        record['payload_rel']=str((fixture['data']/'payload').relative_to(work))
        record['user_control_rel']=str((fixture['account']/'control').relative_to(work))
        record['user_catalogue_rel']=record['user_control_rel']+'/catalogue.sqlite3'
        record['tree']=capture(work)
        record['static_hashes']={str(path.relative_to(ROOT)):sha(path) for path in ROOT.rglob('*') if path.is_file() and path.name not in ('controller.json','catalogue.sqlite3','writer.lock')}
        xml='''<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><selinux><associate own="org.aetheri.P0ExecutionUser1" context="system_u:object_r:aetheri_user_service_t:s0"/></selinux><policy user="root"><allow send_destination="org.aetheri.System1" send_interface="org.aetheri.Execution1"/><allow send_destination="org.aetheri.System1" send_interface="org.aetheri.P0ExecutionTest1"/><allow send_destination="org.aetheri.P0ExecutionUser1" send_interface="org.aetheri.P0ExecutionUserTest1"/><deny send_destination="org.aetheri.System1" send_type="method_call" min_fds="1"/></policy>
<policy user="aetheri"><allow own="org.aetheri.P0ExecutionUser1"/><allow send_destination="org.aetheri.System1" send_interface="org.aetheri.P0ExecutionBrokerTest1"/><allow send_destination="org.aetheri.System1" send_interface="org.aetheri.P0ExecutionTest1" send_member="Observe"/><deny send_destination="org.aetheri.System1" send_type="method_call" min_fds="1"/></policy><policy context="mandatory"><deny send_destination="org.aetheri.P0ExecutionUser1" send_type="method_call" min_fds="1"/></policy></busconfig>
'''
        with CONFIG.open('x',encoding='utf-8') as out: out.write(xml)
        CONFIG.chmod(0o644); run('restorecon',str(CONFIG)); record['config_sha256']=sha(CONFIG); save(record)
        common=f'''User=root
Group=root
NoNewPrivileges=yes
PrivateMounts=yes
PrivateNetwork=yes
PrivateDevices=yes
ProtectHome=yes
ProtectControlGroups=yes
RestrictNamespaces=mnt
BindPaths={work}:/var/lib/aetheri-p0
KillMode=control-group
SendSIGKILL=no
TimeoutStartSec=15s
TimeoutStopSec=8s
RuntimeMaxSec=45s
MemoryMax=96M
TasksMax=1
UMask=0077
'''
        write_unit(ROOT_UNIT,'[Service]\nType=notify\n'+common+'SELinuxContext=system_u:system_r:aetheri_system_t:s0\nCapabilityBoundingSet=\nAmbientCapabilities=\nExecStart='+str(ROOT/'driver')+' serve\n',record)
        write_unit(app_unit,'[Unit]\nBindsTo='+ROOT_UNIT+'\nAfter='+ROOT_UNIT+'\n[Service]\nType=exec\n'+common+'SELinuxContext=system_u:system_r:aetheri_starter_t:s0\nCapabilityBoundingSet=CAP_SYS_ADMIN CAP_DAC_READ_SEARCH CAP_SETUID CAP_SETGID CAP_SETPCAP\nAmbientCapabilities=CAP_SETUID\nExecStart=/var/lib/aetheri-p0/execution-starter\n',record)
        user_common=common.replace('User=root\nGroup=root\n','User=aetheri\nGroup=aetheri\n')+'SELinuxContext=system_u:system_r:aetheri_user_t:s0\nCapabilityBoundingSet=\nAmbientCapabilities=\nExecStart=/var/lib/aetheri-p0/user-broker '
        write_unit(USER_UNIT,'[Unit]\nBindsTo='+ROOT_UNIT+'\nAfter='+ROOT_UNIT+'\n[Service]\nType=notify\n'+user_common+'user-serve\n',record)
        write_unit(REJECT_UNIT,'[Unit]\nBindsTo='+ROOT_UNIT+'\nAfter='+ROOT_UNIT+'\n[Service]\nType=oneshot\n'+user_common+'user-deny\n',record)
        flush_tree(ROOT); run('systemctl','daemon-reload'); run('busctl','--system','call','org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','ReloadConfig')
        record['pending']='root-start'; save(record); run('systemctl','start',ROOT_UNIT)
        facts=dict(x.split('=',1) for x in run('systemctl','show',ROOT_UNIT,'--property=ActiveState,MainPID,InvocationID').splitlines())
        if facts['ActiveState']!='active' or int(facts['MainPID'])<1: raise RuntimeError('unknown Root start')
        root_pin=os.pidfd_open(int(facts['MainPID']))
        user_pin=-1
        try:
            record.update(pending='user-rejection',state='running',root_pid=int(facts['MainPID']),root_invocation=facts['InvocationID']); save(record)
            record['root_capabilities']=require_empty_capabilities(record['root_pid']); save(record)
            run('systemctl','start',REJECT_UNIT); drained(REJECT_UNIT)
            record['pending']='user-start'; save(record); run('systemctl','start',USER_UNIT)
            user_facts=dict(x.split('=',1) for x in run('systemctl','show',USER_UNIT,'--property=ActiveState,MainPID,InvocationID').splitlines())
            if user_facts['ActiveState']!='active' or int(user_facts['MainPID'])<1: raise RuntimeError('unknown User start')
            user_pin=os.pidfd_open(int(user_facts['MainPID'])); record.update(user_pid=int(user_facts['MainPID']),user_invocation=user_facts['InvocationID'])
            record['user_capabilities']=require_empty_capabilities(record['user_pid'])
            verify_static(record)
            acknowledge_user_start(record,work)
            record['pending']='application'; save(record)
            print(run(str(ROOT/'driver'),'drive',str(work),timeout=25))
            drained(app_unit); record['pending']=None; record['state']='draining'; save(record)
            run('systemctl','stop',USER_UNIT); poll=select.poll(); poll.register(user_pin,select.POLLIN)
            if not poll.poll(10000): raise RuntimeError('original User process not dead')
            drained(USER_UNIT)
            run('systemctl','stop',ROOT_UNIT); poll=select.poll(); poll.register(root_pin,select.POLLIN)
            if not poll.poll(10000): raise RuntimeError('original Root process not dead')
            drained(ROOT_UNIT)
        finally:
            if user_pin>=0: os.close(user_pin)
            os.close(root_pin)
        verify_static(record); quiet_normal(ordinary)
        with closing(sqlite3.connect('file:'+str(work/'system/catalogue.sqlite3')+'?mode=ro',uri=True)) as db:
            if db.execute("SELECT state FROM generation_leases").fetchall()!=[('running',)] or db.execute("SELECT count(*) FROM operations WHERE state='completed'").fetchone()[0]: raise RuntimeError('unexpected lease settlement')
            if db.execute('PRAGMA integrity_check').fetchall()!=[('ok',)] or db.execute('PRAGMA foreign_key_check').fetchall(): raise RuntimeError('catalogue corruption')
            record['steps']=db.execute('SELECT step_no,state FROM operation_steps ORDER BY step_no').fetchall()
        evidence=driver.parent/('live-execution-'+record['token']); evidence.mkdir(mode=0o700)
        shutil.copyfile(work/'system/catalogue.sqlite3',evidence/'historical-test-catalogue.sqlite3')
        user_database=work/record['user_catalogue_rel']
        with closing(sqlite3.connect('file:'+str(user_database)+'?mode=ro',uri=True)) as db:
            if db.execute('PRAGMA integrity_check').fetchall()!=[('ok',)] or db.execute('PRAGMA foreign_key_check').fetchall(): raise RuntimeError('User catalogue corruption')
            if db.execute('SELECT state,readiness,invocation_ref FROM launches').fetchall()!=[('preparing','unknown',None)]: raise RuntimeError('unexpected User readiness')
            if db.execute('SELECT step_no,state FROM operation_steps ORDER BY step_no').fetchall()!=[(0,'applied'),(1,'executing')]: raise RuntimeError('missing User execution intent')
        shutil.copyfile(user_database,evidence/'historical-user-catalogue.sqlite3')
        record.update(state='passed',evidence=str(evidence)); save(record)
        (evidence/'report.json').write_text(json.dumps(record,indent=2)+'\n')
        retire(record)
        print('Live integration passed and exact owned fixture retired; isolated held-lease history archived. No production admission/readiness/removal acceptance.')
    except BaseException as error:
        if isinstance(error,subprocess.CalledProcessError): print(error.output,file=sys.stderr)
        for unit in record['unit_hashes']:
            print(run('journalctl','-u',unit,'--no-pager','--output=cat','-n','35'),file=sys.stderr)
        print('Preserved live execution fixture/policy; inspect original intent/process/unit/job before any retry or retirement.',file=sys.stderr)
        raise
    finally: os.close(lock)
if __name__=='__main__': main()
