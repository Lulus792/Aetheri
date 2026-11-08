"""Bounded regular-image I/O. Sparse copying is NEVER a physical-device writer."""
import ctypes
import hashlib
import os
from pathlib import Path
import stat

ZERO=b'\0'*1024**2


def _snapshot(stream):
    s=os.fstat(stream.fileno())
    if not stat.S_ISREG(s.st_mode):raise ValueError('image must be a regular file')
    return s,(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)


def allocated_ranges(stream,start,length):
    """Return possible data extents only for local, offline NTFS sparse files.

    None means normal sequential I/O. This is not a snapshot of concurrent
    writers, a network-file optimization or physical-device verification.
    """
    info,_=_snapshot(stream)
    if os.name!='nt' or not info.st_file_attributes&0x200:return None
    if info.st_file_attributes&0x400 or info.st_size>32*1024**3:raise ValueError('unsupported sparse image')
    import msvcrt
    from ctypes import wintypes
    kernel=ctypes.WinDLL('kernel32',use_last_error=True);handle=msvcrt.get_osfhandle(stream.fileno())
    volume=kernel.GetVolumeInformationByHandleW
    volume.argtypes=[wintypes.HANDLE,wintypes.LPWSTR,wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),ctypes.POINTER(wintypes.DWORD),ctypes.POINTER(wintypes.DWORD),wintypes.LPWSTR,wintypes.DWORD];volume.restype=wintypes.BOOL
    filesystem=ctypes.create_unicode_buffer(32);flags=wintypes.DWORD()
    if not volume(handle,None,0,None,None,ctypes.byref(flags),filesystem,32):raise ctypes.WinError(ctypes.get_last_error())
    final=kernel.GetFinalPathNameByHandleW;final.argtypes=[wintypes.HANDLE,wintypes.LPWSTR,wintypes.DWORD,wintypes.DWORD];final.restype=wintypes.DWORD
    name=ctypes.create_unicode_buffer(32768);count=final(handle,name,len(name),0)
    drive=kernel.GetDriveTypeW;drive.argtypes=[wintypes.LPCWSTR];drive.restype=wintypes.UINT
    if not 7<=count<len(name) or not name.value.startswith('\\\\?\\') or name.value[5:7]!=':\\' or drive(name.value[4:7])!=3 or filesystem.value!='NTFS' or not flags.value&0x40:
        raise ValueError('sparse images require a local fixed NTFS volume')
    class Range(ctypes.Structure):_fields_=[('offset',ctypes.c_longlong),('length',ctypes.c_longlong)]
    call=kernel.DeviceIoControl
    call.argtypes=[wintypes.HANDLE,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),ctypes.c_void_p];call.restype=wintypes.BOOL
    result=[];cursor=start;end=start+length;pages=0
    while cursor<end:
        pages+=1
        if pages>32:raise ValueError('allocated-range query bound')
        query=Range(cursor,end-cursor);entries=(Range*256)();used=wintypes.DWORD()
        ok=call(handle,0x940cf,ctypes.byref(query),ctypes.sizeof(query),ctypes.byref(entries),ctypes.sizeof(entries),ctypes.byref(used),None)
        error=ctypes.get_last_error() if not ok else 0
        if error not in (0,234):raise ctypes.WinError(error)
        if used.value>ctypes.sizeof(entries) or used.value%ctypes.sizeof(Range):raise ValueError('invalid allocated-range response')
        progress=cursor
        for entry in entries[:used.value//ctypes.sizeof(Range)]:
            if entry.offset<0 or entry.length<=0 or entry.length>2**63-1-entry.offset:raise ValueError('invalid allocated extent')
            left=max(cursor,entry.offset);right=min(end,entry.offset+entry.length)
            if left>=right or (result and left<result[-1][1]):raise ValueError('unordered allocated extents')
            if result and left==result[-1][1]:result[-1]=(result[-1][0],right)
            else:result.append((left,right))
            if len(result)>4096:raise ValueError('allocated-range count bound')
            progress=right
        if ok:break
        if progress<=cursor or progress>=end:raise ValueError('incomplete allocated-range response')
        cursor=progress
    return result


def segments(stream,start,length):
    info,_=_snapshot(stream)
    if type(start) is not int or type(length) is not int or start<0 or length<0 or start>info.st_size or length>info.st_size-start:raise ValueError('image extent outside regular file')
    ranges=allocated_ranges(stream,start,length)
    if ranges is None:
        if length:yield True,start,length
        return
    at=start
    for left,right in ranges:
        if left>at:yield False,at,left-at
        yield True,left,right-left;at=right
    if at<start+length:yield False,at,start+length-at


def hash_zero(digest,length):
    while length:
        n=min(length,len(ZERO));digest.update(ZERO[:n]);length-=n


def stream_range_sha(stream,start,length):
    _,before=_snapshot(stream);digest=hashlib.sha256()
    for data,at,size in segments(stream,start,length):
        if not data:hash_zero(digest,size);continue
        stream.seek(at);left=size
        while left:
            block=stream.read(min(left,len(ZERO)))
            if not block:raise ValueError('truncated image hash')
            digest.update(block);left-=len(block)
    if _snapshot(stream)[1]!=before:raise ValueError('image changed during hash')
    stream.seek(start+length);return digest.hexdigest()


def hash_file(path):
    with Path(path).open('rb') as stream:return stream_range_sha(stream,0,os.fstat(stream.fileno()).st_size)


def read_window(stream,start,length):
    if type(length) is not int or not 0<=length<=128*1024**2:raise ValueError('oversized image structure')
    _,before=_snapshot(stream);result=bytearray(length)
    for data,at,size in segments(stream,start,length):
        if not data:continue
        stream.seek(at);left=size;offset=at-start
        while left:
            block=stream.read(min(left,len(ZERO)))
            if not block:raise ValueError('truncated image structure')
            result[offset:offset+len(block)]=block;offset+=len(block);left-=len(block)
    if _snapshot(stream)[1]!=before:raise ValueError('image changed during structure read')
    stream.seek(start+length);return bytes(result)


def sparse_file(stream):
    """Mark only a fresh owned regular file sparse before extending it."""
    info=os.fstat(stream.fileno())
    if not stat.S_ISREG(info.st_mode) or info.st_size or stream.tell():
        raise ValueError('sparse output must be a fresh empty regular file')
    if os.name=='nt':
        import msvcrt
        from ctypes import wintypes
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        call=kernel.DeviceIoControl
        call.argtypes=[wintypes.HANDLE,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),ctypes.c_void_p]
        call.restype=wintypes.BOOL;returned=wintypes.DWORD()
        if not call(msvcrt.get_osfhandle(stream.fileno()),0x900c4,None,0,None,0,ctypes.byref(returned),None):
            raise ctypes.WinError(ctypes.get_last_error())


