"""Host-only discovery/guard tests. Never opens a bus or changes a guest."""
from pathlib import Path
import unittest
from unittest.mock import Mock,patch

from audio_mac_bus import Bus,BusError
import test_audio_actor as actor


class Discovery(unittest.TestCase):
    def test_only_initial_name_absence_waits(self):
        absent=BusError(6,'org.freedesktop.DBus.Error.NameHasNoOwner','not registered yet')
        query=Mock(side_effect=[absent,':1.77',':1.88'])
        with patch.object(actor.time,'sleep') as sleep:
            self.assertEqual(actor.initial_owner(query),':1.77')
            self.assertEqual(query.call_count,2);sleep.assert_called_once_with(.025)

    def test_transport_and_other_remote_errors_never_retry(self):
        for error in (BusError(6,'org.freedesktop.DBus.Error.NoReply','unknown reply'),OSError(5,'transport')):
            query=Mock(side_effect=error)
            with patch.object(actor.time,'sleep') as sleep,self.assertRaises(OSError):actor.initial_owner(query)
            query.assert_called_once();sleep.assert_not_called()

    def test_discovery_deadline_preserves_original_error(self):
        error=BusError(6,'org.freedesktop.DBus.Error.NameHasNoOwner','not registered yet')
        query=Mock(side_effect=error)
        with patch.object(actor.time,'monotonic',side_effect=[0,6]),patch.object(actor.time,'sleep') as sleep:
            with self.assertRaises(BusError) as result:actor.initial_owner(query)
        self.assertIs(result.exception,error);query.assert_called_once();sleep.assert_not_called()

    def test_invalid_owner_and_conflicting_bus_selectors_fail(self):
        for name in ('org.freedesktop.systemd1','',':'+('x'*255),None):
            with self.assertRaises(ValueError):actor.initial_owner(lambda:name)
        with self.assertRaises(ValueError):Bus(private=True,user=True)

    def test_ordinary_user_is_rejected_before_guest_paths_or_writes(self):
        with patch.object(actor.os,'geteuid',return_value=1000,create=True),self.assertRaises(ValueError):
            actor.run(Path('/not-a-binary'),Path('/not-a-source'),Path('/not-an-output'))


if __name__=='__main__':unittest.main()
