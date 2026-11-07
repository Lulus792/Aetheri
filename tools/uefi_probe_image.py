"""Read-only independent checks for the fixed GPT/FAT32/AMD64 UKI profile.

No filesystem mount, native PE loader, external program or write is involved.
This verifies a development artifact's structure/integrity, not publisher trust.
"""
import hashlib
import json
from pathlib import Path
import stat
import struct
import uuid
import zlib

DISK_BYTES = 128 * 1024**2
ROOT_DISK_BYTES = 512 * 1024**2
ESP_TYPE = uuid.UUID("c12a7328-f81f-11d2-ba4b-00a0c93ec93b")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def pe_sections(data):
    require(len(data) >= 64 and data[:2] == b"MZ", "invalid DOS header")
    pe = struct.unpack_from("<I", data, 60)[0]
    require(64 <= pe <= len(data)-24 and data[pe:pe+4] == b"PE\0\0", "invalid PE header")
    machine, count = struct.unpack_from("<HH", data, pe+4)
    optional = struct.unpack_from("<H", data, pe+20)[0]
    start = pe+24
    require(machine == 0x8664 and 0 < count <= 96 and optional >= 112, "wrong PE target")
    table = start+optional
    require(table+count*40 <= len(data), "truncated section table")
    require(struct.unpack_from("<H", data, start)[0] == 0x20b and struct.unpack_from("<H", data, start+68)[0] == 10, "not an AMD64 EFI application")
    image_size, headers_size = struct.unpack_from("<II", data, start+56)
    require(table+count*40 <= headers_size <= len(data), "invalid PE header size")
    raw_ranges, memory_ranges, result = [], [], {}
    for i in range(count):
        at = table+i*40
        name = data[at:at+8].rstrip(b"\0").decode("ascii")
        size, address, raw_size, raw = struct.unpack_from("<IIII", data, at+8)
        require(name and name not in result, "duplicate/empty PE section")
        require(raw_size == 0 or (headers_size <= raw <= len(data) and raw_size <= len(data)-raw), "PE raw section outside image")
        extent = max(size, raw_size)
        require(address >= headers_size and extent <= image_size-address, "PE memory section outside image")
        if raw_size:
            raw_ranges.append((raw, raw+raw_size))
        if extent:
            memory_ranges.append((address, address+extent))
        if name in (".linux", ".initrd", ".cmdline", ".osrel", ".uname"):
            require(0 < size <= raw_size, "invalid UKI resource size")
        result[name] = data[raw:raw+min(size, raw_size)]
    for ranges in (raw_ranges, memory_ranges):
        ranges.sort()
        require(all(a[1] <= b[0] for a, b in zip(ranges, ranges[1:])), "overlapping PE sections")
    return result


def esp_extent(data, manifest):
    root_profile = "root_partition" in manifest
    require(len(data) == (ROOT_DISK_BYTES if root_profile else DISK_BYTES) and data[510:512] == b"\x55\xaa", "invalid image/MBR")
    last = len(data)//512-1
    require(data[450] == 0xee and struct.unpack_from("<II", data, 454) == (1, last), "invalid protective MBR")
    require(not any(data[462:510]), "unexpected hybrid MBR")
    entry_sets = []
    for current, other, array in ((1, last, 2), (last, 1, last-32)):
        offset = current*512
        h = bytearray(data[offset:offset+92])
        require(h[:8] == b"EFI PART" and struct.unpack_from("<II", h, 8) == (0x10000, 92), "invalid GPT header")
        expected = struct.unpack_from("<I", h, 16)[0]
        h[16:20] = b"\0"*4
        require(zlib.crc32(h) == expected, "GPT header checksum")
        this, backup, first, final = struct.unpack_from("<QQQQ", h, 24)
        require((this, backup, first, final) == (current, other, 2048, last-33), "unexpected GPT geometry")
        require(uuid.UUID(bytes_le=bytes(h[56:72])) == uuid.UUID(manifest["disk_guid"]), "wrong disk GUID")
        at, count, size, checksum = struct.unpack_from("<QIII", h, 72)
        require((at, count, size) == (array, 128, 128), "unexpected GPT array")
        entries = data[at*512:at*512+count*size]
        require(zlib.crc32(entries) == checksum and not any(entries[256 if root_profile else 128:]), "GPT partition array checksum/profile")
        entry_sets.append(entries)
    require(entry_sets[0] == entry_sets[1], "GPT copies disagree")
    entry = bytes(entry_sets[0][:128])
    require(uuid.UUID(bytes_le=entry[:16]) == ESP_TYPE and uuid.UUID(bytes_le=entry[16:32]) == uuid.UUID(manifest["esp_guid"]), "wrong ESP identity")
    start, end, attributes = struct.unpack_from("<QQQ", entry, 32)
    require((start, end-start+1, attributes) == (2048, 258048, 0), "unexpected ESP extent")
    require((manifest["esp_start"], manifest["esp_sectors"]) == (start, end-start+1), "ESP manifest mismatch")
    if root_profile:
        root = bytes(entry_sets[0][128:256])
        expected = manifest["root_partition"]
        require(uuid.UUID(bytes_le=root[:16]) == uuid.UUID("4f68bce3-e8cd-4db1-96e7-fbcaf984b709") and uuid.UUID(bytes_le=root[16:32]) == uuid.UUID(expected["guid"]), "wrong root partition identity")
        require(struct.unpack_from("<QQQ", root, 32) == (262144, 786431, 1 << 60), "unexpected root extent/flags")
        require((expected["start"], expected["sectors"]) == (262144, 524288), "root extent manifest mismatch")
        at = expected["start"]*512
        superblock = data[at+1024:at+2048]
        require(superblock[56:58] == b"\x53\xef" and uuid.UUID(bytes=bytes(superblock[104:120])) == uuid.UUID(expected["filesystem_uuid"]), "wrong root filesystem")
        require(digest(data[at:at+expected["sectors"]*512]) == expected["sha256"], "changed root filesystem")
    return data[start*512:(end+1)*512]


