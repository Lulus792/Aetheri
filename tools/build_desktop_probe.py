"""Build the separate graphical development image as an ordinary Linux user.

No host device, mount, package installation, carrier service or installed
authority is modified. This explicitly selected autostart account is not login
authentication, production app admission or the finished laptop installation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import build_data_probe as data_image
import desktop_payload
import test_boot_probe as boot

ROOT=Path(__file__).resolve().parents[1]
WLR=Path('/home/aetheri/deps/wlroots-session-release-02/prefix')
UI=Path('/home/aetheri/deps/ui-gles-release-04/prefix')
NEXA=Path('/home/aetheri/deps/nexaui-settings-release-05/prefix')
APPS=('aetheri-compositor','aetheri-desktop','aetheri-settings-nexaui','aetheri-files-nexaui','aetheri-terminal-nexaui','aetheri-terminal-pty-starter','aetheri-shell','aetheri-editor','aetheri-text-editor')


def build(base,packages,output):
    if sys.platform!='linux' or os.uname().machine!='x86_64' or os.geteuid()==0:
        raise ValueError('use the ordinary x86-64 Linux image builder')
    raw=(ROOT/'spec/desktop-image-inputs.json').read_bytes();manifest=json.loads(raw)
    desktop_payload.validate(manifest)
    selected=json.loads((ROOT/'spec/nexaui-inputs.json').read_text())
    if manifest['sdk']['nexaui_revision']!=selected['revision'] or manifest['sdk']['nexaui_version']!=selected['version']:
        raise ValueError('different reviewed NexaUI image inputs')

    def prepare(tree,nonce,volume):
        logs=output/'desktop-logs';logs.mkdir()
        def run(name,args,env=None):
            p=subprocess.run(args,capture_output=True,env=env)
            (logs/(name+'.stdout')).write_bytes(p.stdout);(logs/(name+'.stderr')).write_bytes(p.stderr)
            p.check_returncode();return p.stdout

        pins=json.loads((ROOT/'spec/boot-probe-inputs.json').read_text())
        baseline={('/usr'+p['source'] if p['source'].startswith('/lib64/') else p['source']):p['sha256'] for p in pins['files']}
        normalized=desktop_payload.normalize_base(tree,manifest,baseline)
        inventory=desktop_payload.install(tree,manifest)
        (output/'desktop-runtime.json').write_bytes(raw)
        (output/'desktop-copy.json').write_text(json.dumps(dict(inventory=inventory,normalized_flat_base=normalized),indent=2)+'\n')
        units=tree/'etc/systemd/system'
        def put(name,body,mode=0o644):
            p=tree/name;p.parent.mkdir(parents=True,exist_ok=True)
            if p.exists() or p.is_symlink():raise ValueError('new image path already exists: '+name)
            p.write_text(body);p.chmod(mode)
        def link(name,target):
            p=tree/name;p.parent.mkdir(parents=True,exist_ok=True);p.symlink_to(target)
        link('lib','usr/lib');link('sbin','usr/bin');link('usr/sbin','bin')
        link('usr/bin/modprobe','kmod')
        link('usr/lib/systemd/system/dbus.service','dbus-broker.service')
        link('usr/lib/systemd/system/dbus-org.freedesktop.login1.service','systemd-logind.service')
        link('usr/lib/systemd/user/dbus.service','dbus-broker.service')
        link('etc/systemd/user/sockets.target.wants/dbus.socket','/usr/lib/systemd/user/dbus.socket')
        put('etc/modules-load.d/aetheri-graphics.conf','bochs\n')
        run('depmod',['/usr/bin/depmod','-C','/dev/null','-b',str(tree),'-a','6.19.10-300.fc44.x86_64'])
        (tree/'etc/passwd').write_text((tree/'etc/passwd').read_text()+'dbus:x:81:81:D-Bus:/nonexistent:/nonexistent\n')
        (tree/'etc/group').write_text((tree/'etc/group').read_text()+'dbus:x:81:\ntty:x:5:\ndisk:x:6:\nlp:x:7:\ncdrom:x:11:\ndialout:x:18:\ntape:x:33:\nkvm:x:36:\naudio:x:63:\nsgx:x:995:\ninput:x:998:\nvideo:x:997:\nrender:x:996:\nclock:x:103:\nkmem:x:9:\n')
        # Credential setup for these fixed root-selected development units only;
        # this is not password authentication or a production login PAM stack.
        put('etc/pam.d/aetheri-graphics','auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so type=wayland class=user\n')
        put('etc/pam.d/systemd-user','auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_systemd.so class=manager\n')
        put('etc/pam.d/other','auth required pam_deny.so\naccount required pam_deny.so\npassword required pam_deny.so\nsession required pam_deny.so\n')
        put('etc/systemd/logind.conf.d/aetheri-development.conf','[Login]\nNAutoVTs=0\nReserveVT=0\nUserStopDelaySec=0\n')
        put('etc/systemd/journald.conf.d/aetheri-development.conf','[Journal]\nStorage=volatile\nRuntimeMaxUse=16M\nRuntimeMaxFileSize=8M\nReadKMsg=no\nForwardToConsole=yes\nForwardToSyslog=no\nForwardToKMsg=no\nForwardToWall=no\nMaxLevelConsole=debug\nTTYPath=/dev/ttyS0\n')
        console='StandardOutput=tty\nStandardError=tty\nTTYPath=/dev/console\n'
        for name in ('dbus-broker.service','systemd-logind.service','user-runtime-dir@.service','user@.service','systemd-user-sessions.service','modprobe@.service'):
            put('etc/systemd/system/'+name+'.d/aetheri-image.conf','[Unit]\nRequires=var.mount tmp.mount\nAfter=var.mount tmp.mount\n[Service]\nRestart=no\n'+console)
        # Keep the broker's vendor PrivateDevices sandbox. Its private /dev
        # intentionally cannot provide the guest console as stdout.
        broker=tree/'etc/systemd/system/dbus-broker.service.d/aetheri-image.conf'
        broker.write_text('[Unit]\nRequires=var.mount tmp.mount systemd-journald.socket systemd-journald.service\nAfter=var.mount tmp.mount systemd-journald.socket systemd-journald.service\n[Service]\nRestart=no\nStandardOutput=journal\nStandardError=journal\n')
        put('etc/systemd/system/systemd-journald.service.d/aetheri-image.conf','[Service]\nRestart=no\n')
        put('etc/systemd/system/dbus.socket.d/aetheri-image.conf','[Unit]\nDefaultDependencies=no\n')
        put('etc/systemd/user/dbus-broker.service.d/aetheri-image.conf','[Service]\nRestart=no\nStandardOutput=inherit\nStandardError=inherit\n')
        for name in ('var.mount','home.mount','tmp.mount'):
            put('etc/systemd/system/'+name+'.d/aetheri-image.conf','[Unit]\nDefaultDependencies=yes\n')
        # The earlier data-only profile triggers block devices. Replay all
        # existing devices here so libinput/logind can admit the PS/2 devices
        # which appeared before udev started. Do not bypass missing-input checks.
        trigger=tree/'etc/systemd/system/aetheri-udev-trigger.service'
        trigger.write_text(trigger.read_text().replace(' --subsystem-match=block',''))
        put('etc/systemd/system/aetheri-udev.service.d/aetheri-image.conf','[Service]\nFileDescriptorStoreMax=512\n')
        put('etc/systemd/system/aetheri-graphics-modules.service','[Unit]\nDefaultDependencies=no\nRequires=aetheri-data-check.service\nAfter=aetheri-data-check.service\nBefore=systemd-logind.service\n[Service]\nType=oneshot\nRemainAfterExit=yes\nExecStart=/usr/lib/systemd/systemd-modules-load\n'+console)
        put('etc/systemd/system/aetheri-graphics-vt.service','[Unit]\nDescription=Select the fixed development image console\nRequires=aetheri-graphics-modules.service systemd-logind.service\nAfter=aetheri-graphics-modules.service systemd-logind.service\nBefore=aetheri-graphics.service\n[Service]\nType=oneshot\nRemainAfterExit=yes\nNoNewPrivileges=yes\nCapabilityBoundingSet=CAP_SYS_TTY_CONFIG\nDevicePolicy=closed\nDeviceAllow=/dev/tty0 rw\nExecStart=/usr/bin/chvt 8\nStandardOutput=journal\nStandardError=journal\n')
        put('etc/systemd/system/aetheri-graphics.service','[Unit]\nDescription=Aetheri development desktop\nRequires=aetheri-data-user.service systemd-logind.service systemd-user-sessions.service aetheri-graphics-modules.service\nAfter=aetheri-data-user.service systemd-logind.service systemd-user-sessions.service aetheri-graphics-modules.service\n[Service]\nType=notify\nUser=aetheri\nGroup=aetheri\nPAMName=aetheri-graphics\nTTYPath=/dev/tty8\nStandardInput=tty\nStandardOutput=tty\nStandardError=tty\nTTYReset=yes\nUMask=0077\nNoNewPrivileges=yes\nCapabilityBoundingSet=\nAmbientCapabilities=\nRestart=no\nTimeoutStartSec=90\nTimeoutStopSec=infinity\nEnvironment=XDG_SESSION_TYPE=wayland XDG_SESSION_CLASS=user XDG_SEAT=seat0 XDG_VTNR=8\nEnvironment=LD_LIBRARY_PATH=/usr/lib/aetheri/runtime DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus\nWorkingDirectory=/home/aetheri/Documents\nExecStart=/usr/libexec/aetheri-desktop-owner\n')
        graphics=tree/'etc/systemd/system/aetheri-graphics.service'
        graphics.write_text(graphics.read_text().replace('aetheri-data-user.service','home.mount var.mount tmp.mount aetheri-graphics-vt.service').replace('StandardOutput=tty\nStandardError=tty\n','StandardOutput=file:/dev/ttyS0\nStandardError=file:/dev/ttyS0\n'))
        put('etc/systemd/system/aetheri-desktop.target','[Unit]\nDescription=Aetheri graphical development image\nDefaultDependencies=no\nRequires=home.mount var.mount tmp.mount dbus.socket dbus.service systemd-journald.socket systemd-journald-dev-log.socket systemd-journald.service aetheri-graphics.service\nAfter=aetheri-graphics.service\n')
        (units/'default.target').unlink();(units/'default.target').symlink_to('aetheri-desktop.target')
        (units/'aetheri-data.target').unlink();(units/'aetheri-data-finish.service').unlink();(units/'aetheri-data-user.service').unlink()
        application_build=output/'apps'
        args=['/usr/bin/cmake','-S',str(ROOT),'-B',str(application_build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_SKIP_RPATH=ON','-DCMAKE_PREFIX_PATH:STRING='+str(WLR)+';'+str(UI),'-DAETHERI_NEXAUI_PREFIX:PATH='+str(NEXA),'-DNexaUI_DIR:PATH='+str(NEXA/'lib/cmake/NexaUI'),'-DAETHERI_SETTINGS_TEST_FONT:FILEPATH='+str(UI/'share/aetheri-fonts/LiberationSans-Regular.ttf'),'-DAETHERI_TERMINAL_TEST_FONT:FILEPATH=/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf','-DAETHERI_TERMINAL_TEST_BOLD_FONT:FILEPATH=/usr/share/fonts/liberation-mono-fonts/LiberationMono-Bold.ttf']
        args+=['-DAETHERI_'+name+'=ON' for name in ('COMPOSITOR','DRM_SESSION','DESKTOP','SETTINGS','FILES','SHELL','TERMINAL','EDITOR','COMPOSE','GRAPHICAL_EDITOR')]
        run('configure',args);run('apps',['/usr/bin/cmake','--build',str(application_build),'-j2','--target',*APPS])
        app_root=tree/'usr/libexec/aetheri';app_root.mkdir()
        for name in APPS:
            shutil.copyfile(application_build/name,app_root/name);(app_root/name).chmod(0o755)
        for source,name in [('LICENSE','LICENSE'),('src/text/UNICODE-LICENSE.txt','UNICODE-LICENSE.txt'),('src/settings/NEXAUI-NOTICE.txt','NEXAUI-NOTICE.txt')]:
            put('usr/share/licenses/aetheri/'+name,(ROOT/source).read_text())
        put('usr/share/licenses/aetheri/COMPOSE-LICENSE.txt',Path('/usr/share/doc/libX11-common/COPYING').read_text())
        flags=['/usr/bin/gcc','-std=c17','-D_GNU_SOURCE','-O2','-g0','-Wall','-Wextra','-Wpedantic','-Wconversion','-Wshadow','-Werror','-fanalyzer',str(ROOT/'src/boot/desktop.c'),'-lsystemd','-o',str(tree/'usr/libexec/aetheri-desktop-owner')]
        run('owner',flags)
        return dict(proof='desktop-image',target='aetheri-desktop.target',desktop_inputs_sha256=hashlib.sha256(raw).hexdigest(),desktop_builder_sha256=boot.sha(Path(__file__)),desktop_owner_sha256=boot.sha(ROOT/'src/boot/desktop.c'),desktop_runtime=inventory,applications={n:boot.sha(app_root/n) for n in APPS})

    data_image.build(base,packages,output,prepare=prepare,geometry='desktop-v1')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--boot-image',type=Path,required=True);p.add_argument('--packages',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();build(a.boot_image.resolve(),a.packages.resolve(),a.output.absolute())
