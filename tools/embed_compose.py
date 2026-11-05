"""Embed exact reviewed Compose data and preserve its license; no runtime Python."""
import hashlib
import json
from pathlib import Path
import re
import sys


def checked(path, size, digest):
    p=Path(path)
    if p.is_symlink() or not p.is_file() or p.stat().st_size!=size or not 0<size<=1024*1024:
        raise ValueError("unexpected Compose input type/size")
    with p.open('rb') as stream:
        raw=stream.read(size+1)
    if len(raw)!=size or hashlib.sha256(raw).hexdigest()!=digest:
        raise ValueError("reviewed Compose input changed")
    return raw


def embed(destination, table, license_path, config):
    raw=checked(table,config["size"],config["sha256"])
    license_bytes=checked(license_path,config["license_size"],config["license_sha256"])
    text=raw.decode("utf-8")
    if b"\0" in raw or re.search(r'^\s*include(?:\s|\")',text,re.M):
        raise ValueError("Compose includes and hidden runtime file lookup are not allowed")
    p=Path(destination);p.parent.mkdir(parents=True,exist_ok=True)
    lines=['/* Generated reviewed standard data; ship compose-libX11-COPYING. */',
           '#include <stddef.h>','const unsigned char ae_compose_data[] = {']
    lines.extend('    '+','.join(str(b) for b in raw[i:i+24])+',' for i in range(0,len(raw),24))
    lines.extend(['};','const size_t ae_compose_data_size=sizeof(ae_compose_data);',''])
    p.with_name('compose-libX11-COPYING').write_bytes(license_bytes)
    p.write_text('\n'.join(lines),encoding='ascii')


if __name__=='__main__':
    config=json.loads((Path(__file__).resolve().parents[1]/'spec/graphics-inputs.json').read_text())["compose_data"]
    embed(*sys.argv[1:4],config)
