"""Native read-only recovery inventory against populated owner schemas/faults."""
from contextlib import closing
import errno
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import catalogue_contracts as cc
from test_contracts import legacy,rows,plan,accept,uid,insert


def pending_metadata(db,f,kind):
    """Synthetic SQL fixtures only; retain all ordinary ownership/state guards."""
    if kind=='system':
        component='aetheri.inventory-fixture';install,generation=uid(),uid()
        scope=db.execute("SELECT scope_id FROM scopes WHERE kind='machine'").fetchone()[0]
        insert(db,'components',component_id=component,publisher_ref=f['publisher'],kind='app',display_name='Inventory fixture',protected=0)
        insert(db,'artifacts',artifact_id=b'F'*32,component_id=component,version_label='test',target='x86_64-linux-gnu',store_state='staged')
        insert(db,'installs',install_id=install,component_id=component,scope_id=scope,state='preparing')
        insert(db,'generations',generation_id=generation,install_id=install,component_id=component,artifact_id=b'F'*32,publication_state='staged')
    else:
        # Establish the synthetic source fixture's first broker context before
        # creating its grants. Native replacement is tested separately below.
        runtime=uid()
        db.execute('UPDATE authority_meta SET runtime_incarnation=?,catalogue_revision=catalogue_revision+1',(runtime,))
        db.execute("UPDATE admission_gates SET state='open',revision=revision+1 WHERE subject_ref=?",(f['data'],))
        resource,grant,access=uid(),uid(),uid()
        insert(db,'resources',resource_id=resource,owner_scope=f['account'],kind='file',fixture_root_ref=uid(),locator='document',display_name='Document',
               observed_boot_id=f['boot'],broker_incarnation=runtime,observed_device=1,observed_inode=1,observed_size=0,state='bound')
        insert(db,'grants',grant_id=grant,resource_id=resource,launch_id=f['launch'],install_id=f['install'],data_epoch_id=f['data'],
               issuer_ref=f['actor'],rights='read',mode='native',lifetime='launch',state='pending')
        db.execute("UPDATE grants SET state='active',revision=revision+1 WHERE grant_id=?",(grant,))
        insert(db,'access_leases',access_id=access,grant_id=grant,launch_id=f['launch'],broker_incarnation=runtime,request_key=uid(),mode='native',state='opening')
        db.execute("UPDATE resources SET state='needs_reselect',revision=revision+1 WHERE resource_id=?",(resource,))
        insert(db,'data_epochs',data_epoch_id=uid(),component_id='aetheri.inventory-fixture',publisher_ref=f['publisher'],owner_scope=f['account'],
               storage_ref=uid(),state='purging',data_schema_version=1)


