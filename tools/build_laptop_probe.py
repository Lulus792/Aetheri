"""Build a new USB laptop-test candidate; never access physical media.

Additional kernel modules and firmware are copied as pinned data. No carrier
module load, service change or package transaction occurs. Hardware, suspend,
accelerated rendering and production admission still require their own evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import build_usb_desktop as usb
import desktop_payload
import test_boot_probe as boot

ROOT=Path(__file__).resolve().parents[1]


def configure_console(tree):
    journal=tree/'etc/systemd/journald.conf.d/aetheri-development.conf'
    text=journal.read_text()
    if text.count('TTYPath=/dev/ttyS0\n')!=1:raise ValueError('different development journal console')
    journal_text=text.replace('TTYPath=/dev/ttyS0\n','TTYPath=/dev/console\n')
    graphics=tree/'etc/systemd/system/aetheri-graphics.service';text=graphics.read_text()
    old='StandardOutput=file:/dev/ttyS0\nStandardError=file:/dev/ttyS0\n'
    if text.count(old)!=1:raise ValueError('different development desktop logging')
    journal.write_text(journal_text)
    graphics.write_text(text.replace(old,'StandardOutput=journal\nStandardError=journal\n'))


def build(base,packages,output,wrong_data_filesystem=False):
    raw=(ROOT/'spec/laptop-probe-inputs.json').read_bytes();pins=json.loads(raw)
    if pins.get('format')!=1 or pins.get('kernel_release')!='6.19.10-300.fc44.x86_64':raise ValueError('different reviewed laptop kernel')
    desktop_payload.validate(pins)
    def prepare(tree,nonce,volume):
        logs=output/'laptop-logs';logs.mkdir()
        inventory=desktop_payload.install(tree,pins)
        path=tree/'etc/modules-load.d/aetheri-graphics.conf'
        if path.read_text()!='bochs\n':raise ValueError('different graphics module selection')
        path.write_text('bochs\ni915\n')
        configure_console(tree)
        args=['/usr/bin/depmod','-C','/dev/null','-b',str(tree),'-a',pins['kernel_release']]
        p=subprocess.run(args,capture_output=True)
        (logs/'depmod.stdout').write_bytes(p.stdout);(logs/'depmod.stderr').write_bytes(p.stderr);p.check_returncode()
        return dict(laptop_inputs_sha256=hashlib.sha256(raw).hexdigest(),laptop_builder_sha256=boot.sha(Path(__file__)),laptop_runtime=inventory,laptop_drm_selection='session-primary-auto',laptop_renderer='development-software')
    usb.build(base,packages,output,wrong_data_filesystem,prepare_extra=prepare,console_fallback=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--boot-image',type=Path,required=True);p.add_argument('--packages',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--wrong-data-filesystem',action='store_true')
    a=p.parse_args();build(a.boot_image.resolve(),a.packages.resolve(),a.output.absolute(),a.wrong_data_filesystem)
