"""Independent development reference for the unit-selection digest v1."""
import hashlib
import struct
from pathlib import Path
import json


def number(value, width=8):
    return (value % (1 << (width*8))).to_bytes(width, 'big')


def text(value):
    raw=value.encode('utf-8')
    return number(len(raw),4)+raw


def root_bytes(inode, mount):
    return (number(mount)+number(inode)+struct.pack('>IIIII',8,1,0,0,0o40700)+
            number(0xef53)+bytes(range(1,9))+number(1000)+number(23,4))


def file_bytes(inode,mount,digest):
    return (number(mount)+number(inode)+struct.pack('>IIIIIIQ',8,1,0o100444,0,0,1,1234)+
            number(2000)+number(5,4)+number(2001)+number(6,4)+number(1001)+number(7,4)+bytes([digest])*32)


def fixture():
    """Golden synthetic observations only; no FD, file, policy or volume claim."""
    base=Path(__file__).resolve().parents[1]
    p=bytes.fromhex(json.loads((base/'spec/vectors/execution-process-v1.json').read_text())['profile_hex'])
    name='aetheri-app-'+p[168:184].hex()+'.service'
    exe='/unit/image/starter'
    edges=bytes([2])+bytes(13)+bytes([2])
    middle=bytes([4])+bytes(13)+bytes([4])
    unit=(b'AETHUNP1'+p[72:88]+number(3,4)+number(1,4)+text(name)+edges+
          text('/run/systemd/system')+root_bytes(201,101)+file_bytes(301,101,0x31)+number(1,4)+
          text('/usr/lib/systemd/system/service.d')+text('10-test.conf')+root_bytes(202,102)+file_bytes(401,102,0x32)+
          text(exe)+number(2,4)+text(exe)+text('--fixed')+number(1,4)+text('ONE=1')+
          text('system_u:system_r:test_starter_t:s0')+number(0x1234)+number(0)+struct.pack('>IIIII',1,0,0,0,0o77)+
          number(4,4)+text('system.slice')+middle+number(5,4)+text('-.slice')+bytes(15))
    image=(b'AETHIMG1'+struct.pack('>QQIIIII',501,4096,8,1,0,0,0o100555)+
           struct.pack('>qIqIqI',1002,3,2002,4,2003,5)+p[288:320])
    assert len(image)==112
    source=b'AETHUIM1'+text('/unit/image')+text('starter')+text('system_u:object_r:bin_t:s0')+root_bytes(203,103)+image
    selection=b'AETHUNT1'+hashlib.sha256(p).digest()+hashlib.sha256(unit).digest()+hashlib.sha256(source).digest()
    placement=b'AETHUPL1'+text('/run/systemd/system')+root_bytes(201,101)
    assert len(selection)==104
    return dict(profile_hex=p.hex(),unit_hex=unit.hex(),image_selection_hex=source.hex(),selection_hex=selection.hex(),
                unit_sha256=hashlib.sha256(unit).hexdigest(),selection_sha256=hashlib.sha256(selection).hexdigest(),
                placement_hex=placement.hex(),placement_sha256=hashlib.sha256(placement).hexdigest())
