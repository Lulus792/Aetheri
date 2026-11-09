"""Pure data materialization controls; no RPM, device or root action."""
import hashlib
import stat
import unittest
from prepare_libinput_data import materialize


class Data(unittest.TestCase):
    def fixture(self):
        a='usr/share/libwacom/layouts/a.svg';b='usr/share/libwacom/layouts/b.svg';raw=b'layout'
        expected={a:dict(size=len(raw),sha256=hashlib.sha256(raw).hexdigest())}
        headers={a:dict(object=(1,7),size=len(raw)),b:dict(object=(1,7),size=len(raw))}
        members=[(a,stat.S_IFREG|0o644,b''),(b,stat.S_IFREG|0o644,raw)]
        return a,b,raw,expected,headers,members

    def test_shared_archive_body_becomes_regular_bytes(self):
        a,b,raw,expected,headers,members=self.fixture();payload,aliases=materialize(expected,headers,members)
        self.assertEqual(payload,{a:raw});self.assertEqual(aliases,{a:b})

    def test_equal_bytes_do_not_join_an_unrelated_class(self):
        a,b,raw,expected,headers,members=self.fixture();headers[b]['object']=(1,8)
        with self.assertRaisesRegex(ValueError,'hardlink class'):materialize(expected,headers,members)

    def test_selected_hash_still_controls_alias_bytes(self):
        a,b,raw,expected,headers,members=self.fixture();members[1]=(b,stat.S_IFREG|0o644,b'wrong!')
        with self.assertRaisesRegex(ValueError,'selected data bytes'):materialize(expected,headers,members)

    def test_links_missing_and_conflicting_sizes_rejected(self):
        for which in ('symlink','missing','size'):
            a,b,raw,expected,headers,members=self.fixture()
            if which=='symlink':members[0]=(a,stat.S_IFLNK|0o777,b'target')
            elif which=='missing':members.pop(0)
            else:headers[a]['size']+=1
            with self.subTest(which=which),self.assertRaises(ValueError):materialize(expected,headers,members)


if __name__=='__main__':unittest.main()
