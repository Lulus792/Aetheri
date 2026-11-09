"""Review/apply exact library updates only in the separate disposable audio lab.

This administrative Fedora test-carrier setup is not an Aetheri updater. It
never changes the original P0 guest. An existing transaction record forbids
replay, including after an unknown acknowledgement. Reboot before audio tests.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import subprocess

import image_state
import prepare_audio

ROOT=Path(__file__).resolve().parents[1]
NAMES={*(f'{name}-2.43-9.fc44.x86_64.rpm' for name in ('glibc','glibc-common','glibc-gconv-extra','glibc-langpack-en')),
       'libgcc-16.2.1-2.fc44.x86_64.rpm','libselinux-3.11-2.fc44.x86_64.rpm',
       'libselinux-utils-3.11-2.fc44.x86_64.rpm','libsepol-3.11-1.fc44.x86_64.rpm'}


def profile():
    raw=(ROOT/'spec/audio-lab-runtime.json').read_bytes();value=json.loads(raw)
    if type(value['format']) is not int or value['format']!=1 or value['signer']!='36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6':raise ValueError('different lab runtime profile')
    if len(value['packages'])!=8 or {p['name'] for p in value['packages']}!=NAMES:raise ValueError('different runtime transaction')
    for p in value['packages']:
        if type(p['size']) is not int or not 0<p['size']<=8*1024**2 or len(p['metadata'])!=3 or p['metadata'][0]+'.rpm'!=p['name']:raise ValueError('invalid runtime input')
        for key in ('sha256','scripts_sha256'):
            if len(p[key])!=64 or any(c not in '0123456789abcdef' for c in p[key]):raise ValueError('invalid input digest')
    return value,hashlib.sha256(raw).hexdigest()


def output(*args,pass_fds=()):return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT,pass_fds=pass_fds)
def inventory():return output('rpm','-qa','--qf','%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n').splitlines()


def guard(directory):
    if os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('separate root-only enforcing audio lab required')
    if not Path('/sys/firmware/efi').is_dir() or Path('/etc/aetheri-audio-lab-carrier').read_text()!='Disposable enforcing audio development carrier; not an Aetheri release.\n':raise ValueError('different carrier')
    if directory.is_symlink() or directory.resolve()!=directory or not directory.is_dir() or directory.stat().st_uid!=0 or directory.stat().st_mode&0o077:raise ValueError('private root-owned input directory required')
    if output('systemctl','list-units','--all','--plain','--no-legend','aetheri-audio-*').strip():raise ValueError('audio test units remain; inspect first')


def execute(action,directory):
    guard(directory)
    with image_state.guard(directory/'provisioning'):
        return locked_execute(action,directory)


def locked_execute(action,directory):
    if action not in ('check','apply'):raise ValueError('unknown action')
    value,digest=profile();guard(directory)
    journal=directory/'transaction.json'
    if action=='apply' and journal.exists():raise ValueError('original transaction exists; inspect without replay')
    before=inventory();proofs=[]
    with contextlib.ExitStack() as stack:
        held=[];paths=[]
        for p in value['packages']:
            fd=stack.enter_context(prepare_audio.sealed_package(directory/p['name'],p));held.append(fd);path='/proc/self/fd/'+str(fd);paths.append(path)
            sig=output('rpmkeys','--checksig','--verbose',path,pass_fds=(fd,))
            if value['signer']+': ok' not in sig.lower() or 'payload sha256 digest: ok' not in sig.lower():raise ValueError('unverified input')
            metadata=output('rpm','-qp','--qf','%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n%{LICENSE}\n%{SOURCERPM}\n',path,pass_fds=(fd,)).splitlines()
            scripts=output('rpm','-qp','--scripts',path,pass_fds=(fd,))
            if metadata!=p['metadata'] or hashlib.sha256(scripts.encode()).hexdigest()!=p['scripts_sha256']:raise ValueError('different reviewed metadata/scripts')
            proofs.append(dict(name=p['name'],sha256=p['sha256'],signature=sig))
        test=output('rpm','-U','--test','--verbose',*paths,pass_fds=tuple(held))
        record=dict(profile_sha256=digest,boot=Path('/proc/sys/kernel/random/boot_id').read_text().strip(),before=before,proofs=proofs,test=test,state='reviewed')
        if action=='check':return record
        # This is one explicit RPM transaction in an empty development carrier.
        # Unknown effects remain pending; no fallback, rollback or retry.
        record['state']='pending';image_state.write(journal,record)
        result=subprocess.run(['rpm','-U','--verbose',*paths],pass_fds=tuple(held),capture_output=True,text=True)
        record.update(exit=result.returncode,stdout=result.stdout,stderr=result.stderr,after=inventory())
        if result.returncode:record['state']='needs-inspection';image_state.write(journal,record);raise RuntimeError('runtime transaction failed; preserve evidence')
        old=set(before);new=set(record['after']);names={p['metadata'][0] for p in value['packages']}
        removed=old-new
        if new-old!=names or len(removed)!=8:
            record['state']='needs-inspection';image_state.write(journal,record);raise RuntimeError('unexpected package inventory delta')
        record.update(state='applied-reboot-required',removed=sorted(removed),added=sorted(new-old));image_state.write(journal,record)
        return record


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('check','apply'));parser.add_argument('directory',type=Path);args=parser.parse_args()
    print(json.dumps(execute(args.action,args.directory.absolute()),indent=2))


if __name__=='__main__':main()
