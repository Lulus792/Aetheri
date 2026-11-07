"""Supervise only the UID0 disposable-test driver, never an Aetheri app.

Unknown start/stop acknowledgements preserve the original unit, image and both
catalogues. Successful administrative retirement settles no native lease.
"""
import errno
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import time
import uuid

from unit_issuer_contracts import decode
from runtime_contracts import image_record

LABEL = 'system_u:system_r:unconfined_service_t:s0'
VENDOR = Path('/usr/lib/systemd/system/service.d/10-timeout-abort.conf')
VENDOR_SHA256 = 'ae6b234f92bc22f1201a7572b59b454c9809f33c80d13f361b9674e1801acc37'


def flush_file(path, data, mode=0o600):
    # Readers must never observe a named, empty or partially written barrier.
    # An anonymous inode gains its sole link only after complete write + fsync.
    parent = os.open(path.parent, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW|os.O_CLOEXEC)
    fd = -1
    try:
        fd = os.open('.', os.O_WRONLY|os.O_TMPFILE|os.O_CLOEXEC, mode, dir_fd=parent)
        with os.fdopen(fd, 'wb', closefd=False) as stream:
            stream.write(data); stream.flush(); os.fsync(fd)
        libc = ctypes.CDLL(None, use_errno=True)
        linkat = libc.linkat
        linkat.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
        linkat.restype = ctypes.c_int
        if linkat(fd, b'', parent, os.fsencode(path.name), 0x1000):
            error = ctypes.get_errno(); raise OSError(error, os.strerror(error), str(path))
        os.fsync(parent)
    finally:
        if fd>=0: os.close(fd)
        os.close(parent)


def show(unit):
    result = subprocess.run(['systemctl', 'show', unit, '--no-pager'], check=True, capture_output=True, text=True, timeout=5)
    return dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)


def retain_log(unit, path):
    result = subprocess.run(['journalctl', '--unit='+unit, '--no-pager', '--output=short-monotonic', '-n', '500'],
                            check=True, capture_output=True, timeout=20)
    flush_file(path, result.stdout)
    return result.stdout.decode('utf-8', errors='replace')


