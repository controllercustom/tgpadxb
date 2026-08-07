"""Gamepad UI data integrity tests (offline)."""
import os
import pytest

WEBPAGE = os.path.join(os.path.dirname(__file__), '..', 'webpage.h')


def _webpage_src():
    with open(WEBPAGE, 'r', encoding='utf-8') as f:
        return f.read()


def test_all_11_hold_buttons_present(gamepad_ui_buttons):
    expected = {'*A', '*B', '*X', '*Y', '*LB', '*RB',
                '*Back', '*Start', '*LThumb', '*RThumb', '*Xbox'}
    assert expected.issubset(set(gamepad_ui_buttons.keys()))


def test_trigger_controls_present(gamepad_ui_buttons):
    expected = {'*LT', '*RT', '*LTC', '*RTC'}
    assert expected.issubset(set(gamepad_ui_buttons.keys()))


def test_no_duplicate_tokens(gamepad_ui_buttons):
    toks = list(gamepad_ui_buttons.keys())
    assert len(toks) == len(set(toks))


def test_dpad_tokens_present(dpad_tokens):
    assert set(dpad_tokens.keys()) == {'*DPAD'}


def test_axis_tokens_present(axis_tokens):
    assert set(axis_tokens.keys()) == {'*LX', '*LY', '*RX', '*RY'}


def test_button_tokens_distinct_from_dpad_and_axes(ws_button_tokens, dpad_tokens, axis_tokens, trigger_tokens):
    b = set(ws_button_tokens.keys())
    d = set(dpad_tokens.keys())
    a = set(axis_tokens.keys())
    t = trigger_tokens['analog'] | trigger_tokens['cc']
    assert b.isdisjoint(d)
    assert b.isdisjoint(a)
    assert b.isdisjoint(t)
    assert d.isdisjoint(a)


def test_webpage_has_xbox_labels():
    src = _webpage_src()
    assert "'LB'" in src
    assert "'RB'" in src
    assert "'BACK'" in src
    assert "'START'" in src
    assert "'LS'" in src
    assert "'RS'" in src
    assert "'CC'" in src
    assert 'xinput-led' in src


def test_webpage_no_old_nintendo_tokens():
    src = _webpage_src()
    for old in ["'*Sq'", "'*Tr'", "'*O'", "'ZL'", "'ZR'", "'Cap'", "'*L2'", "'*R2'"]:
        assert old not in src


def test_webpage_led_panel_present():
    src = _webpage_src()
    assert 'id="led-panel"' in src
    assert '#LED:' in src
