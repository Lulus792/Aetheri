"""Wrong-parent boot using NEW private qcow2 overlays over verified raw fixtures.

No physical device, shared folder or network. Bases remain read-only backing
files. Unknown attempts/overlays are preserved, never restarted or committed.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import uuid

import run_vm as vm
import test_boot_probe as boot
import test_data_probe as data_test
import test_usb_parent_negative as negative
import uefi_probe_image as media


def image_tool():
    path=boot.TOOLS/'qemu-img.exe'
    receipt=json.loads((boot.ROOT/'local/verification/host-tools.json').read_text())
    expected=receipt['files'].get(path.relative_to(boot.ROOT).as_posix())
    if not expected or path.is_symlink() or boot.sha(path)!=expected:
        raise ValueError('changed original qemu-img extraction')
    return path,expected


def chain_check(chain,overlay,base,size):
    if not isinstance(chain,list) or len(chain)!=2:raise ValueError('different backing chain')
    for entry,path,format in zip(chain,(overlay,base),('qcow2','raw')):
        if not isinstance(entry,dict) or entry.get('format')!=format or entry.get('virtual-size')!=size or entry.get('encrypted',False) is not False or Path(entry.get('filename','')).resolve()!=path.resolve():
            raise ValueError('different backing object/format/size')
    if Path(chain[0].get('full-backing-filename','')).resolve()!=base.resolve() or chain[0].get('backing-filename-format')!='raw' or chain[1].get('backing-filename') is not None:
        raise ValueError('different backing selection')


def command(image,output,primary,external,identifier):
    args=negative.command(image,output,primary,external,identifier)
    return [value.replace('format=raw,file='+str(primary),'format=qcow2,file='+str(primary)).replace('format=raw,file='+str(external),'format=qcow2,file='+str(external)) for value in args]


def run(fixtures,output):
    output=vm.owned(output);fixtures=vm.owned(fixtures)
    if shutil.disk_usage(output.parent).free<8*1024**3:raise OSError(28,'overlay test needs 8 GiB free headroom')
    if any(',' in str(p) for p in (fixtures,output,boot.TOOLS)):raise ValueError('unsupported QEMU path')
    if json.loads((fixtures/'fixture.json').read_text()).get('state')!='prepared':raise ValueError('unfinished split fixture')
    manifests={role:media.inspect(fixtures/role) for role in ('primary','external')}
    first=manifests['primary'];second=manifests['external']
    if first.get('usb_parent_negative')!='primary' or second.get('usb_parent_negative')!='external' or first['nonce']!=second['nonce'] or first['root_partition']['guid']==second['root_partition']['guid'] or first['data_partition']['guid']==second['data_partition']['guid'] or first['data_partition']['filesystem_uuid']!=second['data_partition']['filesystem_uuid'] or first['usb_configuration']!=second['usb_configuration'] or first['usb_wrong_data_filesystem'] or second['usb_wrong_data_filesystem']:
        raise ValueError('different wrong-parent fixture pair')
    tool,tool_hash=image_tool();tools=boot.host_tools();tools[tool.relative_to(boot.ROOT/'local').as_posix()]=tool_hash
    output.mkdir(mode=0o700);vm.private_path(output,directory=True)
    state=dict(state='preparing',pending=None,tool_sha256=boot.sha(Path(__file__)),qemu_img_sha256=tool_hash,inputs={r:boot.sha(fixtures/r/'manifest.json') for r in manifests},steps=[])
    def save():vm.write_state(output/'overlays.json',state)
    def execute(name,args):
        state['pending']=name;save()
        p=subprocess.run([str(tool),*args],capture_output=True)
        (output/(name+'.stdout')).write_bytes(p.stdout);(output/(name+'.stderr')).write_bytes(p.stderr)
        state['steps'].append(dict(name=name,exit=p.returncode));save()
        if p.returncode:raise RuntimeError('qemu-img operation failed: '+name)
        state['pending']=None;save();return p.stdout
    handles=[];overlay_handles=[]
    try:
        for role in manifests:
            base=fixtures/role/'boot.raw';held=base.open('rb');handles.append((role,held,data_test.identity(os.fstat(held.fileno()))))
            overlay=output/(role+'.qcow2')
            if overlay.exists() or overlay.is_symlink():raise ValueError('overlay must be new')
            execute(role+'-create',['create','-q','-f','qcow2','-F','raw','-b',str(base),str(overlay)])
            vm.private_path(overlay)
            overlay_held=overlay.open('rb');overlay_snapshot=data_test.identity(os.fstat(overlay_held.fileno()))
            if overlay_snapshot['size']>16*1024**2:raise ValueError('unexpected initial overlay extent')
            overlay_handles.append((role,overlay_held,overlay_snapshot))
            chain=json.loads(execute(role+'-info-before',['info','-f','qcow2','--output=json','--backing-chain',str(overlay)]))
            chain_check(chain,overlay,base,31457280000)
        shutil.copyfile(boot.TOOLS/'share/edk2-i386-vars.fd',output/'nvram.fd')
        state.update(state='dispatch_pending',pending='original-vm');save()
        result=boot.observe(output,command(fixtures/'primary',output,output/'primary.qcow2',output/'external.qcow2',str(uuid.uuid4())),first,tools,state['inputs']['primary'],'data-parent',180,evaluator=negative.evaluate)
        state.update(state='checking',pending=None);save()
        for role,held,snapshot in overlay_handles:
            data_test.check_held(held,output/(role+'.qcow2'),snapshot)
        for role,held,snapshot in handles:
            base=fixtures/role/'boot.raw';overlay=output/(role+'.qcow2')
            data_test.check_held(held,base,snapshot)
            if boot.sha(base)!=manifests[role]['outputs']['boot.raw']:raise ValueError('changed read-only backing input')
            chain=json.loads(execute(role+'-info-after',['info','-f','qcow2','--output=json','--backing-chain',str(overlay)]))
            chain_check(chain,overlay,base,31457280000)
            execute(role+'-check',['check','-f','qcow2','--output=json',str(overlay)])
            execute(role+'-compare',['compare','-q','-f','qcow2','-F','raw',str(overlay),str(base)])
        state.update(state='accepted',pending=None,both_bases_unchanged=True,both_guest_views_unchanged=True,serial_sha256=result['serial_sha256']);save()
        return state
    except BaseException as error:
        state.update(state='failed_preserved',error=str(error));save();raise
    finally:
        for _,held,_ in handles:held.close()
        for _,held,_ in overlay_handles:held.close()


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fixtures',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();run(a.fixtures.resolve(),a.output.absolute())
