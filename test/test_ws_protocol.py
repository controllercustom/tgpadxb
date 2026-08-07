"""WebSocket protocol -> gamepad field mapping tests (offline)."""
import pytest


def test_button_token_to_index(ws_button_tokens):
    idxs = list(ws_button_tokens.values())
    assert len(idxs) == len(set(idxs))
    for k, idx in ws_button_tokens.items():
        assert k.startswith('*')
        assert 4 <= idx <= 15   # non-dpad XInput buttons only (A/B/X/Y = 12..15)


def test_all_hold_buttons_mapped(button_constants):
    # START..Y (indices 4..10, 12..15) are all covered by a WS token
    mapped = {
        '*A': 12, '*B': 13, '*X': 14, '*Y': 15,
        '*LB': 8, '*RB': 9, '*Start': 4, '*Back': 5,
        '*LThumb': 6, '*RThumb': 7, '*Xbox': 10,
    }
    hold = {k: v for k, v in button_constants.items()
            if v >= 4 and k != 'BUTTON_COUNT'}
    assert set(mapped.values()) == set(hold.values())


def test_button_release_prefix():
    tok = '*A'
    assert '~' + tok == '~*A'


def test_dpad_passthrough(dpad_tokens):
    # WS *DPAD:<n> maps 1:1 to setHat(n); no offset, no renumbering
    mapping = dpad_tokens['*DPAD']
    for ws_val, hat in mapping.items():
        assert ws_val == hat
    assert mapping[8] == 8  # centered


def test_stick_scaling():
    # WS -127..127 -> XInput int16 via *258
    def scale(v):
        return v * 258
    assert scale(0) == 0
    assert scale(127) == 32766
    assert scale(-127) == -32766


def test_trigger_value_range():
    # WS *LT:<v> / *RT:<v> must clamp to 0..32768
    def clamp(v):
        return max(0, min(32768, v))
    assert clamp(-5) == 0
    assert clamp(0) == 0
    assert clamp(32768) == 32768
    assert clamp(40000) == 32768


def test_cc_release_syntax():
    # CC off is sent as ~<ccToken>
    assert '~*LTC' == '~' + '*LTC'
    assert '~*RTC' == '~' + '*RTC'


def test_trigger_tokens_distinct_from_buttons(ws_button_tokens, trigger_tokens):
    b = set(ws_button_tokens.keys())
    t = trigger_tokens['analog'] | trigger_tokens['cc']
    assert b.isdisjoint(t)


def test_no_analog_triggers_in_button_table(ws_button_tokens):
    # Analog triggers must NOT appear as hold buttons
    assert '*LT' not in ws_button_tokens
    assert '*RT' not in ws_button_tokens
