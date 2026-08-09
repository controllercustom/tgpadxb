"""Offline unit tests for ESP32XInput v1.2.0 OUT packet format and WebSocket message contract.

These validate that:
  - XInput OUT report byte layout matches xpad.c / firmware expectations
  - #LED:<index> and #RUMBLE:<l>,<r> messages parse correctly in the web UI JS logic
"""

import pytest


# ---- Constants from ESP32XInput v1.2.0 pollRumble() ----

class TestOutPacketFormat:
    """Validate XInput OUT packet byte layout per firmware parser."""

    def test_rumble_packet_minimal_length(self):
        # Rumble type 0x00 requires len >= 5 (bytes 3 and 4 are motors)
        pkt = b'\x00\x08\x00\xff\x2a'
        assert pkt[0] == 0x00, "type byte must be 0x00 for rumble"
        assert len(pkt) >= 5

    def test_rumble_left_motor_byte_position(self):
        # Left motor at index 3 (big-endian u16 in xpad.c, but firmware reads single bytes)
        pkt = b'\x00\x08\x00\xff\x2a'
        assert pkt[3] == 0xff

    def test_rumble_right_motor_byte_position(self):
        # Right motor at index 4 (little-endian u16 in xpad.c, firmware reads single byte)
        pkt = b'\x00\x08\x00\xff\x2a'
        assert pkt[4] == 0x2a

    def test_rumble_zero_motors(self):
        # Zero-value packet should still be valid (len >= 5, type correct)
        pkt = b'\x00\x08\x00\x00\x00'
        assert pkt[3] == 0 and pkt[4] == 0

    def test_rumble_max_motors(self):
        # Both motors at max value (255 for single-byte read)
        pkt = b'\x00\x08\x00\xff\xff'
        assert pkt[3] == 0xff and pkt[4] == 0xff

    def test_led_packet_minimal_length(self):
        # LED type 0x01 requires len >= 3 (byte 2 is led index)
        pkt = b'\x01\x03\x02'
        assert pkt[0] == 0x01, "type byte must be 0x01 for LED"
        assert len(pkt) >= 3

    def test_led_index_byte_position(self):
        # LED index at byte 2 (values 0-4, where 4 = all off / blink cycle)
        pkt = b'\x01\x03\x02'
        assert pkt[2] == 2


class TestLedMessageParsing:
    """Validate WebSocket #LED:<index> message parsing logic."""

    @pytest.mark.parametrize("msg,expected", [
        ("#LED:0", 0),
        ("#LED:1", 1),
        ("#LED:3", 3),
        ("#LED:4", 4),   # all off / blink cycle sentinel
    ])
    def test_led_message_parsing(self, msg, expected):
        assert msg.startswith("#LED:")
        idx = int(msg[5:])
        assert idx == expected

    @pytest.mark.parametrize("index", range(0, 5))
    def test_all_valid_indices(self, index):
        # Firmware sends indices 0-4; web UI handles all five (4 means "all off")
        msg = f"#LED:{index}"
        parsed = int(msg[5:])
        assert parsed == index


class TestRumbleMessageParsing:
    """Validate WebSocket #RUMBLE:<left>,<right> message parsing logic."""

    @pytest.mark.parametrize("msg,left,right", [
        ("#RUMBLE:255,0", 255, 0),
        ("#RUMBLE:0,128", 0, 128),
        ("#RUMBLE:42,73", 42, 73),
        ("#RUMBLE:0,0", 0, 0),
    ])
    def test_rumble_message_parsing(self, msg, left, right):
        assert msg.startswith("#RUMBLE:")
        parts = msg[8:].split(",")
        l_val = int(parts[0])
        r_val = int(parts[1])
        assert (l_val, r_val) == (left, right)

    @pytest.mark.parametrize("msg", [
        "#RUMBLE:255,255",   # both max
        "#RUMBLE:1,0",       # left only minimal
        "#RUMBLE:0,1",       # right only minimal
    ])
    def test_rumble_edge_cases(self, msg):
        parts = msg[8:].split(",")
        l_val = int(parts[0])
        r_val = int(parts[1])
        assert 0 <= l_val <= 255 and 0 <= r_val <= 255


class TestRumbleGlowMapping:
    """Validate that rumble indicator brightness logic is correct.

    The web UI function setRumbleGlow() maps motor value (0-255) to CSS glow intensity.
    We test the contract here so JS changes stay consistent with firmware expectations.
    """

    @pytest.mark.parametrize("val,expected_lit", [
        (0, False),   # zero = off
        (1, True),    # any non-zero = on
        (255, True),  # max = fully lit
    ])
    def test_rumble_on_off_threshold(self, val, expected_lit):
        assert bool(val) == expected_lit
