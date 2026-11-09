"""Linux-only private socket/FD controls; no bus, unit or policy mutations."""
import array
import os
from pathlib import Path
import socket
import tempfile
import unittest

from audio_user_worker import LIMIT,packet,receive
from test_audio_mac import Audio


@unittest.skipUnless(hasattr(socket,'SCM_RIGHTS'),'Linux ancillary-FD fixture required')
class Channel(unittest.TestCase):
    def setUp(self):self.parent,self.child=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET|socket.SOCK_CLOEXEC)
    def tearDown(self):self.parent.close();self.child.close()
    def descriptors(self):return len(list(Path('/proc/self/fd').iterdir()))

    def test_original_file_object_and_cloexec_survive_transfer(self):
        with tempfile.TemporaryFile() as stream:
            stream.write(b'original-private-fixture');stream.flush();baseline=self.descriptors()
            packet(self.parent,dict(op='original'),(stream.fileno(),));value,fds=receive(self.child)
            self.assertEqual(value,dict(op='original'));self.assertEqual(len(fds),1)
            try:
                self.assertFalse(os.get_inheritable(fds[0]));self.assertEqual(os.fstat(fds[0]).st_ino,os.fstat(stream.fileno()).st_ino)
                self.assertEqual(os.pread(fds[0],24,0),b'original-private-fixture')
            finally:os.close(fds[0])
            self.assertEqual(self.descriptors(),baseline);self.assertTrue(os.fstat(stream.fileno()).st_ino)

    def test_invalid_json_closes_received_fds(self):
        with tempfile.TemporaryFile() as stream:
            baseline=self.descriptors();self.parent.sendmsg([b'not JSON'],[(socket.SOL_SOCKET,socket.SCM_RIGHTS,array.array('i',[stream.fileno()]))])
            with self.assertRaises(ValueError):receive(self.child)
            self.assertEqual(self.descriptors(),baseline)

    def test_ancillary_truncation_does_not_leak_partial_rights(self):
        with tempfile.TemporaryFile() as stream:
            baseline=self.descriptors();self.parent.sendmsg([b'{}'],[(socket.SOL_SOCKET,socket.SCM_RIGHTS,array.array('i',[stream.fileno()]*20))])
            with self.assertRaises(ValueError):receive(self.child)
            self.assertEqual(self.descriptors(),baseline)

    def test_foreign_credentials_alongside_rights_close_every_fd(self):
        self.child.setsockopt(socket.SOL_SOCKET,socket.SO_PASSCRED,1)
        with tempfile.TemporaryFile() as stream:
            baseline=self.descriptors();self.parent.sendmsg([b'{}'],[(socket.SOL_SOCKET,socket.SCM_RIGHTS,array.array('i',[stream.fileno()]))])
            with self.assertRaises(ValueError):receive(self.child)
            self.assertEqual(self.descriptors(),baseline)

    def test_packet_budget_rejects_before_send_and_channel_remains_usable(self):
        with self.assertRaises(AssertionError):packet(self.parent,dict(raw='x'*LIMIT))
        with tempfile.TemporaryFile() as stream:
            with self.assertRaises(AssertionError):packet(self.parent,dict(op='too-many'),(stream.fileno(),)*9)
        packet(self.parent,dict(op='still-original'));self.assertEqual(receive(self.child),(dict(op='still-original'),[]))

    def test_failed_case_capture_closes_only_after_independent_retirement(self):
        with tempfile.TemporaryFile() as file:
            fixture=Audio.__new__(Audio);fixture.pending={'kind':'unknown'};fixture.record={'units':{'source':{'state':'original-active'}}}
            fixture.capture_reader=os.dup(file.fileno());st=os.fstat(fixture.capture_reader);fixture.capture_object=(st.st_dev,st.st_ino)
            try:
                with self.assertRaises(AssertionError):fixture.retire_capture()
                fixture.pending=None
                with self.assertRaises(AssertionError):fixture.retire_capture()
                fixture.record['units']['source']['state']='retired';fd=fixture.capture_reader;fixture.retire_capture()
                with self.assertRaises(OSError):os.fstat(fd)
                fixture.retire_capture();self.assertTrue(os.fstat(file.fileno()).st_ino)
            finally:
                if fixture.capture_reader is not None:os.close(fixture.capture_reader)

    def test_capture_handle_replacement_is_preserved_for_inspection(self):
        with tempfile.TemporaryFile() as file,tempfile.TemporaryFile() as replacement:
            fixture=Audio.__new__(Audio);fixture.pending=None;fixture.record={'units':{}}
            fixture.capture_reader=os.dup(file.fileno());st=os.fstat(fixture.capture_reader);fixture.capture_object=(st.st_dev,st.st_ino)
            os.dup2(replacement.fileno(),fixture.capture_reader,inheritable=False)
            try:
                with self.assertRaises(AssertionError):fixture.retire_capture()
                self.assertEqual(os.fstat(fixture.capture_reader).st_ino,os.fstat(replacement.fileno()).st_ino)
            finally:os.close(fixture.capture_reader)


if __name__=='__main__':unittest.main()
