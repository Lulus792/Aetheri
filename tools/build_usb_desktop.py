"""Build one new regular USB desktop image, never a physical device.

Uses the authorized medium's fixed geometry. The initrd and real-root data
checks share independently generated configuration, not candidate volume data.
Laptop hardware integration and physical acceptance remain separate work.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

import build_desktop_probe as desktop
import build_usb_root_probe as usb
import test_boot_probe as boot

ROOT=Path(__file__).resolve().parents[1]


def selected_layout():
    raw=(ROOT/'spec/usb-desktop-inputs.json').read_bytes();pin=json.loads(raw)
    layout={k:pin[k] for k in ('disk_bytes','root_start','root_sectors','data_start','data_sectors')}
    if pin.get('format')!=1 or any(type(v) is not int for v in layout.values()) or tuple(layout.values())!=(31457280000,262144,1048576,1310720,60127232):
        raise ValueError('different reviewed USB desktop geometry')
    return raw,pin,layout


def build(base,packages,output,wrong_data_filesystem=False,*,prepare_extra=None,console_fallback=False):
    if sys.platform!='linux' or os.uname().machine!='x86_64' or os.geteuid()==0:raise ValueError('use the ordinary x86-64 Linux image builder')
    raw,pin,layout=selected_layout()
    if boot.sha(base/'manifest.json')!=pin['boot_source_manifest_sha256']:raise ValueError('different independently pinned RAM source')
    original=boot.inspect(base);output.mkdir(mode=0o700)
    def prepare(tree,nonce,volume):
        if nonce!=original['nonce'] or volume['size']!=layout['data_sectors']*512:raise ValueError('different fresh desktop/data selection')
        namespace=uuid.UUID('e64786b8-e09e-496c-8175-731a1debcf8f')
        partition=str(uuid.uuid5(namespace,nonce+'/root-partition'));filesystem=str(uuid.uuid5(namespace,nonce+'/root-filesystem'))
        data_partition=str(uuid.uuid5(namespace,nonce+'/data-partition'));expected=volume['uuid']
        if wrong_data_filesystem:expected=('0' if expected[0]!='0' else '1')+expected[1:]
        configuration=f'AETHERI-USB2\n{nonce}\n{partition}\n{filesystem}\n{original["kernel_release"]}\n{layout["disk_bytes"]//512}\n{layout["root_sectors"]}\n{data_partition}\n{expected}\n{layout["data_start"]}\n{layout["data_sectors"]}\n'
        path=tree/'etc/aetheri-usb-root'
        with path.open('x') as f:f.write(configuration)
        flags=['/usr/bin/gcc','-std=c17','-D_GNU_SOURCE','-O2','-g0','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror','-fanalyzer',str(ROOT/'src/boot/initrd.c'),'-o',str(tree/'usr/libexec/aetheri-usb-check')]
        p=subprocess.run(flags,capture_output=True)
        (output/'usb-probe.stdout').write_bytes(p.stdout);(output/'usb-probe.stderr').write_bytes(p.stderr);p.check_returncode()
        units=tree/'etc/systemd/system'
        path=units/'aetheri-data-check.service';text=path.read_text();old='ExecStart=/usr/libexec/aetheri-data-probe --check'
        if text.count(old)!=1:raise ValueError('different original data unit')
        path.write_text(text.replace(old,'ExecStart=/usr/libexec/aetheri-usb-check --data'))
        path=units/'var.mount';text=path.read_text()
        if text.count('What=/dev/vdb\n')!=1:raise ValueError('different original data mount')
        path.write_text(text.replace('What=/dev/vdb\n','What=/dev/disk/by-partuuid/'+data_partition+'\n'))
        path=tree/'etc/udev/rules.d/98-aetheri-root-identity.rules';text=path.read_text();old='ENV{ID_PART_ENTRY_UUID}=="'+partition+'"'
        if text.count(old)!=1:raise ValueError('different root alias rule')
        path.write_text(text.replace(old,'ENV{ID_PART_ENTRY_UUID}=="?*"'))
        (units/'aetheri-usb-data-ready.service').write_text('[Unit]\nDescription=Check the mounted single-medium data view\nDefaultDependencies=no\nRequires=home.mount var.mount tmp.mount\nAfter=home.mount var.mount tmp.mount\nFailureAction=poweroff\n[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/libexec/aetheri-usb-check --data-mounted\nStandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n')
        directory=units/'aetheri-graphics-vt.service.d'
        with (directory/'aetheri-usb-data.conf').open('x') as f:f.write('[Unit]\nRequires=aetheri-usb-data-ready.service\nAfter=aetheri-usb-data-ready.service\n')
        # These are known files just created in this NEW ordinary-user tree,
        # not installed carrier paths or an app-removal implementation.
        (tree/'etc/aetheri-data-proof').unlink();(tree/'usr/libexec/aetheri-data-probe').unlink()
        result=dict(usb_layout=layout,usb_configuration=configuration,usb_desktop_builder_sha256=boot.sha(Path(__file__)),usb_desktop_inputs_sha256=hashlib.sha256(raw).hexdigest(),usb_wrong_data_filesystem=wrong_data_filesystem,data_partition=dict(guid=data_partition,filesystem_uuid=volume['uuid'],start=layout['data_start'],sectors=layout['data_sectors'],initial_sha256=volume['initial_sha256']))
        if prepare_extra:
            extra=prepare_extra(tree,nonce,volume)
            if not isinstance(extra,dict) or set(extra)&set(result):raise ValueError('conflicting additional profile metadata')
            result.update(extra)
        return result
    desktop.build(base,packages,output/'system',prepare_extra=prepare,data_bytes=layout['data_sectors']*512)
    usb.build(base,output/'system',packages,output/'media',desktop=layout,console_fallback=console_fallback)
    (output/'build.json').write_text(json.dumps(dict(format=1,media_manifest_sha256=boot.sha(output/'media/manifest.json'),builder_sha256=boot.sha(Path(__file__)),wrong_data_filesystem=wrong_data_filesystem),indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--boot-image',type=Path,required=True);p.add_argument('--packages',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--wrong-data-filesystem',action='store_true')
    a=p.parse_args();build(a.boot_image.resolve(),a.packages.resolve(),a.output.absolute(),a.wrong_data_filesystem)
