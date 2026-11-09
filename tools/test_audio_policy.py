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
from test_audio import run_case,check_policy


def run(binary,prefix,policy,work,policy_profile='rpm'):
    if os.getuid()==0:raise ValueError('ordinary test user required')
    prepare_audio.check(prefix);check_policy(policy,policy_profile)
    root=Path(tempfile.mkdtemp(prefix='pol-',dir=work));print('Policy evidence:',root,flush=True)
    results=[]
    for name,frames,volume,muted in (('unity',48000,1.0,False),('half',48000,.5,False),('muted',257,1.0,True)):
        results.append(run_case(binary,prefix,root/name,frames,policy=policy,volume=volume,muted=muted,policy_profile=policy_profile))
    results.append(run_case(binary,prefix,root/'without-manager',257,failure='deadline',policy=policy,manager=False,policy_profile=policy_profile))
    prepare_audio.check(prefix);check_policy(policy,policy_profile)
    (root/'result.json').write_text(json.dumps(dict(passed=True,policy_profile=policy_profile,cases=results),indent=2)+'\n')
    return root


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('prefix',type=Path);p.add_argument('policy',type=Path);p.add_argument('work',type=Path);p.add_argument('--policy-profile',choices=('rpm','lua55-candidate'),default='rpm');a=p.parse_args();run(a.binary.resolve(),a.prefix.resolve(),a.policy.resolve(),a.work.resolve(),a.policy_profile)
