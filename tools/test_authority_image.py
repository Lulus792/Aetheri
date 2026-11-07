"""Explicit enforcing-carrier image-selection negatives; no catalogue adoption.

Run against the independently installed normal P0 images, before/after its
reviewed update. Private read-only shadow registries contain NO catalogue and
cannot mutate the normal owners. A byte-identical different ELF object executes
in each actual authority domain. Baseline records the old missing guard; candidate
must reject the image before attempting the absent catalogue. Unknown effects
preserve owned units/inputs. This is not a production drain/recovery protocol.
"""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import time
import uuid
import p0_services as p0


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert sys.platform=='linux' and os.geteuid()==0 and sys.argv[1:] in (['baseline'],['candidate'])
    assert Path('/sys/class/dmi/id/product_name').read_text().strip()=='Aetheri P0 disposable VM'
    assert Path('/sys/fs/selinux/enforce').read_text().strip()=='1' and Path('/sys/firmware/efi').is_dir()
    mode=sys.argv[1]
    marker=os.open('/etc/aetheri-p0-carrier',os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW)
    info=os.fstat(marker);assert stat.S_ISREG(info.st_mode) and info.st_uid==0 and not info.st_mode&0o022
    fcntl.flock(marker,fcntl.LOCK_EX|fcntl.LOCK_NB)
    normal=p0.load();p0.verified(normal)
    token=uuid.uuid4().hex
    root=Path('/var/lib')/('aetheri-authority-image-test-'+token)
    p0.directory(root,'aetheri_root_t')
    record=dict(token=token,mode=mode,state='preparing',normal_authority_sha256=normal['authority_sha256'],units=[],pending=None,results=[])
    def save():
        temp=root/'record.next'
        with temp.open('x',encoding='utf-8') as f:
            json.dump(record,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
        temp.chmod(0o600);os.replace(temp,root/'record.json')
        fd=os.open(root,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC)
        try:os.fsync(fd)
        finally:os.close(fd)
    save();print('EVIDENCE '+str(root),flush=True)
    shadow=root/'shadow'
    for path,label,uid,permissions in ((shadow,'aetheri_root_t',0,0o755),(shadow/'registry','aetheri_registry_t',0,0o755),
        (shadow/'system','aetheri_system_state_t',0,0o700),(shadow/'users','aetheri_root_t',0,0o755),
        (shadow/'users'/normal['owner'],'aetheri_root_t',0,0o755),
        (shadow/'users'/normal['owner']/'control','aetheri_user_state_t',1000,0o700)):
        p0.directory(path,label,uid=uid,mode=permissions)
    for role in ('system','user'):
        source=p0.ROOT/(role+'-authority');assert sha(source)==normal['authority_sha256']
        p0.publish(shadow/(role+'-authority'),source.read_bytes(),'aetheri_'+role+'_exec_t',mode=0o555)
        p0.publish(root/(role+'-actor'),source.read_bytes(),'aetheri_'+role+'_exec_t',mode=0o555)
        name='system.conf' if role=='system' else 'user-1000.conf'
        p0.publish(shadow/'registry'/name,(p0.ROOT/'registry'/name).read_bytes(),'aetheri_registry_t')
        assert sha(shadow/(role+'-authority'))==sha(root/(role+'-actor'))==normal['authority_sha256']
        assert (shadow/(role+'-authority')).stat().st_ino!=(root/(role+'-actor')).stat().st_ino
    static={p.relative_to(root).as_posix():(p.lstat().st_ino,p.lstat().st_uid,p.lstat().st_gid,stat.S_IMODE(p.lstat().st_mode),os.getxattr(p,'security.selinux').hex(),sha(p) if p.is_file() else None)
            for p in root.rglob('*') if p.name!='record.json'}
    record['inputs']=static;save()
    for role in ('system','user'):
        unit='aetheri-authority-image-'+token+'-'+role+'.service'
        path=Path('/run/systemd/system')/unit
        assert not path.exists() and not path.is_symlink()
        assert p0.run('systemctl','show',unit,'--property=LoadState','--value')=='not-found'
        text='[Service]\nType=exec\nUser='+('root' if role=='system' else 'aetheri')+'\nGroup='+('root' if role=='system' else 'aetheri')+'\n'
        text+='SELinuxContext=system_u:system_r:aetheri_'+role+'_t:s0\nNoNewPrivileges=yes\nCapabilityBoundingSet=\nAmbientCapabilities=\n'
        text+='PrivateMounts=yes\nProtectSystem=strict\nProtectHome=yes\nRestrictNamespaces=yes\nKillMode=control-group\nSendSIGKILL=no\n'
        text+='TimeoutStartSec=5s\nTimeoutStopSec=5s\nBindReadOnlyPaths='+str(shadow)+':'+str(p0.ROOT)+'\n'
        text+='ExecStart='+str(root/(role+'-actor'))+' '+role+'\n'
        with path.open('x') as f:f.write(text)
        path.chmod(0o644);p0.run('restorecon',str(path))
        entry=dict(unit=unit,definition_sha256=sha(path),definition_inode=path.stat().st_ino)
        record['units'].append(entry);record.update(state='starting',pending=unit);save()
        p0.run('systemctl','daemon-reload')
        start=subprocess.run(['systemctl','start',unit],capture_output=True,text=True,timeout=10)
        (root/(role+'-start.log')).write_text(start.stdout+start.stderr)
        end=time.monotonic()+8
        while True:
            facts=dict(line.split('=',1) for line in p0.run('systemctl','show',unit,'--property=ActiveState,MainPID,ControlPID,ControlGroup,ExecMainStatus,InvocationID').splitlines())
            if facts['ActiveState'] in ('failed','inactive') and facts['MainPID']=='0' and facts['ControlPID']=='0' and not p0.unit_has_job(unit):break
            assert time.monotonic()<end,('unit did not reach a known end',unit,facts)
            time.sleep(0.05)
        assert not facts['ControlGroup'],facts
        assert facts['ExecMainStatus']=='5' and len(facts['InvocationID'])==32 and int(facts['InvocationID'],16),facts
        output=p0.run('journalctl','-u',unit,'--output=cat','--no-pager')
        (root/(role+'-journal.log')).write_text(output+'\n')
        image_rejected='Authority selected image unavailable: -116' in output
        if mode=='candidate':assert image_rejected,(role,output)
        else:
            assert not image_rejected and 'Authority selected image unavailable:' not in output,(role,output)
            assert any('Authority startup/runtime failed: '+str(n) in output for n in (-2,-30)),(role,output)
        assert sha(path)==entry['definition_sha256'] and path.stat().st_ino==entry['definition_inode']
        for name,expected in static.items():
            p=root/name;s=p.lstat()
            assert not p.is_symlink() and (s.st_ino,s.st_uid,s.st_gid,stat.S_IMODE(s.st_mode),os.getxattr(p,'security.selinux').hex(),sha(p) if p.is_file() else None)==expected,name
        assert not any(p.name in ('catalogue.sqlite3','writer.lock') for p in shadow.rglob('*'))
        record['results'].append(dict(role=role,unit=facts,image_rejected=image_rejected,journal=output))
        record.update(pending=None,state='ended');save()
        p0.run('systemctl','reset-failed',unit)
        assert not p0.unit_has_job(unit)
        path.unlink();p0.run('systemctl','daemon-reload')
        assert p0.run('systemctl','show',unit,'--property=LoadState','--value')=='not-found'
    p0.verified(normal)
    record.update(state='closed',result='passed',normal_catalogues_accessed=False);save()
    os.close(marker)
    print(json.dumps(dict(result='passed',mode=mode,artifacts=str(root),roles=2)))


if __name__=='__main__':main()
