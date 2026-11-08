"""Explicitly authorized Intenso laptop-image flashes, not a general disk utility.

--inspect only queries metadata. --write needs an independently prepared,
hash-bound intent and elevation. Every attempt is fresh-only; no automatic replay.
"""
import argparse
import ctypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
MEDIA_BYTES=31457280000
APPROVED_IMAGE='06aad2b908ce008d37744c5171d8d10444d81c31bd7692c86f6dc01754337d20'
# Closed until the new complete image has independent VM/filesystem acceptance.
APPROVED_REPLACEMENT_IMAGE=None
BLOCK=1024**2
SERIAL='212300100050'
BRIDGE=r'USBSTOR\DISK&VEN_INTENSO&PROD_SPEED_LINE&REV_PMAP\22010500000313&0:'


def digest(path):
    with Path(path).open('rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()


def owned(path):
    path=Path(path)
    if path.is_symlink() or not path.resolve().is_relative_to(ROOT/'local'):raise ValueError('use owned local inputs')
    return path.resolve()


def admit_disk(value):
    if not isinstance(value,dict):raise ValueError('invalid storage observation')
    if any(not isinstance(value.get(name),str) for name in ('FriendlyName','SerialNumber','UniqueId','BusType')):raise ValueError('invalid device identity fields')
    if type(value.get('Number')) is not int or not 0<=value['Number']<=1024:raise ValueError('invalid disk number')
    if value.get('FriendlyName','').strip()!='Intenso Speed Line' or value.get('SerialNumber','').strip()!=SERIAL or not value.get('UniqueId','').casefold().startswith(BRIDGE.casefold()):raise ValueError('different authorized asset')
    if value.get('BusType')!='USB' or type(value.get('Size')) is not int or value['Size']!=MEDIA_BYTES:raise ValueError('different bus/geometry')
    for name in ('IsBoot','IsSystem','IsOffline','IsReadOnly'):
        if value.get(name) is not False:raise ValueError('unsafe/unavailable disk: '+name)
    if value.get('LogicalSectorSize')!=512 or type(value.get('PhysicalSectorSize')) is not int:raise ValueError('different sector geometry')
    sector=value['PhysicalSectorSize']
    if not 512<=sector<=65536 or sector&(sector-1):raise ValueError('unsupported physical alignment')
    return value


def disks():
    command="Get-Disk | Select-Object Number,FriendlyName,SerialNumber,UniqueId,Size,LogicalSectorSize,PhysicalSectorSize,IsBoot,IsSystem,IsOffline,IsReadOnly,@{Name='BusType';Expression={$_.BusType.ToString()}} | ConvertTo-Json -Compress"
    return ps(command)


def ps(command):
    executable=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    result=subprocess.run([str(executable),'-NoProfile','-NonInteractive','-Command',command],capture_output=True,timeout=30,creationflags=0x08000000)
    if result.returncode:raise RuntimeError('storage query failed: '+result.stderr.decode(errors='replace')[:512])
    value=json.loads(result.stdout.decode('utf-8-sig'))
    return value if isinstance(value,list) else [value]


def selected_disk():
    found=[value for value in disks() if isinstance(value,dict) and isinstance(value.get('SerialNumber'),str) and value['SerialNumber'].strip()==SERIAL]
    if len(found)!=1:raise ValueError('authorized USB is absent/ambiguous/unsafe')
    return admit_disk(found[0])


def partition(number):
    if type(number) is not int or not 0<=number<=1024:raise ValueError('invalid selected number')
    rows=ps('Get-Partition -DiskNumber '+str(number)+' | Select-Object DiskNumber,PartitionNumber,DriveLetter,Offset,Size | ConvertTo-Json -Compress')
    if len(rows)!=1:raise ValueError('first flash requires the inspected single partition')
    value=rows[0]
    if value.get('DiskNumber')!=number or value.get('PartitionNumber')!=1 or value.get('DriveLetter')!='G':raise ValueError('different original G: partition')
    if type(value.get('Offset')) is not int or type(value.get('Size')) is not int or value['Offset']<=0 or value['Size']<=0 or value['Offset']+value['Size']>MEDIA_BYTES:raise ValueError('invalid original partition extent')
    return value


def admit_replacement_partitions(rows,number):
    """Only the exact previous Aetheri layout; never a drive-letter guess."""
    expected=(
        (1048576,132120576,'95faca9a-919a-5721-a142-7fd2b3480321','c12a7328-f81f-11d2-ba4b-00a0c93ec93b'),
        (134217728,536870912,'8112b47d-a0fa-53ba-9676-46434744a761','4f68bce3-e8cd-4db1-96e7-fbcaf984b709'),
        (671088640,30785142784,'fa6f5b93-6372-5028-a36a-304736e93a32','0fc63daf-8483-4772-8e79-3d69d8477de4'))
    if type(number) is not int or not 0<=number<=1024 or not isinstance(rows,list) or len(rows)!=3:raise ValueError('different previous Aetheri partition count')
    result=[]
    for index,(row,(start,size,guid,kind)) in enumerate(zip(rows,expected),1):
        if (not isinstance(row,dict) or type(row.get('DiskNumber')) is not int or row['DiskNumber']!=number
            or type(row.get('PartitionNumber')) is not int or row['PartitionNumber']!=index
            or type(row.get('Offset')) is not int or row['Offset']!=start or type(row.get('Size')) is not int or row['Size']!=size
            or row.get('IsBoot') is not False or row.get('IsSystem') is not False
            or row.get('DriveLetter') not in (None,'','\0')
            or not isinstance(row.get('Guid'),str) or row['Guid'].lower()!='{'+guid+'}'
            or not isinstance(row.get('GptType'),str) or row['GptType'].lower()!='{'+kind+'}'):
            raise ValueError('different previous Aetheri partition binding')
        paths=row.get('AccessPaths')
        if paths is None:paths=[]
        if not isinstance(paths,list):raise ValueError('invalid volume/access path observation')
        volume='\\\\?\\Volume{'+guid+'}\\'
        if paths!=([volume] if index==1 else []):raise ValueError('unexpected mounted volume/access path')
        result.append(dict(row,AccessPaths=paths))
    return result


def replacement_partitions(number):
    if type(number) is not int or not 0<=number<=1024:raise ValueError('invalid disk number')
    rows=ps('Get-Partition -DiskNumber '+str(number)+' | Sort-Object PartitionNumber | Select-Object DiskNumber,PartitionNumber,DriveLetter,Offset,Size,Guid,GptType,IsBoot,IsSystem,AccessPaths | ConvertTo-Json -Depth 3')
    return admit_replacement_partitions(rows,number)


def approved_intent(intent):
    kind=intent.get('format');expected=intent.get('source_sha256')
    if type(kind) is not int:raise ValueError('invalid flash intent version')
    if kind==1 and expected==APPROVED_IMAGE:return False
    if (kind==2 and isinstance(APPROVED_REPLACEMENT_IMAGE,str)
        and re.fullmatch('[0-9a-f]{64}',APPROVED_REPLACEMENT_IMAGE)
        and expected==APPROVED_REPLACEMENT_IMAGE
        and intent.get('replaces_source_sha256')==APPROVED_IMAGE
        and intent.get('authorization')=='user-2026-11-08-reflash-after-tests'):
        return True
    raise ValueError('different reviewed flash image/authorization')


def descriptor(raw):
    if len(raw)<36:raise ValueError('short device descriptor')
    version,size=struct.unpack_from('<II',raw)
    if not 36<=version<=4096 or not 36<=size<=len(raw) or raw[8]!=0 or raw[10]!=1 or struct.unpack_from('<I',raw,28)[0]!=7:raise ValueError('different device descriptor profile')
    def text_at(position):
        offset=struct.unpack_from('<I',raw,position)[0]
        if offset==0:return ''
        if not 36<=offset<size:raise ValueError('invalid descriptor string offset')
        end=raw.find(b'\0',offset,min(size,offset+129))
        if end<0:raise ValueError('unbounded descriptor string')
        return raw[offset:end].decode('ascii').strip()
    result=dict(vendor=text_at(12),product=text_at(16),revision=text_at(20),serial=text_at(24),bus=7,removable=True)
    model=(result['vendor']+' '+result['product']).casefold()
    if 'intenso' not in model or 'speed line' not in model or result['serial']!=SERIAL:raise ValueError('different held-device identity')
    return result


def device_number(raw,expected):
    if len(raw)!=12:raise ValueError('short device-number observation')
    kind,number,part=struct.unpack('<III',raw)
    if kind!=7 or number!=expected or part not in (0,0xffffffff):raise ValueError('different held physical device')
    return number


def extent(raw,part):
    if len(raw)!=32 or struct.unpack_from('<I',raw)[0]!=1:raise ValueError('volume is not one physical extent')
    number,start,length=struct.unpack_from('<I4xqq',raw,8)
    if (number,start,length)!=(part['DiskNumber'],part['Offset'],part['Size']):raise ValueError('different held volume/disk binding')
    return dict(number=number,start=start,length=length)


def transfer(get_block,target,size,expected,emit,block=BLOCK):
    """Write AND read every byte, including zeros. Never retry short/failed I/O."""
    if type(size) is not int or size<=0 or type(block) is not int or block<=0 or size%block or not re.fullmatch('[0-9a-f]{64}',expected):raise ValueError('invalid complete transfer profile')
    emit('write_dispatched',0);target.seek(0);written=hashlib.sha256()
    for at in range(0,size,block):
        data=get_block(at,block)
        if len(data)!=block:raise ValueError('short source block')
        if target.write(data)!=block:raise OSError('short physical write; no retry')
        written.update(data)
        if (at+block)%(256*1024**2)==0:emit('write_progress',at+block)
    if written.hexdigest()!=expected:raise ValueError('source changed during complete write')
    target.flush();emit('written_flushed',size)
    emit('readback_dispatched',0);target.seek(0);readback=hashlib.sha256()
    for at in range(0,size,block):
        data=target.read(block)
        if len(data)!=block:raise OSError('short physical read; no retry')
        readback.update(data)
        if (at+block)%(256*1024**2)==0:emit('readback_progress',at+block)
    if readback.hexdigest()!=expected:raise ValueError('complete physical readback differs')
    return readback.hexdigest()


class Journal:
    def __init__(self,path):self.file=Path(path).open('xb');self.sequence=0;self.previous='0'*64
    def emit(self,phase,bytes=0,**fields):
        if {'sequence','previous','time','record_sha256'}&fields.keys():raise ValueError('reserved journal field')
        record=dict(sequence=self.sequence,previous=self.previous,phase=phase,bytes=bytes,time=time.time(),**fields)
        body=json.dumps(record,sort_keys=True,separators=(',',':')).encode();record['record_sha256']=hashlib.sha256(body).hexdigest()
        raw=json.dumps(record,sort_keys=True).encode()+b'\n';self.file.write(raw);self.file.flush();os.fsync(self.file.fileno())
        self.previous=record['record_sha256'];self.sequence+=1
    def close(self):self.file.close()


def read_journal(path):
    with Path(path).open('rb') as source:raw=source.read(4*1024**2+1)
    if len(raw)>4*1024**2:raise ValueError('oversized flash journal')
    lines=raw.split(b'\n');partial=bool(lines[-1]);records=[];previous='0'*64
    for line in lines[:-1]:
        if len(line)>65536:raise ValueError('oversized flash record')
        record=json.loads(line);claimed=record.pop('record_sha256')
        if type(record.get('sequence')) is not int or record['sequence']!=len(records) or record.get('previous')!=previous:raise ValueError('different flash journal sequence')
        if hashlib.sha256(json.dumps(record,sort_keys=True,separators=(',',':')).encode()).hexdigest()!=claimed:raise ValueError('changed flash journal record')
        record['record_sha256']=claimed;records.append(record);previous=claimed
    return records,partial


class Windows:
    def __init__(self):
        if os.name!='nt':raise ValueError('Windows only')
        from ctypes import wintypes as w
        self.k=ctypes.WinDLL('kernel32',use_last_error=True)
        def function(name,args,result):
            value=getattr(self.k,name);value.argtypes=args;value.restype=result;return value
        self.create=function('CreateFileW',[w.LPCWSTR,w.DWORD,w.DWORD,ctypes.c_void_p,w.DWORD,w.DWORD,w.HANDLE],w.HANDLE)
        self.close=function('CloseHandle',[w.HANDLE],w.BOOL)
        self.control=function('DeviceIoControl',[w.HANDLE,w.DWORD,ctypes.c_void_p,w.DWORD,ctypes.c_void_p,w.DWORD,ctypes.POINTER(w.DWORD),ctypes.c_void_p],w.BOOL)
        self.move=function('SetFilePointerEx',[w.HANDLE,ctypes.c_longlong,ctypes.POINTER(ctypes.c_longlong),w.DWORD],w.BOOL)
        self.write=function('WriteFile',[w.HANDLE,ctypes.c_void_p,w.DWORD,ctypes.POINTER(w.DWORD),ctypes.c_void_p],w.BOOL)
        self.read=function('ReadFile',[w.HANDLE,ctypes.c_void_p,w.DWORD,ctypes.POINTER(w.DWORD),ctypes.c_void_p],w.BOOL)
        self.flush=function('FlushFileBuffers',[w.HANDLE],w.BOOL)
        self.allocate=function('VirtualAlloc',[ctypes.c_void_p,ctypes.c_size_t,w.DWORD,w.DWORD],ctypes.c_void_p)
        self.free=function('VirtualFree',[ctypes.c_void_p,ctypes.c_size_t,w.DWORD],w.BOOL)
    def check(self,value):
        if not value:raise ctypes.WinError(ctypes.get_last_error())
        return value
    def open(self,path,access,share=3,flags=0x80,disposition=3):
        value=self.create(str(path),access,share,None,disposition,flags,None)
        if value==ctypes.c_void_p(-1).value:raise ctypes.WinError(ctypes.get_last_error())
        return value
    def ioctl(self,handle,code,data=b'',size=4096):
        from ctypes import wintypes
        source=ctypes.create_string_buffer(data) if data else None;output=ctypes.create_string_buffer(size) if size else None;used=wintypes.DWORD()
        self.check(self.control(handle,code,source,len(data),output,size,ctypes.byref(used),None))
        if used.value>size:raise ValueError('invalid control result size')
        return output.raw[:used.value] if output else b''
    def observe(self,handle,number,full=False):
        device_number(self.ioctl(handle,0x2d1080,size=12),number)
        identity=descriptor(self.ioctl(handle,0x2d1400,b'\0'*12))
        geometry=self.ioctl(handle,0x700a0)
        if len(geometry)<32 or struct.unpack_from('<I',geometry,20)[0]!=512 or struct.unpack_from('<q',geometry,24)[0]!=MEDIA_BYTES:raise ValueError('different held media geometry')
        if full and self.ioctl(handle,0x7405c,size=8)!=struct.pack('<q',MEDIA_BYTES):raise ValueError('different held device length')
        return dict(number=number,identity=identity,size=MEDIA_BYTES,logical_sector=512)


class NativeIO:
    def __init__(self,api,handle):
        self.api=api;self.handle=handle;self.buffer=api.allocate(None,BLOCK,0x3000,4)
        if not self.buffer:raise ctypes.WinError(ctypes.get_last_error())
        if self.buffer%65536:api.free(self.buffer,0,0x8000);raise ValueError('unaligned native buffer')
    def seek(self,at):
        value=ctypes.c_longlong();self.api.check(self.api.move(self.handle,at,ctypes.byref(value),0))
        if value.value!=at:raise ValueError('physical position differs')
    def write(self,data):
        from ctypes import wintypes
        if not 0<len(data)<=BLOCK or len(data)%512:raise ValueError('invalid aligned block')
        ctypes.memmove(self.buffer,data,len(data));used=wintypes.DWORD();self.api.check(self.api.write(self.handle,self.buffer,len(data),ctypes.byref(used),None));return used.value
    def read(self,size):
        from ctypes import wintypes
        if type(size) is not int or not 0<size<=BLOCK or size%512:raise ValueError('invalid aligned read')
        used=wintypes.DWORD();self.api.check(self.api.read(self.handle,self.buffer,size,ctypes.byref(used),None))
        if used.value>size:raise ValueError('oversized native read result')
        return ctypes.string_at(self.buffer,used.value)
    def flush(self):self.api.check(self.api.flush(self.handle))
    def close(self):
        if self.buffer:self.api.check(self.api.free(self.buffer,0,0x8000));self.buffer=None


def load_io(expected):
    path=ROOT/'tools/image_io.py'
    if digest(path)!=expected:raise ValueError('image reader differs from reviewed intent')
    spec=importlib.util.spec_from_file_location('aetheri_flash_image_io',path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


def run(intent_path,intent_sha):
    if os.name!='nt' or not ctypes.windll.shell32.IsUserAnAdmin():raise PermissionError('Windows administrator access is required; no device write attempted')
    intent_path=owned(intent_path)
    if digest(intent_path)!=intent_sha:raise ValueError('changed prepared intent')
    intent=json.loads(intent_path.read_text());source_path=owned(intent['source']);directory=intent_path.parent
    replacement=approved_intent(intent)
    if intent.get('media_bytes')!=MEDIA_BYTES or intent.get('script_sha256')!=digest(Path(__file__)) or intent.get('vm_acceptance') is not True:raise ValueError('different reviewed flash intent')
    expected=intent['source_sha256']
    if source_path.name!='boot.raw' or not stat.S_ISREG(source_path.stat().st_mode) or source_path.stat().st_size!=MEDIA_BYTES:raise ValueError('different prepared image')
    io=load_io(intent['image_io_sha256']);api=Windows();handles=[];native=None;source=None;journal=None;dispatched=False;verified=False
    # Exclusive durable attempt marker prevents concurrent or lost-reply replay.
    with (directory/'attempt.json').open('x') as marker:
        json.dump(dict(intent_sha256=intent_sha,pid=os.getpid(),source_sha256=expected),marker);marker.flush();os.fsync(marker.fileno())
    try:
        journal=Journal(directory/'journal.jsonl');journal.emit('begin',intent_sha256=intent_sha)
        asset_lock=api.open(owned(ROOT/'local/usb-flash-intenso.lock'),0xc0000000,share=0,disposition=4);handles.append(asset_lock)
        import msvcrt
        source_handle=api.open(source_path,0x80000000,share=1)
        try:source_fd=msvcrt.open_osfhandle(source_handle,os.O_RDONLY|os.O_BINARY)
        except BaseException:api.close(source_handle);raise
        try:os.set_inheritable(source_fd,False);source=os.fdopen(source_fd,'rb')
        except BaseException:os.close(source_fd);raise
        if os.fstat(source.fileno()).st_size!=MEDIA_BYTES:raise ValueError('held source size differs')
        if io.stream_range_sha(source,0,MEDIA_BYTES)!=expected:raise ValueError('fresh complete source hash differs')
        journal.emit('source_verified',MEDIA_BYTES)
        disk=selected_disk()
        layout=replacement_partitions(disk['Number']) if replacement else [partition(disk['Number'])]
        part=layout[0]
        if disk['UniqueId']!=intent['device_unique_id']:raise ValueError('different prepared USB unique ID')
        device=api.open(r'\\.\PhysicalDrive'+str(disk['Number']),0xc0000000,flags=0xa0000000);handles.append(device)
        original=api.observe(device,disk['Number'],True)
        volume_path=part['AccessPaths'][0].rstrip('\\') if replacement else r'\\.\G:'
        volume=api.open(volume_path,0xc0000000);handles.append(volume)
        extent(api.ioctl(volume,0x560000,size=32),part)
        api.ioctl(volume,0x90018,size=0)  # LOCK: never force-dismount open files.
        api.ioctl(volume,0x90020,size=0)  # DISMOUNT while retaining this lock.
        current=replacement_partitions(disk['Number']) if replacement else [partition(disk['Number'])]
        if selected_disk()!=disk or current!=layout or api.observe(device,disk['Number'],True)!=original:raise ValueError('device context changed before writing')
        journal.emit('device_locked',device=original,storage_observation=disk,partition=part,partition_layout=layout)
        native=NativeIO(api,device)
        def emit(phase,bytes):
            nonlocal dispatched
            journal.emit(phase,bytes)
            if phase=='write_dispatched':dispatched=True
        result=transfer(lambda at,size:io.read_window(source,at,size),native,MEDIA_BYTES,expected,emit)
        if api.observe(device,disk['Number'],True)!=original or selected_disk()!=disk:raise ValueError('held target context changed after readback')
        journal.emit('verified',MEDIA_BYTES,image_sha256=result)
        verified=True
    except BaseException as error:
        if journal:
            try:journal.emit('failed_preserved',write_may_have_started=dispatched,error=str(error)[:2048])
            except BaseException:pass
        raise
    finally:
        cleanup=[]
        for close in ([native.close] if native else [])+[lambda h=h:api.check(api.close(h)) for h in reversed(handles)]+([source.close] if source else []):
            try:close()
            except BaseException as error:cleanup.append(str(error)[:512])
        if journal:
            try:
                if cleanup:journal.emit('cleanup_failed',errors=cleanup)
                elif verified:journal.emit('complete',MEDIA_BYTES,image_sha256=expected,handles_closed=True)
            finally:journal.close()
        if cleanup and sys.exception() is None:raise RuntimeError('flash handle cleanup failed: '+repr(cleanup))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--inspect',action='store_true');parser.add_argument('--replacement-layout',action='store_true');parser.add_argument('--write',action='store_true');parser.add_argument('--status',action='store_true');parser.add_argument('--intent',type=Path);parser.add_argument('--intent-sha256')
    args=parser.parse_args()
    if args.status and not args.write and not args.inspect and not args.replacement_layout and args.intent:
        intent=owned(args.intent);records,partial=read_journal(intent.parent/'journal.jsonl')
        if not records or records[0].get('intent_sha256')!=digest(intent):raise ValueError('different intent/journal binding')
        print(json.dumps(dict(last=records[-1],incomplete_tail=partial,historical_complete_record=not partial and records[-1]['phase']=='complete',original_process_exit_still_required=True)))
    elif args.inspect and not args.write and not args.status:
        value=selected_disk();api=Windows();handle=api.open(r'\\.\PhysicalDrive'+str(value['Number']),0)
        try:print(json.dumps(dict(storage=value,held=api.observe(handle,value['Number']),partitions=replacement_partitions(value['Number'])) if args.replacement_layout else dict(storage=value,held=api.observe(handle,value['Number']),partition=partition(value['Number']))))
        finally:api.check(api.close(handle))
    elif args.write and not args.inspect and not args.status and not args.replacement_layout and args.intent and args.intent_sha256:run(args.intent,args.intent_sha256)
    else:parser.error('choose metadata-only --inspect OR explicit --write with the prepared intent/hash')
