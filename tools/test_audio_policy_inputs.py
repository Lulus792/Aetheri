"""Check real prefix corruptions in a fresh ordinary-user Linux clone only."""
import argparse
import json
import os
from pathlib import Path
import shutil

import prepare_audio_policy as policy


def run(prefix,work):
    if os.name!='posix' or os.getuid()==0:raise ValueError('ordinary native Linux test required')
    policy.check(prefix);copy=work/'policy-prefix-check';shutil.copytree(prefix,copy,symlinks=True)
    results=[]
    def reject(name):
        try:policy.check(copy)
        except ValueError:results.append(name)
        else:raise AssertionError('changed prefix accepted: '+name)
    library=copy/'usr/lib64/libwireplumber-0.5.so.0.519.0';raw=library.read_bytes()
    library.write_bytes(b'X'+raw[1:]);reject('changed-library');library.write_bytes(raw)
    script=copy/'usr/share/wireplumber/scripts/metadata.lua';raw=script.read_bytes();script.unlink()
    reject('missing-script');script.write_bytes(raw);script.chmod(0o644)
    alias=copy/'usr/lib64/libwireplumber-0.5.so.0';target=os.readlink(alias);alias.unlink();alias.symlink_to('foreign.so')
    reject('changed-alias');alias.unlink();alias.symlink_to(target)
    extra=copy/'unexpected';extra.write_bytes(b'x');reject('unexplained-file');extra.unlink()
    record=copy/'inputs.json';raw=record.read_bytes();wrong=json.loads(raw);wrong['profile_sha256']='0'*64;record.write_text(json.dumps(wrong))
    reject('changed-profile');record.write_bytes(raw)
    linked=work/'policy-aliased-prefix';linked.symlink_to(copy,target_is_directory=True)
    try:policy.check(linked)
    except ValueError:results.append('aliased-prefix')
    else:raise AssertionError('aliased prefix accepted')
    linked.unlink();policy.check(copy);policy.check(prefix)
    result=dict(passed=True,rejected=results,original_unchanged=True)
    (work/'policy-input-checks.json').write_text(json.dumps(result,indent=2)+'\n');return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('prefix',type=Path);p.add_argument('work',type=Path);a=p.parse_args();print(json.dumps(run(a.prefix.resolve(),a.work.resolve())))
