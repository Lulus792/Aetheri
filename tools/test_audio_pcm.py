"""Ensure independent captured-PCM validation rejects concrete corruptions."""
import math
import struct
import unittest
from test_audio import validate_pcm


def tone(frames=257,gain=1.0):
    result = bytearray(16 * 8)
    for i in range(frames):
        result += struct.pack('<ff', *[gain * .125 * math.sin(2 * math.pi * f * i / 48000) for f in (440, 880)])
    result += bytes(16 * 8)
    return bytes(result)


class PCM(unittest.TestCase):
    def test_realistic_padding_and_partial_quantum(self):
        result = validate_pcm(tone(), 257)
        self.assertEqual(result['signal_start'], 16)
        self.assertEqual(result['signal_frames'], 257)

    def test_silence_is_a_separate_control_not_tone_acceptance(self):
        self.assertTrue(validate_pcm(bytes(512 * 8), 257, True)['silent'])
        with self.assertRaises(AssertionError): validate_pcm(bytes(512 * 8), 257)
        with self.assertRaises(AssertionError): validate_pcm(tone(), 257, True)

    def test_dropped_initial_right_channel_is_detected(self):
        raw = bytearray(tone())
        for i in range(16, 48): raw[i * 8 + 4:i * 8 + 8] = bytes(4)
        with self.assertRaises(AssertionError): validate_pcm(raw, 257)

    def test_swapped_channels_and_missing_frame_are_detected(self):
        raw = tone()
        swapped = b''.join(struct.pack('<ff', b, a) for a, b in struct.iter_unpack('<ff', raw))
        for changed in (swapped, raw[:80 * 8] + raw[81 * 8:]):
            with self.assertRaises(AssertionError): validate_pcm(changed, 257)

    def test_nonfinite_truncated_and_unframed_data_are_detected(self):
        raw = tone()
        for changed in (raw[:80 * 8], raw + b'x', struct.pack('<f', float('nan')) + raw[4:], b''):
            with self.assertRaises(AssertionError): validate_pcm(changed, 257)

    def test_requested_gain_requires_actual_scaled_samples(self):
        self.assertEqual(validate_pcm(tone(gain=.125),257,gain=.125)['signal_frames'],257)
        with self.assertRaises(AssertionError):validate_pcm(tone(),257,gain=.125)
        with self.assertRaises(AssertionError):validate_pcm(tone(gain=.5),257,gain=.125)

    def test_invalid_expected_gain_is_refused(self):
        for gain in (0,-1,1.1,float('nan'),float('inf')):
            with self.assertRaises(ValueError):validate_pcm(tone(),257,gain=gain)


if __name__ == '__main__': unittest.main()
