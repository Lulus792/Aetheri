"""First-flash guards, complete zero writes and actual unbuffered FILE I/O.

No test opens or writes any physical device or volume.
"""
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

import usb_flash as flash


class Target(io.BytesIO):
    def __init__(self,size):super().__init__(b'\xff'*size);self.writes=0;self.reads=0;self.flushes=0
    def write(self,data):self.writes+=1;return super().write(data)
    def read(self,size):self.reads+=1;return super().read(size)
    def flush(self):self.flushes+=1


class FlashContract(unittest.TestCase):
    @unittest.skipUnless(os.name=='nt','Windows elevation guard')
    def test_unelevated_entry_cannot_open_any_device_or_create_an_attempt(self):
        from unittest import mock
        with tempfile.TemporaryDirectory(dir=flash.ROOT/'local') as temporary:
            intent=Path(temporary)/'intent.json'
            with mock.patch.object(flash.ctypes.windll.shell32,'IsUserAnAdmin',return_value=0),mock.patch.object(flash,'Windows') as api:
                with self.assertRaises(PermissionError):flash.run(intent,'0'*64)
                api.assert_not_called()
            self.assertEqual(list(Path(temporary).iterdir()),[])

    def fixture(self):
        return dict(Number=4,FriendlyName='Intenso Speed Line',SerialNumber=flash.SERIAL,UniqueId=flash.BRIDGE+'Lulus',BusType='USB',Size=flash.MEDIA_BYTES,LogicalSectorSize=512,PhysicalSectorSize=512,IsBoot=False,IsSystem=False,IsOffline=False,IsReadOnly=False)

    def test_only_the_authorized_non_system_usb_and_geometry_are_admitted(self):
        good=self.fixture();self.assertEqual(flash.admit_disk(good),good)
        for key,value in [('FriendlyName',None),('SerialNumber',None),('FriendlyName','Generic Multi-Card'),('SerialNumber','different'),('UniqueId','different'),('BusType','NVMe'),('Size',flash.MEDIA_BYTES+512),('IsBoot',True),('IsSystem',True),('IsOffline',True),('IsReadOnly',True),('IsBoot',0),('Number',True),('Number',-1),('LogicalSectorSize',4096),('PhysicalSectorSize',True),('PhysicalSectorSize',513),('PhysicalSectorSize',131072)]:
            bad=good.copy();bad[key]=value
            with self.assertRaises(ValueError,msg=key):flash.admit_disk(bad)

    def test_duplicate_serial_cannot_hide_an_unsafe_matching_device(self):
        from unittest import mock
        unsafe=self.fixture();unsafe['Number']=0;unsafe['IsSystem']=True
        with mock.patch.object(flash,'disks',return_value=[self.fixture(),unsafe]):
            with self.assertRaises(ValueError):flash.selected_disk()

    def replacement_layout(self):
        facts=((1048576,132120576,'95faca9a-919a-5721-a142-7fd2b3480321','c12a7328-f81f-11d2-ba4b-00a0c93ec93b'),
            (134217728,536870912,'8112b47d-a0fa-53ba-9676-46434744a761','4f68bce3-e8cd-4db1-96e7-fbcaf984b709'),
            (671088640,30785142784,'fa6f5b93-6372-5028-a36a-304736e93a32','0fc63daf-8483-4772-8e79-3d69d8477de4'))
        return [dict(DiskNumber=4,PartitionNumber=i,DriveLetter='\0',Offset=a,Size=b,Guid='{'+c+'}',GptType='{'+d+'}',IsBoot=False,IsSystem=False,
            AccessPaths=['\\\\?\\Volume{'+c+'}\\'] if i==1 else None) for i,(a,b,c,d) in enumerate(facts,1)]

    def test_replacement_requires_all_three_previous_partitions_and_only_esp_volume(self):
        import copy
        good=self.replacement_layout();observed=flash.admit_replacement_partitions(good,4)
        self.assertEqual(observed[0]['AccessPaths'],good[0]['AccessPaths'])
        self.assertEqual(observed[1]['AccessPaths'],[])
        for rows in (good[:1],good+[good[-1]],list(reversed(good))):
            with self.assertRaises(ValueError):flash.admit_replacement_partitions(rows,4)
        for index in range(3):
            for key,value in (('DiskNumber',0),('PartitionNumber',True),('Offset',0),('Size',1),('Guid','{foreign}'),('GptType','{foreign}'),('IsBoot',True),('IsSystem',True),('DriveLetter','G'),('AccessPaths',False),('AccessPaths',['\\\\?\\Volume{foreign}\\'])):
                rows=copy.deepcopy(good);rows[index][key]=value
                with self.assertRaises(ValueError,msg=(index,key)):flash.admit_replacement_partitions(rows,4)

    def test_replacement_image_stays_closed_without_exact_new_approval(self):
        from unittest import mock
        self.assertFalse(flash.approved_intent(dict(format=1,source_sha256=flash.APPROVED_IMAGE)))
        intent=dict(format=2,source_sha256='a'*64,replaces_source_sha256=flash.APPROVED_IMAGE,authorization='user-2026-11-08-reflash-after-tests')
        with mock.patch.object(flash,'APPROVED_REPLACEMENT_IMAGE',None):
            with self.assertRaises(ValueError):flash.approved_intent(intent)
        with mock.patch.object(flash,'APPROVED_REPLACEMENT_IMAGE','a'*64):
            self.assertTrue(flash.approved_intent(intent))
            for key,value in (('format',True),('format',1),('source_sha256','b'*64),('replaces_source_sha256','b'*64),('authorization',None)):
                with self.assertRaises(ValueError):flash.approved_intent({**intent,key:value})

    def test_device_descriptor_is_bounded_and_matches_actual_identity(self):
        raw=bytearray(128);struct.pack_into('<II',raw,0,40,len(raw));raw[10]=1;struct.pack_into('<I',raw,28,7)
        for position,offset,text in ((12,40,b'Intenso'),(16,48,b'Speed Line'),(20,60,b'PMAP'),(24,65,flash.SERIAL.encode())):
            struct.pack_into('<I',raw,position,offset);raw[offset:offset+len(text)]=text
        self.assertEqual(flash.descriptor(bytes(raw))['serial'],flash.SERIAL)
        cases=[bytes(raw[:35])]
        for offset,data in ((10,b'\0'),(28,struct.pack('<I',17)),(24,struct.pack('<I',127)),(65,b'X')):
            bad=raw.copy();bad[offset:offset+len(data)]=data;cases.append(bytes(bad))
        for bad in cases:
            with self.assertRaises(ValueError):flash.descriptor(bad)

    def test_same_handle_device_and_single_volume_extent_are_required(self):
        self.assertEqual(flash.device_number(struct.pack('<III',7,4,0),4),4)
        part=dict(DiskNumber=4,Offset=1048576,Size=31456198656)
        raw=struct.pack('<I4xI4xqq',1,4,part['Offset'],part['Size'])
        self.assertEqual(flash.extent(raw,part)['number'],4)
        for bad in (struct.pack('<III',7,0,0),struct.pack('<III',7,4,1),b''):
            with self.assertRaises(ValueError):flash.device_number(bad,4)
        for bad in (raw[:24],struct.pack('<I4xI4xqq',2,4,part['Offset'],part['Size']),struct.pack('<I4xI4xqq',1,0,part['Offset'],part['Size'])):
            with self.assertRaises(ValueError):flash.extent(bad,part)

    def source(self):return b'\0'*512+b'payload'+b'\0'*(1024-7)
    def invoke(self,target,source=None,expected=None):
        source=self.source() if source is None else source;events=[]
        result=flash.transfer(lambda at,size:source[at:at+size],target,len(source),expected or hashlib.sha256(source).hexdigest(),lambda phase,size:events.append((phase,size)),512)
        return result,events

    def test_all_zero_regions_overwrite_existing_nonzero_bytes_and_read_back(self):
        target=Target(1536);digest,events=self.invoke(target)
        self.assertEqual(target.getvalue(),self.source());self.assertEqual((target.writes,target.reads,target.flushes),(3,3,1))
        self.assertEqual(digest,hashlib.sha256(self.source()).hexdigest())
        self.assertEqual([e[0] for e in events],['write_dispatched','written_flushed','readback_dispatched'])

    def test_short_source_never_submits_the_short_block(self):
        target=Target(1536)
        with self.assertRaises(ValueError):flash.transfer(lambda at,size:b'\0'*(size-1),target,1536,'0'*64,lambda *args:None,512)
        self.assertEqual((target.writes,target.reads),(0,0))

    def test_short_write_is_never_retried(self):
        class Short(Target):
            def write(self,data):return super().write(data[:256])
        target=Short(1536)
        with self.assertRaises(OSError):self.invoke(target)
        self.assertEqual((target.writes,target.reads),(1,0))

    def test_failed_flush_and_changed_source_never_proceed_to_readback(self):
        class Failed(Target):
            def flush(self):raise OSError('unknown flush')
        for target,expected in ((Failed(1536),None),(Target(1536),'0'*64)):
            with self.assertRaises((OSError,ValueError)):self.invoke(target,expected=expected)
            self.assertEqual(target.reads,0)

    def test_short_or_corrupt_readback_cannot_pass(self):
        class Short(Target):
            def read(self,size):return super().read(size-1)
        class Corrupt(Target):
            def read(self,size):
                data=super().read(size);return bytes([data[0]^1])+data[1:]
        for target in (Short(1536),Corrupt(1536)):
            with self.assertRaises((OSError,ValueError)):self.invoke(target)
        self.assertEqual(target.reads,3)

    def test_journal_is_create_only_and_hash_chained(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'journal';journal=flash.Journal(path)
            try:
                journal.emit('begin');journal.emit('write_dispatched');journal.emit('verified',image_sha256='1'*64);journal.emit('complete',image_sha256='1'*64)
                with self.assertRaises(ValueError):journal.emit('bad',record_sha256='forged')
            finally:journal.close()
            previous='0'*64
            for number,line in enumerate(path.read_bytes().splitlines()):
                record=json.loads(line);digest=record.pop('record_sha256');self.assertEqual(record['sequence'],number);self.assertEqual(record['previous'],previous)
                self.assertEqual(hashlib.sha256(json.dumps(record,sort_keys=True,separators=(',',':')).encode()).hexdigest(),digest);previous=digest
                if record['phase'] in ('verified','complete'):self.assertEqual(record['image_sha256'],'1'*64)
            with self.assertRaises(FileExistsError):flash.Journal(path)

    def test_live_append_allows_an_ordinary_reader_and_partial_tail_is_not_complete(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'journal';journal=flash.Journal(path)
            try:
                journal.emit('begin')
                with path.open('rb') as reader:
                    journal.emit('complete',image_sha256='1'*64)
                    self.assertEqual(len(reader.read().splitlines()),2)
            finally:journal.close()
            records,partial=flash.read_journal(path);self.assertFalse(partial);self.assertEqual(records[-1]['image_sha256'],'1'*64)
            original=path.read_bytes();path.write_bytes(original+b'{')
            self.assertTrue(flash.read_journal(path)[1])
            path.write_bytes(original.replace(b'"bytes": 0',b'"bytes": 1',1))
            with self.assertRaises(ValueError):flash.read_journal(path)

    @unittest.skipUnless(os.name=='nt','actual Windows aligned file I/O')
    def test_actual_windows_unbuffered_write_flush_and_complete_readback(self):
        source=b'A'*flash.BLOCK+b'\0'*flash.BLOCK+b'Z'*flash.BLOCK;expected=hashlib.sha256(source).hexdigest();api=flash.Windows()
        with tempfile.TemporaryDirectory(dir=flash.ROOT/'local') as temporary:
            path=Path(temporary)/'new-file.bin';handle=api.open(path,0xc0000000,share=0,flags=0xa0000000,disposition=1);native=None
            try:
                native=flash.NativeIO(api,handle)
                self.assertEqual(flash.transfer(lambda at,size:source[at:at+size],native,len(source),expected,lambda *args:None),expected)
            finally:
                if native:native.close()
                api.check(api.close(handle))
            self.assertEqual(path.read_bytes(),source)


if __name__=='__main__':unittest.main(verbosity=2)