def expected(db,kind,version):
    # Reference walks complete source rows, independently of native SQL joins.
    tables={name:[dict(zip(columns,r)) for r in values]
            for name,(columns,values) in rows(db).items()}
    result=[]
    def add(k,r,key,related=None,state='state',detail='',phase='',reason='',steps=0):
        ref=r.get(related) if related else None
        result.append((k,r[key].hex(),ref.hex() if ref is not None else '-',r['revision'],steps,r[state],detail,phase,reason))
    plans={r['plan_id']:r for r in tables['plans']}
    for r in tables['operations']:
        unfinished=sum(s['state']!='applied' for s in tables['operation_steps'] if s['operation_id']==r['operation_id'])
        if r['state']!='completed' or unfinished:
            p=plans[r['plan_id']];r={**r,'related':p['install_ref'] or p['data_epoch_ref']}
            add(1,r,'operation_id','related',detail=p['kind'],phase=r['phase'],reason=r['reason_code'],steps=unfinished)
    for r in tables['admission_gates']:
        if r['state']!='closed' or r['closing_operation_id'] is not None:add(2,r,'subject_ref','closing_operation_id',detail=r['subject_kind'])
    if kind=='system':
        for r in tables['installs']:
            if r['state'] in ('preparing','removing'):add(3,r,'install_id','active_generation_id')
        for r in tables['generations']:
            if r['publication_state'] in ('staged','reclaiming'):add(4,r,'generation_id','install_id','publication_state')
        for r in tables['generation_leases']:
            if r['state']!='released':add(5,r,'lease_id','generation_id',detail=r['kind'])
        for r in tables['launch_dispatches']:
            if r['state']!='terminal':add(6,r,'launch_id','lease_id')
    else:
        for r in tables['launches']:
            if r['state'] not in ('exited','start_failed'):add(7,r,'launch_id','operation_id',detail=r['readiness'])
        for r in tables['grants']:
            if r['state'] not in ('revoked','expired'):add(8,r,'grant_id','launch_id')
        for r in tables['access_leases']:
            if r['state']!='released':add(9,r,'access_id','grant_id')
        for r in tables['resources']:
            if r['state']=='needs_reselect':add(10,r,'resource_id','owner_scope')
        for r in tables['data_epochs']:
            if r['state']=='purging':add(11,r,'data_epoch_id','storage_ref')
    if version>=3:
        for r in tables['run_closures']:
            if r['state']!='released':add(12,r,'launch_id','start_operation_id')
    if kind=='system' and version>=5:
        for r in tables['execution_handoffs']:
            if r['state']!='retired':add(13,r,'launch_id')
    if version>=8:
        stages=tables['run_interruption_steps'];first,terminal=(1,3) if kind=='system' else (2,4)
        for r in stages:
            if r['kind']==first and not any(x['launch_id']==r['launch_id'] and x['kind']==terminal for x in stages):
                add(14,{**r,'state':'begin' if first==1 else 'quiet'},'launch_id','start_operation_id')
    return sorted(result)


