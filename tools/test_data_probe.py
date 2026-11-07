"""Two cold boots of a NEW owned data copy; never attaches a real disk.

Any failed/unknown attempt stops the sequence and preserves its journal and files.
File identity here is an observation under the original open handle, not durable
volume/clone authentication or a general-purpose recovery protocol.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import stat

import test_boot_probe as boot
import uefi_probe_image as media


def identity(info):
    return dict(device=info.st_dev, inode=info.st_ino, size=info.st_size)


def check_held(stream, path, expected):
    held = os.fstat(stream.fileno())
    named = path.lstat()
    if not stat.S_ISREG(held.st_mode) or not stat.S_ISREG(named.st_mode) or identity(held) != expected or identity(named) != expected:
        raise ValueError('original data file replaced or resized; preserve attempt')


def command(image, output, data, serial):
    return [str(boot.TOOLS/'qemu-system-x86_64.exe'), '-name', 'aetheri-writable-data-proof',
            '-machine', 'pc-q35-11.1,accel=tcg', '-cpu', 'max', '-smp', '2', '-m', '768M',
            '-no-user-config', '-nodefaults', '-display', 'none', '-monitor', 'none', '-nic', 'none', '-no-reboot',
            '-drive', 'if=pflash,format=raw,readonly=on,file='+str(boot.TOOLS/'share/edk2-x86_64-code.fd'),
            '-drive', 'if=pflash,format=raw,file='+str(output/'nvram.fd'),
            '-drive', 'if=none,id=bootmedia,format=raw,readonly=on,file='+str(image/'boot.raw'),
            '-device', 'virtio-blk-pci,drive=bootmedia,serial=AETHERI_ROOT_PROBE,bootindex=1',
            '-drive', 'if=none,id=userdata,format=raw,file='+str(data),
            '-device', 'virtio-blk-pci,drive=userdata,serial='+serial,
            '-serial', 'file:'+str(output/'serial.log')]


def run(image, output, negative_volume=False, timeout=120):
    if any(',' in str(p) for p in (image, output, boot.TOOLS)):
        raise ValueError('QEMU paths must not contain option delimiters')
    manifest = media.inspect(image)
    if manifest.get('proof') != 'writable-data':
        raise ValueError('expected explicit writable-data profile')
    volume = manifest['data_volume']
    if volume['size'] != 256*1024**2 or not re.fullmatch(r'AETHDATA[0-9a-f]{12}', volume['serial']):
        raise ValueError('unexpected fixed data profile')
    source = image/'data.raw'
    if source.is_symlink() or not source.is_file() or source.stat().st_size != volume['size'] or boot.sha(source) != volume['initial_sha256']:
        raise ValueError('changed initial data image')
    hashes = boot.host_tools()
    output.mkdir(mode=0o700)
    data = output/'data.raw'
    state = dict(state='preparing', negative_volume=negative_volume, manifest_sha256=boot.sha(image/'manifest.json'), initial_data_sha256=volume['initial_sha256'], attempts=[])

    def record():
        with (output/'sequence.json').open('w') as f:
            json.dump(state, f, indent=2);f.write('\n');f.flush();os.fsync(f.fileno())

    record()
    with data.open('x+b') as held:
        with source.open('rb') as original:
            shutil.copyfileobj(original, held, 1024**2)
        held.flush();os.fsync(held.fileno())
        if boot.sha(data) != volume['initial_sha256']:
            raise ValueError('changed data copy')
        if negative_volume:
            held.seek(1024+104);value=held.read(1)
            held.seek(1024+104);held.write(bytes([value[0]^1]));held.flush();os.fsync(held.fileno())
        expected = identity(os.fstat(held.fileno()))
        state.update(data_object=expected, before_dispatch_sha256=boot.sha(data))
        previous = None
        for counter in range(1, 2 if negative_volume else 3):
            check_held(held, data, expected)
            if media.inspect(image) != manifest:
                raise ValueError('changed selected boot profile')
            attempt = output/('boot-'+str(counter));attempt.mkdir(mode=0o700)
            shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd', attempt/'nvram.fd')
            state.update(state='dispatch_pending', pending=counter);record()
            observed = boot.observe(attempt, command(image, attempt, data, volume['serial']), manifest, hashes, state['manifest_sha256'], 'volume-context' if negative_volume else False, timeout)
            check_held(held, data, expected)
            if not negative_volume:
                serial=(attempt/'serial.log').read_text(errors='replace')
                actual=re.findall(r'^AETHERI_DATA_PASS nonce='+manifest['nonce']+r' counter=([0-9]+) .*? boot=([0-9a-f-]{36})\s*$', serial, re.MULTILINE)
                if len(actual)!=1 or int(actual[0][0])!=counter or actual[0][1]==previous:
                    raise ValueError('persistence/cold-boot sequence mismatch; preserve data')
                previous=actual[0][1]
            elif boot.sha(data)!=state['before_dispatch_sha256']:
                raise ValueError('rejected volume changed')
            state['attempts'].append(dict(counter=counter, boot_ids=observed['boot_ids'], data_sha256=boot.sha(data)))
            state.update(state='boot_accepted', pending=None);record()
        if media.inspect(image)!=manifest:
            raise ValueError('boot artifact changed after dispatch')
        state.update(state='accepted', data_final_sha256=boot.sha(data));record()
    print(json.dumps(state), flush=True)
    return state


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--negative-volume', action='store_true')
    a=parser.parse_args();run(a.image.resolve(), a.output.absolute(), a.negative_volume)
