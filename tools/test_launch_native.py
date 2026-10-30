"""Independent user launch canonical bytes; separate unconfined owner stores."""
from pathlib import Path
from contextlib import contextmanager, nullcontext
import hashlib
import os
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import runtime_contracts as reference

def identity(n): return "000000000000400080000000000000"+f"{n:02x}"

@contextmanager
def private_bus(root):
    """Component bus only: no activatable services or installed policy changes."""
    if os.geteuid()!=0:
        raise RuntimeError("execution endpoint tests require the disposable root carrier")
    config=root/"bus.conf"
    config.write_text('''<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>session</type><auth>EXTERNAL</auth>
<policy context="default"><allow user="root"/><allow own="*"/>
<allow send_destination="*"/><allow receive_sender="*"/></policy></busconfig>
''',encoding="utf-8")
    # dbus-broker-launch v37 requires one socket-activation FD. The exec wrapper
    # sets LISTEN_PID to its own preserved PID. Test calls use this private bus;
    # system scope supplies only the launcher's normal PID1 subscription.
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as listener, (root/"bus.log").open("wb") as log:
        listener.bind(str(root/"bus")); listener.listen()
        code="""import os,sys
fd=int(sys.argv[1])
if fd!=3: os.dup2(fd,3); os.close(fd)
os.set_inheritable(3,True)
os.environ.update(LISTEN_PID=str(os.getpid()),LISTEN_FDS='1')
os.execv('/usr/bin/dbus-broker-launch',['dbus-broker-launch','--scope','system','--config-file',sys.argv[2]])
"""
        process=subprocess.Popen([sys.executable,"-c",code,str(listener.fileno()),str(config)],
            pass_fds=(listener.fileno(),),stdout=log,stderr=log,start_new_session=True)
        environment=dict(os.environ,DBUS_SESSION_BUS_ADDRESS="unix:path="+str(root/"bus"))
        try:
            yield environment
        finally:
            # This exact unreaped child owns a process group containing only the
            # private launcher/broker. Configuration has no activation entries.
            if process.poll() is None:
                os.killpg(process.pid,signal.SIGTERM)
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid,signal.SIGKILL); process.wait(timeout=5)
            if process.returncode not in (0,-signal.SIGTERM):
                print((root/"bus.log").read_text(encoding="utf-8",errors="replace"),file=sys.stderr)

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
                         "exec-overflow-lease","exec-overflow-dispatch","exec-overflow-operation","exec-overflow-catalogue",
                         "exec-diagnostic-failure","exec-diagnostic-unknown","exec-diagnostic-malformed","exec-diagnostic-observer") if len(sys.argv)==4 and sys.argv[3]=="execution" else (
                         "faults","exit-before","exit-after","recovery","lost-root-reply","setup","setup-restart","setup-terminal","setup-dispatch-overflow","setup-lease-overflow")
        if len(sys.argv)==4 and sys.argv[3]=="service":
            scenarios=tuple("exec-service-"+case for case in (
                "failure","unknown","malformed","request-timeout","request-commit-lost","bind-commit-lost",
                "prepare-commit-lost","arm-commit-lost","release-timeout","context-drift","root-restart","broker-replacement","early-eof",
                "observer-error","report-commit-lost","early-data"))
        if len(sys.argv)==4 and sys.argv[3]=="user-execution":
            scenarios=tuple("exec-user-prepare-"+case for case in (
                "fresh","lost","crash-before","crash-after","restart","gate",
                "overflow-launches","overflow-operations","overflow-authority_meta"))
        if len(sys.argv)==4 and sys.argv[3]=="user-readiness":
            scenarios=tuple("exec-user-observe-"+case for case in (
                "fresh","lost-register","lost-ready","crash-register-before","crash-register-after",
                "crash-ready-before","crash-ready-after","restart","gate","overflow-launches","overflow-operations","overflow-authority_meta"))
        if len(sys.argv)==4 and sys.argv[3]=="observation":
            scenarios=tuple("exec-observation-"+case for case in (
                "fresh","lost","crash-before","crash-after","closed-gate","restart",
                "overflow-dispatch","overflow-operation","overflow-catalogue","early-diagnostic","late-diagnostic",
                "later-closing","later-terminal","later-uncertain"))
        if len(sys.argv)==4 and sys.argv[3]=="completion":
            scenarios=tuple("exec-completion-"+case for case in (
                "fresh","root-lost","user-lost","root-crash-before","root-crash-after","user-crash-before","user-crash-after",
                "root-gate","user-gate","root-restart","user-restart","root-overflow-operation","root-overflow-catalogue",
                "user-overflow-operation","user-overflow-catalogue","missing-diagnostic","bad-diagnostic","missing-ready","later-terminal"))
            vector=reference.load_json((Path(__file__).resolve().parents[1]/'spec/vectors/execution-completion-v1.json').read_bytes())
            encoded=root/'completion.vector'; encoded.write_bytes(bytes.fromhex(vector['hex']))
            subprocess.run([executable,'completion-codec',encoded],check=True,timeout=10)
        if len(sys.argv)==4 and sys.argv[3]=="wire":
            scenarios=tuple("exec-wire-"+case for case in (
                "success","wrong-peer","wrong-tuple","bind-commit-lost","prepare-commit-lost","arm-commit-lost",
                "register-send-before","register-send-lost","release-send-before","release-send-lost",
                "deadline-event","deadline-direct","release-deadline","context-drift","broker-replacement","root-restart"))
        with private_bus(root) if len(sys.argv)==4 and sys.argv[3]=="wire" else nullcontext(None) as environment:
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
                    diagnostics=bytearray()
                    for kind,phase,error,reason in ((1,0,0,"exec_channel_unknown"),(2,8,1,"exec_failure_p8_e1"),
                                                   (3,0,0,"exec_channel_malformed"),(4,0,5,"exec_observer_e5")):
                        record=b"AETHEXD1"+hashlib.sha256(profile).digest()+bytes.fromhex(identity(66))+bytes([2])*32+struct.pack(">III",kind,phase,error)
                        assert len(record)==100
                        diagnostics+=hashlib.sha256(record).digest()+reason.encode().ljust(64,b"\0")
                    expected.with_name(expected.name+".diagnostics").write_bytes(diagnostics)
                case=root/scenario; case.mkdir(); system,user=case/"system",case/"user"
                system.mkdir(mode=0o700); user.mkdir(mode=0o700)
                subprocess.run([executable,system,user,expected,scenario],check=True,timeout=50,env=environment)

if __name__=="__main__": main()
