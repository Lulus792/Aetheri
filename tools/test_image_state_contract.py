"""Actual Windows replacement/held-reader semantics plus bounded JSON guards."""
import os
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest

import image_state
import test_laptop_no_serial as laptop


class StateContract(unittest.TestCase):
    def test_coordinated_reader_holds_old_state_then_separate_writer_publishes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)/'état';root.mkdir();path=root/'state.json'
            image_state.write(path,{'generation':1})
            worker=root/'writer.py';worker.write_text('import sys\nsys.path.insert(0,sys.argv[1])\nimport image_state\nprint("starting",flush=True)\nimage_state.write(sys.argv[2],{"generation":2})\n')
            process=None
            try:
                with image_state.guard(path),path.open('rb') as held:
                    process=subprocess.Popen([sys.executable,str(worker),str(Path(__file__).resolve().parent),str(path)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                    self.assertEqual(process.stdout.readline().strip(),b'starting');self.assertIsNone(process.poll())
                    self.assertEqual(held.read().strip(),b'{\n  "generation": 1\n}')
                stdout,stderr=process.communicate(timeout=10);self.assertEqual((process.returncode,stderr),(0,b''))
            finally:
                if process:
                    if process.poll() is None:process.terminate()
                    process.communicate(timeout=10)
            self.assertEqual(image_state.read(path),{'generation':2})

    @unittest.skipUnless(os.name=='nt','reproduce Windows non-delete-sharing reader')
    def test_ordinary_reader_reproduces_access_denied_and_preserves_both_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);path=root/'state.json';next=root/'next.json'
            path.write_bytes(b'old');next.write_bytes(b'new')
            with path.open('rb') as held:
                with self.assertRaises(PermissionError) as error:os.replace(next,path)
                self.assertEqual(error.exception.winerror,5)
                self.assertEqual(held.read(),b'old');self.assertEqual(next.read_bytes(),b'new')
            os.replace(next,path);self.assertEqual(path.read_bytes(),b'new')

    def test_empty_large_nonobject_and_invalid_bounds_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'state'
            for raw in (b'',b'[]',b'null',b'{',b'{"x":123456789}'):
                path.write_bytes(raw)
                with self.assertRaises(ValueError):image_state.read(path,8)
            path.write_bytes(b'{}')
            for bound in (0,True,8*1024**2+1):
                with self.assertRaises(ValueError):image_state.read(path,bound)

    def test_failed_recording_still_retires_original_process_and_closes_transport(self):
        from unittest import mock
        process=mock.Mock();process.poll.side_effect=[None,1];monitor=mock.Mock();monitor.close.side_effect=OSError('close record failed')
        current={};save=mock.Mock(side_effect=PermissionError('replace failed'))
        issues=laptop.retire_failed(process,monitor,current,save)
        process.terminate.assert_called_once();process.wait.assert_called_once_with(timeout=30);monitor.close.assert_called_once()
        self.assertEqual(current['exit'],1);self.assertEqual(len(issues),3)

    def test_failed_retirement_keeps_unknown_exit_and_still_closes_transport(self):
        from unittest import mock
        process=mock.Mock();process.poll.return_value=None;process.wait.side_effect=TimeoutError('still live')
        monitor=mock.Mock();current={}
        issues=laptop.retire_failed(process,monitor,current,lambda:None)
        self.assertIsNone(current['exit']);self.assertEqual(len(issues),1);monitor.close.assert_called_once()

    def test_unknown_temporary_publication_is_preserved_without_replay(self):
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'state.json';image_state.write(path,{'generation':1})
            pending=path.with_suffix('.json.tmp');pending.write_bytes(b'unknown')
            with self.assertRaises(FileExistsError):image_state.write(path,{'generation':2})
            self.assertEqual(image_state.read(path),{'generation':1});self.assertEqual(pending.read_bytes(),b'unknown')


if __name__=='__main__':unittest.main(verbosity=2)
