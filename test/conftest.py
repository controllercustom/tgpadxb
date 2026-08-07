"""Shared fixtures for tgpadxb test suite."""
import os

import pytest

_MCLIENT_MSG = (
    "multi-client test: requires TGPADXB_MULTICLIENT=1 (a board whose "
    "WebSocketsServer reports distinct per-client slot indices, e.g. the "
    "generic ESP32-S3 dev module). The M5Stack AtomS3 reports slot 0 for "
    "every client, so multi-client OR-combine cannot be verified there."
)


def pytest_collection_modifyitems(config, items):
    enabled = os.environ.get("TGPADXB_MULTICLIENT", "") == "1"
    if enabled:
        return
    skip_mc = pytest.mark.skip(reason=_MCLIENT_MSG)
    for item in items:
        if "multiclient" in item.keywords:
            item.add_marker(skip_mc)


@pytest.fixture
def button_constants():
    """Button indices matching ESP32XInputClass::Button enum (from ESP32XInput.h)."""
    return {
        'DPAD_UP':        0,
        'DPAD_DOWN':      1,
        'DPAD_LEFT':      2,
        'DPAD_RIGHT':     3,
        'START':          4,
        'BACK':           5,
        'LEFT_THUMB':     6,
        'RIGHT_THUMB':    7,
        'LEFT_SHOULDER':  8,
        'RIGHT_SHOULDER': 9,
        'XBOX':           10,
        'A':              12,
        'B':              13,
        'X':              14,
        'Y':              15,
        'BUTTON_COUNT':   16,
    }


@pytest.fixture
def hat_constants():
    """Hat direction values for ESP32XInput.setHat(): 0-7 directions, 8=centered."""
    return {
        'CENTERED':  8,
        'N':         0,
        'NE':        1,
        'E':         2,
        'SE':        3,
        'S':         4,
        'SW':        5,
        'W':         6,
        'NW':        7,
    }


@pytest.fixture
def ws_button_tokens():
    """Mapping of WS button tokens -> XInput button enum index (from tgpadxb.ino btnIndex())."""
    return {
        '*A':      12,  # A
        '*B':      13,  # B
        '*X':      14,  # X
        '*Y':      15,  # Y
        '*LB':     8,   # LEFT_SHOULDER
        '*RB':     9,   # RIGHT_SHOULDER
        '*Start':  4,   # START
        '*Back':   5,   # BACK
        '*LThumb': 6,   # LEFT_THUMB
        '*RThumb': 7,   # RIGHT_THUMB
        '*Xbox':   10,  # XBOX
    }


@pytest.fixture
def trigger_tokens():
    """Analog trigger / cruise-control WS tokens."""
    return {
        'analog':   {'*LT', '*RT'},
        'cc':       {'*LTC', '*RTC'},
    }


@pytest.fixture
def dpad_tokens():
    """Directional pad WS token -> setHat direction values (0-7, 8=centered).

    Same convention as tgpadns: *DPAD:<n>, n = 0..7 (N, NE, E, SE, S, SW, W, NW),
    8 = centered. The firmware passes the value straight through to setHat().
    """
    mapping = {
        0: 0,  # N
        1: 1,  # NE
        2: 2,  # E
        3: 3,  # SE
        4: 4,  # S
        5: 5,  # SW
        6: 6,  # W
        7: 7,  # NW
        8: 8,  # centered
    }
    return {'*DPAD': mapping}


@pytest.fixture
def axis_tokens():
    """Analog axis WS tokens -> (signed?, invert-Y?). Triggers handled separately."""
    return {
        '*LX': (True,  False),
        '*LY': (True,  False),
        '*RX': (True,  False),
        '*RY': (True,  False),
    }


@pytest.fixture
def gamepad_ui_buttons():
    """Button definitions rendered by webpage.h (label -> token)."""
    return {
        '*DPAD': 'DPad',
        '*A': 'A', '*B': 'B', '*X': 'X', '*Y': 'Y',
        '*LB': 'LB', '*RB': 'RB',
        '*LT': 'LT', '*RT': 'RT',
        '*LTC': 'CC', '*RTC': 'CC',
        '*Back': 'BACK', '*Start': 'START',
        '*Xbox': 'Xbox',
        '*LThumb': 'LS', '*RThumb': 'RS',
    }
