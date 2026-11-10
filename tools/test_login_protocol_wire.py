"""Independent synthetic JSON peer for the native conversation test executable.

Runs no daemon/PAM/session and accepts no real credentials. Never reports frame
contents. The native peer owns its pure model; this is not transport admission.
"""
import argparse
import json
import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import time


def check(binary):
    process=subprocess.Popen([str(binary),'--wire'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    deadline=time.monotonic()+10
    def exact(size):
        result=bytearray()
        while len(result)<size:
            remaining=deadline-time.monotonic()
            if remaining<=0 or not select.select([process.stdout],[],[],remaining)[0]:raise TimeoutError('native protocol peer timed out')
            data=os.read(process.stdout.fileno(),size-len(result))
            if not data:raise ValueError('native protocol peer ended early')
            result.extend(data)
        return bytes(result)
    def receive():
        length=struct.unpack('=I',exact(4))[0]
        if not 0<length<=32768:raise ValueError('native request frame budget')
        return json.loads(exact(length))
    def reply(value):
        raw=json.dumps(value,ensure_ascii=True,separators=(',',':')).encode()
        frame=struct.pack('=I',len(raw))+raw
        # Deliberately fragment the header and Unicode escape sequences.
        for byte in frame:process.stdin.write(bytes([byte]));process.stdin.flush()
    try:
        if receive()!=dict(type='create_session',username='test"\\é'):raise ValueError('different native create request')
        reply(dict(auth_message='Pässword: 😀',auth_message_type='secret',type='auth_message'))
        if receive()!=dict(type='post_auth_message_response',response='synthetic"\\\n€'):raise ValueError('different native answer encoding')
        reply(dict(type='success'))
        if receive()!=dict(type='start_session',cmd=['/usr/libexec/aetheri/aetheri-session'],env=['XDG_SESSION_TYPE=wayland','XDG_CURRENT_DESKTOP=Aetheri','XDG_SESSION_DESKTOP=Aetheri']):raise ValueError('different native fixed session request')
        reply(dict(type='success'));process.stdin.close()
        if process.wait(timeout=5)!=0 or process.stdout.read() or process.stderr.read():raise ValueError('native protocol peer failed')
    finally:
        if process.poll() is None:process.kill();process.wait(timeout=5)
        for stream in (process.stdin,process.stdout,process.stderr):
            if stream and not stream.closed:stream.close()


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--binary',type=Path,required=True);args=parser.parse_args()
    if sys.platform!='linux' or not os.getuid():return 77
    check(args.binary.resolve(strict=True));print('Independent synthetic JSON peer passed native framing, Unicode and fixed session contract');return 0


if __name__=='__main__':raise SystemExit(main())
