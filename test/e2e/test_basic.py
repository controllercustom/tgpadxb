"""End-to-end hardware tests for tgpadxb.

Drives the real board over its WebSocket control port (81) and observes the
USB HID gamepad output via python3-evdev. These are skipped unless a board is
reachable (set TGPADXB_HOST, or rely on the default tgpadxb.local mDNS name).

Requirements:
  - python3-evdev (system package)
  - websocket-client (in test/requirements.txt)
  - root (the harness grabs the evdev device so events don't leak to console)

Run:
  sudo TGPADXB_HOST=192.168.1.x python3 -m pytest test/e2e -v
"""

import os
import time
import threading

import pytest
import evdev
from evdev import InputDevice, ecodes

import websocket


HOST = os.environ.get('TGPADXB_HOST') or 'tgpadxb.local'
WS_URL = 'ws://%s:81/' % HOST

# XInput -> evdev (xpad driver) event code mapping.
BTN_CODE = {
    '*A': ecodes.BTN_A,
    '*B': ecodes.BTN_B,
    '*X': ecodes.BTN_X,
    '*Y': ecodes.BTN_Y,
    '*LB': ecodes.BTN_TL,
    '*RB': ecodes.BTN_TR,
    '*Back': ecodes.BTN_SELECT,
    '*Start': ecodes.BTN_START,
    '*LThumb': ecodes.BTN_THUMBL,
    '*RThumb': ecodes.BTN_THUMBR,
    '*Xbox': ecodes.BTN_MODE,
}

TRIGGER_CODE = {
    '*LT': ecodes.ABS_Z,
    '*RT': ecodes.ABS_RZ,
}


def candidate_codes(tok):
    if tok in BTN_CODE:
        return [BTN_CODE[tok]]
    return []


def _find_gamepad():
    jsdev = os.environ.get('JSDEV')
    if jsdev:
        if jsdev.startswith('/dev/input/js'):
            base = os.path.basename(jsdev)
            js_link = os.path.realpath('/sys/class/input/%s/device' % base)
            for ent in sorted(os.listdir(js_link)):
                if not ent.startswith('event'):
                    continue
                dev_path = '/dev/input/%s' % ent
                if not os.path.exists(dev_path):
                    continue
                d = InputDevice(dev_path)
                n = d.name.lower()
                if 'motion sensors' in n or 'touchpad' in n:
                    continue
                return d
        else:
            return InputDevice(jsdev)
    candidates = []
    for p in evdev.list_devices():
        d = InputDevice(p)
        n = d.name.lower()
        if 'xbox' in n or 'xpad' in n or 'gamepad' in n or 'controller' in n:
            candidates.append(d)
    if candidates:
        return candidates[0]
    for p in evdev.list_devices():
        d = InputDevice(p)
        caps = d.capabilities()
        if ecodes.EV_KEY in caps and ecodes.EV_ABS in caps:
            return d
    return None


class EventWatcher:
    def __init__(self, dev):
        self.dev = dev
        self.events = []
        self.lock = threading.Lock()
        self.stop = False
        self.t = threading.Thread(target=self._run, daemon=True)

    def _run(self):
        while not self.stop:
            try:
                for e in self.dev.read_loop():
                    with self.lock:
                        self.events.append((e.type, e.code, e.value))
                    if self.stop:
                        break
            except OSError:
                break

    def start(self):
        self.t.start()

    def stop_now(self):
        self.stop = True

    def clear(self):
        with self.lock:
            self.events = []

    def has(self, etype, code, value):
        with self.lock:
            return (etype, code, value) in self.events

    def has_any_code(self, etype, codes, value):
        with self.lock:
            return any((etype, c, value) in self.events for c in codes)

    def abs_values(self, code):
        with self.lock:
            return [v for (t, c, v) in self.events if c == code]

    def wait_for(self, etype, codes, value, timeout=4.0, poll=0.02):
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                if any((etype, c, value) in self.events for c in codes):
                    return True
            time.sleep(poll)
        return False


def open_ws():
    return websocket.create_connection(WS_URL, timeout=5)


from contextlib import contextmanager

@contextmanager
def using_ws():
    ws = open_ws()
    try:
        yield ws
    finally:
        try:
            ws.close()
        except Exception:
            pass


@pytest.fixture(scope='module')
def harness():
    d = _find_gamepad()
    if d is None:
        pytest.skip("No XInput evdev device found")
    try:
        d.grab()
    except OSError:
        pytest.skip("Could not grab evdev device (need root?)")
    w = EventWatcher(d)
    w.start()
    time.sleep(0.3)
    yield (d, w)
    w.stop_now()
    try:
        d.ungrab()
    except Exception:
        pass