def run(executable, system, user, expected, mode):
    assert os.geteuid()==0 and subprocess.check_output(['getenforce'], text=True).strip()=='Enforcing'
    assert hashlib.sha256(VENDOR.read_bytes()).hexdigest()==VENDOR_SHA256
    case = system.parent
    # Literal generated paths only: no specifier, unit-command or env expansion.
    for path in (executable, system, user, expected, case):
        assert re.fullmatch(r'/[a-zA-Z0-9_./-]+', str(path))
    assert re.fullmatch(r'exec-root-unit-[a-z-]+', mode)
    token = uuid.uuid4().hex
    unit = 'aetheri-issuer-test-'+token+'.service'
    source = Path('/run/systemd/system')/unit
    driver, image, log = case/'issuer-driver', case/'issuer-image.bin', case/'issuer.log'
    binary = executable.read_bytes(); digest = hashlib.sha256(binary).hexdigest()
    flush_file(driver, binary, 0o555)
    subprocess.run(['chcon', 'system_u:object_r:bin_t:s0', str(driver)], check=True, timeout=5)
    driver_stat = driver.stat(); assert driver_stat.st_nlink==1 and stat.S_IMODE(driver_stat.st_mode)==0o555
    subprocess.run([str(executable), 'issuer-image', str(driver), digest, str(image)], check=True, timeout=15)
    image_bytes = image.read_bytes(); assert len(image_bytes)==112 and image_bytes[:8]==b'AETHIMG1'
    fragment = ('[Unit]\nDescription=Aetheri administrative issuer test\n'
        '[Service]\nType=exec\nUser=root\nGroup=root\nRestart=no\nRemainAfterExit=yes\n'
        'KillMode=control-group\nSlice=system.slice\nSELinuxContext='+LABEL+'\n'
        'Environment=AETHERI_TEST_ISSUER_UNIT='+unit+'\n'
        'Environment=AETHERI_TEST_ISSUER_LABEL='+LABEL+'\n'
        'Environment=AETHERI_TEST_ISSUER_SHA256='+digest+'\n'
        'Environment=AETHERI_TEST_ISSUER_IMAGE='+str(image)+'\n'
        'StandardInput=null\nStandardOutput=journal\nStandardError=inherit\n'
        'ExecStart='+str(driver)+' '+str(system)+' '+str(user)+' '+str(expected)+' '+mode+'\n').encode()
    flush_file(source, fragment, 0o644)
    subprocess.run(['chcon', 'system_u:object_r:systemd_unit_file_t:s0', str(source)], check=True, timeout=5)
    source_stat = source.stat()
    subprocess.run(['systemctl', 'daemon-reload'], check=True, timeout=10)
    flush_file(case/'issuer-start-intent.json', json.dumps(dict(unit=unit, driver=digest,
        image=image_bytes.hex(), fragment=hashlib.sha256(fragment).hexdigest(), token=token), indent=2).encode())
    # Dispatch once. Exceptions intentionally leave the original artifacts intact.
    subprocess.run(['systemctl', 'start', '--no-block', unit], check=True, timeout=5)
    deadline = time.monotonic()+40
    ready = system/'issuer-start.bin'
    while not ready.exists():
        state = show(unit)
        if state.get('ActiveState') in ('failed', 'inactive') or time.monotonic()>=deadline:
            output = retain_log(unit, log)
            flush_file(case/'issuer-failed.json', json.dumps(state, indent=2).encode())
            raise RuntimeError('issuer failed before original-process barrier: '+unit+' '+state.get('Result', '')+'\n'+output)
        time.sleep(.02)
    witness = decode(ready.read_bytes()); state = show(unit)
    assert image_record(witness['image'])==image_bytes
    assert witness['unit']==unit and witness['label']==LABEL and state['InvocationID']==witness['invocation']
    assert witness['boot']==Path('/proc/sys/kernel/random/boot_id').read_text().strip().replace('-', '')
    assert state['ControlGroup']==witness['cgroup'] and int(state['MainPID'])>0
    assert state['FragmentPath']==str(source) and state['Restart']=='no'
    assert state.get('DropInPaths', '').split()==[str(VENDOR)] and state.get('ExecStop', '')=='' and state.get('ExecReload', '')==''
    assert image.read_bytes()==image_bytes and driver.read_bytes()==binary and source.read_bytes()==fragment
    group = Path('/sys/fs/cgroup'+witness['cgroup'])
    held = os.open(group, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW|os.O_CLOEXEC)
    events = os.open('cgroup.events', os.O_RDONLY|os.O_NOFOLLOW|os.O_CLOEXEC, dir_fd=held)
    try:
        assert os.fstat(held).st_ino==witness['cgroup_id']
        flush_file(case/'issuer-original.json', json.dumps(dict(witness=witness, state=state), indent=2).encode())
        flush_file(system/'issuer-go', b'G')
        while True:
            state = show(unit)
            assert state['InvocationID']==witness['invocation'] and state['FragmentPath']==str(source)
            if state['SubState'] in ('exited', 'failed') or state['ActiveState']=='failed': break
            if time.monotonic()>=deadline: raise RuntimeError('original issuer observation deadline; preserving fixture')
            time.sleep(.02)
        flush_file(case/'issuer-ended.json', json.dumps(state, indent=2).encode())
        output = retain_log(unit, log)
        assert state['ActiveState']=='active' and state['SubState']=='exited' and state['MainPID']=='0' and state['ControlPID']=='0'
        assert state['ExecMainCode']=='1' and state['ExecMainStatus']=='0' and state['Result']=='success', output
        try:
            assert 'populated 0' in os.pread(events, 4096, 0).decode()
        except OSError as error:
            # PID1 may already deactivate the empty cgroup while the successful
            # RemainAfterExit unit remains active/exited. Require both original
            # held-reader ENODEV and missing name, never absence alone.
            if error.errno!=errno.ENODEV or group.exists(): raise
        assert source.read_bytes()==fragment and driver.read_bytes()==binary and image.read_bytes()==image_bytes
        assert source.stat().st_ino==source_stat.st_ino and driver.stat().st_ino==driver_stat.st_ino
        flush_file(case/'issuer-stop-intent.json', json.dumps(dict(unit=unit, invocation=witness['invocation'])).encode())
        subprocess.run(['systemctl', 'stop', unit], check=True, timeout=5)
        # This carrier reports removed held cgroups by ENODEV, not link count.
        removed = False
        for _ in range(100):
            try: os.pread(events, 4096, 0)
            except OSError as error:
                if error.errno!=errno.ENODEV: raise
                removed = not group.exists()
            if removed: break
            time.sleep(.02)
        assert removed
        final = show(unit)
        assert final.get('ActiveState')=='inactive' and final.get('MainPID')=='0' and final.get('ControlPID')=='0'
        assert final.get('Job', '') in ('', '0')
        assert source.read_bytes()==fragment and source.stat().st_ino==source_stat.st_ino
        assert driver.read_bytes()==binary and driver.stat().st_ino==driver_stat.st_ino
        source.unlink(); driver.unlink()
        subprocess.run(['systemctl', 'daemon-reload'], check=True, timeout=10)
        flush_file(case/'issuer-retired.json', json.dumps(dict(unit=unit, invocation=witness['invocation'],
            administrative_only=True, native_leases_settled=False, state=final), indent=2).encode())
    finally:
        os.close(events); os.close(held)
