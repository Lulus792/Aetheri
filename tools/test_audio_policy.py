"""Run actual private policy/default-sink/mixer tests without hardware access.

This graph-level suite precedes ALSA monitor and normal session admission. It
does not load any audio/video/BlueZ monitor or use the carrier's user bus.
"""
import argparse
import json
import os
from pathlib import Path
import tempfile

import prepare_audio
import prepare_audio_policy
from test_audio import run_case


def run(binary,prefix,policy,work):
    if os.getuid()==0:raise ValueError('ordinary test user required')
    prepare_audio.check(prefix);prepare_audio_policy.check(policy)
    root=Path(tempfile.mkdtemp(prefix='pol-',dir=work));print('Policy evidence:',root,flush=True)
    results=[]
    for name,frames,volume,muted in (('unity',48000,1.0,False),('half',48000,.5,False),('muted',257,1.0,True)):
        results.append(run_case(binary,prefix,root/name,frames,policy=policy,volume=volume,muted=muted))
    results.append(run_case(binary,prefix,root/'without-manager',257,failure='deadline',policy=policy,manager=False))
    prepare_audio.check(prefix);prepare_audio_policy.check(policy)
    (root/'result.json').write_text(json.dumps(dict(passed=True,cases=results),indent=2)+'\n')
    return root


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('prefix',type=Path);p.add_argument('policy',type=Path);p.add_argument('work',type=Path);a=p.parse_args();run(a.binary.resolve(),a.prefix.resolve(),a.policy.resolve(),a.work.resolve())
