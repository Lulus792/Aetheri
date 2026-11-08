"""Invalid playback requests fail before creating an audio connection."""
import os
from pathlib import Path
import subprocess
import sys

binary = Path(sys.argv[1]).resolve()
assert os.getuid() != 0
cases = [[], ['--remote'], ['--remote','relative'], ['--unknown'],
         ['--remote','/absent','--remote','/other'], ['--silent','--silent'],
         ['--wait-for-start','--wait-for-start'],['--autoconnect','--autoconnect']]
for value in ('0','96001','9999999','-1','+1','1x',' 1',''):
    cases.append(['--remote','/absent','--frames',value])
for args in cases:
    p = subprocess.run([str(binary), *args], capture_output=True, timeout=4)
    assert p.returncode == 2 and not p.stdout and not p.stderr, (args, p.returncode, p.stdout, p.stderr)
print(len(cases), 'invalid audio requests rejected before connection')
