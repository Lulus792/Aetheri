"""Copy pinned runtime payloads into a private image tree, never install to a host.

The caller supplies the fresh ordinary-user build tree. Content/provenance pins
are development input checks, not publisher/volume admission or a package manager.
Failure preserves the partial new tree; it never overwrites conflicting entries.
"""
import hashlib
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import stat


def destination(value):
    if not isinstance(value,str) or not re.fullmatch(r'/usr/[A-Za-z0-9._+/@-]+',value):
        raise ValueError('invalid runtime destination')
    path=PurePosixPath(value)
    if str(path)!=value or '..' in path.parts:
        raise ValueError('escaping or noncanonical runtime destination')
    return path


def validate(manifest):
    if not isinstance(manifest,dict) or manifest.get('format')!=1:
        raise ValueError('unsupported runtime manifest')
    files,links=manifest.get('files'),manifest.get('symlinks')
    if not isinstance(files,list) or not 1<=len(files)<=4096 or not isinstance(links,dict) or len(links)>1024:
        raise ValueError('runtime inventory bound')
    selected={};total=0
    for entry in files:
        if not isinstance(entry,dict):raise ValueError('invalid runtime entry')
        path=destination(entry.get('destination'))
        if str(path) in selected:raise ValueError('duplicate runtime destination')
        size=entry.get('size');digest=entry.get('sha256');mode=entry.get('mode');source=entry.get('source')
        if type(size) is not int or not 0<=size<=256*1024**2 or not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest) or type(mode) is not int or mode not in (0o644,0o755) or not isinstance(source,str) or not (Path(source).is_absolute() or PurePosixPath(source).is_absolute()):
            raise ValueError('invalid runtime source/size/digest/mode')
        if bool(entry.get('package'))==bool(entry.get('provenance')):
            raise ValueError('runtime provenance must identify one RPM or SDK')
        total+=size;selected[str(path)]=entry
    if total>512*1024**2:raise ValueError('runtime byte bound')
    for name,target in links.items():
        alias=destination(name)
        if name in selected or not isinstance(target,str) or not target or target.startswith('/') or '\\' in target or '\x00' in target or posixpath.normpath(target)!=target:
            raise ValueError('conflicting or invalid runtime alias')
        parts=list(alias.parent.parts[1:])
        for part in target.split('/'):
            if part=='..':
                if not parts:raise ValueError('escaping runtime alias')
                parts.pop()
            elif part in ('','.'):
                raise ValueError('noncanonical runtime alias')
            else:parts.append(part)
        resolved=posixpath.normpath(str(alias.parent)+'/'+target)
        if resolved not in selected:raise ValueError('runtime alias must select a pinned file')
    names=set(selected)|set(links)
    for name in names:
        if any(str(parent) in names for parent in PurePosixPath(name).parents):
            raise ValueError('runtime entry used as an ancestor')
    return total


def parents(tree,path):
    current=tree
    for part in path.parts[1:-1]:
        current=current/part
        try:info=current.lstat()
        except FileNotFoundError:current.mkdir();continue
        if not stat.S_ISDIR(info.st_mode):raise ValueError('runtime parent is not a directory')


def install(tree,manifest):
    total=validate(manifest)
    if not stat.S_ISDIR(tree.lstat().st_mode):raise ValueError('image tree must be a directory')
    copied=reused=0
    for entry in manifest['files']:
        relative=destination(entry['destination']);parents(tree,relative)
        path=tree/str(relative).lstrip('/')
        try:incumbent=path.lstat()
        except FileNotFoundError:incumbent=None
        if incumbent is not None:
            if not stat.S_ISREG(incumbent.st_mode) or incumbent.st_size!=entry['size'] or incumbent.st_nlink!=1 or (os.name=='posix' and stat.S_IMODE(incumbent.st_mode)!=entry['mode']):
                raise ValueError('conflicting existing runtime object')
            with path.open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
            if actual!=entry['sha256']:raise ValueError('conflicting existing runtime bytes')
            reused+=1;continue
        named=Path(entry['source']).lstat()
        if not stat.S_ISREG(named.st_mode):raise ValueError('runtime source must not be a link/device')
        fd=os.open(entry['source'],os.O_RDONLY|getattr(os,'O_CLOEXEC',0)|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_BINARY',0))
        with os.fdopen(fd,'rb') as source:
            info=os.fstat(source.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_size!=entry['size'] or (info.st_dev,info.st_ino)!=(named.st_dev,named.st_ino):
                raise ValueError('runtime source is not the pinned regular file')
            digest=hashlib.sha256();used=0
            with path.open('xb') as target:
                while block:=source.read(1024**2):
                    used+=len(block)
                    if used>entry['size']:raise ValueError('runtime source grew')
                    digest.update(block);target.write(block)
            if used!=entry['size'] or digest.hexdigest()!=entry['sha256']:
                raise ValueError('runtime source bytes changed; preserve partial tree')
        path.chmod(entry['mode']);copied+=1
    for name,target in manifest['symlinks'].items():
        relative=destination(name);parents(tree,relative);path=tree/name.lstrip('/')
        try:info=path.lstat()
        except FileNotFoundError:path.symlink_to(target);continue
        if not stat.S_ISLNK(info.st_mode) or os.readlink(path)!=target:
            raise ValueError('conflicting existing runtime alias')
    return dict(files=len(manifest['files']),aliases=len(manifest['symlinks']),bytes=total,copied=copied,reused=reused)


def normalize_base(tree,manifest,baseline):
    """Retire only pinned flat-library COPIES inside the fresh build tree.

    The original RAM builder flattened a few SONAMEs into regular files. The
    independently supplied baseline hashes must agree with both that copy and
    the new pinned target. This is not an installed-file migration/adoption API.
    """
    validate(manifest)
    entries={e['destination']:e for e in manifest['files']};selected=[]
    for name,target in manifest['symlinks'].items():
        path=tree/name.lstrip('/')
        try:info=path.lstat()
        except FileNotFoundError:continue
        if not stat.S_ISREG(info.st_mode):continue
        expected=entries[posixpath.normpath(str(PurePosixPath(name).parent)+'/'+target)]['sha256']
        if baseline.get(name)!=expected or info.st_nlink!=1:
            raise ValueError('unrecognized flat base alias')
        with path.open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
        if actual!=expected:raise ValueError('changed flat base alias')
        selected.append(path)
    for path in selected:path.unlink()
    return [path.relative_to(tree).as_posix() for path in selected]