def boot_file(volume):
    require(len(volume) == 258048*512 and volume[510:512] == b"\x55\xaa", "invalid FAT volume")
    sector = struct.unpack_from("<H", volume, 11)[0]
    cluster_sectors = volume[13]
    reserved = struct.unpack_from("<H", volume, 14)[0]
    fats = volume[16]
    total = struct.unpack_from("<I", volume, 32)[0]
    fat_sectors = struct.unpack_from("<I", volume, 36)[0]
    root = struct.unpack_from("<I", volume, 44)[0]
    require(sector == 512 and cluster_sectors in (1, 2, 4, 8, 16, 32, 64, 128) and reserved >= 1 and fats == 2, "unsupported FAT geometry")
    require(volume[17:19] == b"\0\0" and volume[22:24] == b"\0\0" and total == len(volume)//512 and fat_sectors > 0, "not the fixed FAT32 profile")
    base = reserved+fats*fat_sectors
    count = (total-base)//cluster_sectors
    require(65525 <= count < 0x0ffffff5 and (count+2)*4 <= fat_sectors*512, "invalid FAT32 cluster count")
    fat = volume[reserved*512:(reserved+fat_sectors)*512]
    require(fat == volume[(reserved+fat_sectors)*512:base*512], "FAT copies disagree")
    cluster_bytes = cluster_sectors*512

    def chain(first, limit):
        parts, seen, current = [], set(), first
        while True:
            require(2 <= current < count+2 and current not in seen, "invalid/cyclic FAT chain")
            seen.add(current)
            require(len(seen)*cluster_bytes <= limit, "oversized FAT chain")
            begin = (base+(current-2)*cluster_sectors)*512
            parts.append(volume[begin:begin+cluster_bytes])
            current = struct.unpack_from("<I", fat, current*4)[0] & 0x0fffffff
            if current >= 0x0ffffff8:
                return b"".join(parts)

    def directory(first, expected, top=False):
        raw = chain(first, 1024**2)
        entries = {}
        for at in range(0, len(raw), 32):
            e = raw[at:at+32]
            if e[0] == 0:
                break
            require(e[0] != 0xe5 and e[11] != 0x0f, "unexpected deleted/long-name entry")
            name = e[:11]
            if top and e[11] == 8 and name == b"AETHERI    ":
                continue
            require(name not in entries, "duplicate FAT name")
            entries[name] = (e[11], (struct.unpack_from("<H", e, 20)[0] << 16) | struct.unpack_from("<H", e, 26)[0], struct.unpack_from("<I", e, 28)[0])
        if not top:
            require(b".          " in entries and b"..         " in entries, "missing parent entries")
            require(entries.pop(b".          ")[0] == 16 and entries.pop(b"..         ")[0] == 16, "invalid parent entries")
        require(set(entries) == {expected}, "unexpected FAT directory contents")
        return entries[expected]

    attr, first, size = directory(root, b"EFI        ", True)
    require(attr == 16 and size == 0, "EFI is not a directory")
    attr, first, size = directory(first, b"BOOT       ")
    require(attr == 16 and size == 0, "BOOT is not a directory")
    attr, first, size = directory(first, b"BOOTX64 EFI")
    require(attr == 32 and 0 < size <= 64*1024**2, "invalid fallback EFI file")
    raw = chain(first, ((size+cluster_bytes-1)//cluster_bytes)*cluster_bytes)
    require(len(raw) == ((size+cluster_bytes-1)//cluster_bytes)*cluster_bytes, "wrong fallback allocation size")
    return raw[:size]


def inspect(directory):
    manifest = json.loads((directory/"manifest.json").read_text())
    expected_size = ROOT_DISK_BYTES if "root_partition" in manifest else DISK_BYTES
    require(manifest["format"] == 1 and manifest["disk_bytes"] == expected_size, "unsupported media manifest")
    require(set(manifest["outputs"]) == {"boot.raw", "BOOTX64.EFI"}, "unexpected media outputs")
    for name, expected in manifest["outputs"].items():
        path = directory/name
        require(not path.is_symlink() and stat.S_ISREG(path.stat().st_mode), "media input must be a regular file")
        require(path.stat().st_size <= expected_size, "oversized media input")
        require(digest(path.read_bytes()) == expected, "changed media input: "+name)
    data = (directory/"boot.raw").read_bytes()
    efi = boot_file(esp_extent(data, manifest))
    require(digest(efi) == manifest["outputs"]["BOOTX64.EFI"], "fallback file mismatch")
    sections = pe_sections(efi)
    require(set(manifest["sections"]) == {".linux", ".initrd", ".cmdline", ".osrel", ".uname"}, "unexpected UKI resources")
    for name, expected in manifest["sections"].items():
        require(name in sections and digest(sections[name]) == expected, "changed UKI resource: "+name)
    return manifest