def set_length(stream,length):
    """Extend a new sparse image without the CRT's explicit zero-fill loop."""
    if type(length) is not int or not 0<=length<=32*1024**3 or not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):raise ValueError('invalid regular image length')
    if os.name!='nt':stream.truncate(length);return
    import msvcrt
    from ctypes import wintypes
    if not os.fstat(stream.fileno()).st_file_attributes&0x200:raise ValueError('Windows image must already be sparse')
    stream.flush();position=stream.tell();kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    move=kernel.SetFilePointerEx;move.argtypes=[wintypes.HANDLE,ctypes.c_longlong,ctypes.c_void_p,wintypes.DWORD];move.restype=wintypes.BOOL
    end=kernel.SetEndOfFile;end.argtypes=[wintypes.HANDLE];end.restype=wintypes.BOOL
    handle=msvcrt.get_osfhandle(stream.fileno())
    if not move(handle,length,None,0) or not end(handle):raise ctypes.WinError(ctypes.get_last_error())
    stream.seek(position)


def copy_extent(source,target,length):
    """Copy into a known-zero range of a NEW regular image, preserving holes."""
    if type(length) is not int or length<0:raise ValueError('invalid copy extent')
    for stream in (source,target):
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):raise ValueError('image must be a regular file')
    _,before=_snapshot(source);digest=hashlib.sha256();begin=source.tell();destination=target.tell()
    for data,at,size in segments(source,begin,length):
        target.seek(destination+at-begin)
        if not data:hash_zero(digest,size);target.seek(size,1);continue
        source.seek(at);left=size
        while left:
            block=source.read(min(left,len(ZERO)))
            if not block:raise ValueError('truncated image extent')
            digest.update(block)
            if block.strip(b'\0'):target.write(block)
            else:target.seek(len(block),1)
            left-=len(block)
    if _snapshot(source)[1]!=before:raise ValueError('source changed during image copy')
    source.seek(begin+length);target.seek(destination+length)
    return digest.hexdigest()


def range_sha(path,start,length):
    if type(start) is not int or type(length) is not int or start<0 or length<0:raise ValueError('invalid hash extent')
    with Path(path).open('rb') as source:
        info=os.fstat(source.fileno())
        if not stat.S_ISREG(info.st_mode) or start>info.st_size or length>info.st_size-start:raise ValueError('image extent outside regular file')
        return stream_range_sha(source,start,length)