def _press_release(ws, w, tok, settle=0.25):
    w.clear()
    time.sleep(0.05)
    ws.send(tok)
    time.sleep(settle)
    ws.send('~' + tok)
    time.sleep(settle + 0.1)


_ALL_RELEASE = ['*A', '*B', '*X', '*Y', '*LB', '*RB',
                '*Start', '*Back', '*LThumb', '*RThumb', '*Xbox',
                '*LX', '*LY', '*RX', '*RY', '*DPAD', '*LT', '*RT',
                '*LTC', '*RTC']


def neutralize(ws, w, wait=0.4):
    for tok in _ALL_RELEASE:
        try:
            ws.send('~' + tok)
        except Exception:
            pass
    time.sleep(wait)
    w.clear()


@pytest.mark.e2e
def test_a_button(harness):
    dev, w = harness
    with using_ws() as ws:
        _press_release(ws, w, '*A')
    codes = candidate_codes('*A')
    assert w.has_any_code(ecodes.EV_KEY, codes, 1), "expected A press"
    assert w.has_any_code(ecodes.EV_KEY, codes, 0), "expected A release"


@pytest.mark.e2e
def test_face_buttons(harness):
    dev, w = harness
    with using_ws() as ws:
        for tok in ['*B', '*X', '*Y']:
            _press_release(ws, w, tok)
            codes = candidate_codes(tok)
            assert w.has_any_code(ecodes.EV_KEY, codes, 1), "expected press for %s" % tok
            assert w.has_any_code(ecodes.EV_KEY, codes, 0), "expected release for %s" % tok


@pytest.mark.e2e
def test_shoulder_and_menu_buttons(harness):
    dev, w = harness
    with using_ws() as ws:
        for tok in ['*LB', '*RB', '*Back', '*Start', '*Xbox']:
            _press_release(ws, w, tok)
            codes = candidate_codes(tok)
            assert w.has_any_code(ecodes.EV_KEY, codes, 1), "expected press for %s" % tok
            assert w.has_any_code(ecodes.EV_KEY, codes, 0), "expected release for %s" % tok


@pytest.mark.e2e
def test_stick_click_toggles(harness):
    dev, w = harness
    with using_ws() as ws:
        for tok in ['*LThumb', '*RThumb']:
            codes = candidate_codes(tok)
            w.clear(); time.sleep(0.05)
            ws.send(tok); time.sleep(0.2)
            assert w.has_any_code(ecodes.EV_KEY, codes, 1), "expected %s press" % tok
            ws.send('~' + tok); time.sleep(0.2)
            assert w.has_any_code(ecodes.EV_KEY, codes, 0), "expected %s release" % tok


@pytest.mark.e2e
def test_left_stick_axis(harness):
    dev, w = harness
    with using_ws() as ws:
        w.clear(); time.sleep(0.05)
        ws.send('*LX:100'); time.sleep(0.2)
        moved = w.abs_values(ecodes.ABS_X)
        ws.send('*LX:0'); time.sleep(0.3)
        rested = w.abs_values(ecodes.ABS_X)
    assert any(v > 128 for v in moved), "expected ABS_X to move above center"
    assert any(v == 0 for v in rested), "expected ABS_X back to center (0)"


@pytest.mark.e2e
def test_dpad_hat(harness):
    dev, w = harness
    with using_ws() as ws:
        w.clear(); time.sleep(0.05)
        ws.send('*DPAD:0'); time.sleep(0.25)
        ws.send('*DPAD:8'); time.sleep(0.3)
    assert w.has(ecodes.EV_ABS, ecodes.ABS_HAT0Y, -1), "expected HAT0Y=-1 (up)"
    assert w.has(ecodes.EV_ABS, ecodes.ABS_HAT0Y, 0), "expected HAT0Y centered"


@pytest.mark.e2e
def test_lt_trigger_axis(harness):
    dev, w = harness
    with using_ws() as ws:
        w.clear(); time.sleep(0.05)
        ws.send('*LT:32768'); time.sleep(0.2)
        moved = w.abs_values(ecodes.ABS_Z)
        ws.send('*LT:0'); time.sleep(0.3)
        rested = w.abs_values(ecodes.ABS_Z)
    assert any(v > 0 for v in moved), "expected ABS_Z (LT) to fire"
    assert any(v == 0 for v in rested), "expected ABS_Z back to 0"


@pytest.mark.e2e
def test_rt_trigger_axis(harness):
    dev, w = harness
    with using_ws() as ws:
        w.clear(); time.sleep(0.05)
        ws.send('*RT:20000'); time.sleep(0.2)
        moved = w.abs_values(ecodes.ABS_RZ)
        ws.send('*RT:0'); time.sleep(0.3)
        rested = w.abs_values(ecodes.ABS_RZ)
    assert any(v > 0 for v in moved), "expected ABS_RZ (RT) to fire"
    assert any(v == 0 for v in rested), "expected ABS_RZ back to 0"


