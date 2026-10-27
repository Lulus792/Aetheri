"""Independent user launch canonical bytes; separate unconfined owner stores."""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile
import runtime_contracts as reference

def identity(n): return "000000000000400080000000000000"+f"{n:02x}"

def main():
    executable,build=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve()
    plan=dict(plan=identity(60),authority=identity(50),actor=identity(62),scope=identity(2),kind="launch",component="aetheri.probe",
        install=identity(10),generation=identity(14),data=identity(12),artifact=None,grant=None,retention="none",action="main",boot=identity(4),expires=2**63-1,
        reads=[dict(epoch=identity(epoch),kind=kind,ref=identity(ref),revision=revision) for epoch,kind,ref,revision in
            ((1,"gate",10,3),(1,"install",10,2),(50,"data",12,1),(50,"gate",12,3))],
        effects=[dict(number=0,kind="data",component="aetheri.probe",ref=identity(12),disposition="retain",reason="launch_data_reservation")])
    with tempfile.TemporaryDirectory(prefix="launch-",dir=build) as temporary:
        root=Path(temporary); expected=root/"expected"; expected.write_bytes(hashlib.sha256(reference.intent(plan)).digest())
        for scenario in ("faults","exit-before","exit-after","recovery","lost-root-reply"):
            case=root/scenario; case.mkdir(); system,user=case/"system",case/"user"
            system.mkdir(mode=0o700); user.mkdir(mode=0o700)
            subprocess.run([executable,system,user,expected,scenario],check=True,timeout=50)

if __name__=="__main__": main()
