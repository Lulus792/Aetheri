"""Real private candidate corruption and explicit-selection controls on Linux."""
import argparse
import json
import os
from pathlib import Path
import shutil

import prepare_audio_policy as original
import prepare_audio_policy_candidate as candidate
from test_audio import check_policy


def run(base,prefix,work):
    if os.name!='posix' or os.getuid()==0:raise ValueError('ordinary native Linux test required')
    original.check(base);candidate.check(prefix);copy=work/'candidate-controls';shutil.copytree(prefix,copy,symlinks=True)
    results=[]
    def reject(label,fn,path):
        try:fn(path)
        except ValueError:results.append(label)
        else:raise AssertionError('changed/unselected candidate accepted: '+label)
    reject('normal-default-refuses-candidate',check_policy,prefix)
    reject('candidate-refuses-normal-base',candidate.check,base)
    for label,name in [('module-bytes',candidate.MODULE),('lua-bytes','usr/lib64/liblua-5.5.so'),('source-notice-bytes',candidate.SOURCE)]:
        p=copy/name;raw=p.read_bytes();p.write_bytes(b'X'+raw[1:]);reject(label,candidate.check,copy);p.write_bytes(raw)
    p=copy/'usr/lib64/liblua-5.4.so';p.write_bytes(b'old ABI');reject('extra-old-Lua',candidate.check,copy);p.unlink()
    p=copy/'usr/share/licenses/lua/COPYRIGHT';raw=p.read_bytes();p.unlink();reject('missing-Lua-notice',candidate.check,copy);p.write_bytes(raw);p.chmod(0o644)
    p=copy/'usr/lib64/libwireplumber-0.5.so.0';target=os.readlink(p);p.unlink();p.symlink_to('foreign.so');reject('changed-core-alias',candidate.check,copy);p.unlink();p.symlink_to(target)
    p=copy/'inputs.json';raw=p.read_bytes();record=json.loads(raw);record['candidate_profile_sha256']='0'*64;p.write_text(json.dumps(record));reject('changed-candidate-record',candidate.check,copy);p.write_bytes(raw)
    p=copy/'usr/lib64/liblua-5.5.so';p.chmod(0o644);reject('changed-Lua-mode',candidate.check,copy);p.chmod(0o755)
    check_policy(copy,'lua55-candidate');original.check(base);candidate.check(prefix)
    result=dict(passed=True,rejected=results,base_and_candidate_unchanged=True)
    (work/'candidate-controls.json').write_text(json.dumps(result,indent=2)+'\n');return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('base','prefix','work'):p.add_argument(n,type=Path)
    a=p.parse_args();print(json.dumps(run(a.base,a.prefix,a.work)))
