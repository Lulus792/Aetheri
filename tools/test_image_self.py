"""Explicit disposable Linux/root fixture: self ELF checks at UID/GID 0 and 1000."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary,build=(Path(p).resolve(strict=True) for p in sys.argv[1:])
    assert sys.platform=='linux' and os.geteuid()==0 and build.is_dir()
    assert Path('/sys/class/dmi/id/product_name').read_text().strip()=='Aetheri P0 disposable VM'
    assert Path('/etc/aetheri-p0-carrier').is_file() and Path('/sys/fs/selinux/enforce').read_text().strip()=='1'
    # Capability-empty UID0 cannot traverse the ordinary user's private home.
    # The root-owned fixture stays on a searchable system temporary path; its
    # parent is sticky and the program files are never user-writable.
    root=Path(tempfile.mkdtemp(prefix='aetheri-image-self-',dir='/var/tmp'));root.chmod(0o755)
    print('EVIDENCE '+str(root),flush=True)
    raw=binary.read_bytes();digest=hashlib.sha256(raw).hexdigest()
    for name in ('actor','same-bytes-other-object'):
        p=root/name
        with p.open('xb') as f:f.write(raw)
        p.chmod(0o555)
    results=[]
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for uid in (0,1000):
        for case in ('current','foreign-object','wrong-digest','wrong-uid','wrong-gid'):
            expected=root/('same-bytes-other-object' if case=='foreign-object' else 'actor')
            args=[str(root/'actor'),str(expected),digest,str(1001 if case=='wrong-uid' else uid),str(1001 if case=='wrong-gid' else uid),case]
            done=subprocess.run(args,user=uid,group=uid,extra_groups=(),capture_output=True,text=True,timeout=20,env=env)
            (root/f'{uid}-{case}.log').write_text(done.stdout+done.stderr)
            assert done.returncode==0,(uid,case,done.stdout,done.stderr)
            results.append(dict(uid=uid,case=case,repeats=12))
    assert hashlib.sha256((root/'actor').read_bytes()).hexdigest()==digest
    assert hashlib.sha256((root/'same-bytes-other-object').read_bytes()).hexdigest()==digest
    assert (root/'actor').stat().st_ino!=(root/'same-bytes-other-object').stat().st_ino
    (root/'result.json').write_text(json.dumps(dict(result='passed',binary_sha256=digest,cases=results,
        scope='self process/image kernel checks; administrative unconfined domain, not current service or recovery admission'),indent=2)+'\n')
    print(json.dumps(dict(result='passed',artifacts=str(root),cases=len(results))))


if __name__=='__main__':main()
