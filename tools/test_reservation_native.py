"""Independent canonical plan/request bytes; no service or process acceptance."""
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
    plan=dict(plan=identity(40),authority=identity(1),actor=identity(50),scope=identity(2),kind="launch",component="aetheri.probe",
        install=identity(10),generation=identity(14),data=identity(12),artifact=None,grant=None,retention="none",action="main",boot=identity(4),expires=2**63-1,
        reads=[dict(epoch=identity(1),kind=kind,ref=identity(ref),revision=revision) for kind,ref,revision in (("gate",10,3),("install",10,2),("scope",2,1))],
        effects=[dict(number=0,kind="generation",component="aetheri.probe",ref=identity(14),disposition="retain",reason="launch_reservation")])
    request=b"AETHRSV1"+b"".join(bytes.fromhex(identity(n)) for n in (1,32,31,50,51,2,10,14,12,4))+struct.pack(">Q",2)
    for value in (b"main",b"p0",b"none"): request+=struct.pack(">I",len(value))+value
    expected=hashlib.sha256(reference.intent(plan)).digest()+hashlib.sha256(request).digest()
    with tempfile.TemporaryDirectory(prefix="reservation-",dir=build) as temporary:
        root=Path(temporary); path=root/"expected"; path.write_bytes(expected)
        for scenario in ("faults","exit-before","exit-after"):
            catalogue=root/scenario; catalogue.mkdir(mode=0o700)
            subprocess.run([executable,catalogue,path,scenario],check=True,timeout=45)


if __name__=="__main__": main()
