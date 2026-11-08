"""Reject a deliberately corrupted data UUID on ONE new owned image copy.

Only the primary ext4 superblock UUID changes. Its stale checksum is deliberate:
this is a corrupt/unexpected-filesystem negative, never a repaired filesystem.
No source, physical disk or previously used VM image is written or replayed.
"""
import argparse
import os
from pathlib import Path
import shutil
import stat
import uuid

import run_vm as vm
import test_boot_probe as boot
import test_data_probe as data_test
import test_usb_desktop as usb
import test_usb_parent_negative as negative
import uefi_probe_image as media


def corrupt_uuid(stream,partition):
    info=os.fstat(stream.fileno());start=partition['start']*512
    expected=uuid.UUID(partition['filesystem_uuid']).bytes
    if not stat.S_ISREG(info.st_mode) or info.st_size!=31457280000 or start!=671088640 or partition['sectors']!=60127232:
        raise ValueError('different owned filesystem fixture')
    at=start+1024;stream.seek(at);block=stream.read(1024)
    if len(block)!=1024 or block[56:58]!=b'\x53\xef' or block[104:120]!=expected:raise ValueError('different original ext4 superblock')
    changed=bytes([expected[0]^1])+expected[1:]
    stream.seek(at+104)
    if stream.write(changed)!=16:raise OSError('incomplete UUID corruption')
    stream.flush();os.fsync(stream.fileno());stream.seek(at)
    if stream.read(1024)!=block[:104]+changed+block[120:]:raise ValueError('unexpected superblock mutation')
    return dict(offset=at+104,before=expected.hex(),after=changed.hex(),bytes=16)


def command(image,output,medium,identifier):
    args=usb.command(image,output,medium,identifier);result=[];at=0
    while at<len(args):
        if args[at]=='-qmp':at+=2;continue
        result.append(args[at]);at+=1
    return result


def run(image,output):
    output=vm.owned(output)
    if shutil.disk_usage(output.parent).free<8*1024**3:raise OSError(28,'corruption test needs 8 GiB headroom')
    if any(',' in str(p) for p in (image,output,boot.TOOLS)):raise ValueError('unsupported QEMU path')
    manifest=media.inspect(image);volume=usb.admit(manifest);tools=boot.host_tools()
    output.mkdir(mode=0o700);vm.private_path(output,directory=True)
    state=dict(state='copy_pending',pending='medium',manifest_sha256=boot.sha(image/'manifest.json'),controller_sha256=boot.sha(Path(__file__)))
    def save():vm.write_state(output/'corruption.json',state)
    save();medium=output/'medium.raw'
    try:
        with medium.open('x+b') as held:
            usb.copy(image/'boot.raw',held,volume);held.flush();os.fsync(held.fileno())
            if boot.sha(medium)!=volume['initial_sha256']:raise ValueError('different initial fixture copy')
            state.update(state='mutation_pending',pending='uuid');save()
            state['mutation']=corrupt_uuid(held,manifest['data_partition'])
            state['mutated_sha256']=boot.sha(medium);usb.check_immutable(medium,manifest)
            expected=data_test.identity(os.fstat(held.fileno()));state['data_object']=expected
            shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd',output/'nvram.fd')
            state.update(state='dispatch_pending',pending='original-vm');save()
            result=boot.observe(output,command(image,output,medium,str(uuid.uuid4())),manifest,tools,state['manifest_sha256'],'filesystem',180,evaluator=negative.evaluate)
            state.update(state='checking',pending=None);save()
            data_test.check_held(held,medium,expected);usb.check_immutable(medium,manifest)
            if boot.sha(medium)!=state['mutated_sha256'] or media.inspect(image)!=manifest:raise ValueError('negative wrote medium/source')
            state.update(state='accepted',pending=None,medium_unchanged=True,source_unchanged=True,serial_sha256=result['serial_sha256']);save()
    except BaseException as error:
        state.update(state='failed_preserved',error=str(error));save();raise
    return state


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.image.resolve(),a.output.absolute())
