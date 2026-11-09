"""Read the already selected libwacom data RPM as data; no installation.

The caller supplies independent destination/hash pins and owns fresh output.
RPM hardlinks become separate regular copies, never filesystem links.
"""
import hashlib
from pathlib import PurePosixPath
import stat
import subprocess

import prepare_audio as audio


def materialize(expected,headers,members):
    contents={}
    for name,mode,raw in members:
        if name in contents:raise ValueError('duplicate data member')
        contents[name]=(mode,bytes(raw))
    result={};aliases={}
    for name,e in expected.items():
        p=PurePosixPath(name)
        if p.is_absolute() or str(p)!=name or '..' in p.parts or not name.startswith('usr/share/libwacom/'):
            raise ValueError('different data destination')
        if name not in contents or name not in headers:raise ValueError('missing data member/header')
        mode,raw=contents[name];header=headers[name]
        if not stat.S_ISREG(mode) or header['size']!=e['size']:raise ValueError('different data type/size')
        if not raw and e['size']:
            # newc stores the bytes in one member of an RPM hardlink class.
            # Require that same class from the sealed RPM header and then the
            # independently selected destination digest; never infer aliases
            # from equal names or zero lengths alone.
            candidates=[(n,b) for n,(m,b) in contents.items() if b and stat.S_ISREG(m)
                and n in headers and headers[n]['object']==header['object']]
            if len(candidates)!=1:raise ValueError('unresolved data hardlink class')
            source,raw=candidates[0];aliases[name]=source
        if len(raw)!=e['size'] or hashlib.sha256(raw).hexdigest()!=e['sha256']:raise ValueError('different selected data bytes')
        result[name]=raw
    return result,aliases


def load(archive,package,expected):
    with audio.sealed_package(archive,package) as fd:
        path='/proc/self/fd/'+str(fd)
        def query(*args):return subprocess.check_output(args,pass_fds=(fd,),text=True)
        signature=query('rpmkeys','--checksig','--verbose',path)
        if '36f612dcf27f7d1a48a835e4dbfcf71c6d9f90a6: OK' not in signature or 'Payload SHA256 digest: OK' not in signature:raise ValueError('unverified libwacom data RPM')
        metadata=query('rpm','-qp','--qf','%{NAME} %{EVR}.%{ARCH}\n%{LICENSE}\n%{SOURCERPM}\n',path)
        if metadata.splitlines()!=['libwacom-data 2.19.0-1.fc44.noarch','HPND','libwacom-2.19.0-1.fc44.src.rpm'] or query('rpm','-qp','--scripts',path):raise ValueError('different data origin/scripts')
        header=query('rpm','-qp','--qf','[%{FILENAMES}\t%{FILEINODES}\t%{FILEDEVICES}\t%{FILESIZES}\n]',path)
        headers={}
        for line in header.splitlines():
            name,inode,device,size=line.split('\t');name=name.lstrip('/')
            if name in headers:raise ValueError('duplicate data header')
            headers[name]=dict(object=(int(device),int(inode)),size=int(size))
        members=list(audio.cpio_members(subprocess.check_output(['rpm2cpio',path],pass_fds=(fd,))))
        payload,aliases=materialize(expected,headers,members)
    return payload,dict(signature=signature,metadata=metadata,aliases=aliases,files=len(payload))
