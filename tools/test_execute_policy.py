"""Explicit temporary MAC/exec test in the registered disposable carrier.

Not a production starter, default CTest, app admission or catalogue writer.
Failure preserves its own fixture/module and every unresolved unit request.
"""
import bz2
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import uuid

from test_execute_native import fixture,cleanup,verify_fixture

ROOT=Path("/var/lib/aetheri-p0-execute-test")
MODULE="aetheri_p0_execute"
SOURCE=Path(__file__).resolve().parents[1]


def run(*args):
    return subprocess.run(args,check=True,text=True,capture_output=True,timeout=60).stdout.strip()


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def save(record):
    temporary=ROOT/"controller.next"
    fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_CLOEXEC,0o600)
    with os.fdopen(fd,"w") as output:
        json.dump(record,output,indent=2); output.flush(); os.fsync(output.fileno())
    os.replace(temporary,ROOT/"controller.json")


def label(path,kind,cats=None):
    run("chcon","system_u:object_r:"+kind+":s0"+(":"+cats if cats else ""),str(path))


def drained(unit):
    facts=dict(line.split("=",1) for line in run("systemctl","show",unit,"--property=ActiveState,MainPID,ControlGroup").splitlines())
    jobs=json.loads(run("busctl","--system","--json=short","call","org.freedesktop.systemd1","/org/freedesktop/systemd1","org.freedesktop.systemd1.Manager","ListJobs"))
    if facts.get("ActiveState") not in ("inactive","failed") or facts.get("MainPID")!="0" or facts.get("ControlGroup") or jobs["type"]!="a(usssoo)" or any(j[1]==unit for j in jobs["data"][0]):
        raise RuntimeError("execution test unit/process/job not independently drained")
    return facts["ActiveState"]


def reset_drained_failures(units):
    # Preserve journal/audit evidence, but do not retain failed transient units
    # after this controller has verified their exact owned process/job drain.
    failed=[unit for unit in units if drained(unit)=="failed"]
    for unit in failed: run("systemctl","reset-failed",unit)


def installed():
    path=Path("/var/lib/selinux/targeted/active/modules/400")/MODULE/"cil"
    data=path.read_bytes(); return bz2.decompress(data) if data.startswith(b"BZh") else data


def retire():
    info=ROOT.lstat(); marker=ROOT/"controller.json"; stored=marker.lstat()
    if ROOT.is_symlink() or not stat.S_ISDIR(info.st_mode) or info.st_uid!=0 or stat.S_IMODE(info.st_mode)!=0o700 or marker.is_symlink() or not stat.S_ISREG(stored.st_mode) or stored.st_uid!=0 or stat.S_IMODE(stored.st_mode)!=0o600 or stored.st_nlink!=1:
        raise RuntimeError("execution fixture ownership changed")
    r=json.loads(marker.read_text())
    if r["pending"] is not None: raise RuntimeError("unacknowledged execution dispatch; no cleanup before causal reconciliation")
    if re.fullmatch("[0-9a-f]{32}",r["token"]) is None: raise RuntimeError("unknown execution token")
    cases=("success","wrong-observation","exec-error","before-exit","fast-exit")
    for unit in r["units"]:
        if unit not in ["aetheri-p0-execute-"+r["token"]+"-"+case+".service" for case in cases]: raise RuntimeError("foreign execution unit")
        drained(unit)
    policy=ROOT/(MODULE+".cil"); launcher=ROOT/"launcher"
    for p,expected in ((policy,r["policy_sha256"]),(launcher,r["launcher_sha256"])):
        if p.is_symlink() or p.stat().st_uid!=0 or p.stat().st_nlink!=1 or sha(p)!=expected: raise RuntimeError("execution input changed")
    if hashlib.sha256(installed()).hexdigest()!=r["policy_sha256"]: raise RuntimeError("execution policy changed")
    expected={"controller.json","launcher",MODULE+".cil",*r["fixture_cases"]}
    if any(c not in cases for c in r["fixture_cases"]) or {p.name for p in ROOT.iterdir()}!=expected: raise RuntimeError("unknown execution fixture entry")
    for case in r["fixture_cases"]:
        work=ROOT/case; image=work/"code/probe"
        if image.is_symlink() or sha(image)!=r["payload_sha256"]: raise RuntimeError("execution payload changed")
        # First verify every complete fixture, then remove exact known entries.
        verify_fixture(work,image.read_bytes())
    reset_drained_failures(r["units"])
    run("semodule","-X","400","-r",MODULE)
    for case in r["fixture_cases"]:
        work=ROOT/case; cleanup(work,(work/"code/probe").read_bytes())
    for name in ("launcher",MODULE+".cil","controller.json"): (ROOT/name).unlink()
    ROOT.rmdir()


