"""Coordinated bounded JSON for cooperating private image-test actors.

The publisher initializes the stable guard before readers start. Keep that inode;
never unlink/recreate it. All live readers must use read(), not ordinary open().
This host-test protocol is not process/volume authority or power-loss recovery.
"""
from contextlib import contextmanager
import errno
import json
import os
from pathlib import Path
import stat
import time


@contextmanager
def guard(path,timeout=5):
    if type(timeout) not in (int,float) or not 0<timeout<=5:raise ValueError('invalid guard deadline')
    path=Path(path);lock=path.with_suffix(path.suffix+'.guard')
    with lock.open('a+b') as held:
        if held.seek(0,2)==0:held.write(b'\0');held.flush()
        info=os.fstat(held.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size!=1:raise ValueError('different stable image-state guard')
        if os.name=='nt':
            import msvcrt
            def acquire():held.seek(0);msvcrt.locking(held.fileno(),msvcrt.LK_NBLCK,1)
            def release():held.seek(0);msvcrt.locking(held.fileno(),msvcrt.LK_UNLCK,1)
        else:
            import fcntl
            def acquire():fcntl.flock(held.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
            def release():fcntl.flock(held.fileno(),fcntl.LOCK_UN)
        deadline=time.monotonic()+timeout
        while True:
            try:acquire();break
            except OSError as error:
                if error.errno not in (errno.EACCES,errno.EAGAIN):raise
                if time.monotonic()>=deadline:raise TimeoutError('image-state guard busy') from error
                time.sleep(.002)
        try:yield
        finally:release()


def read(path,limit=8*1024**2):
    if type(limit) is not int or not 1<=limit<=8*1024**2:raise ValueError('invalid state bound')
    with guard(path),Path(path).open('rb') as stream:
        info=os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or not 0<info.st_size<=limit:raise ValueError('invalid published state file')
        raw=stream.read(limit+1)
        if not 0<len(raw)<=limit:raise ValueError('oversized published state')
    value=json.loads(raw)
    if not isinstance(value,dict):raise ValueError('published state must be an object')
    return value


def write(path,value):
    if not isinstance(value,dict):raise ValueError('published state must be an object')
    raw=(json.dumps(value,indent=2)+'\n').encode('utf-8')
    if len(raw)>8*1024**2:raise ValueError('oversized published state')
    path=Path(path)
    with guard(path):
        temporary=path.with_suffix(path.suffix+'.tmp')
        with temporary.open('xb') as output:output.write(raw);output.flush();os.fsync(output.fileno())
        os.replace(temporary,path)
