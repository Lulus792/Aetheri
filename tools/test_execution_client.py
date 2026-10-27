"""Explicit confined Root reply-client probe in the registered disposable guest.

The temporary producer only serves test replies: no catalogue, launcher, mount
view or target exec. It reuses the installed Root domain and temporary anonymous-
pipe permissions, removed only after its exact owned unit is drained.
Use a Release producer; normal peer capture remains separately ASan/UBSan tested.
"""
import bz2
import hashlib
import json
import os
from pathlib import Path
import select
import stat
import struct
import subprocess
import sys
import uuid

ROOT=Path("/var/lib/aetheri-p0-execution-client-test")
MODULE="aetheri_p0_execution_client"
SOURCE=Path(__file__).resolve().parents[1]


def run(*args): return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT,timeout=90).strip()
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def save(record):
    fd=os.open(ROOT/"controller.next",os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_CLOEXEC,0o600)
    with os.fdopen(fd,"w") as f:
        json.dump(record,f,indent=2); f.flush(); os.fsync(f.fileno())
    os.replace(ROOT/"controller.next",ROOT/"controller.json")
    fd=os.open(ROOT,os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC); os.fsync(fd); os.close(fd)


def installed():
    value=(Path("/var/lib/selinux/targeted/active/modules/400")/MODULE/"cil").read_bytes()
    return bz2.decompress(value) if value.startswith(b"BZh") else value


def drained(unit):
    facts=dict(line.split("=",1) for line in run("systemctl","show",unit,"--property=ActiveState,MainPID,ControlGroup").splitlines())
    jobs=json.loads(run("busctl","--system","--json=short","call","org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","ListJobs"))
    if facts.get("ActiveState") not in ("inactive","failed") or facts.get("MainPID")!="0" or facts.get("ControlGroup") or jobs["type"]!="a(usssoo)" or any(j[1]==unit for j in jobs["data"][0]):
        raise RuntimeError("reply fixture not independently drained")
    return facts["ActiveState"]


def retire():
    s=ROOT.lstat(); marker=ROOT/"controller.json"; m=marker.lstat()
    if ROOT.is_symlink() or not stat.S_ISDIR(s.st_mode) or s.st_uid!=0 or stat.S_IMODE(s.st_mode)!=0o755 or marker.is_symlink() or not stat.S_ISREG(m.st_mode) or m.st_uid!=0 or stat.S_IMODE(m.st_mode)!=0o600 or m.st_nlink!=1:
        raise RuntimeError("fixture ownership changed")
    r=json.loads(marker.read_text()); expected="aetheri-p0-execution-client-"+uuid.UUID(hex=r["token"]).hex+".service"
    if r["pending"] is not None or r["unit"]!=expected: raise RuntimeError("unknown/unacknowledged unit request; preserve")
    state=drained(r["unit"])
    if {p.name for p in ROOT.iterdir()}!={"controller.json","server","profile",MODULE+".cil"}: raise RuntimeError("unknown fixture entry")
    for name,key,mode in (("server","binary_sha256",0o555),("profile","profile_sha256",0o444),(MODULE+".cil","policy_sha256",0o600)):
        p=ROOT/name; s=p.lstat()
        if p.is_symlink() or not stat.S_ISREG(s.st_mode) or s.st_uid!=0 or s.st_gid!=0 or s.st_nlink!=1 or stat.S_IMODE(s.st_mode)!=mode or sha(p)!=r[key]: raise RuntimeError("fixture input changed")
    if hashlib.sha256(installed()).hexdigest()!=r["policy_sha256"]: raise RuntimeError("temporary policy changed")
    if state=="failed": run("systemctl","reset-failed",r["unit"])
    run("semodule","-X","400","-r",MODULE)
    for name in ("server","profile",MODULE+".cil","controller.json"): (ROOT/name).unlink()
    ROOT.rmdir()


