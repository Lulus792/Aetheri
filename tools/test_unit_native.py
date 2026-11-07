"""Explicit enforcing disposable-carrier unit admission/custody component test."""
from pathlib import Path
import json,subprocess,sys,tempfile,time

def main():
    executable,build=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve()
    cases=('normal','nested','fork','copy','graph','argv','dropin','privilege-flag','restart','force-restart','source','guard-source','guard-bus','guard-deny',
           'subscribe-before','subscribe-after','ref-before','ref-after','ref2-before','ref2-after',
           'unref-before','unref-after','unsubscribe-before','unsubscribe-after')
    root=Path(tempfile.mkdtemp(prefix='unit-custody-',dir=build))
    # Real PID1 effects need retained independent fixture receipts on failure.
    # Never discard these as a TemporaryDirectory after an unknown dispatch.
    reports=[]
    for case in cases:
        start=time.monotonic()
        result=subprocess.run([str(executable),case,str(root)],timeout=35)
        reports.append(dict(case=case,exit=result.returncode,seconds=time.monotonic()-start))
        (root/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
        result.check_returncode()
    print('UNIT_CUSTODY_RECEIPTS '+str(root),flush=True)

if __name__=='__main__':main()
