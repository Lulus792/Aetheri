"""Boot owned split-disk fixtures and require rejection before any data mount."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import uuid

import run_vm as vm
import test_boot_probe as boot
import test_data_probe as data_test
import test_usb_desktop as desktop
import uefi_probe_image as media
import prepare_usb_parent_negative as preparation


def evaluate(serial,manifest,exit_code,timed_out=False,expect_failure=False):
    nonce=re.escape(manifest['nonce']);identifier=r'([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})'
    forms=[r'AETHERI_INITRD_DEVICE nonce='+nonce+r' diskseq=[1-9][0-9]{0,19} boot='+identifier,
           r'AETHERI_INITRD_PASS nonce='+nonce+r' usb=1 ext4_ro=1 boot='+identifier,
           r'AETHERI_INITRD_HANDOFF nonce='+nonce+r' usb=1 ext4_ro=1 boot='+identifier,
           r'AETHERI_ROOT_PASS nonce='+nonce+r' kernel='+re.escape(manifest['kernel_release'])+r' arch=x86_64 pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot='+identifier]
    observations=[list(re.finditer('^'+form+r'\s*$',serial,re.MULTILINE)) for form in forms]
    failures=re.findall(r'^AETHERI_\w+_FAIL reason=(\S+)\s*$',serial,re.MULTILINE)
    valid=expect_failure=='data-parent' and failures==['data-parent'] and all(len(found)==1 for found in observations)
    roots=[]
    if valid:
        positions=[found[0].start() for found in observations];roots=[observations[-1][0].group(1)]
        valid=all(found[0].group(1)==roots[0] for found in observations) and positions==sorted(positions)
        stopped=serial.find('aetheri-initrd-udev.service: Deactivated successfully.');switched=serial.find('Switching root.')
        valid=valid and positions[1]<stopped<positions[2]<switched<positions[3]<serial.find('AETHERI_USB_DATA_FAIL reason=data-parent')
    results=re.findall(r'^AETHERI_.*$',serial,re.MULTILINE)
    status=re.sub(r'\x1b\[[0-9;]*m','',serial)
    valid=valid and len(results)==5 and 'AETHERI_USB_DATA_PASS' not in serial and 'AETHERI_USB_DATA_MOUNTED' not in serial and 'AETHERI_DESKTOP_READY' not in serial and 'Mounting var.mount' not in status and 'Mounted var.mount' not in status and "aetheri-data-check.service: Failed with result 'exit-code'." in status
    return bool(valid and exit_code==0 and not timed_out and 'reboot: Power down' in serial),roots,failures


def command(image,output,primary,external,identifier):
    args=desktop.command(image,output,primary,identifier);result=[];at=0
    while at<len(args):
        if args[at]=='-qmp':at+=2;continue
        result.append(args[at]);at+=1
    return result+['-drive','if=none,id=external,format=raw,file='+str(external),'-device','usb-storage,bus=xhci.0,drive=external,removable=on']


def run(fixtures,output):
    output=vm.owned(output)
    preparation.require_space(output)
    if any(',' in str(path) for path in (fixtures,output,boot.TOOLS)):raise ValueError('unsupported QEMU path')
    if json.loads((fixtures/'fixture.json').read_text()).get('state')!='prepared':raise ValueError('unfinished split fixture')
    manifests={role:media.inspect(fixtures/role) for role in ('primary','external')}
    first=manifests['primary'];second=manifests['external']
    if first.get('usb_parent_negative')!='primary' or second.get('usb_parent_negative')!='external' or first['nonce']!=second['nonce'] or first['root_partition']['guid']==second['root_partition']['guid'] or first['data_partition']['guid']==second['data_partition']['guid'] or first['data_partition']['filesystem_uuid']!=second['data_partition']['filesystem_uuid'] or first['usb_configuration']!=second['usb_configuration'] or first['usb_wrong_data_filesystem'] or second['usb_wrong_data_filesystem']:raise ValueError('different wrong-parent fixture pair')
    output.mkdir(mode=0o700);vm.private_path(output,directory=True);shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd',output/'nvram.fd')
    with (output/'primary.raw').open('x+b') as primary,(output/'external.raw').open('x+b') as external:
        receipts=[]
        for role,held in [('primary',primary),('external',external)]:
            m=manifests[role];desktop.copy(fixtures/role/'boot.raw',held,dict(size=m['disk_bytes'],initial_sha256=m['outputs']['boot.raw']));held.flush();os.fsync(held.fileno())
            snapshot=data_test.identity(os.fstat(held.fileno()));receipts.append((role,held,snapshot))
            if boot.sha(output/(role+'.raw'))!=m['outputs']['boot.raw']:raise ValueError('changed copied negative')
        result=boot.observe(output,command(fixtures/'primary',output,output/'primary.raw',output/'external.raw',str(uuid.uuid4())),first,boot.host_tools(),boot.sha(fixtures/'primary/manifest.json'),'data-parent',180,evaluator=evaluate)
        for role,held,snapshot in receipts:
            path=output/(role+'.raw');data_test.check_held(held,path,snapshot)
            if boot.sha(path)!=manifests[role]['outputs']['boot.raw']:raise ValueError('negative wrote to a medium before rejection')
        vm.write_state(output/'postchecks.json',dict(accepted=True,both_media_unchanged=True,original_result=result['serial_sha256']))
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fixtures',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.fixtures.resolve(),a.output.absolute())
