"""Independent user launch canonical bytes; separate unconfined owner stores."""
from pathlib import Path
import hashlib
import struct
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
        root=Path(temporary); expected=root/"expected"
        scenarios=("exec-fresh","exec-lost","exec-crash-request-before","exec-crash-request-after","exec-crash-arm-before","exec-crash-arm-after",
                         "exec-restart-1","exec-restart-2","exec-restart-3","exec-restart-4","exec-user-restart","exec-closed-gate",
                         "exec-overflow-lease","exec-overflow-dispatch","exec-overflow-operation","exec-overflow-catalogue") if len(sys.argv)==4 and sys.argv[3]=="execution" else (
                         "faults","exit-before","exit-after","recovery","lost-root-reply","setup","setup-restart","setup-terminal","setup-dispatch-overflow","setup-lease-overflow")
        for scenario in scenarios:
            plan["action"]="run" if scenario.startswith("exec-") else "main"
            expected.write_bytes(hashlib.sha256(reference.intent(plan)).digest())
            if scenario.startswith("exec-"):
                request=b"AETHRSV1"+b"".join(bytes.fromhex(identity(n)) for n in (1,32,31,50,51,2,10,14,12,4))+struct.pack(">Q",2)
                for value in (b"run",b"p0",b"none"): request+=struct.pack(">I",len(value))+value
                profile=b"AETHEXE1"+b"".join(bytes.fromhex(identity(n)) for n in (1,5,50,51,4,2,10,14,12,13,31,32,41,61))
                profile+=struct.pack(">QQQ",2,3,3)+bytes(32)+bytes([0x34])*32+hashlib.sha256(request).digest()+bytes([0x56])*32
                assert len(profile)==384
                expected.with_name(expected.name+".execution").write_bytes(profile+hashlib.sha256(profile).digest())
            case=root/scenario; case.mkdir(); system,user=case/"system",case/"user"
            system.mkdir(mode=0o700); user.mkdir(mode=0o700)
            subprocess.run([executable,system,user,expected,scenario],check=True,timeout=50)

if __name__=="__main__": main()