@pytest.mark.e2e
def test_lt_cruise_control_locks_after_release(harness):
    dev, w = harness
    with using_ws() as ws:
        w.clear(); time.sleep(0.05)
        ws.send('*LT:25000'); time.sleep(0.1)
        ws.send('*LTC'); time.sleep(0.1)     # lock CC
        ws.send('~*LT'); time.sleep(0.2)     # release slider
        locked = w.abs_values(ecodes.ABS_Z)
        ws.send('~*LTC'); time.sleep(0.2)    # unlock CC
        after = w.abs_values(ecodes.ABS_Z)
    assert any(v > 0 for v in locked), "expected ABS_Z held by CC after slider release"
    assert any(v == 0 for v in after), "expected ABS_Z released after CC unlock"


@pytest.fixture(scope='function')
def two_clients():
    try:
        a = websocket.create_connection(WS_URL, timeout=5)
    except Exception as e:
        pytest.skip("Cannot reach WebSocket (client A): %s" % e)
    try:
        b = websocket.create_connection(WS_URL, timeout=5)
    except Exception:
        a.close()
        pytest.skip("Cannot open second WebSocket client")
    yield (a, b)
    try: a.close()
    except Exception: pass
    try: b.close()
    except Exception: pass


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_independent(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    w.clear(); time.sleep(0.1)
    a.send('*A'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*A'), 1), "A's A press lost"
    b.send('*B'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*B'), 1), "B's B press lost"
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*A'), 1), "A's A cleared by B connect"
    b.send('~*B'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*A'), 1), "A's A cleared by B release"
    a.send('~*A'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*A'), 0), "A's A release lost"


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_same_button_no_stuck(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    w.clear(); time.sleep(0.1)
    a.send('*A'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*A'), 1), "A's A press lost"
    b.send('*A'); time.sleep(0.1)
    b.send('~*A'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*A'), 1), "B dropped while A holds"
    w.clear(); time.sleep(0.1)
    a.send('~*A'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*A'), 0, timeout=6.0), "A stuck ON after A release"


def _heartbeat(ws, stop, period=2.0):
    import time as _t
    while not stop.is_set():
        try:
            ws.send('#PING')
        except Exception:
            pass
        stop.wait(period)


@pytest.mark.e2e
def test_single_client_sticky_held_survives_watchdog(harness):
    dev, w = harness
    ws = open_ws()
    try:
        w.clear(); time.sleep(0.1)
        ws.send('*RThumb'); time.sleep(0.2)
        assert w.wait_for(ecodes.EV_KEY, candidate_codes('*RThumb'), 1), "RThumb press lost"
        stop = threading.Event()
        t = threading.Thread(target=_heartbeat, args=(ws, stop), daemon=True)
        t.start()
        time.sleep(8)
        stop.set(); t.join(timeout=2)
        w.clear(); time.sleep(0.1)
        assert not w.wait_for(ecodes.EV_KEY, candidate_codes('*RThumb'), 0, timeout=1.0), \
            "RThumb was released by watchdog during the hold"
        ws.send('~*RThumb'); time.sleep(0.2)
        assert w.wait_for(ecodes.EV_KEY, candidate_codes('*RThumb'), 0, timeout=4.0), \
            "RThumb release not seen after explicit off"
    finally:
        try:
            ws.send('~*RThumb')
        except Exception:
            pass
        try:
            ws.close()
        except Exception:
            pass


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_sticky_lthumb_disconnect(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    neutralize(a, w); neutralize(b, w)
    stop = threading.Event()
    hb = threading.Thread(target=_heartbeat, args=(a, stop), daemon=True)
    hb.start()
    a.send('*LThumb'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 1), "A's LThumb press lost"
    b.send('*LThumb'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 1), "B's LThumb press lost"
    b.close()
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 1, timeout=8.0), \
        "A's LThumb dropped when B disconnected"
    w.clear(); time.sleep(0.1)
    a.send('~*LThumb'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 0, timeout=4.0), \
        "A's LThumb not released after explicit off"
    stop.set()


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_sticky_lthumb_rthumb_four_step(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    neutralize(a, w); neutralize(b, w)
    a.send('*LThumb'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 1), "step1 LThumb on"
    b.send('*LThumb'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 1), "step2 LThumb on"
    b.send('~*LThumb'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 1), "step3 LThumb still on (A holds)"
    a.send('~*LThumb'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*LThumb'), 0, timeout=4.0), "step4 LThumb off"