def main():
    if sys.platform!="linux" or os.geteuid()!=0 or Path("/sys/class/dmi/id/product_name").read_text().strip()!="Aetheri P0 disposable VM" or run("getenforce")!="Enforcing":
        raise RuntimeError("requires registered enforcing disposable carrier")
    import fcntl
    lock=os.open("/etc/aetheri-p0-carrier",os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW); fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    import p0_services as services
    normal=services.load(); services.verified(normal)
    if sys.argv[1:]==["--cleanup"]:
        retire(); os.close(lock); return
    if normal["pending"] is not None: raise RuntimeError("normal carrier request is pending")
    if ROOT.exists() or ROOT.is_symlink() or MODULE in run("semodule","-l").split(): raise RuntimeError("existing reply fixture requires inspection")
    if run("busctl","--system","call","org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","NameHasOwner","s","org.aetheri.System1")!="b false": raise RuntimeError("System1 already owned; do not displace it")
    binary=Path(sys.argv[1]).resolve(strict=True)
    driver=Path(sys.argv[2]).resolve(strict=True) if len(sys.argv)==3 else binary
    if driver.parent.parent!=binary.parent.parent or driver.name!=binary.name: raise RuntimeError("client driver must be the same workspace probe target")
    if binary.parent.name!="release" or b"libasan" in binary.read_bytes(): raise RuntimeError("use reviewed Release producer; no policy broadening for LeakSanitizer")
    token=uuid.uuid4().hex; unit="aetheri-p0-execution-client-"+token+".service"
    def ident(n): return bytes.fromhex("000000000000400080000000000000"+f"{n:02x}")
    boot=uuid.UUID(Path("/proc/sys/kernel/random/boot_id").read_text().strip()).bytes
    request=b"AETHRSV1"+b"".join(boot if n==4 else ident(n) for n in (1,32,31,50,51,2,10,14,12,4))+struct.pack(">Q",2)
    for text in (b"run",b"p0",b"none"): request+=struct.pack(">I",len(text))+text
    profile=b"AETHEXE1"+b"".join(boot if n==4 else ident(n) for n in (1,5,50,51,4,2,10,14,12,13,31,32,41,61))
    profile+=struct.pack(">QQQ",2,3,3)+bytes(64)+hashlib.sha256(request).digest()+bytes(32)
    ROOT.mkdir(mode=0o755); run("chcon","system_u:object_r:aetheri_root_t:s0",str(ROOT))
    r=dict(token=token,unit=unit,pending=None,state="preparing",binary_sha256=sha(binary),profile_sha256=hashlib.sha256(profile).hexdigest(),policy_sha256=sha(SOURCE/"policy/p0-execution-client.cil"))
    save(r)
    for name,data,mode,kind in (("server",binary.read_bytes(),0o555,"aetheri_system_exec_t"),("profile",profile,0o444,"aetheri_registry_t"),(MODULE+".cil",(SOURCE/"policy/p0-execution-client.cil").read_bytes(),0o600,None)):
        p=ROOT/name; p.write_bytes(data); p.chmod(mode)
        if kind: run("chcon","system_u:object_r:"+kind+":s0",str(p))
    run("semodule","-X","400","-i",str(ROOT/(MODULE+".cil")))
    if hashlib.sha256(installed()).hexdigest()!=r["policy_sha256"]: raise RuntimeError("installed fixture policy differs")
    properties=dict(User="root",Group="root",SELinuxContext="system_u:system_r:aetheri_system_t:s0",NoNewPrivileges="yes",CapabilityBoundingSet="",
        PrivateNetwork="yes",PrivateDevices="yes",ProtectHome="yes",ProtectSystem="strict",ProtectControlGroups="yes",RestrictNamespaces="yes",
        KillMode="control-group",SendSIGKILL="no",TimeoutStartSec="15s",TimeoutStopSec="10s",RuntimeMaxSec="120s",MemoryMax="96M",TasksMax="1",RemainAfterExit="yes")
    r["pending"]=unit; save(r)
    try:
        run("systemd-run","--unit="+unit,"--service-type=notify",*("--property="+k+"="+v for k,v in properties.items()),str(ROOT/"server"),"serve",str(ROOT/"profile"))
    except BaseException:
        print("Preserved possibly dispatched reply fixture; inspect its original unit/job before reconciliation",file=sys.stderr); raise
    facts=dict(line.split("=",1) for line in run("systemctl","show",unit,"--property=MainPID,InvocationID,ActiveState").splitlines())
    if facts["ActiveState"]!="active" or int(facts["MainPID"])<1 or len(facts["InvocationID"])!=32: raise RuntimeError("unknown actual reply producer")
    pidfd=os.pidfd_open(int(facts["MainPID"])); r.update(pending=None,state="active",pid=int(facts["MainPID"]),invocation=facts["InvocationID"]); save(r)
    try:
        print(run(str(driver),"client",str(ROOT/"profile"),unit))
        poll=select.poll(); poll.register(pidfd,select.POLLIN)
        if not poll.poll(10000): raise RuntimeError("original fixture process has not exited")
        run("systemctl","stop",unit); drained(unit)
        r.update(state="passed"); save(r)
    except BaseException as error:
        if isinstance(error,subprocess.CalledProcessError): print(error.output,file=sys.stderr)
        print(run("journalctl","-u",unit,"--no-pager","--output=cat"),file=sys.stderr)
        print("Preserved exact reply fixture for inspection; no automatic redispatch",file=sys.stderr); raise
    finally:
        os.close(pidfd)
    services.verified(normal); retire(); os.close(lock)
    print("Confined reply-client probe passed; original process/unit/job drained, temporary module removed, normal protected inputs unchanged; no app dispatch.")


if __name__=="__main__": main()
