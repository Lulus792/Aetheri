"""Confined PipeWire/WirePlumber virtual HDA test, separate audio lab only.

Root grants a temporary fixed-user ACL on independently checked playback/control
nodes. MAC permits only the graph backend to open them. Restore original nodes
only after owned actor/unit/cgroup retirement. Not production device admission,
session authorization, revocation or recovery; unknown effects never replay.
"""
import argparse
import base64
import bz2
import errno
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import sys

from test_audio_mac import Fixture,label,run
from test_audio_user import UserAudio
from test_audio_pcm_device import node
import prepare_audio,prepare_audio_policy,prepare_audio_control,prepare_audio_hda


ACL='system.posix_acl_access'
GRANT=struct.pack('<I',2)+b''.join(struct.pack('<HHI',tag,permission,identity) for tag,permission,identity in
    ((1,6,0xffffffff),(2,6,1000),(4,6,0xffffffff),(16,6,0xffffffff),(32,0,0xffffffff)))


def acl(path):
    try:return os.getxattr(path,ACL)
    except OSError as error:
        if error.errno!=errno.ENODATA:raise
        return None


def check_device_runtime(prefix):
    """Project only the already pinned udev DSO and its existing notice."""
    prefix=Path(prefix).absolute()
    if prefix.is_symlink() or prefix.resolve()!=prefix:raise ValueError('aliased device runtime')
    pins=prepare_audio_hda.profile()[0]['base_runtime']
    selected=[e for e in pins['files'] if e['destination'] in ('/usr/lib64/libudev.so.1.7.12','/usr/share/licenses/systemd/LICENSE.LGPL2.1')]
    assert len(selected)==2 and pins['symlinks']['/usr/lib64/libudev.so.1']=='libudev.so.1.7.12'
    expected={'usr/lib64/libudev.so.1'}
    for entry in selected:
        name=entry['destination'].lstrip('/');path=prefix/name
        if path.is_symlink() or not path.is_file() or path.stat().st_size!=entry['size'] or stat.S_IMODE(path.stat().st_mode)!=entry['mode'] or hashlib.sha256(path.read_bytes()).hexdigest()!=entry['sha256']:raise ValueError('changed device runtime object')
        expected.add(name)
    link=prefix/'usr/lib64/libudev.so.1'
    if not link.is_symlink() or os.readlink(link)!='libudev.so.1.7.12':raise ValueError('changed device runtime alias')
    if {p.relative_to(prefix).as_posix() for p in prefix.rglob('*') if p.is_file() or p.is_symlink()}!=expected:raise ValueError('unexpected device runtime object')


