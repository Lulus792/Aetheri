"""Fixed unsigned laptop development boot path, not a production boot policy."""
import re
import uuid

PROFILE='systemd-boot-v1'
MANAGER_SHA256='1dfce189a5a281bfa0bfc0e6b0ada38674893bfdb4f4d378b58a5c29d79a7821'
MANAGER_BYTES=139264
ESP_BYTES=258048*512
LOADER=b'default aetheri.conf\ntimeout 0\neditor no\nauto-entries no\nauto-firmware no\nrandom-seed-mode off\n'


def arguments(manifest):
    nonce=manifest.get('nonce');partition=manifest.get('root_partition',{}).get('guid')
    if (manifest.get('proof')!='usb-desktop' or manifest.get('usb_console_fallback') is not True
        or manifest.get('kernel_release')!='6.19.10-300.fc44.x86_64'
        or not isinstance(nonce,str) or not re.fullmatch('[0-9a-f]{32}',nonce)
        or not isinstance(partition,str) or str(uuid.UUID(partition))!=partition):
        raise ValueError('different reviewed laptop boot profile')
    return (f'console=tty0 panic=-1 rdinit=/init root=PARTUUID={partition} ro '
        'rootfstype=ext4 rootflags=noload rd.systemd.unit=aetheri-initrd.target '
        'systemd.unit=aetheri-desktop.target systemd.log_target=console '
        f'systemd.show_status=yes aetheri.boot_nonce={nonce}')


def entry(manifest,embedded):
    expected=arguments(manifest).encode()
    if embedded not in (expected,expected+b'\n'):
        raise ValueError('different embedded laptop arguments')
    return b'title Aetheri development desktop\nefi /EFI/Aetheri/system.efi\noptions '+expected+b'\n'
