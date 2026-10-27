"""Read-only execution-source admission in owned enforcing carrier fixtures.

The native harness maps only the fixed root anchor. Actual openat2, statx,
inventory/ELF/digest and full SELinux file contexts remain real. No starter policy
or execution domain is installed and no app is dispatched by this test.
"""
import errno
import hashlib
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

import runtime_contracts as reference


def identity(n): return bytes.fromhex("000000000000400080000000000000"+f"{n:02x}")
def sha(value): return hashlib.sha256(value).digest()


def label(path,kind,categories=""):
    os.setxattr(path,"security.selinux",("system_u:object_r:"+kind+":s0"+(":"+categories if categories else "")).encode())


def snapshot(root):
    result={}
    for path in [root,*root.rglob("*")]:
        s=path.lstat()
        result[path.relative_to(root).as_posix()]=(s.st_ino,s.st_uid,s.st_gid,s.st_mode,s.st_nlink,s.st_size,s.st_mtime_ns,s.st_ctime_ns,
            os.getxattr(path,"security.selinux",follow_symlinks=False),sha(path.read_bytes()) if stat.S_ISREG(s.st_mode) else None)
    return result


def live_read_only(executable,build):
    """Check existing enrolled sources, never create a protected launch profile.

    Synthetic request/operation IDs exist only in a build-local input. Closed
    gate observations are not launch admission; no intent/lease is reserved.
    """
    import fcntl
    import sqlite3
    import uuid
    import p0_services as services
    lock=os.open("/etc/aetheri-p0-carrier",os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW)
    try:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        record=services.load(); services.verified(record)
        if record["pending"] is not None: raise RuntimeError("pending carrier request")
        selected=record["exec_fixture"]; f=selected["fixture"]; data=selected["app_data"]
        before={}; meta={}; gates={}
        for role,kind,subject in (("system","install",f["install"]),("user","data",data["owner"]["data"])):
            path=services.state_path(record,role)/"catalogue.sqlite3"; before[path]=sha(path.read_bytes())
            with sqlite3.connect(f"file:{path}?mode=ro",uri=True) as db:
                meta[role]=db.execute("SELECT authority_epoch,runtime_incarnation,current_boot_id FROM authority_meta").fetchone()
                gates[role]=db.execute("SELECT state,revision FROM admission_gates WHERE subject_kind=? AND subject_ref=?",(kind,bytes.fromhex(subject))).fetchone()
                if role=="system" and (db.execute("SELECT count(*) FROM launch_dispatches WHERE state!='terminal'").fetchone()[0] or db.execute("SELECT count(*) FROM generation_leases WHERE state!='released'").fetchone()[0]):
                    raise RuntimeError("unsettled live launch/lease")
        boot=uuid.UUID(Path("/proc/sys/kernel/random/boot_id").read_text().strip()).bytes
        if any(m[2]!=boot or m[1] is None for m in meta.values()) or any(g[0]!="closed" for g in gates.values()):
            raise RuntimeError("expected current runtime metadata and closed gates")
        launch,lease,root_op,user_op=[uuid.uuid4().bytes for _ in range(4)]
        account=bytes.fromhex(record["owner"]); install=bytes.fromhex(f["install"]); generation=bytes.fromhex(f["generation"])
        epoch=bytes.fromhex(data["owner"]["data"]); storage=bytes.fromhex(data["owner"]["storage"])
        request=b"AETHRSV1"+b"".join((meta["system"][0],lease,launch,meta["user"][0],meta["user"][1],account,install,generation,epoch,boot))+struct.pack(">Q",data["install_revision"])
        for text in (b"run",b"p0",b"none"): request+=struct.pack(">I",len(text))+text
        profile=b"AETHEXE1"+b"".join((meta["system"][0],meta["system"][1],meta["user"][0],meta["user"][1],boot,account,install,generation,epoch,storage,launch,lease,root_op,user_op))
        profile+=struct.pack(">QQQ",data["install_revision"],gates["system"][1],gates["user"][1])+bytes.fromhex(f["artifact"])+bytes(32)+sha(request)+sha(bytes.fromhex(selected["app_data_witness"]))
        with tempfile.TemporaryDirectory(prefix="execution-source-observation-",dir=build) as temporary:
            expected=Path(temporary)/"profile"; expected.write_bytes(profile)
            subprocess.run([executable,"LIVE",expected,"sources","0","8"],check=True,timeout=35)
        services.verified(record)
        if any(sha(path.read_bytes())!=value for path,value in before.items()): raise AssertionError("source observation changed catalogue")
        print("Existing exec installation: actual sealed code/private data opened eight times; Enforcing, protected inputs/catalogues unchanged, gates closed, no dispatch/lease.")
    finally:
        os.close(lock)


