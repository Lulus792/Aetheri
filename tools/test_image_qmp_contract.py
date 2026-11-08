"""Correlated one-connection QMP state and unambiguous guest shutdown causes."""
import json
import unittest
from unittest import mock
from pathlib import Path

import image_qmp as qmp
import test_laptop_no_serial as serial


class Socket:
    def __init__(self,frames):self.frames=[json.dumps(v).encode()+b'\n' for v in frames];self.sent=[];self.closed=False
    def recv(self,size):return self.frames.pop(0) if self.frames else b''
    def sendall(self,value):self.sent.append(json.loads(value))
    def close(self):self.closed=True


class QMPContract(unittest.TestCase):
    def test_lower_headroom_requires_proven_bounded_sparse_allocation(self):
        gib=1024**3;mib=1024**2
        for free,allocation in ((8*gib,None),(4*gib,512*mib),(5*gib,0)):
            serial.require_headroom(free,allocation)
        for free,allocation in ((4*gib-1,0),(7*gib,None),(7*gib,512*mib+1),(7*gib,-1),(7*gib,True)):
            with self.assertRaises(OSError):serial.require_headroom(free,allocation)

    def connect(self,frames,identifier='original'):
        sock=Socket([{'QMP':{}},{'return':{},'id':1},{'return':{'UUID':identifier},'id':2},*frames]);state={}
        process=mock.Mock();process.poll.return_value=None
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(qmp.vm,'qmp_socket_at',return_value=sock).start()
        mock.patch.object(qmp.select,'select',side_effect=lambda read,write,error,timeout:(read,[],[])).start()
        connection=qmp.Connection(Path('/owned/qmp.sock'),'original',process,state,lambda:None)
        self.addCleanup(connection.close)
        return connection,sock,state

    def test_events_are_retained_and_replies_bind_the_original_request(self):
        event={'event':'STOP','data':{}}
        c,s,state=self.connect([event,{'return':{'running':True},'id':3}])
        self.assertEqual(c.execute('query-status'),{'running':True})
        self.assertEqual(state['events'],[event]);self.assertEqual([v['id'] for v in s.sent],[1,2,3])
        self.assertTrue(all(r['state']=='acknowledged' for r in state['requests']))

    def test_wrong_or_error_reply_and_lost_connection_forbid_followup_dispatch(self):
        for frames in ([{'return':{},'id':99}],[{'error':{'class':'GenericError'},'id':3}],[]):
            with self.subTest(frames=frames):
                c,s,state=self.connect(frames)
                with self.assertRaises((ValueError,EOFError)):c.execute('input-send-event',{'events':[]})
                before=len(s.sent)
                with self.assertRaises(RuntimeError):c.execute('system_powerdown')
                self.assertEqual(len(s.sent),before);self.assertEqual(state['requests'][-1]['state'],'pending')
                c.close();mock.patch.stopall()

    def test_foreign_uuid_is_rejected_before_guest_commands(self):
        with self.assertRaises(ValueError):self.connect([],identifier='foreign')

    def test_poweroff_requires_one_actual_guest_shutdown_not_reset_panic_or_host_exit(self):
        good={'event':'SHUTDOWN','data':{'guest':True,'reason':'guest-shutdown'}}
        self.assertTrue(qmp.guest_poweroff([good]))
        self.assertFalse(qmp.guest_poweroff([]));self.assertFalse(qmp.guest_poweroff([good,good]))
        for reason in ('guest-reset','guest-panic','host-qmp-quit','host-signal'):
            self.assertFalse(qmp.guest_poweroff([{'event':'SHUTDOWN','data':{'guest':True,'reason':reason}}]))
        self.assertFalse(qmp.guest_poweroff([{'event':'SHUTDOWN','data':{'guest':False,'reason':'guest-shutdown'}}]))

    def test_no_serial_command_keeps_one_usb_and_only_protected_unix_qmp(self):
        args=serial.command(Path('/image'),Path('/owned'),Path('/owned/medium.raw'),'original')
        self.assertEqual(args[args.index('-serial')+1],'none');self.assertEqual(args.count('-serial'),1)
        self.assertEqual(sum(v.startswith('usb-storage,') for v in args),1)
        self.assertTrue(args[args.index('-qmp')+1].startswith('unix:'))
        self.assertFalse(any('tcp:' in v for v in args))


if __name__=='__main__':unittest.main(verbosity=2)
