"""Real detached/attached mounts in fresh child namespaces of the dedicated VM."""
from pathlib import Path
import os
import select
import subprocess
import sys
import tempfile


def main():
    binary,build=Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]).resolve(strict=True)
    root=Path(tempfile.mkdtemp(prefix="view-",dir=build))
    try:
        for case in ("mounts","faults","fork","namespace","unshare-fault","private-fault"):
            work=root/case; work.mkdir(mode=0o700)
            child=subprocess.Popen([str(binary),str(work),case],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
            try:
                if case not in ("unshare-fault","private-fault"):
                    if not select.select([child.stdout],[],[],8)[0]: raise TimeoutError("mount probe did not reach the observation boundary")
                    if child.stdout.readline().strip()!="views-attached": raise AssertionError("mount probe failed before attachment")
                    # Observe the parent namespace WHILE the other namespace's
                    # views remain attached, not merely after the child's cleanup.
                    for slot in ("app","data"):
                        target=work/"setup"/slot
                        if os.path.ismount(target) or set(target.iterdir()): raise AssertionError("private mount propagated to parent")
                    if (work/"code/nested/foreign").exists() or not (work/"code/nested/underlay").is_file():
                        raise AssertionError("nested test mount propagated to parent")
                output,_=child.communicate("continue\n",timeout=8)
                if child.returncode: raise subprocess.CalledProcessError(child.returncode,child.args,output)
            finally:
                if child.poll() is None:
                    # Drop the test handshake, not a process or ownership claim.
                    # The reviewed child has a bounded wait and exits on EOF.
                    child.communicate("abort\n",timeout=8)
            # Mounts are only in the now-terminal child namespace. Parent fixtures
            # were never covered or remounted. Test removes its tiny known files.
            if set(work.iterdir()): raise AssertionError("test left fixture entries")
            work.rmdir()
        root.rmdir()
    except BaseException:
        print(f"Preserved view fixture for inspection: {root}",file=sys.stderr)
        raise
    print("Real private mount views: source identity/flags, bounded attach, namespace/fork guards and failures passed.")


if __name__=="__main__": main()
