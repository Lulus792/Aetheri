"""Deterministic cross-process visibility/collision/failure tests for test barriers."""
import errno
import os
from pathlib import Path
import select
import stat
import tempfile

import root_issuer_fixture as fixture


def main():
    assert os.geteuid()==0
    with tempfile.TemporaryDirectory(prefix='issuer-publish-') as temporary:
        root=Path(temporary); path=root/'barrier'; content=b'complete original barrier\n'
        ready_r,ready_w=os.pipe(); go_r,go_w=os.pipe()
        child=os.fork()
        if child==0:
            os.close(ready_r); os.close(go_w)
            actual=fixture.os.fsync; observed=False
            def pause(fd):
                nonlocal observed
                actual(fd)
                if not observed and stat.S_ISREG(os.fstat(fd).st_mode):
                    observed=True
                    assert os.fstat(fd).st_nlink==0
                    assert os.write(ready_w,b'R')==1 and os.read(go_r,1)==b'G'
            fixture.os.fsync=pause
            fixture.flush_file(path,content)
            os._exit(0)
        os.close(ready_w); os.close(go_r)
        waited=False
        try:
            assert select.select([ready_r],[],[],5)[0] and os.read(ready_r,1)==b'R'
            assert not path.exists(), 'a reader observed a named file before publication'
            assert os.write(go_w,b'G')==1
            pid,status=os.waitpid(child,0)
            waited=True
            assert pid==child and os.WIFEXITED(status) and os.WEXITSTATUS(status)==0
        finally:
            os.close(ready_r); os.close(go_w)
            if not waited: os.waitpid(child,0)
        observed=path.stat()
        assert path.read_bytes()==content and observed.st_nlink==1 and stat.S_IMODE(observed.st_mode)==0o600
        try: fixture.flush_file(path,b'foreign replacement')
        except FileExistsError: pass
        else: raise AssertionError('existing barrier overwritten')
        assert path.read_bytes()==content and path.stat().st_ino==observed.st_ino
        baseline=len(list(Path('/proc/self/fd').iterdir()))
        actual=fixture.os.fsync
        def fail(fd):
            if stat.S_ISREG(os.fstat(fd).st_mode): raise OSError(errno.EIO,'test pre-publication flush')
            actual(fd)
        fixture.os.fsync=fail
        try:
            try: fixture.flush_file(root/'failed',b'unpublished')
            except OSError as error: assert error.errno==errno.EIO
            else: raise AssertionError('flush failure ignored')
        finally: fixture.os.fsync=actual
        assert not (root/'failed').exists() and len(list(Path('/proc/self/fd').iterdir()))==baseline
    print('issuer barriers: invisible until flushed, sole create-only link, preserved collision and no FD leak')


if __name__=='__main__': main()