class HardwareAudio(UserAudio):
    def __init__(self,output,case):
        if case not in ('unity','half','muted'):raise ValueError('fixed hardware case required')
        self.selected_case=case;super().__init__(output)

    def session_cases(self):return (self.selected_case,)
    def signal_frames(self):return 96000

    def access(self,phase,allow):
        command=[*self.bus.base,str(self.access_program),'--allow' if allow else '--deny']
        result=subprocess.run(command,capture_output=True,timeout=15)
        for suffix,data in (('stdout',result.stdout),('stderr',result.stderr)):
            with (self.output/(phase+'-access.'+suffix)).open('xb') as stream:stream.write(data)
        assert result.returncode==0 and not result.stderr,(phase,result.returncode,result.stdout,result.stderr)
        marker='DEVICE_OPEN_ALLOWED' if allow else 'DEVICE_OPEN_DENIED'
        assert result.stdout.count(marker.encode())==2
        self.record.setdefault('device_access',{})[phase]=result.stdout.decode();self.save()

    def setup(self,sdk,policy,control,binary,source,hardware,runtime):
        if any(line.split()[1]=='audio-device-test' for line in run('semodule','-lfull').splitlines()):raise ValueError('foreign/pending device policy')
        prepare_audio_hda.check(hardware)
        check_device_runtime(runtime)
        self.device_nodes={}
        for leaf,kind in (('controlC0','control'),('pcmC0D0p','playback')):
            path=Path('/dev/snd')/leaf;original=node(path)
            if acl(path) is not None:raise ValueError('preexisting device ACL; do not adopt')
            self.device_nodes[leaf]=dict(path=path,original=original,context=os.getxattr(path,'security.selinux'),kind=kind)
        super().setup(sdk,policy,control,binary,source)
        self.hardware=self.root/'hardware';self.device_runtime=self.root/'device-runtime'
        for prefix,original in ((self.hardware,hardware),(self.device_runtime,runtime)):
            shutil.copytree(original,prefix,symlinks=True)
            for path in (prefix,*prefix.rglob('*')):
                label(path,'aetheri_audio_library_test_t')
                if path.is_dir() and not path.is_symlink():path.chmod(0o755)
        spa=self.root/'spa';spa.mkdir();label(spa,'aetheri_audio_library_test_t')
        for name,prefix in (('support',self.sdk),('audioconvert',self.sdk),('alsa',self.hardware)):
            path=spa/name;path.symlink_to(prefix/'usr/lib64/spa-0.2'/name,target_is_directory=True);label(path,'aetheri_audio_library_test_t')
        self.access_program=self.root/'code/device-access';self.baseline=self.root/'code/device-baseline'
        for path,name,kind in ((self.access_program,'probe_audio_device_access','aetheri_audio_gateway_test_t'),(self.baseline,'probe_audio_pcm','aetheri_audio_fixture_test_t')):
            shutil.copyfile(binary/name,path);path.chmod(0o555);label(path,kind)
        self.record['devices']={leaf:dict(path=str(v['path']),object=v['original'],original_context=base64.b64encode(v['context']).decode(),original_acl=None) for leaf,v in self.device_nodes.items()};self.save()
        self.access('before',False)
        fd=os.open(self.device_nodes['controlC0']['path'],os.O_RDWR|os.O_NOFOLLOW|os.O_CLOEXEC)
        try:
            st=os.fstat(fd);expected=self.device_nodes['controlC0']['original'];assert (st.st_dev,st.st_ino,st.st_rdev)==(expected['device'],expected['inode'],expected['rdev'])
            launcher="import os,sys;os.dup2(int(sys.argv[1]),3,inheritable=True);os.execv(sys.argv[2],sys.argv[2:])"
            result=subprocess.run([sys.executable,'-c',launcher,str(fd),str(self.baseline),'--baseline'],pass_fds=(fd,),capture_output=True,timeout=25)
            (self.output/'baseline.stdout').write_bytes(result.stdout);(self.output/'baseline.stderr').write_bytes(result.stderr)
            assert result.returncode==0 and not result.stderr,(result.returncode,result.stdout,result.stderr)
            self.record['device_baseline']=result.stdout.decode();self.save()
        finally:os.close(fd)
        module=source/'policy/audio-device-test.cil';self.record['device_policy_sha256']=hashlib.sha256(module.read_bytes()).hexdigest();self.save()
        self.effect('device-policy-install','audio-device-test',lambda:run('semodule','-X','400','-i',str(module)))
        self.device_module=Path('/var/lib/selinux/targeted/active/modules/400/audio-device-test/cil').read_bytes()
        assert bz2.decompress(self.device_module)==module.read_bytes()
        for leaf,value in self.device_nodes.items():
            path=value['path'];assert node(path)==value['original'] and os.getxattr(path,'security.selinux')==value['context'] and acl(path) is None
            def activate():
                label(path,'aetheri_audio_'+value['kind']+'_device_test_t');os.setxattr(path,ACL,GRANT,os.XATTR_CREATE)
                assert node(path)==value['original'] and acl(path)==GRANT
            self.effect('device-test-grant',str(path),activate)
            self.record['devices'][leaf].update(active_acl=base64.b64encode(GRANT).decode(),active_context=base64.b64encode(os.getxattr(path,'security.selinux')).decode());self.save()
        self.access('active-dac',True)
        negative=Fixture.start(self,'device-access','foreign',[str(self.images['gateway']),'exec','none',str(self.access_program),'--deny'],{})
        self.release(negative);self.finished(negative)
        assert self.units[negative]['log'].read_text().count('DEVICE_OPEN_DENIED')==2
        # Fedora forwards these kernel audit records through the journal's
        # audit transport; --dmesg/-k excludes them. Select this boot and actor.
        pid=self.units[negative]['pid'];journal=run('journalctl','-b','--no-pager','-o','cat')
        avcs=[line for line in journal.splitlines() if 'avc:  denied' in line and 'pid='+str(pid)+' ' in line and 'aetheri_audio_foreign_test_t' in line and 'permissive=0' in line]
        assert all(any('aetheri_audio_'+kind+'_device_test_t' in line for line in avcs) for kind in ('control','playback')),avcs
        self.record['device_negative']=dict(pid=pid,avcs=avcs);self.save();self.stop(negative)

    def configure_case(self,root,fifo):
        conf=super().configure_case(root,fifo);path=conf/'server.conf';text=path.read_text()
        lines=text.splitlines(keepends=True);assert sum('name = libpipewire-module-pipe-tunnel' in line for line in lines)==1
        text=''.join(line for line in lines if 'name = libpipewire-module-pipe-tunnel' not in line)
        old='context.objects = [ { factory = spa-node-factory args = { factory.name = support.node.driver node.name = Test-Driver priority.driver = 200000 } } ]'
        assert text.count(old)==1 and text.count('audio.convert.* = audioconvert/libspa-audioconvert }')==1
        text=text.replace('audio.convert.* = audioconvert/libspa-audioconvert }','audio.convert.* = audioconvert/libspa-audioconvert api.alsa.* = alsa/libspa-alsa }')
        path.write_text(text.replace(old,'''context.objects = [ { factory = adapter args = {
 factory.name = api.alsa.pcm.sink node.name = aetheri.test.sink api.alsa.path = aetheri_hda
 node.always-process = true node.pause-on-idle = false node.suspend-on-idle = false
 api.alsa.use-ucm = false api.alsa.period-size = 1024 api.alsa.disable-batch = true
 audio.format = S16LE audio.rate = 48000 audio.channels = 2 audio.position = [ FL FR ]
 adapter.auto-port-config = { mode = dsp monitor = false position = preserve }
} } ]'''))
        (conf/'alsa.conf').write_text('pcm.aetheri_hda { type hw card 0 device 0 subdevice 0 }\nctl.hw { @args [ CARD ] @args.CARD { type string default "0" } type hw card $CARD }\n')
        return conf

    def extra_environment(self,root,conf):
        return dict(LD_LIBRARY_PATH=':'.join(str(p/'usr/lib64') for p in (self.device_runtime,self.hardware,self.control,self.policy,self.sdk)),SPA_PLUGIN_DIR=str(self.root/'spa'),ALSA_CONFIG_PATH=str(conf/'alsa.conf'))

    def image(self,name,path):
        super().image(name,path)
        if self.units[name]['role']!='graph':return
        expected=[self.hardware/'usr/lib64/libasound.so.2.0.0',self.hardware/'usr/lib64/spa-0.2/alsa/libspa-alsa.so',self.device_runtime/'usr/lib64/libudev.so.1.7.12']
        maps=Path('/proc')/str(self.units[name]['pid'])/'maps'
        self.wait(lambda:all(str(p) in maps.read_text() for p in expected),[name],'selected ALSA library images')
        self.record['units'][name]['device_libraries']={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in expected};self.save()

    def validate_case(self,case,raw):
        assert not raw,'hardware case unexpectedly used synthetic FIFO'
        return dict(host_capture_required=True,case=case,signal_frames=self.signal_frames(),gain=.125 if case=='half' else 1,muted=case=='muted')

    def retire(self):
        assert self.pending is None and all(u['state']=='retired' for u in self.record['units'].values())
        assert not self.observers and not self.extra_pins and all(t['state']=='retired' for t in self.record['targets'].values())
        for leaf,value in self.device_nodes.items():
            path=value['path'];assert node(path)==value['original'] and acl(path)==GRANT
            assert base64.b64encode(os.getxattr(path,'security.selinux')).decode()==self.record['devices'][leaf]['active_context']
            def restore():
                os.removexattr(path,ACL);os.setxattr(path,'security.selinux',value['context'])
                assert node(path)==value['original'] and acl(path) is None and os.getxattr(path,'security.selinux')==value['context']
            self.effect('device-test-restore',str(path),restore)
        self.access('after',False)
        stored=Path('/var/lib/selinux/targeted/active/modules/400/audio-device-test/cil');assert stored.read_bytes()==self.device_module
        self.effect('device-policy-remove','audio-device-test',lambda:run('semodule','-X','400','-r','audio-device-test'))
        self.record['devices_restored']=True;self.save();super().retire()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('sdk','policy','control','binary','source','hardware','runtime','output'):parser.add_argument(name,type=Path)
    parser.add_argument('--case',choices=('unity','half','muted'),required=True);args=parser.parse_args()
    if os.geteuid()!=0 or Path('/sys/class/dmi/id/product_name').read_text().strip()!='Aetheri audio lab VM' or Path('/sys/fs/selinux/enforce').read_text().strip()!='1':raise ValueError('separate enforcing root-only audio lab required')
    for field,value in (('vendor','0x8086'),('device','0x2668')):
        if (Path('/sys/class/sound/card0/device')/field).read_text().strip()!=value:raise ValueError('different virtual HDA controller')
    for check,path in ((prepare_audio.check,args.sdk),(prepare_audio_policy.check,args.policy),(prepare_audio_control.check,args.control),(prepare_audio_hda.check,args.hardware)):check(path)
    check_device_runtime(args.runtime)
    fixture=HardwareAudio(args.output,args.case)
    try:
        fixture.setup(args.sdk,args.policy,args.control,args.binary,args.source,args.hardware,args.runtime);fixture.case(args.case);fixture.retire()
        print(json.dumps(fixture.record))
    except BaseException as error:
        fixture.record.update(state='needs-inspection',error=repr(error));fixture.save();raise
    finally:fixture.bus.close()


if __name__=='__main__':main()
