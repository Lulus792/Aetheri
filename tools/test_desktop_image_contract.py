"""Desktop-image controller boundaries; never launches a VM."""
import copy
import io
import json
from pathlib import Path
import unittest
from unittest.mock import MagicMock,patch

import test_desktop_image as desktop


class Stream(io.BytesIO):
    def __init__(self,rows):
        super().__init__(b''.join(json.dumps(x).encode()+b'\r\n' for x in rows));self.sent=[]
    def write(self,data):self.sent.append(json.loads(data));return len(data)


class DesktopController(unittest.TestCase):
    def setUp(self):
        self.identifier='12345678-1234-1234-1234-123456789abc'
        self.process=MagicMock();self.process.poll.return_value=None
        self.volume=dict(size=256*1024**2,serial='AETHDATA'+'a'*12,uuid=self.identifier,initial_sha256='0'*64)
        self.manifest=dict(nonce='a'*32,data_volume=self.volume)

    def connection(self,rows):
        stream=Stream(rows);sock=MagicMock();sock.makefile.return_value=stream
        cm=MagicMock();cm.__enter__.return_value=sock
        return stream,cm

    def test_uuid_is_checked_on_same_connection_before_every_mutation(self):
        # Use actual QMP field spelling; the first reply is capability admission.
        stream,cm=self.connection([dict(QMP={}),{'return':{}},{'return':{'UUID':self.identifier}},{'return':{'ack':True}}])
        with patch.object(desktop.vm,'qmp_socket_at',return_value=cm):
            result=desktop.query(Path('/private/qmp.sock'),self.identifier,self.process,'input-send-event',{'events':[]})
        self.assertEqual(result,{'ack':True})
        self.assertEqual([x['execute'] for x in stream.sent],['qmp_capabilities','query-uuid','input-send-event'])

    def test_foreign_uuid_never_receives_the_requested_command(self):
        stream,cm=self.connection([dict(QMP={}),{'return':{}},{'return':{'UUID':'00000000-0000-0000-0000-000000000000'}}])
        with patch.object(desktop.vm,'qmp_socket_at',return_value=cm):
            with self.assertRaisesRegex(ValueError,'foreign'):desktop.query(Path('/private/qmp.sock'),self.identifier,self.process,'system_powerdown')
        self.assertEqual([x['execute'] for x in stream.sent],['qmp_capabilities','query-uuid'])

    def test_terminal_original_process_does_not_connect_or_reissue(self):
        self.process.poll.return_value=0
        with patch.object(desktop.vm,'qmp_socket_at') as connect:
            with self.assertRaises(RuntimeError):desktop.query(Path('/private/qmp.sock'),self.identifier,self.process,'input-send-event')
            connect.assert_not_called()

    def test_unknown_mutation_reply_is_not_replayed(self):
        stream,cm=self.connection([dict(QMP={}),{'return':{}},{'return':{'UUID':self.identifier}}])
        with patch.object(desktop.vm,'qmp_socket_at',return_value=cm):
            with self.assertRaisesRegex(RuntimeError,'incomplete'):desktop.query(Path('/private/qmp.sock'),self.identifier,self.process,'input-send-event',{'events':[]})
        self.assertEqual(len([x for x in stream.sent if x['execute']=='input-send-event']),1)

    def test_volume_metadata_cannot_inject_qemu_options_or_expand_storage(self):
        self.assertEqual(desktop.validate_volume(self.manifest),self.volume)
        for key,value in [('size',True),('size',512*1024**2),('serial','AETHDATA'+12*'a'+',drive=bootmedia'),('serial','other'),('uuid',self.identifier.upper()),('initial_sha256','x'*64)]:
            m=copy.deepcopy(self.manifest);m['data_volume'][key]=value
            with self.assertRaises(ValueError):desktop.validate_volume(m)

    def test_command_has_only_owned_images_and_local_qmp(self):
        image=Path('/image');attempt=Path('/attempt');data=Path('/owned/data.raw')
        args=desktop.command(image,attempt,data,self.volume['serial'],self.identifier)
        self.assertIn('unix:'+str(attempt/'qmp.sock')+',server=on,wait=off',args)
        self.assertEqual(args.count('-drive'),4)
        self.assertIn('VGA',args)
        for bad in ('-kernel','-initrd','-append','-netdev','-fsdev','-virtfs'):self.assertNotIn(bad,args)
        self.assertFalse(any('tcp:' in x for x in args))


if __name__=='__main__':unittest.main(verbosity=2)
