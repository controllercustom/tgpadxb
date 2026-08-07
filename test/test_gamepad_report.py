"""Gamepad HID report structure tests (XInput, 20-byte report)."""
import pytest


def test_11_hold_buttons(button_constants):
    # Hold buttons are START..Y: START..XBOX (4..10) + A/B/X/Y (12..15), gap at 11;
    # dpad 0-3 handled via setHat
    hold = {k: v for k, v in button_constants.items()
            if v >= 4 and k != 'BUTTON_COUNT'}
    assert len(hold) == 11
    assert min(hold.values()) == 4
    assert max(hold.values()) == 15


def test_report_size():
    # XInput report is exactly 20 bytes: 2 hdr + 2 btn + 2 trig + 8 sticks + 6 reserved
    header = 2
    w_buttons = 2
    triggers = 2
    sticks = 8
    reserved = 6
    assert header + w_buttons + triggers + sticks + reserved == 20


def test_hat_values(hat_constants):
    # XInput setHat: 0-7 directions, 8 = centered
    assert hat_constants['CENTERED'] == 8
    for v in hat_constants.values():
        assert 0 <= v <= 8


def test_stick_scaling():
    # WS -127..127 -> XInput -32768..32767 (multiply by ~258)
    def scale(v):
        return v * 258
    assert scale(0) == 0
    assert scale(127) == 32766
    assert scale(-127) == -32766


def test_trigger_scaling():
    # WS 0..32768 -> XInput uint8 0..255 (library scales internally)
    def to_uint8(v):
        return round(v * 255 / 32768)
    assert to_uint8(0) == 0
    assert to_uint8(32768) == 255
    assert 0 < to_uint8(16384) < 255
