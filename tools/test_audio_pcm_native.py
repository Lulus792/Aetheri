"""Ordinary Linux bad-input checks. No sound device is opened or modified."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(binary):
    if os.getuid()!=1000:raise ValueError('dedicated ordinary builder required')
    cases=[[],['--unknown'],['--play','extra'],['--play'],['--baseline']]
    for args in cases:
        with tempfile.TemporaryFile() as source:
            source.write(b'keep-original-file-bytes');source.flush()
            launcher="import os,sys;os.dup2(int(sys.argv[1]),3,inheritable=True);os.execv(sys.argv[2],sys.argv[2:])"
            p=subprocess.run([sys.executable,'-c',launcher,str(source.fileno()),str(binary),*args],pass_fds=(source.fileno(),),capture_output=True,timeout=5)
            assert p.returncode==2 and not p.stdout and not p.stderr,(args,p.returncode,p.stdout,p.stderr)
            source.seek(0);assert source.read()==b'keep-original-file-bytes'
    print('PCM bad-input controls: five cases, original file preserved, no device access')


if __name__=='__main__':run(Path(sys.argv[1]).resolve())
