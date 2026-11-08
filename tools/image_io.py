"""Bounded regular-image I/O. Sparse copying is NEVER a physical-device writer."""
import ctypes
import hashlib
import os
from pathlib import Path
import stat


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
    digest=hashlib.sha256();left=length
    while left:
        block=source.read(min(left,1024**2))
        if not block:raise ValueError('truncated image extent')
        digest.update(block)
        if block.strip(b'\0'):target.write(block)
        else:target.seek(len(block),1)
        left-=len(block)
    return digest.hexdigest()


def range_sha(path,start,length):
    if type(start) is not int or type(length) is not int or start<0 or length<0:raise ValueError('invalid hash extent')
    digest=hashlib.sha256()
    with Path(path).open('rb') as source:
        info=os.fstat(source.fileno())
        if not stat.S_ISREG(info.st_mode) or start>info.st_size or length>info.st_size-start:raise ValueError('image extent outside regular file')
        source.seek(start);left=length
        while left:
            block=source.read(min(left,1024**2))
            if not block:raise ValueError('truncated hash extent')
            digest.update(block);left-=len(block)
    return digest.hexdigest()