def main():
    if sys.platform!="linux" or os.geteuid()!=0 or not Path("/sys/firmware/efi").is_dir() or Path("/sys/class/dmi/id/product_name").read_text().strip()!="Aetheri P0 disposable VM" or run("getenforce")!="Enforcing":
        raise RuntimeError("requires the registered enforcing disposable carrier")
    import fcntl
    lock=os.open("/etc/aetheri-p0-carrier",os.O_RDONLY|os.O_CLOEXEC|os.O_NOFOLLOW)
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    if sys.argv[1:]==["--cleanup"]:
        retire(); os.close(lock); print("Known drained execution test retired; no setup dispatch was repeated."); return
    if ROOT.exists() or ROOT.is_symlink(): raise RuntimeError("existing execution fixture requires inspection; no replacement/retry")
    if MODULE in run("semodule","-l").split(): raise RuntimeError("execution test module is already installed")
    binary,payload=map(lambda p:Path(p).resolve(strict=True),sys.argv[1:])
    ROOT.mkdir(mode=0o700); run("restorecon",str(ROOT))
    record=dict(token=uuid.uuid4().hex,units=[],pending=None,policy_sha256=sha(SOURCE/"policy/p0-execute.cil"),
                launcher_sha256=sha(binary),payload_sha256=sha(payload),cases=[],fixture_cases=[],state="preparing")
    save(record)
    policy=ROOT/(MODULE+".cil"); policy.write_bytes((SOURCE/"policy/p0-execute.cil").read_bytes()); policy.chmod(0o600)
    run("semodule","-X","400","-i",str(policy))
    if hashlib.sha256(installed()).hexdigest()!=record["policy_sha256"]: raise RuntimeError("installed test policy differs")
    launcher=ROOT/"launcher"; launcher.write_bytes(binary.read_bytes()); launcher.chmod(0o555)
    label(launcher,"aetheri_exec_launcher_test_t")
    # The probe's program/credential/FD assertions run after a real ELF exec and
    # transition to this MCS-constrained domain. No unconfined target fallback.
    for case in ("success","wrong-observation","exec-error","before-exit","fast-exit"):
        record["fixture_cases"].append(case); save(record)
        work=ROOT/case; fixture(work,payload.read_bytes())
        for path in (work,work/"code",work/"setup",work/"setup/app",work/"setup/data"): label(path,"aetheri_exec_fixture_test_t")
        label(work/"code/probe","aetheri_exec_code_test_t"); label(work/"payload","aetheri_exec_data_test_t","c33,c34")
        unit="aetheri-p0-execute-"+record["token"]+"-"+case+".service"
        record["units"].append(unit); record["pending"]=unit; save(record)
        props={"User":"root","Group":"root","SELinuxContext":"system_u:system_r:aetheri_exec_setup_test_t:s0",
            "NoNewPrivileges":"yes","CapabilityBoundingSet":"CAP_SYS_ADMIN CAP_DAC_READ_SEARCH CAP_SETUID CAP_SETGID CAP_SETPCAP CAP_SYS_PTRACE CAP_KILL",
            # v259's seccomp setup drops SETUID unless explicitly retained here.
            # The exec boundary clears ambient/inheritable before changing UID.
            "AmbientCapabilities":"CAP_SETUID",
            "PrivateMounts":"yes","PrivateNetwork":"yes","PrivateDevices":"yes","ProtectHome":"yes","ProtectSystem":"strict","ProtectControlGroups":"yes","RestrictNamespaces":"mnt",
            "ReadWritePaths":str(work/"payload"),"KillMode":"control-group","SendSIGKILL":"no","TimeoutStopSec":"10s",
            "TimeoutStartSec":"20s","RuntimeMaxSec":"25s","MemoryMax":"96M","TasksMax":"3","UMask":"0077"}
        try:
            run("systemd-run","--unit="+unit,"--service-type=exec","--wait",*("--property="+k+"="+v for k,v in props.items()),
                str(launcher),str(work),case,"system_u:system_r:aetheri_exec_app_test_t:s0:c33,c34")
        except subprocess.CalledProcessError as error:
            print(error.stderr,file=sys.stderr)
            print(run("journalctl","-u",unit,"--output=cat","--no-pager"),file=sys.stderr)
            # Even a known failure is not success/retry authorization. Its unit,
            # fixture and module stay available for exact subsequent inspection.
            drained(unit); record["pending"]=None; record["state"]="failed"; save(record); raise
        drained(unit); record["pending"]=None; record["cases"].append(case); save(record)
        if sha(launcher)!=record["launcher_sha256"]: raise RuntimeError("test launcher changed")
        cleanup(work,payload.read_bytes())
        record["fixture_cases"].remove(case); save(record)
    record["state"]="passed"; save(record)
    if {p.name for p in ROOT.iterdir()}!={"controller.json","launcher",MODULE+".cil"}: raise RuntimeError("unknown execution fixture entry; preserve")
    for unit in record["units"]: drained(unit)
    if hashlib.sha256(installed()).hexdigest()!=record["policy_sha256"] or sha(policy)!=record["policy_sha256"]: raise RuntimeError("policy changed before retirement")
    reset_drained_failures(record["units"])
    run("semodule","-X","400","-r",MODULE)
    for name in ("launcher",MODULE+".cil","controller.json"): (ROOT/name).unlink()
    ROOT.rmdir(); os.close(lock)
    print(json.dumps(dict(enforcing=True,actual_target_domain=True,uid_gid_1000=True,capabilities_empty=True,fd_seal=True,
        exact_executable_observed=True,empty_channel_unknown=True,owned_units_drained=True,test_module_removed=True,registered_app=False)))


if __name__=="__main__": main()
