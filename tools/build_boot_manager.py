"""Prepare an explicit boot entry, then assemble a NEW regular laptop image.

Preparation extracts the reviewed EFI manager as data under an ordinary Linux
account. Assembly independently verifies the full base; it never accepts a
physical device, modifies an existing image or changes carrier/firmware policy.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys

import boot_manager_profile as profile
import image_io
import uefi_probe_image as media

ROOT=Path(__file__).resolve().parents[1]
PINS=ROOT/'spec/boot-manager-inputs.json'


def regular(path,maximum):
    if path.is_symlink() or not stat.S_ISREG(path.stat().st_mode) or path.stat().st_size>maximum:
        raise ValueError('expected bounded regular input: '+str(path))
    return path.read_bytes()


def source_profile(image):
    raw=regular(image/'manifest.json',4*1024**2);manifest=json.loads(raw)
    if 'boot_manager' in manifest:raise ValueError('source already has a managed boot path')
    profile.arguments(manifest)
    uki=regular(image/'BOOTX64.EFI',64*1024**2)
    if media.digest(uki)!=manifest['outputs']['BOOTX64.EFI']:raise ValueError('changed source UKI')
    sections=media.pe_sections(uki)
    if set(manifest['sections'])!={'.linux','.initrd','.cmdline','.osrel','.uname'} or any(media.digest(sections.get(k,b''))!=v for k,v in manifest['sections'].items()):
        raise ValueError('changed source UKI resources')
    return raw,manifest,uki,profile.entry(manifest,sections['.cmdline'])


def prepare(image,packages,output):
    if sys.platform!='linux' or os.uname().machine!='x86_64' or os.geteuid()==0:
        raise ValueError('prepare as an ordinary x86-64 Linux build user')
    source,manifest,uki,entry=source_profile(image)
    pins_raw=PINS.read_bytes();pins=json.loads(pins_raw);package=pins['package'];payload=pins['payload']
    if (pins.get('format')!=1 or pins.get('profile')!=profile.PROFILE
        or payload!={'path':'usr/lib/systemd/boot/efi/systemd-bootx64.efi','size':profile.MANAGER_BYTES,'sha256':profile.MANAGER_SHA256}
        or Path(package['name']).name!=package['name']):raise ValueError('different reviewed manager inputs')
    rpm=regular(packages/package['name'],1024**2)
    if len(rpm)!=package['size'] or media.digest(rpm)!=package['sha256']:raise ValueError('changed manager RPM')
    output.mkdir(mode=0o700);logs=output/'logs';logs.mkdir()
    env={**os.environ,'LC_ALL':'C','MTOOLSRC':str(output/'empty.conf')};(output/'empty.conf').write_bytes(b'')
    def run(name,args,data=None):
        p=subprocess.run(args,input=data,capture_output=True,env=env)
        (logs/(name+'.stdout')).write_bytes(p.stdout);(logs/(name+'.stderr')).write_bytes(p.stderr)
        p.check_returncode();return p.stdout
    archive=output/package['name'];archive.write_bytes(rpm)
    signature=run('signature',['/usr/bin/rpmkeys','--checksig','--verbose',str(archive)]).decode()
    if package['fedora_signer'].lower()+': OK' not in signature:raise ValueError('unexpected manager signer')
    packed=run('rpm2cpio',['/usr/bin/rpm2cpio',str(archive)])
    manager=run('manager',['/usr/bin/cpio','--quiet','--extract','--to-stdout','./'+payload['path']],packed)
    if len(manager)!=profile.MANAGER_BYTES or media.digest(manager)!=profile.MANAGER_SHA256:raise ValueError('changed manager payload')
    media.pe_sections(manager)
    files={'BOOTX64.EFI':manager,'system.efi':uki,'loader.conf':profile.LOADER,'aetheri.conf':entry}
    for name,data in files.items():
        with (output/name).open('xb') as f:f.write(data)
    esp=output/'esp.raw'
    with esp.open('xb') as f:f.truncate(profile.ESP_BYTES)
    if manifest.get('esp_start')!=2048 or manifest.get('esp_sectors')!=258048:raise ValueError('different fixed ESP placement')
    run('fat',['/usr/bin/mkfs.fat','--invariant','-F','32','-i',manifest['nonce'][:8],'-n','AETHERI','-h','2048',str(esp),str(profile.ESP_BYTES//1024)])
    for i,path in enumerate(('::/EFI','::/EFI/BOOT','::/EFI/AETHERI','::/LOADER','::/LOADER/ENTRIES')):
        run('mkdir-'+str(i),['/usr/bin/mmd','-i',str(esp),path])
    for i,(name,path) in enumerate((('BOOTX64.EFI','::/EFI/BOOT/BOOTX64.EFI'),('system.efi','::/EFI/AETHERI/SYSTEM.EFI'),('loader.conf','::/LOADER/loader.conf'),('aetheri.conf','::/LOADER/ENTRIES/aetheri.conf'))):
        run('copy-'+str(i),['/usr/bin/mcopy','-i',str(esp),str(output/name),path])
    if media.boot_file(esp.read_bytes(),managed=True)!=files:raise ValueError('independent ESP readback differs')
    files['esp.raw']=esp.read_bytes()
    result=dict(format=1,profile=profile.PROFILE,source_manifest_sha256=media.digest(source),
        pins_sha256=media.digest(pins_raw),builder_sha256=image_io.hash_file(Path(__file__)),
        files={n:media.digest(b) for n,b in files.items()})
    (output/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def checked_preparation(image,prepared):
    source,manifest,uki,entry=source_profile(image)
    record=json.loads(regular(prepared/'manifest.json',16384))
    if (record.get('format')!=1 or record.get('profile')!=profile.PROFILE
        or record.get('source_manifest_sha256')!=media.digest(source)
        or record.get('pins_sha256')!=image_io.hash_file(PINS)
        or set(record.get('files',{}))!={'BOOTX64.EFI','system.efi','loader.conf','aetheri.conf','esp.raw'}):
        raise ValueError('different prepared source/profile')
    files={n:regular(prepared/n,profile.ESP_BYTES) for n in record['files']}
    if any(media.digest(files[n])!=v for n,v in record['files'].items()):raise ValueError('changed preparation')
    esp=files.pop('esp.raw')
    if (len(esp)!=profile.ESP_BYTES or media.boot_file(esp,managed=True)!=files
        or files['system.efi']!=uki or files['aetheri.conf']!=entry or files['loader.conf']!=profile.LOADER
        or len(files['BOOTX64.EFI'])!=profile.MANAGER_BYTES or media.digest(files['BOOTX64.EFI'])!=profile.MANAGER_SHA256):
        raise ValueError('different prepared boot files')
    return source,manifest,files,esp


def assemble(image,prepared,output):
    source,manifest,files,esp=checked_preparation(image,prepared)
    if media.inspect(image)!=manifest:raise ValueError('base image changed')
    if shutil.disk_usage(output.parent).free<8*1024**3:raise OSError(28,'assembly needs 8 GiB headroom')
    output.mkdir(mode=0o700);target=output/'boot.raw'
    with (image/'boot.raw').open('rb') as before,target.open('x+b') as after:
        image_io.sparse_file(after);image_io.set_length(after,manifest['disk_bytes'])
        if image_io.copy_extent(before,after,manifest['disk_bytes'])!=manifest['outputs']['boot.raw']:
            raise ValueError('source changed during copy')
        after.seek(manifest['esp_start']*512)
        if after.write(esp)!=len(esp):raise OSError('short ESP write')
        after.flush();os.fsync(after.fileno())
    for name in ('BOOTX64.EFI','system.efi'):
        with (output/name).open('xb') as f:f.write(files[name]);f.flush();os.fsync(f.fileno())
    # Prove every byte outside the replaced ESP, including root/data and both GPTs.
    start=manifest['esp_start']*512;end=start+len(esp)
    for offset,size in ((0,start),(end,manifest['disk_bytes']-end)):
        if image_io.range_sha(image/'boot.raw',offset,size)!=image_io.range_sha(target,offset,size):
            raise ValueError('bytes outside ESP changed')
    result=dict(manifest);result['boot_manager']={'format':1,'profile':profile.PROFILE,'sha256':profile.MANAGER_SHA256}
    result.update(boot_manager_base_manifest_sha256=media.digest(source),
        boot_manager_preparation_sha256=image_io.hash_file(prepared/'manifest.json'),
        boot_manager_builder_sha256=image_io.hash_file(Path(__file__)),
        outputs={n:image_io.hash_file(output/n) for n in ('boot.raw','BOOTX64.EFI','system.efi')})
    result['immutable_ranges']=[{**r,'sha256':image_io.range_sha(target,r['start'],r['size'])} for r in manifest['immutable_ranges']]
    (output/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    if media.inspect(image)!=manifest or media.inspect(output)!=result:raise ValueError('final image verification differs')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    for action in ('prepare','assemble'):
        q=sub.add_parser(action);q.add_argument('--image',type=Path,required=True);q.add_argument('--output',type=Path,required=True)
        q.add_argument('--'+('packages' if action=='prepare' else 'prepared'),type=Path,required=True)
    a=p.parse_args();extra=a.packages if a.action=='prepare' else a.prepared
    result=globals()[a.action](a.image.resolve(),extra.resolve(),a.output.absolute());print(json.dumps(dict(output=str(a.output),format=result['format'])))
