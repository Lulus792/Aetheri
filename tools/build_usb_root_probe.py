"""Assemble a selected USB-initrd proof around the original read-only root.

Ordinary Linux builder only. All outputs are NEW regular files; no mount, host
device, package installation or carrier-service operation is performed.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import uuid

import build_root_probe as root_image
import build_uefi_probe as uefi
import test_boot_probe as boot
import uefi_probe_image as media
import image_io

ROOT=Path(__file__).resolve().parents[1]


def checked_payloads(pins):
    if pins.get('format')!=1 or pins.get('kernel_release')!='6.19.10-300.fc44.x86_64':raise ValueError('different USB proof inputs')
    files=pins.get('files');seen=set();total=0;result=[]
    if not isinstance(files,list) or not 1<=len(files)<=256:raise ValueError('USB payload count')
    for e in files:
        name=e.get('destination');size=e.get('size');digest=e.get('sha256');mode=e.get('mode')
        if not isinstance(name,str) or not re.fullmatch(r'/usr/[A-Za-z0-9._+/-]+',name) or str(PurePosixPath(name))!=name or '..' in PurePosixPath(name).parts or name in seen or type(size) is not int or not 0<=size<=32*1024**2 or mode not in (0o644,0o755) or not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest) or not e.get('package'):
            raise ValueError('invalid USB payload')
        seen.add(name);total+=size
        source=Path(e['source'])
        if not source.is_absolute() or source.is_symlink() or not source.is_file():raise ValueError('USB source is not a selected regular file')
        data=source.read_bytes()
        if len(data)!=size or hashlib.sha256(data).hexdigest()!=digest:raise ValueError('changed USB payload: '+str(source))
        result.append((name,data,mode))
    if total>48*1024**2:raise ValueError('USB runtime byte bound')
    return result


def device_unit(partition):
    if str(uuid.UUID(partition))!=partition:raise ValueError('noncanonical partition UUID')
    return r'dev-disk-by\x2dpartuuid-'+partition.replace('-',r'\x2d')+'.device'


def console_option(virtual_console):
    # A physical desktop must never select a nonexistent UART for /dev/console.
    # The serial-only diagnostic remains unchanged for existing controllers.
    if type(virtual_console) is not bool:raise ValueError('invalid console profile')
    return 'console=tty0' if virtual_console else 'console=ttyS0,115200n8'


def build(base,system,packages,output,wrong_filesystem=False,*,desktop=None,console_fallback=False):
    if sys.platform!='linux' or os.uname().machine!='x86_64' or os.geteuid()==0:raise ValueError('use an ordinary x86-64 Linux image builder')
    raw=(ROOT/'spec/usb-root-inputs.json').read_bytes();pins=json.loads(raw)
    if boot.sha(base/'manifest.json')!=pins['boot_source_manifest_sha256'] or (desktop is None and boot.sha(system/'manifest.json')!=pins['root_source_manifest_sha256']):raise ValueError('different independently pinned base manifests')
    original=boot.inspect(base);system_manifest=media.inspect(system)
    if original.get('proof','ram-root')!='ram-root' or system_manifest.get('proof')!=('desktop-image' if desktop else 'readonly-root') or original['nonce']!=system_manifest['nonce'] or original['kernel_release']!=pins['kernel_release'] or system_manifest['kernel_release']!=pins['kernel_release']:raise ValueError('different original root profile')
    if desktop:
        selected=json.loads((ROOT/'spec/usb-desktop-inputs.json').read_text())
        if desktop!={k:selected[k] for k in ('disk_bytes','root_start','root_sectors','data_start','data_sectors')} or system_manifest.get('usb_layout')!=desktop or system_manifest.get('usb_desktop_builder_sha256')!=boot.sha(ROOT/'tools/build_usb_desktop.py'):raise ValueError('different selected desktop layout/source')
    payloads=checked_payloads(pins)
    part=system_manifest['root_partition'];partition=str(uuid.UUID(part['guid']));filesystem=str(uuid.UUID(part['filesystem_uuid']))
    if partition!=part['guid'] or filesystem!=part['filesystem_uuid'] or (part['start'],part['sectors'])!=(262144,1048576 if desktop else 524288):raise ValueError('unexpected root partition')
    output.mkdir(mode=0o700);tree=output/'initrd';tree.mkdir();logs=output/'logs';logs.mkdir()
    def run(name,args,cwd=None,data=None):
        p=subprocess.run(args,cwd=cwd,input=data,capture_output=True,env={**os.environ,'LC_ALL':'C'})
        (logs/(name+'.stdout')).write_bytes(p.stdout);(logs/(name+'.stderr')).write_bytes(p.stderr);p.check_returncode();return p.stdout
    links=[]
    for name,mode,data in root_image.archive_entries(gzip.decompress((base/'initramfs.cpio.gz').read_bytes())):
        path=tree/name;kind=mode&0o170000
        if kind==0o040000:path.mkdir(parents=True,exist_ok=True);path.chmod(mode&0o7777)
        elif kind==0o120000:links.append((path,data.decode()))
        else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);path.chmod(mode&0o7777)
    for path,target in links:path.symlink_to(target)
    for name in ('etc/aetheri-boot-proof','etc/systemd/system/aetheri-boot.service','etc/systemd/system/aetheri-boot.target','usr/libexec/aetheri-boot-probe'):(tree/name).unlink()
    def put(name,data,mode=0o644,reuse=False):
        path=tree/name;path.parent.mkdir(parents=True,exist_ok=True)
        if path.exists() or path.is_symlink():
            if not reuse or path.is_symlink() or not path.is_file() or path.read_bytes()!=data:raise ValueError('conflicting initrd entry: '+name)
            return
        with path.open('xb') as f:f.write(data)
        path.chmod(mode)
    for name,data,mode in payloads:put(name.lstrip('/'),data,mode,reuse=True)
    put('usr/lib/systemd/systemd-udevd',(tree/'usr/bin/udevadm').read_bytes(),0o755)
    (tree/'lib').symlink_to('usr/lib');(tree/'bin').symlink_to('usr/bin');(tree/'usr/bin/modprobe').symlink_to('kmod')
    for name in ('sysroot','var','root'):(tree/name).mkdir(exist_ok=True)
    (tree/'etc/os-release').unlink()
    put('etc/os-release',b'NAME="Aetheri USB initrd proof"\nID=aetheri-initrd\n')
    put('etc/initrd-release',b'ID=aetheri-initrd\n')
    nonce=original['nonce'];expected_filesystem=('0' if filesystem[0]!='0' else '1')+filesystem[1:] if wrong_filesystem else filesystem
    configuration=f'AETHERI-USB1\n{nonce}\n{partition}\n{expected_filesystem}\n{original["kernel_release"]}\n'
    if desktop:
        if wrong_filesystem:raise ValueError('separate USB-root negative only')
        configuration=system_manifest['usb_configuration']
        if system_manifest['root_inventory']['etc/aetheri-usb-root']['sha256']!=hashlib.sha256(configuration.encode()).hexdigest():raise ValueError('different root/initrd configuration')
    put('etc/aetheri-usb-root',configuration.encode())
    # Match properties imported by blkid only in the following rule. A match in
    # the importing rule is evaluated before that rule's import is available.
    put('etc/udev/rules.d/99-aetheri-initrd.rules',b'ACTION!="remove", SUBSYSTEM=="block", ENV{DEVTYPE}=="partition", IMPORT{builtin}="blkid"\nACTION!="remove", SUBSYSTEM=="block", ENV{DEVTYPE}=="partition", ENV{ID_PART_ENTRY_UUID}=="?*", SYMLINK+="disk/by-partuuid/$env{ID_PART_ENTRY_UUID}"\nSUBSYSTEM=="block", TAG+="systemd"\n')
    run('depmod',['/usr/bin/depmod','-C','/dev/null','-b',str(tree),'-a',pins['kernel_release']])
    run('probe',['/usr/bin/gcc','-std=c17','-D_GNU_SOURCE','-O2','-g0','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror','-fanalyzer',str(ROOT/'src/boot/initrd.c'),'-o',str(tree/'usr/libexec/aetheri-initrd-check')])
    put('usr/share/licenses/aetheri/LICENSE',(ROOT/'LICENSE').read_bytes())
    unit=device_unit(partition)
    console='StandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n'
    failure='FailureAction=poweroff-immediate\n'
    definitions={
      'aetheri-initrd.target':'[Unit]\nDefaultDependencies=no\nRequires=aetheri-initrd-udev.service aetheri-initrd-modules.service aetheri-initrd-trigger.service aetheri-initrd-check.service\nAfter=aetheri-initrd-check.service\n',
      'aetheri-initrd-udev.service':'[Unit]\nDefaultDependencies=no\n[Service]\nType=notify\nExecStart=/usr/lib/systemd/systemd-udevd --debug\nRuntimeDirectory=udev\nFileDescriptorStoreMax=512\n'+console,
      'aetheri-initrd-modules.service':'[Unit]\nDefaultDependencies=no\n'+failure+'[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/bin/modprobe -C /dev/null usb_storage\nExecStart=/usr/bin/modprobe -C /dev/null uas\n'+console,
      'aetheri-initrd-trigger.service':'[Unit]\nDefaultDependencies=no\nAfter=aetheri-initrd-udev.service aetheri-initrd-modules.service\n'+failure+'[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/bin/udevadm trigger --action=add --subsystem-match=block\nExecStart=/usr/bin/udevadm settle --timeout=20\n'+console,
      'aetheri-initrd-device.service':'[Unit]\nDefaultDependencies=no\nRequires='+unit+'\nAfter='+unit+' aetheri-initrd-trigger.service\n'+failure+'[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/libexec/aetheri-initrd-check --device\n'+console,
      'sysroot.mount':'[Unit]\nDefaultDependencies=no\nRequires=aetheri-initrd-device.service\nAfter=aetheri-initrd-device.service\n'+failure+'[Mount]\nWhat=/dev/disk/by-partuuid/'+partition+'\nWhere=/sysroot\nType=ext4\nOptions=ro,noload\nTimeoutSec=20\n'+console,
      'aetheri-initrd-check.service':'[Unit]\nDefaultDependencies=no\nRequires=sysroot.mount\nAfter=sysroot.mount\nOnSuccess=aetheri-initrd-switch.target\nOnSuccessJobMode=isolate\n'+failure+'[Service]\nType=oneshot\nExecStart=/usr/libexec/aetheri-initrd-check --mounted\n'+console,
      'aetheri-initrd-switch.target':'[Unit]\nDefaultDependencies=no\nAllowIsolate=yes\nRequires=sysroot.mount aetheri-initrd-switch.service\nAfter=aetheri-initrd-check.service aetheri-initrd-udev.service aetheri-initrd-trigger.service\n',
      'aetheri-initrd-switch.service':'[Unit]\nDefaultDependencies=no\nAfter=aetheri-initrd-switch.target\n'+failure+'[Service]\nType=oneshot\nExecStartPre=/usr/libexec/aetheri-initrd-check --handoff\nExecStart=/usr/bin/systemctl --no-block switch-root /sysroot /usr/lib/systemd/systemd\n'+console,
    }
    for name,body in definitions.items():put('etc/systemd/system/'+name,body.encode())
    put('etc/systemd/system/'+unit+'.d/aetheri.conf',b'[Unit]\nJobTimeoutSec=30\nJobTimeoutAction=poweroff-immediate\n')
    (tree/'etc/systemd/system/default.target').symlink_to('aetheri-initrd.target')
    paths=sorted(tree.rglob('*'))
    for path in [tree,*paths]:os.utime(path,(0,0),follow_symlinks=False)
    names=b'\0'.join(p.relative_to(tree).as_posix().encode() for p in paths)+b'\0'
    packed=run('cpio',['/usr/bin/cpio','--quiet','--null','--create','--format=newc','--owner=0:0','--reproducible'],cwd=tree,data=names)
    inputs=output/'kernel-inputs';inputs.mkdir();(inputs/'initramfs.cpio.gz').write_bytes(gzip.compress(packed,mtime=0));shutil.copyfile(base/'vmlinuz',inputs/'vmlinuz')
    target='aetheri-desktop.target' if desktop else 'aetheri-root.target'
    command=console_option(console_fallback)+f' panic=-1 rdinit=/init root=PARTUUID={partition} ro rootfstype=ext4 rootflags=noload rd.systemd.unit=aetheri-initrd.target systemd.unit={target} systemd.log_target=console systemd.show_status=yes aetheri.boot_nonce={nonce}\n'
    (inputs/'cmdline.txt').write_bytes(command.encode())
    initrd_manifest=dict(format=1,proof='usb-root',nonce=nonce,kernel_release=original['kernel_release'],outputs={n:boot.sha(inputs/n) for n in ('vmlinuz','initramfs.cpio.gz','cmdline.txt')},root={p.relative_to(tree).as_posix():({'symlink':os.readlink(p)} if p.is_symlink() else dict(sha256=boot.sha(p),mode=oct(p.stat().st_mode&0o7777))) for p in paths if p.is_symlink() or p.is_file()})
    (inputs/'manifest.json').write_bytes((json.dumps(initrd_manifest,indent=2)+'\n').encode());boot.inspect(inputs)
    uefi.build(inputs,packages,output/'efi');efi=json.loads((output/'efi/manifest.json').read_text())
    if efi['esp_guid']!=system_manifest['esp_guid']:raise ValueError('different original ESP binding')
    final=output/'boot.raw'
    with final.open('xb') as target:
        target.truncate(desktop['disk_bytes'] if desktop else 512*1024**2)
        if not desktop:
            with (system/'boot.raw').open('rb') as source:
                at=0
                while block:=source.read(1024**2):
                    if not 1024**2<=at<127*1024**2 and block.strip(b'\0'):target.seek(at);target.write(block)
                    at+=len(block)
                if at!=512*1024**2:raise ValueError('changed original root length')
        else:
            volume=system_manifest['data_partition'];disk_id=system_manifest['disk_guid']
            layout=f'label: gpt\nlabel-id: {disk_id}\nunit: sectors\nfirst-lba: 2048\n\nstart=2048,size=258048,type=U,uuid={efi["esp_guid"]},name="Aetheri boot"\nstart=262144,size=1048576,type=4f68bce3-e8cd-4db1-96e7-fbcaf984b709,uuid={partition},attrs="GUID:60",name="Aetheri system"\nstart={volume["start"]},size={volume["sectors"]},type=0fc63daf-8483-4772-8e79-3d69d8477de4,uuid={volume["guid"]},name="Aetheri data"\n'
            run('combined-gpt',['/usr/bin/sfdisk','--quiet','--no-reread','--no-tell-kernel',str(final)],data=layout.encode())
            with (system/'boot.raw').open('rb') as source:
                source.seek(262144*512);target.seek(262144*512)
                if image_io.copy_extent(source,target,1048576*512)!=part['sha256']:raise ValueError('changed copied root')
            with (system/'data.raw').open('rb') as source:
                target.seek(volume['start']*512)
                if image_io.copy_extent(source,target,volume['sectors']*512)!=volume['initial_sha256']:raise ValueError('changed copied data')
        with (output/'efi/boot.raw').open('rb') as source:
            source.seek(1024**2);at=1024**2
            for _ in range(126):
                block=source.read(1024**2)
                if len(block)!=1024**2:raise ValueError('truncated new ESP')
                if block.strip(b'\0'):target.seek(at);target.write(block)
                at+=len(block)
    shutil.copyfile(output/'efi/BOOTX64.EFI',output/'BOOTX64.EFI')
    result=dict(system_manifest);result.update(efi)
    result.update(proof='usb-desktop' if desktop else 'usb-root',disk_bytes=desktop['disk_bytes'] if desktop else 512*1024**2,disk_guid=system_manifest['disk_guid'],root_partition=part,usb_source_manifest_sha256=boot.sha(system/'manifest.json'),usb_builder_sha256=boot.sha(Path(__file__)),usb_probe_source_sha256=boot.sha(ROOT/'src/boot/initrd.c'),usb_inputs_sha256=hashlib.sha256(raw).hexdigest(),usb_wrong_filesystem=wrong_filesystem,usb_console_fallback=console_fallback,outputs={n:boot.sha(output/n) for n in ('boot.raw','BOOTX64.EFI')})
    if desktop:
        end=(result['data_partition']['start']+result['data_partition']['sectors'])*512
        result['immutable_ranges']=[dict(start=0,size=desktop['data_start']*512,sha256=image_io.range_sha(final,0,desktop['data_start']*512)),dict(start=end,size=desktop['disk_bytes']-end,sha256=image_io.range_sha(final,end,desktop['disk_bytes']-end))]
    (output/'manifest.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());media.inspect(output)
    print(json.dumps(dict(output=str(output),proof='usb-root',initramfs_bytes=(inputs/'initramfs.cpio.gz').stat().st_size)))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--boot-image',type=Path,required=True);p.add_argument('--root-image',type=Path,required=True);p.add_argument('--packages',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--wrong-filesystem',action='store_true')
    a=p.parse_args();build(a.boot_image.resolve(),a.root_image.resolve(),a.packages.resolve(),a.output.absolute(),a.wrong_filesystem)
