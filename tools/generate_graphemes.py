"""Regenerate owned Unicode-18 range/vector views from exact reviewed data only."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
GCB=['Other','CR','LF','Control','Extend','ZWJ','Regional_Indicator','Prepend','SpacingMark','L','V','T','LV','LVT']
NAMES=['OTHER','CR','LF','CONTROL','EXTEND','ZWJ','RI','PREPEND','SPACINGMARK','L','V','T','LV','LVT']
INCB=['None','Linker','Extend','Consonant']


def generate(cache):
    manifest=json.loads((ROOT/'spec/unicode-inputs.json').read_text(encoding='utf-8'))
    contents={}
    for r in manifest['inputs']:
        p=cache/r['name']
        if p.is_symlink() or not p.is_file() or p.stat().st_size!=r['size']:raise ValueError('input type/size')
        raw=p.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=r['digest']:raise ValueError('input hash')
        contents[r['name'].removeprefix('unicode-18.0.0-')]=raw
    channels=[[],[],[]]
    for filename,index in (('GraphemeBreakProperty.txt',0),('DerivedCoreProperties.txt',1),('emoji-data.txt',2)):
        for line in contents[filename].decode('utf-8').splitlines():
            fields=[f.strip() for f in line.split('#',1)[0].split(';')]
            if len(fields)<2:continue
            if index==1 and fields[1]!='InCB':continue
            if index==2 and fields[1]!='Extended_Pictographic':continue
            points=fields[0].split('..');first,last=int(points[0],16),int(points[-1],16)
            if not 0<=first<=last<=0x10ffff:raise ValueError('range')
            value=GCB.index(fields[1]) if index==0 else INCB.index(fields[2])<<4 if index==1 else 64
            channels[index].append((first,last,value))
    for channel in channels:
        channel.sort()
        for a,b in zip(channel,channel[1:]):
            if a[1]>=b[0]:raise ValueError('overlapping property ranges')
    points=sorted({p for channel in channels for a,b,_ in channel for p in (a,b+1)})
    ranges=[];indices=[0,0,0]
    for lo,hi in zip(points,points[1:]):
        value=0
        for j,channel in enumerate(channels):
            while indices[j]<len(channel) and channel[indices[j]][1]<lo:indices[j]+=1
            if indices[j]<len(channel) and channel[indices[j]][0]<=lo:value|=channel[indices[j]][2]
        if not value:continue
        if ranges and ranges[-1][1]+1==lo and ranges[-1][2]==value:ranges[-1]=(ranges[-1][0],hi-1,value)
        else:ranges.append((lo,hi-1,value))
    comment='/* Generated Unicode 18.0.0 data; Unicode-3.0. See src/text/UNICODE-LICENSE.txt. */\n'
    result=comment+'enum { '+', '.join('GB_'+name+'='+str(i) for i,name in enumerate(NAMES))+' };\n'
    result+='enum { INCB_NONE=0, INCB_LINKER=1, INCB_EXTEND=2, INCB_CONSONANT=3 };\n'
    result+='static const struct { uint32_t first,last; unsigned char property; } ae_grapheme_ranges[] = {\n'
    result+=''.join(f'    {{0x{a:x},0x{b:x},{v}}},\n' for a,b,v in ranges)+'};\n'
    vectors=[]
    for line in contents['GraphemeBreakTest.txt'].decode('utf-8').splitlines():
        parts=line.split('#',1)[0].split()
        if not parts:continue
        raw=bytearray();offsets=[]
        for token in parts:
            if token=='÷':offsets.append(len(raw))
            elif token!='×':raw.extend(chr(int(token,16)).encode('utf-8'))
        if not offsets or offsets[0]!=0 or offsets[-1]!=len(raw) or len(offsets)>128:raise ValueError('vector')
        vectors.append((bytes(raw),offsets))
    tests=comment+'static const struct { const char *text; size_t size,count; size_t offsets[128]; } grapheme_vectors[] = {\n'
    tests+=''.join('    {"'+''.join('\\x%02x'%b for b in raw)+'",'+str(len(raw))+','+str(len(offsets))+',{'+','.join(map(str,offsets))+'}},\n' for raw,offsets in vectors)+'};\n'
    return {ROOT/'src/text/grapheme_data.h':result.encode(),ROOT/'tests/grapheme_vectors.h':tests.encode(),ROOT/'src/text/UNICODE-LICENSE.txt':contents['LICENSE.txt']},len(ranges),len(vectors)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('cache',type=Path);p.add_argument('--check',action='store_true');args=p.parse_args()
    files,ranges,vectors=generate(args.cache)
    for path,raw in files.items():
        if args.check:
            if path.read_bytes()!=raw:raise ValueError('generated file differs: '+str(path))
        else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
    print(f'Unicode 18.0.0: {ranges} combined ranges; {vectors} complete official test cases; full license retained.')


if __name__=='__main__':main()
