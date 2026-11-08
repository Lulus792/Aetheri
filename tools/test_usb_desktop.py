"""Two cold USB desktop boots on ONE new owned medium, no physical disk access."""
import argparse
from pathlib import Path
import re
import sys

import image_io
import test_boot_probe as boot
import test_desktop_image as desktop


def admit(manifest):
    p=manifest.get('data_partition',{});ranges=manifest.get('immutable_ranges')
    if manifest.get('proof')!='usb-desktop' or manifest.get('disk_bytes')!=31457280000 or manifest.get('usb_wrong_data_filesystem') is not False or (p.get('start'),p.get('sectors'))!=(1310720,60127232):raise ValueError('different positive USB desktop profile')
    end=(p['start']+p['sectors'])*512
    if not isinstance(ranges,list) or len(ranges)!=2 or [(r.get('start'),r.get('size')) for r in ranges]!=[(0,p['start']*512),(end,31457280000-end)] or any(not re.fullmatch('[0-9a-f]{64}',r.get('sha256','')) for r in ranges):raise ValueError('invalid immutable medium ranges')
    return dict(size=31457280000,initial_sha256=manifest['outputs']['boot.raw'])


def copy(original,held,volume):
    image_io.sparse_file(held)
    with original.open('rb') as source:
        if image_io.copy_extent(source,held,volume['size'])!=volume['initial_sha256']:raise ValueError('changed source medium')
    image_io.set_length(held,volume['size'])


def check_immutable(path,manifest):
    for r in manifest['immutable_ranges']:
        if image_io.range_sha(path,r['start'],r['size'])!=r['sha256']:raise ValueError('system/EFI/GPT bytes changed outside data partition')


def final_hashes(path,manifest):
    p=manifest['data_partition']
    return dict(data_partition_sha256=image_io.range_sha(path,p['start']*512,p['sectors']*512))


def command(image,attempt,medium,identifier):
    args=desktop.command(image,attempt,medium,'unused',identifier)
    # Replace both legacy virtual disks with one owned writable removable disk.
    result=[];at=0
    while at<len(args):
        if args[at] in ('-drive','-device') and at+1<len(args) and (args[at+1].startswith(('if=none,id=bootmedia,','if=none,id=userdata,','virtio-blk-pci,'))):at+=2;continue
        result.append(args[at]);at+=1
    return result+['-drive','if=none,id=usbmedium,format=raw,file='+str(medium),'-device','qemu-xhci,id=xhci','-device','usb-storage,bus=xhci.0,drive=usbmedium,removable=on,bootindex=1']


def check_boot(serial,manifest,boot_id):
    nonce=re.escape(manifest['nonce']);identifier=re.escape(boot_id)
    forms=[r'AETHERI_INITRD_DEVICE nonce='+nonce+r' diskseq=[1-9][0-9]{0,19} boot='+identifier,
           r'AETHERI_INITRD_PASS nonce='+nonce+r' usb=1 ext4_ro=1 boot='+identifier,
           r'AETHERI_INITRD_HANDOFF nonce='+nonce+r' usb=1 ext4_ro=1 boot='+identifier,
           r'AETHERI_ROOT_PASS nonce='+nonce+r' .* boot='+identifier,
           r'AETHERI_USB_DATA_PASS nonce='+nonce+r' same_disk=1 root_ro=1 data_rw=0 boot='+identifier,
           r'AETHERI_USB_DATA_MOUNTED nonce='+nonce+r' same_disk=1 root_ro=1 data_rw=1 boot='+identifier]
    positions=[]
    for form in forms:
        found=list(re.finditer(r'^'+form+r'\s*$',serial,re.MULTILINE))
        if len(found)!=1:raise ValueError('missing/duplicate USB boot/data observation')
        positions.append(found[0].start())
    if positions!=sorted(positions) or len(re.findall(r'^AETHERI_(?:INITRD|USB_DATA)_\w+',serial,re.MULTILINE))!=5 or re.search(r'AETHERI_\w+_FAIL',serial):raise ValueError('mixed/failed USB boot context')
    stopped=serial.find('aetheri-initrd-udev.service: Deactivated successfully.');switched=serial.find('Switching root.')
    if not positions[1]<stopped<positions[2]<switched<positions[3]:raise ValueError('missing stopped-udev/root handover')
    return True


def unmounted(serial):
    # systemd inserts SGR colour sequences around unit names on this console.
    status=re.sub(r'\x1b\[[0-9;]*m','',serial)
    return 'Unmounted var.mount' in status and 'Unmounted home.mount' in status and re.search(r'EXT4-fs \([^\r\n)]+\): unmounting filesystem',status) is not None


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();desktop.run(a.image.resolve(),a.output.absolute(),profile=sys.modules[__name__])
