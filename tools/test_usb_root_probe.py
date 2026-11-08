"""Boot only a new readonly USB-root fixture; never accepts a physical device.

An explicit virtio-only negative proves the transport guard. This is an early
root/module/handover proof, not the final USB desktop or a production installer.
"""
import argparse
from pathlib import Path
import re
import shutil

import run_vm as vm
import test_boot_probe as boot
import uefi_probe_image as media


def evaluate(serial,manifest,exit_code,timed_out=False,expect_failure=False):
    nonce=re.escape(manifest['nonce']);identifier=r'([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})'
    device=re.findall(r'^AETHERI_INITRD_DEVICE nonce='+nonce+r' diskseq=([1-9][0-9]{0,19}) boot='+identifier+r'\s*$',serial,re.MULTILINE)
    checked=re.findall(r'^AETHERI_INITRD_PASS nonce='+nonce+r' usb=1 ext4_ro=1 boot='+identifier+r'\s*$',serial,re.MULTILINE)
    handoff=re.findall(r'^AETHERI_INITRD_HANDOFF nonce='+nonce+r' usb=1 ext4_ro=1 boot='+identifier+r'\s*$',serial,re.MULTILINE)
    root=re.findall(r'^AETHERI_ROOT_PASS nonce='+nonce+r' kernel='+re.escape(manifest['kernel_release'])+r' arch=x86_64 pid1=systemd uefi=1 ext4_ro=1 write_erofs=1 cgroup2=1 boot='+identifier+r'\s*$',serial,re.MULTILINE)
    failures=re.findall(r'^AETHERI_\w+_FAIL reason=(\S+)\s*$',serial,re.MULTILINE)
    all_results=re.findall(r'^AETHERI_\w+_(?:DEVICE|PASS|HANDOFF|FAIL) .*?$',serial,re.MULTILINE)
    if expect_failure:
        valid=expect_failure in ('transport','filesystem') and failures==[expect_failure] and not device and not checked and not handoff and not root and len(all_results)==1 and "aetheri-initrd-device.service: Failed with result 'exit-code'." in serial and 'Mounting sysroot.mount' not in serial
    else:
        stopped='aetheri-initrd-udev.service: Deactivated successfully.'
        switched='Switching root.'
        valid=len(device)==len(checked)==len(handoff)==len(root)==1 and device[0][1]==checked[0]==handoff[0]==root[0] and not failures and len(all_results)==4 and stopped in serial and switched in serial and serial.index('AETHERI_INITRD_DEVICE')<serial.index('AETHERI_INITRD_PASS')<serial.index(stopped)<serial.index('AETHERI_INITRD_HANDOFF')<serial.index(switched)<serial.index('AETHERI_ROOT_PASS') and 'aetheri-root.service: Deactivated successfully.' in serial
    return bool(exit_code==0 and not timed_out and 'reboot: Power down' in serial and valid),root,failures


def command(image,output,virtio_reject=False):
    args=[str(boot.TOOLS/'qemu-system-x86_64.exe'),'-name','aetheri-usb-root-proof','-machine','pc-q35-11.1,accel=whpx','-cpu','host,vmx=off,svm=off','-smp','2','-m','768M','-no-user-config','-nodefaults','-display','none','-monitor','none','-nic','none','-no-reboot',
          '-drive','if=pflash,format=raw,readonly=on,file='+str(boot.TOOLS/'share/edk2-x86_64-code.fd'),'-drive','if=pflash,format=raw,file='+str(output/'nvram.fd'),'-serial','file:'+str(output/'serial.log'),'-drive','if=none,id=bootmedia,format=raw,readonly=on,file='+str(image/'boot.raw')]
    if virtio_reject:args+=['-device','virtio-blk-pci,drive=bootmedia,serial=AETHERI_USB_NEGATIVE,bootindex=1']
    else:args+=['-device','qemu-xhci,id=xhci','-device','usb-storage,bus=xhci.0,drive=bootmedia,removable=on,bootindex=1']
    return args


def run(image,output,virtio_reject=False):
    output=vm.owned(output)
    if any(',' in str(p) for p in (image,output,boot.TOOLS)):raise ValueError('unsupported QEMU option delimiter')
    manifest=media.inspect(image)
    if manifest.get('proof')!='usb-root':raise ValueError('expected USB initrd profile')
    if virtio_reject and manifest.get('usb_wrong_filesystem'):raise ValueError('negative fixtures cannot be mixed')
    hashes=boot.host_tools();output.mkdir(mode=0o700);vm.private_path(output,directory=True)
    shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd',output/'nvram.fd')
    expected='transport' if virtio_reject else 'filesystem' if manifest.get('usb_wrong_filesystem') else False
    result=boot.observe(output,command(image,output,virtio_reject),manifest,hashes,boot.sha(image/'manifest.json'),expected,120,evaluator=evaluate)
    for name,digest in manifest['outputs'].items():
        if boot.sha(image/name)!=digest:raise ValueError('readonly USB source changed')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--virtio-reject',action='store_true')
    a=p.parse_args();run(a.image.resolve(),a.output.absolute(),a.virtio_reject)
