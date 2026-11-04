"""Independent observation bytes plus real read-only boot checks, no services."""
from pathlib import Path
import hashlib
import json
import os
import stat
import subprocess
import sys
import tempfile
import runtime_contracts as reference

def require(condition,message):
    if not condition: raise RuntimeError(message)

def inspection(binary):
    """Opt-in administrative observation of the exact retained failed attempt.

    No install, dispatch, service change, journal recovery or lease settlement.
    Inputs must already be independently quiescent/fenced. The current boot
    observation and protected metadata are not proof of those prerequisites.
    """
    require(os.getuid()==0 and os.geteuid()==0,'administrative carrier only')
    require(Path('/sys/fs/selinux/enforce').read_text().strip()=='1','enforcing required')
    require(Path('/sys/firmware/efi').is_dir() and
            Path('/sys/class/dmi/id/product_name').read_text()=='Aetheri P0 disposable VM\n','wrong carrier')
    marker=Path('/etc/aetheri-p0-carrier').lstat()
    require(stat.S_ISREG(marker.st_mode) and marker.st_uid==0 and marker.st_nlink==1 and not marker.st_mode&0o022,'untrusted marker')
    fixture=Path('/var/lib/aetheri-p0-execution-runtime-test')
    st=fixture.lstat()
    require(not fixture.is_symlink() and fixture.is_dir() and st.st_uid==0 and st.st_mode&0o7777==0o700,'untrusted fixture')
    controller=fixture/'controller.json'; st=controller.lstat()
    require(not controller.is_symlink() and controller.is_file() and st.st_uid==0 and
            st.st_nlink==1 and st.st_mode&0o7777==0o600,'untrusted controller')
    record=json.loads(controller.read_bytes())
    require(record['token']=='0f04cd9ae92d4500a42177e3e14de76b' and
            record['state']=='running' and record['pending']=='recover-handoff','wrong retained attempt')
    root=fixture/'root'; database=root/'system/catalogue.sqlite3'
    def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
    require(digest(database)=='bdfca485ed5079d62afc2270946cfd9999a354b2240a51c9914b4c64e9291f2d','original catalogue changed')
    inputs=[controller,database,root/record['user_catalogue_rel']]
    for name,expected in record['static_hashes'].items():
        path=fixture/name
        require(not path.is_symlink() and path.resolve().is_relative_to(fixture) and digest(path)==expected,'persistent input changed')
        inputs.append(path)
    for path in inputs:
        require(not path.is_symlink() and path.resolve().is_relative_to(fixture),'input escaped fixture')
    for path in (database,root/record['user_catalogue_rel']):
        for suffix in ('-journal','-wal','-shm'):
            sidecar=path.with_name(path.name+suffix)
            require(not sidecar.exists() and not sidecar.is_symlink(),'original sidecar requires reconciliation')
    before={str(path):digest(path) for path in inputs}
    child="""import subprocess,sys
subprocess.run(['mount','--bind',sys.argv[1],'/var/lib/aetheri-p0'],check=True)
if sys.argv[3]: subprocess.run(['mount','--bind',sys.argv[3],'/var/lib/aetheri-p0/system'],check=True)
r=subprocess.run([sys.argv[2],'inspect-boot'],text=True,capture_output=True)
sys.stdout.write(r.stdout); sys.stderr.write(r.stderr); sys.exit(r.returncode)
"""
    def run(shadow=''):
        return subprocess.run(['unshare','--mount','--propagation','private',sys.executable,'-c',child,
            str(root),str(binary),str(shadow)],text=True,capture_output=True,timeout=30)
    result=run()
    require(result.returncode==0 and 'AETHERI_BOOT_INSPECTION ' in result.stdout,
            'positive inspection failed: '+result.stdout+result.stderr)
    print(result.stdout,end='')
    with tempfile.TemporaryDirectory(prefix='aetheri-boot-inspection-') as temporary:
        for case in ('missing-lock','symlink-lock','-journal','-wal','-shm'):
            shadow=Path(temporary)/case; shadow.mkdir(mode=0o700)
            db=shadow/'catalogue.sqlite3'; db.write_bytes(b'Not SQLite; preflight must reject before catalogue open.'); db.chmod(0o600)
            lock=shadow/'writer.lock'
            if case=='symlink-lock': lock.symlink_to(db.name)
            elif case!='missing-lock': lock.write_bytes(b''); lock.chmod(0o600)
            if case.startswith('-'):
                sidecar=shadow/(db.name+case); sidecar.write_bytes(b'Preserve unknown journal evidence.'); sidecar.chmod(0o600)
            def snapshot():
                return {p.name:('link',os.readlink(p)) if p.is_symlink() else ('file',digest(p)) for p in shadow.iterdir()}
            original=snapshot(); rejected=run(shadow)
            require(rejected.returncode==1 and 'fstatat(parent,' in rejected.stderr,
                    'preflight did not reject '+case+': '+rejected.stdout+rejected.stderr)
            require(snapshot()==original,'inspection changed shadow inputs: '+case)
    require({str(path):digest(path) for path in inputs}==before,'inspection changed retained inputs')
    print('Five private preflight refusals passed; retained catalogues/controller/static inputs unchanged. No settlement.')

def main():
    binary,build=(Path(p).resolve(strict=True) for p in sys.argv[1:3])
    vector=reference.load_json((Path(__file__).resolve().parents[1]/'spec/vectors/boot-context-v1.json').read_bytes())
    with tempfile.TemporaryDirectory(prefix='boot-',dir=build) as temporary:
        path=Path(temporary)/'vector'
        path.write_bytes(bytes.fromhex(vector['hex'])+bytes.fromhex(vector['sha256']))
        subprocess.run([str(binary),'--codec',str(path)],check=True,timeout=10)
        subprocess.run([str(binary)],check=True,timeout=25)

if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--inspection': inspection(Path(sys.argv[2]).resolve(strict=True))
    else: main()
