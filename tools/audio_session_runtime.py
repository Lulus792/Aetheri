"""Select the pinned desktop's small login/D-Bus runtime and ELF closure.

Read ELF metadata as data; never run the selected binaries or package scripts.
The caller copies through desktop_payload, which repeats exact input checks.
"""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess

import desktop_payload


def select(pins,tree):
    raw=Path(pins).read_bytes();inventory=json.loads(raw);desktop_payload.validate(inventory)
    files={e['destination']:e for e in inventory['files']};aliases=inventory['symlinks']
    selected={};links={};pending=[]
    seeds={n for n in files if n.startswith(('/usr/lib/systemd/','/usr/lib64/security/','/usr/share/dbus-1/','/usr/lib/udev/rules.d/'))}
    seeds|={'/usr/bin/systemctl','/usr/bin/chvt','/usr/bin/dbus-broker','/usr/bin/dbus-broker-launch','/usr/bin/true','/usr/bin/false'}
    # systemd 259 loads these through dlopen, outside DT_NEEDED. Preserve its
    # ACL/device mediation and vendor sandbox support rather than disabling it.
    seeds|={'/usr/lib64/libacl.so.1','/usr/lib64/libseccomp.so.2'}
    def add(name):
        if name in aliases:
            target=str(PurePosixPath(name).parent/aliases[name]);links[name]=aliases[name];add(target);return
        if name in selected:return
        entry=files.get(name)
        if entry is None:raise ValueError('unpinned login runtime object: '+name)
        selected[name]=entry;pending.append(entry)
    for name in sorted(seeds):add(name)
    while pending:
        e=pending.pop();p=Path(e['source']);data=p.read_bytes()
        if p.is_symlink() or len(data)!=e['size'] or hashlib.sha256(data).hexdigest()!=e['sha256']:raise ValueError('changed login runtime input: '+str(p))
        if not data.startswith(b'\x7fELF'):continue
        result=subprocess.run(['readelf','--dynamic','--wide',str(p)],capture_output=True,check=True,text=True,env={**os.environ,'LC_ALL':'C'})
        for soname in re.findall(r'\(NEEDED\).*Shared library: \[([^\]]+)\]',result.stdout):
            if '/' in soname:raise ValueError('unexpected absolute ELF dependency')
            name='/usr/lib64/'+soname
            # The reviewed boot root has some independently pinned flat copies.
            # Keep those when no canonical member is needed by this selection.
            if (tree/name.lstrip('/')).is_file():continue
            if name not in files and name not in aliases:
                matches=[n for n in files if PurePosixPath(n).name==soname]
                if len(matches)!=1:raise ValueError('unresolved login dependency: '+soname)
                name=matches[0]
            add(name)
    packages={e['package'] for e in selected.values()}
    for n,e in files.items():
        if '/licenses/' in n and e['package'] in packages:selected[n]=e
    profile=dict(format=1,purpose='Existing pinned login/user-manager runtime for isolated audio tests',files=list(sorted(selected.values(),key=lambda e:e['destination'])),symlinks=dict(sorted(links.items())))
    desktop_payload.validate(profile)
    return profile,hashlib.sha256(raw).hexdigest()
