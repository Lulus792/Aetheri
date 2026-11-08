"""Create two NEW regular-image fixtures for actual wrong-parent rejection.

Only GPT GUIDs/CRCs change in fresh copies. No filesystem or physical device is
edited. Source images and user-created documents are never adopted as fixtures.
"""
import argparse
import copy
import errno
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import uuid
import zlib

import image_io
import run_vm as vm
import test_boot_probe as boot
import test_usb_desktop as desktop
import uefi_probe_image as media


def require_space(output):
    # Allow both logical copies plus headroom even when sparse allocation fails.
    # This is an observation, not a reservation against unrelated host writers.
    needed=2*31457280000+8*1024**3
    if shutil.disk_usage(output.parent).free<needed:
        raise OSError(errno.ENOSPC,'USB pair test needs both full logical copies plus 8 GiB free headroom')


def replace_guid(stream,ordinal,old,new,*,disk=None):
    """One-shot mutation of independently admitted, owned GPT copy metadata."""
    info=os.fstat(stream.fileno())
    if not stat.S_ISREG(info.st_mode) or info.st_size%512 or not 4*1024**2<=info.st_size<=32*1024**3 or ordinal not in (2,3) or not isinstance(old,uuid.UUID) or not isinstance(new,uuid.UUID) or old==new or (disk is not None and not isinstance(disk,uuid.UUID)):
        raise ValueError('invalid owned GPT mutation')
    last=info.st_size//512-1
    for current,other,array in ((1,last,2),(last,1,last-32)):
        stream.seek(current*512);header=bytearray(stream.read(92))
        if len(header)!=92 or header[:8]!=b'EFI PART' or struct.unpack_from('<II',header,8)!=(0x10000,92) or struct.unpack_from('<QQ',header,24)!=(current,other) or struct.unpack_from('<QII',header,72)!=(array,128,128):raise ValueError('different original GPT structure')
        crc=struct.unpack_from('<I',header,16)[0];struct.pack_into('<I',header,16,0)
        if zlib.crc32(header)!=crc:raise ValueError('bad original GPT header CRC')
        stream.seek(array*512);entries=bytearray(stream.read(16384));at=(ordinal-1)*128+16
        if len(entries)!=16384 or zlib.crc32(entries)!=struct.unpack_from('<I',header,88)[0] or uuid.UUID(bytes_le=bytes(entries[at:at+16]))!=old:raise ValueError('different original GPT entry')
        entries[at:at+16]=new.bytes_le
        if disk is not None:header[56:72]=disk.bytes_le
        struct.pack_into('<I',header,88,zlib.crc32(entries));struct.pack_into('<I',header,16,zlib.crc32(header))
        stream.seek(array*512);stream.write(entries);stream.seek(current*512);stream.write(header)
    stream.flush();os.fsync(stream.fileno())


def prepare(image,output):
    output=vm.owned(output);require_space(output);manifest=media.inspect(image);volume=desktop.admit(manifest)
    if manifest.get('usb_parent_negative') is not None:raise ValueError('never derive a fixture from an earlier negative')
    source_sha=boot.sha(Path(__file__).resolve().parents[1]/'src/boot/initrd.c')
    if manifest.get('usb_probe_source_sha256')!=source_sha:raise ValueError('use the current independently checked observation profile')
    output.mkdir(mode=0o700);vm.private_path(output,directory=True)
    state=dict(state='copy_pending',source_manifest_sha256=boot.sha(image/'manifest.json'),tool_sha256=boot.sha(Path(__file__)))
    vm.write_state(output/'fixture.json',state)
    namespace=uuid.UUID(manifest['disk_guid']);primary_data=uuid.uuid5(namespace,'wrong-parent/primary-data');external_root=uuid.uuid5(namespace,'wrong-parent/external-root');external_disk=uuid.uuid5(namespace,'wrong-parent/external-disk')
    for role in ('primary','external'):
        directory=output/role;directory.mkdir(mode=0o700);vm.private_path(directory,directory=True);selected=copy.deepcopy(manifest)
        with (directory/'boot.raw').open('x+b') as target:
            desktop.copy(image/'boot.raw',target,volume);target.flush();os.fsync(target.fileno())
            state.update(state='mutation_pending',role=role);vm.write_state(output/'fixture.json',state)
            if role=='primary':replace_guid(target,3,uuid.UUID(manifest['data_partition']['guid']),primary_data);selected['data_partition']['guid']=str(primary_data)
            else:replace_guid(target,2,uuid.UUID(manifest['root_partition']['guid']),external_root,disk=external_disk);selected['root_partition']['guid']=str(external_root);selected['disk_guid']=str(external_disk)
        selected['usb_parent_negative']=role;selected['outputs']['boot.raw']=boot.sha(directory/'boot.raw')
        for r in selected['immutable_ranges']:r['sha256']=image_io.range_sha(directory/'boot.raw',r['start'],r['size'])
        shutil.copyfile(image/'BOOTX64.EFI',directory/'BOOTX64.EFI')
        if boot.sha(directory/'BOOTX64.EFI')!=selected['outputs']['BOOTX64.EFI']:raise ValueError('changed EFI copy')
        (directory/'manifest.json').write_text(json.dumps(selected,indent=2)+'\n')
        media.inspect(directory)
        state.update(state='copy_verified',role=role);vm.write_state(output/'fixture.json',state)
    state.update(state='prepared');vm.write_state(output/'fixture.json',state);return state


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();prepare(a.image.resolve(),a.output.absolute())