def main():
    binary,build=map(lambda p:Path(p).resolve(strict=True),sys.argv[1:]);assert sys.platform=='linux'
    root=Path(tempfile.mkdtemp(prefix='recovery-inventory-',dir=build));reports=[]
    print('EVIDENCE '+str(root),flush=True)
    for kind in ('system','user'):
        directory=root/kind;directory.mkdir(mode=0o700);path=directory/'catalogue.sqlite3'
        with closing(sqlite3.connect(path)) as db, closing(sqlite3.connect(root/(kind+'-backup.sqlite3'))) as backup:
            f=legacy(db,kind);cc.migrate_v1_to_v2(db,kind,backup=backup)
            pending_metadata(db,f,kind)
            # More than one allocation chunk, with a true unfinished step set.
            for i in range(35):
                p=plan(db,f);op=accept(db,p)
                if i==0:db.execute('UPDATE operations SET reason_code=?,revision=revision+1 WHERE operation_id=?',('é'*64,op))
                if i==1:db.execute('UPDATE operations SET reason_code=?,revision=revision+1 WHERE operation_id=?',('🧪'*64,op))
                if i in (0,2):
                    insert(db,'operation_steps',operation_id=op,step_no=0,action='stage',subject_kind='storage',subject_ref=uid(),state='planned' if i==0 else 'uncertain')
                if i==2:
                    db.execute("UPDATE operations SET state='completed',outcome='failed',completed_unix_ms=1792972801000,revision=revision+1 WHERE operation_id=?",(op,))
            db.commit()
        path.chmod(0o600)
        def run(version,mode=0,limit=65536,wanted=0):
            with closing(sqlite3.connect(path)) as db:before=rows(db);reference=expected(db,kind,version)
            sha=hashlib.sha256(path.read_bytes()).hexdigest()
            command=[str(binary),str(directory),kind,f['epoch'].hex(),f['account'].hex() if kind=='user' else '-',str(limit),str(mode),str(wanted)]
            done=subprocess.run(command,capture_output=True,text=True,timeout=20)
            name=f'{len(reports):02d}-{kind}-v{version}-m{mode}-l{limit}';(root/(name+'.log')).write_text(done.stdout+done.stderr)
            assert done.returncode==0,(name,done.stdout,done.stderr)
            assert hashlib.sha256(path.read_bytes()).hexdigest()==sha,name
            with closing(sqlite3.connect(path)) as db:assert rows(db)==before,name
            assert not any(p.name.endswith(('-journal','-wal','-shm')) for p in directory.iterdir()),name
            if not wanted:
                lines=done.stdout.splitlines();header=lines[0].split();assert header[0]=='CONTEXT' and int(header[1])==version and int(header[-1])==len(reference)
                actual=[]
                for line in lines[1:]:
                    tag,k,key,related,rev,steps,*texts=line.split();assert tag=='ITEM' and len(texts)==4
                    actual.append((int(k),key,related,int(rev),int(steps),*(bytes.fromhex(v).decode() if v!='-' else '' for v in texts)))
                assert actual==reference,(name,actual,reference)
                assert any(r[0]==1 and r[5]=='completed' and r[4]==1 for r in actual)
                required={1,2,3,4,5} if kind=='system' else {1,2,7,8,9,10,11}
                assert {r[0] for r in actual}>=required
                if kind=='system':assert any(r[0]==5 and r[5] in ('reserved','reconcile') for r in actual) # Completed install history must not hide retained code.
                else:assert any(r[0]==7 and r[5] in ('running','recovery_required') for r in actual)
            reports.append({'case':name,'records':len(reference),'expected_errno':wanted})
        versions=(2,3,4,5,6,7,8) if kind=='system' else (2,3,4,7,8)
        for version in versions:
            if version>2:
                with closing(sqlite3.connect(path)) as db:
                    (cc.migrate_owner_recovery if version==7 else getattr(cc,f'migrate_v{version-1}_to_v{version}'))(db,kind)
            run(version)
        for mode,code in ((1,errno.EIO),(2,errno.EIO),(3,errno.EIO),(4,errno.EIO),(5,errno.ENOMEM),(6,errno.EIO),(7,errno.ENOMEM),(8,errno.ENOMEM),(9,errno.ENOMEM),(10,0),(13,0)):
            run(8,mode,wanted=-code)
        run(8,limit=1,wanted=-errno.E2BIG)
        run(8) # Exact current facts remain available after all read failures.
        def start(mode):
            done=subprocess.run([str(binary),str(directory),kind,f['epoch'].hex(),f['account'].hex() if kind=='user' else '-',
                                 '65536',str(mode),'0'],capture_output=True,text=True,timeout=20)
            (root/f'{kind}-startup-{mode}.log').write_text(done.stdout+done.stderr)
            assert done.returncode==0 and done.stdout.strip()=='STARTED',done.stderr
        start(11)
        with closing(sqlite3.connect(path)) as db:
            state=db.execute('SELECT state FROM '+('generation_leases' if kind=='system' else 'launches')).fetchone()[0]
            # Root runtime replacement marks dispatches uncertain; it does not
            # rewrite or release the separate legacy recovery lease.
            assert state==('reserved' if kind=='system' else 'recovery_required')
            p=plan(db,f,'remove' if kind=='system' else 'purge',
                   **({'install_ref':f['install'],'retention':'keep_data'} if kind=='system' else {'data_epoch_ref':f['data'],'retention':'purge_data'}))
            cause=accept(db,p)
            db.execute("UPDATE admission_gates SET state='closing',closing_operation_id=?,revision=revision+1 WHERE subject_ref=?",
                       (cause,f['install'] if kind=='system' else f['data']))
            db.commit()
        start(12)
        run(8)
        with closing(sqlite3.connect(path)) as db:
            assert db.execute('SELECT state,closing_operation_id FROM admission_gates WHERE closing_operation_id=?',(cause,)).fetchone()==('closing',cause)
    (root/'result.json').write_text(json.dumps({'result':'passed','cases':reports,'scope':'native owner SQL snapshots/faults; no actor/holder/recovery admission'},indent=2)+'\n')
    print(json.dumps({'result':'passed','artifacts':str(root),'cases':len(reports)}))


if __name__=='__main__':main()
