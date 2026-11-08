"""Actual local NTFS range/hash/window/copy conformance; no physical device."""
import hashlib
import os
from pathlib import Path
import tempfile
import unittest

import image_io as io


@unittest.skipUnless(os.name=='nt','local NTFS API conformance is Windows-only')
class SparseIO(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]/'local')
        self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)

    def test_actual_sparse_ranges_hash_windows_and_copy_match_literal_bytes(self):
        size=8*1024**2;expected=b'first'+b'\0'*(size-9)+b'last';a=self.root/'a';b=self.root/'b'
        with a.open('x+b') as f:
            io.sparse_file(f);io.set_length(f,size);f.write(b'first');f.seek(-4,2);f.write(b'last');f.flush();os.fsync(f.fileno())
            ranges=io.allocated_ranges(f,0,size)
            self.assertIsInstance(ranges,list);self.assertLess(sum(end-start for start,end in ranges),size//2)
            self.assertEqual(io.stream_range_sha(f,0,size),hashlib.sha256(expected).hexdigest())
            for start,length in ((0,10),(1024**2,4096),(size-9,9),(size,0)):
                self.assertEqual(io.read_window(f,start,length),expected[start:start+length])
            f.seek(0)
            with b.open('x+b') as target:
                io.sparse_file(target);self.assertEqual(io.copy_extent(f,target,size),hashlib.sha256(expected).hexdigest());io.set_length(target,size);target.flush();os.fsync(target.fileno())
        self.assertEqual(io.hash_file(b),hashlib.sha256(expected).hexdigest())
        # Small independent sequential read checks the NTFS hole convention too.
        self.assertEqual(b.read_bytes(),expected)

    def test_real_more_data_pagination_covers_every_disjoint_extent(self):
        block=1024**2;count=300;path=self.root/'many';expected=hashlib.sha256()
        with path.open('x+b') as f:
            io.sparse_file(f);io.set_length(f,block*count)
            for index in range(count):f.seek(index*block);f.write(b'X');expected.update(b'X'+b'\0'*(block-1))
            f.flush();os.fsync(f.fileno())
            ranges=io.allocated_ranges(f,0,block*count)
            self.assertEqual(len(ranges),count)
            self.assertEqual(io.stream_range_sha(f,0,block*count),expected.hexdigest())
            self.assertLess(sum(end-start for start,end in ranges),block*count//8)

    def test_bounds_and_nonsparse_path_still_hash_actual_bytes(self):
        path=self.root/'ordinary';path.write_bytes(b'actual bytes')
        with path.open('rb') as f:
            self.assertIsNone(io.allocated_ranges(f,0,12))
            for start,length in ((-1,1),(0,True),(12,1),(13,0)):
                with self.assertRaises(ValueError):io.stream_range_sha(f,start,length)
            with self.assertRaises(ValueError):io.read_window(f,0,128*1024**2+1)
        self.assertEqual(io.hash_file(path),hashlib.sha256(b'actual bytes').hexdigest())


if __name__=='__main__':unittest.main(verbosity=2)
