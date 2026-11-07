"""Independent AETHUIS1 metadata codec; never process or role admission."""
import re
import struct

import runtime_contracts as rc


def encode(value):
    rc.fields(value, 'boot pidfs_id invocation cgroup_id image unit cgroup label')
    boot, invocation = (rc.rawhex(value[k], 16) for k in ('boot', 'invocation'))
    rc.require(any(boot) and any(invocation), 'empty process epoch')
    numbers = [rc.integer(value[k], 2**64-1, 1) for k in ('pidfs_id', 'cgroup_id')]
    parts = []
    for name, maximum in (('unit', 256), ('cgroup', 1024), ('label', 512)):
        text = value[name]
        rc.require(type(text) is str, 'text type')
        try:
            raw = text.encode('ascii')
        except UnicodeError as error:
            raise rc.InvalidContract('non-ASCII process metadata') from error
        rc.require(0 < len(raw) < maximum and all(32 <= n < 127 for n in raw), 'bounded printable text')
        parts.append(struct.pack('>I', len(raw)) + raw)
    rc.require(re.fullmatch(r'[a-zA-Z0-9_.:@\\-]+\.service', value['unit']) is not None, 'service name')
    group = value['cgroup']
    rc.require(group.startswith('/') and all(p and p not in ('.', '..') for p in group[1:].split('/')), 'canonical cgroup')
    return (b'AETHUIS1' + boot + struct.pack('>Q', numbers[0]) + invocation +
            struct.pack('>Q', numbers[1]) + rc.image_record(value['image']) + b''.join(parts))


def decode(raw):
    rc.require(type(raw) is bytes and 192 <= len(raw) <= 2048 and raw[:8] == b'AETHUIS1', 'issuer frame')
    image = raw[56:168]
    rc.require(image[:8] == b'AETHIMG1', 'image tag')
    keys = 'inode size device_major device_minor uid gid mode birth_seconds birth_ns mtime_seconds mtime_ns ctime_seconds ctime_ns'.split()
    fields = struct.unpack('>QQIIIIIqIqIqI', image[8:80])
    value = dict(boot=raw[8:24].hex(), pidfs_id=int.from_bytes(raw[24:32], 'big'),
                 invocation=raw[32:48].hex(), cgroup_id=int.from_bytes(raw[48:56], 'big'),
                 image=dict(zip(keys, fields), digest=image[80:112].hex()))
    offset = 168
    for name, maximum in (('unit', 256), ('cgroup', 1024), ('label', 512)):
        rc.require(offset + 4 <= len(raw), 'text length prefix')
        size = int.from_bytes(raw[offset:offset+4], 'big'); offset += 4
        rc.require(0 < size < maximum and size <= len(raw)-offset, 'text extent')
        try:
            value[name] = raw[offset:offset+size].decode('ascii')
        except UnicodeError as error:
            raise rc.InvalidContract('non-ASCII process metadata') from error
        offset += size
    rc.require(offset == len(raw) and encode(value) == raw, 'canonical issuer')
    return value