def main():
    if sys.platform!="linux" or os.geteuid()!=0 or Path("/sys/class/dmi/id/product_name").read_text().strip()!="Aetheri P0 disposable VM" or subprocess.check_output(["getenforce"],text=True).strip()!="Enforcing":
        raise RuntimeError("requires the registered enforcing disposable carrier")
    if len(sys.argv)==4 and sys.argv[1]=="--live-read-only":
        executable,build=map(lambda p:Path(p).resolve(strict=True),sys.argv[2:])
        live_read_only(executable,build); return
    executable,payload,build=map(lambda p:Path(p).resolve(strict=True),sys.argv[1:])
    temporary=Path(tempfile.mkdtemp(prefix="execution-profile-",dir=build))
    if temporary.parent!=build: raise RuntimeError("fixture outside build")
    def observe(path):
        a,b,c,d,e=subprocess.check_output([executable,"observe",path],text=True).split()
        return int(a),bytes.fromhex(b),int(c),int(d),int(e)
    def witness(path): return struct.pack(">Q8sQQI",*observe(path))
    def directory(path,kind="aetheri_root_t",uid=0,mode=0o755,categories=""):
        path.mkdir(mode=mode); os.chown(path,uid,uid); path.chmod(mode); label(path,kind,categories)
    def file(path,value,kind="aetheri_registry_t",uid=0,mode=0o444):
        path.write_bytes(value); os.chown(path,uid,uid); path.chmod(mode); label(path,kind)
    cases=0
    def fixture(component="aetheri.p0.exec",action="run",image=None):
        nonlocal cases
        cases+=1; root=temporary/str(cases); directory(root)
        directory(root/"registry","aetheri_registry_t")
        directory(root/"store",mode=0o700); directory(root/"store/sha256","aetheri_store_parent_t",mode=0o700)
        image=payload.read_bytes() if image is None else image
        manifest=dict(component=component,publisher=identity(21).hex(),version="p0-exec",target="x86_64-linux-gnu",
            files=[dict(path="bin/probe",mode=0o555,size=len(image),sha256=sha(image).hex()),
                   dict(path="share/message",mode=0o444,size=6,sha256=sha(b"hello\n").hex())],
            actions=[dict(id=action,executable="bin/probe",arguments=["--literal",""])],dependencies=[])
        encoded=reference.fixture(manifest); artifact=sha(encoded)
        container=root/"store/sha256"/artifact.hex(); directory(container,"aetheri_store_dir_t",mode=0o555)
        file(container/"manifest",encoded,"aetheri_store_manifest_t")
        code=container/"payload"; directory(code,"aetheri_store_dir_t",mode=0o555)
        for name in ("bin","share"): directory(code/name,"aetheri_store_dir_t",mode=0o555)
        file(code/"bin/probe",image,"aetheri_store_code_t",mode=0o555)
        file(code/"share/message",b"hello\n","aetheri_store_data_t")
        fic=b"AETHFIC1"+b"".join(identity(n) for n in (60,61,62,63,2,10,14,64,21,4))+artifact+struct.pack(">QQQ",1,0,0)+bytes(72)+bytes(32)
        assert len(fic)==328
        file(root/"registry/exec-fixture.conf",fic)
        file(root/"registry/exec-fixture-store.conf",b"AETHSTO1"+sha(fic)+witness(root/"store/sha256")+witness(container))
        directory(root/"users"); account=root/"users"/identity(2).hex(); directory(account)
        directory(account/"control","aetheri_user_state_t",1000,0o700)
        directory(account/"control/app-allocations","aetheri_allocation_t",1000,0o700)
        anchor=account/"app-data"; directory(anchor,"aetheri_user_state_t",1000,0o700)
        data_root=anchor/identity(13).hex(); directory(data_root,"aetheri_user_state_t",1000,0o700)
        owner=b"AETHOWN1"+b"".join(identity(n) for n in (70,71,2,13,12,72))
        file(data_root/"owner",owner,"aetheri_user_state_t",1000,0o600)
        directory(data_root/"payload","aetheri_app_data_t",1000,0o700,"c35,c36")
        a=observe(anchor); b=observe(data_root)
        allocation=b"AETHALC1"+owner+struct.pack(">Q8sQQIQQI",a[0],a[1],*a[2:],*b[2:])
        assert len(allocation)==168 and a[:2]==b[:2]
        file(account/"control/app-allocations"/identity(13).hex(),allocation,"aetheri_allocation_t",1000,0o600)
        enrollment=b"AETHDTA1"+owner+b"".join(identity(n) for n in (10,21,1,50))+struct.pack(">QQQ",2,2,0)+allocation
        file(root/"registry/exec-app-data.conf",enrollment)
        file(root/"registry/exec-app-data-owner.conf",owner)
        request=b"AETHRSV1"+b"".join(identity(n) for n in (1,32,31,50,51,2,10,14,12,4))+struct.pack(">Q",2)
        for value in (b"run",b"p0",b"none"): request+=struct.pack(">I",len(value))+value
        profile=b"AETHEXE1"+b"".join(identity(n) for n in (1,5,50,51,4,2,10,14,12,13,31,32,41,61))
        profile+=struct.pack(">QQQ",2,3,3)+artifact+sha(image)+sha(request)+sha(allocation)
        assert len(profile)==384
        file(root/"registry/execution.conf",profile)
        expected=temporary/(str(cases)+".expected"); expected.write_bytes(profile)
        return dict(root=root,expected=expected,code=code,container=container,data=data_root,account=account)
    def invoke(f,mode="sources",expected=0,repeat=1):
        before=snapshot(f["root"])
        subprocess.run([executable,f["root"],f["expected"],mode,str(expected),str(repeat)],check=True,timeout=35)
        if snapshot(f["root"])!=before: raise AssertionError("read-only check changed fixture")
    try:
        f=fixture(); invoke(f,repeat=64); invoke(f,"profile",repeat=64); invoke(f,"starter",-errno.ENOENT)
        # This separate starter type is deliberately not installed by the test.
        # A protected binary with a different purpose must still be refused.
        file(f["root"]/"execution-starter",payload.read_bytes(),"aetheri_setup_exec_t",mode=0o555)
        invoke(f,"starter",-errno.EACCES,repeat=8)
        for kwargs,error in ((dict(component="aetheri.p0.probe"),errno.ESTALE),(dict(action="main"),errno.ENOENT),(dict(image=b"not-an-ELF\n"),errno.EBADMSG)):
            invoke(fixture(**kwargs),expected=-error,repeat=8)
        for change,error in (("profile-size",errno.EBADMSG),("profile-mode",errno.EACCES),("profile-label",errno.EACCES),("profile-link",errno.ELOOP),("registry-mode",errno.EACCES),("root-mode",errno.EACCES)):
            f=fixture(); p=f["root"]/"registry/execution.conf"
            if change=="profile-size": p.write_bytes(p.read_bytes()+b"x")
            elif change=="profile-mode": p.chmod(0o644)
            elif change=="profile-label": label(p,"aetheri_root_t")
            elif change=="profile-link": p.rename(p.with_suffix(".old")); p.symlink_to("execution.old")
            elif change=="registry-mode": p.parent.chmod(0o777)
            elif change=="root-mode": f["root"].chmod(0o777)
            invoke(f,"profile",-error,repeat=8)
        for change,error in (("scope",errno.ESTALE),("generation",errno.ESTALE),("publication-digest",errno.ESTALE),("publication-object",errno.ESTALE),
                ("extra-file",errno.EBADMSG),("extra-dir",errno.EBADMSG),("file-content",errno.EBADMSG),
                ("code-label",errno.EACCES),("code-category",errno.EACCES),("directory-label",errno.EACCES),
                ("file-mode",errno.EACCES),("file-owner",errno.EACCES),("file-group",errno.EACCES),("file-hardlink",errno.EBADMSG),
                ("file-symlink",errno.EBADMSG),("data-category",errno.EACCES),("data-mode",errno.EACCES),
                ("data-owner",errno.EACCES),("data-link",errno.ENOTDIR),("header",errno.ESTALE),
                ("allocation",errno.ESTALE),("owner-config",errno.ESTALE),("data-witness",errno.ESTALE),
                ("data-install",errno.ESTALE),("allocation-digest",errno.ESTALE)):
            f=fixture(); root=f["root"]; code=f["code"]; data=f["data"]
            if change in ("scope","generation","publication-digest","publication-object"):
                p=root/"registry"/("exec-fixture.conf" if change in ("scope","generation") else "exec-fixture-store.conf")
                b=bytearray(p.read_bytes()); b[8+(4 if change=="scope" else 6)*16+15 if change in ("scope","generation") else 8 if change=="publication-digest" else 76+16+7]^=1; p.write_bytes(b)
                if change in ("scope","generation"):
                    # Keep the publication digest internally consistent so only
                    # the execution-profile tuple check can reject this change.
                    p=root/"registry/exec-fixture-store.conf"; publication=bytearray(p.read_bytes())
                    publication[8:40]=sha(b); p.write_bytes(publication)
            elif change=="extra-file": file(code/"extra",b"extra","aetheri_store_data_t")
            elif change=="extra-dir": directory(code/"extra","aetheri_store_dir_t",mode=0o555)
            elif change=="file-content": p=code/"bin/probe"; b=bytearray(p.read_bytes()); b[-1]^=1; p.write_bytes(b)
            elif change=="code-label": label(code/"bin/probe","aetheri_store_data_t")
            elif change=="code-category": label(code/"bin/probe","aetheri_store_code_t","c35,c36")
            elif change=="directory-label": label(code/"bin","aetheri_root_t")
            elif change=="file-mode": (code/"bin/probe").chmod(0o755)
            elif change=="file-owner": os.chown(code/"bin/probe",1000,1000)
            elif change=="file-group": os.chown(code/"bin/probe",0,1000)
            elif change=="file-hardlink": os.link(code/"bin/probe",temporary/(str(cases)+".link"))
            elif change=="file-symlink": (code/"bin/probe").unlink(); (code/"bin/probe").symlink_to("../share/message")
            elif change=="data-category": label(data/"payload","aetheri_app_data_t","c37,c38")
            elif change=="data-mode": (data/"payload").chmod(0o755)
            elif change=="data-owner": os.chown(data/"payload",0,0)
            elif change=="data-link": (data/"payload").rmdir(); (data/"payload").symlink_to("../payload")
            elif change=="header": p=data/"owner"; b=bytearray(p.read_bytes()); b[-1]^=1; p.write_bytes(b)
            elif change=="allocation": p=f["account"]/"control/app-allocations"/identity(13).hex(); b=bytearray(p.read_bytes()); b[-1]^=1; p.write_bytes(b)
            elif change=="owner-config": p=root/"registry/exec-app-data-owner.conf"; b=bytearray(p.read_bytes()); b[-1]^=1; p.write_bytes(b)
            elif change in ("data-witness","data-install"):
                p=root/"registry/exec-app-data.conf"; b=bytearray(p.read_bytes()); b[327 if change=="data-witness" else 127]^=1; p.write_bytes(b)
                if change=="data-witness":
                    # Preserve valid profile/allocation hash; only the recorded
                    # filesystem witness differs from the actual original root.
                    p=f["expected"]; profile=bytearray(p.read_bytes()); profile[352:384]=sha(b[200:]); p.write_bytes(profile)
                    (root/"registry/execution.conf").write_bytes(profile)
                    (f["account"]/"control/app-allocations"/identity(13).hex()).write_bytes(b[200:])
            elif change=="allocation-digest": p=f["expected"]; b=bytearray(p.read_bytes()); b[-1]^=1; p.write_bytes(b)
            invoke(f,expected=-error,repeat=8)
        print(f"Execution profile: {cases} owned fixtures; exact source/owner/MAC binding, unchanged outputs and FD balance passed.")
    except BaseException:
        print(f"Preserved execution-profile fixtures: {temporary}",file=sys.stderr)
        raise
    else:
        for path,dirs,files in os.walk(temporary,followlinks=False):
            if not Path(path).is_symlink(): Path(path).chmod(0o700)
        shutil.rmtree(temporary)


if __name__=="__main__": main()
