"""Check concrete SDK corruptions only in a new ordinary-user clone."""
import argparse
import json
import os
from pathlib import Path
import shutil

import prepare_audio_control as control


def run(prefix,work):
    if os.name!='posix' or os.getuid()==0:raise ValueError('ordinary Linux test required')
    control.check(prefix);copy=work/'control-prefix-check';shutil.copytree(prefix,copy,symlinks=True);rejected=[]
    def reject(name):
        try:control.check(copy)
        except ValueError:rejected.append(name)
        else:raise AssertionError('changed SDK accepted: '+name)
    for relative,name in [('usr/include/wireplumber-0.5/wp/wp.h','changed-header'),('usr/lib64/libglib-2.0.so.0.8800.0','changed-runtime')]:
        p=copy/relative;raw=p.read_bytes();p.write_bytes(b'X'+raw[1:]);reject(name);p.write_bytes(raw)
    p=copy/'usr/lib64/libglib-2.0.so.0';target=os.readlink(p);p.unlink();p.symlink_to('foreign.so');reject('changed-alias');p.unlink();p.symlink_to(target)
    p=copy/'unexpected';p.write_bytes(b'x');reject('unexplained-file');p.unlink()
    p=copy/'inputs.json';raw=p.read_bytes();record=json.loads(raw);record['profile_sha256']='0'*64;p.write_text(json.dumps(record));reject('changed-profile');p.write_bytes(raw)
    p=work/'aliased-control';p.symlink_to(copy,target_is_directory=True)
    try:control.check(p)
    except ValueError:rejected.append('aliased-prefix')
    else:raise AssertionError('aliased prefix accepted')
    p.unlink();control.check(copy);control.check(prefix)
    result=dict(passed=True,rejected=rejected,original_unchanged=True)
    (work/'control-input-checks.json').write_text(json.dumps(result,indent=2)+'\n');return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('prefix',type=Path);p.add_argument('work',type=Path);a=p.parse_args();print(json.dumps(run(a.prefix.resolve(),a.work.resolve())))
