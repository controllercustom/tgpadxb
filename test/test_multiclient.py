"""Offline tests for true multi-client (per-client) reference counting.

These mirror the per-client recompute model in tgpadxb.ino:

  - Buttons are OR-combined across all active clients.
  - Sticks / d-pad / live triggers take the value of the most recently active
    contributing client (last-writer-wins); fall back to center/zero when no
    client contributes.
  - Cruise control (CC): a client that has a trigger locked ON keeps its
    locked value regardless of axis freshness until it explicitly unlocks.
  - A client connecting does NOT clear other clients' held inputs.
  - A client disconnecting (or going silent >5s) releases ONLY its own
    contributions; remaining clients keep theirs.

The recompute function below is a faithful port of recomputeAndSend() so the
offline suite validates the same behavior the firmware runs.
"""
import pytest

MAX_WS_CLIENTS = 5
CENTERED = 8
BUTTON_LO = 4  # first non-dpad XInput button (START)


class ClientState:
    def __init__(self):
        self.active = False
        self.last_seen = 0
        self.btn = [False] * 16
        self.lx = self.ly = self.rx = self.ry = 0
        self.dpad_dir = CENTERED
        self.lt_cc_on = False
        self.rt_cc_on = False
        self.lt_val = 0
        self.rt_val = 0
        self.lt_cc = 0
        self.rt_cc = 0
        self.axis_ts = self.dpad_ts = 0


def recompute(clients, now):
    """Return the effective gamepad state from all active clients.

    Mirrors recomputeAndSend() in tgpadxb.ino.
    """
    btn_ref = [0] * 16
    for c in clients:
        if not c.active:
            continue
        for b in range(BUTTON_LO, 16):
            if c.btn[b]:
                btn_ref[b] += 1

    lx = ly = rx = ry = 0
    dpad = CENTERED
    best_axis = best_dpad = 0

    for c in clients:
        if not c.active:
            continue
        if c.axis_ts >= best_axis:
            best_axis = c.axis_ts
            lx, ly, rx, ry = c.lx, c.ly, c.rx, c.ry
        if c.dpad_ts >= best_dpad:
            best_dpad = c.dpad_ts
            dpad = c.dpad_dir

    lt = rt = 0
    lt_ts = rt_ts = 0
    for c in clients:
        if not c.active:
            continue
        if c.lt_cc_on and c.lt_cc > 0:
            lt = c.lt_cc
            lt_ts = float('inf')
        elif c.axis_ts >= lt_ts:
            lt_ts = c.axis_ts
            lt = c.lt_val
        if c.rt_cc_on and c.rt_cc > 0:
            rt = c.rt_cc
            rt_ts = float('inf')
        elif c.axis_ts >= rt_ts:
            rt_ts = c.axis_ts
            rt = c.rt_val

    return {
        'buttons': [btn_ref[b] > 0 for b in range(15)],
        'btn_ref': btn_ref[:15],
        'lx': lx, 'ly': ly, 'rx': rx, 'ry': ry,
        'dpad': dpad,
        'lt': lt, 'rt': rt,
    }


def fresh_clients():
    return [ClientState() for _ in range(MAX_WS_CLIENTS)]


def test_two_clients_both_buttons_pressed():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[11] = True   # A
    clients[1].active = True
    clients[1].btn[12] = True   # B
    state = recompute(clients, now=1000)
    assert state['buttons'][11] is True
    assert state['buttons'][12] is True
    assert state['btn_ref'][11] == 1
    assert state['btn_ref'][12] == 1


def test_two_clients_same_button_or_combined():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[11] = True
    clients[1].active = True
    clients[1].btn[11] = True
    state = recompute(clients, now=1000)
    assert state['buttons'][11] is True
    assert state['btn_ref'][11] == 2
    clients[0].btn[11] = False
    state = recompute(clients, now=1001)
    assert state['buttons'][11] is True
    clients[1].btn[11] = False
    state = recompute(clients, now=1002)
    assert state['buttons'][11] is False


def test_connect_does_not_clear_others():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[11] = True
    clients[1] = ClientState()
    clients[1].active = True
    state = recompute(clients, now=1000)
    assert state['buttons'][11] is True


def test_disconnect_releases_only_own_button():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[11] = True
    clients[1].active = True
    clients[1].btn[12] = True
    clients[1] = ClientState()
    state = recompute(clients, now=1000)
    assert state['buttons'][11] is True
    assert state['buttons'][12] is False


def test_axis_last_writer_wins_across_clients():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lx = 100
    clients[0].axis_ts = 100
    clients[1].active = True
    clients[1].lx = -50
    clients[1].axis_ts = 200
    state = recompute(clients, now=300)
    assert state['lx'] == -50
    clients[1] = ClientState()
    state = recompute(clients, now=301)
    assert state['lx'] == 100


def test_axis_centers_when_no_contributor():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lx = 100
    clients[0].axis_ts = 100
    clients[0] = ClientState()
    state = recompute(clients, now=200)
    assert state['lx'] == 0
    assert state['dpad'] == CENTERED


def test_trigger_last_writer_wins_across_clients():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lt_val = 5000
    clients[0].axis_ts = 100
    clients[1].active = True
    clients[1].lt_val = 9000
    clients[1].axis_ts = 200
    state = recompute(clients, now=300)
    assert state['lt'] == 9000
    clients[1] = ClientState()
    state = recompute(clients, now=301)
    assert state['lt'] == 5000


def test_cc_lock_wins_over_live_value():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lt_val = 5000
    clients[0].axis_ts = 100
    clients[1].active = True
    clients[1].lt_val = 12000
    clients[1].lt_cc_on = True
    clients[1].lt_cc = 8000
    clients[1].axis_ts = 200
    state = recompute(clients, now=300)
    # client 1 is freshest but its CC-locked value (8000) is what must win
    assert state['lt'] == 8000


def test_cc_lock_persists_after_live_release():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lt_val = 0          # slider released
    clients[0].axis_ts = 200
    clients[0].lt_cc_on = True
    clients[0].lt_cc = 8000        # locked earlier
    state = recompute(clients, now=300)
    assert state['lt'] == 8000


def test_cc_unlock_returns_to_live():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lt_val = 1000
    clients[0].axis_ts = 100
    clients[0].lt_cc_on = True
    clients[0].lt_cc = 8000
    state = recompute(clients, now=150)
    assert state['lt'] == 8000
    clients[0].lt_cc_on = False
    clients[0].lt_cc = 0
    state = recompute(clients, now=151)
    assert state['lt'] == 1000


def test_cc_lock_persists_across_other_client_disconnect():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lt_cc_on = True
    clients[0].lt_cc = 8000
    clients[0].axis_ts = 100
    clients[1].active = True
    clients[1].rt_val = 9000
    clients[1].axis_ts = 200
    state = recompute(clients, now=300)
    assert state['lt'] == 8000
    assert state['rt'] == 9000
    clients[1] = ClientState()
    state = recompute(clients, now=301)
    assert state['lt'] == 8000
    assert state['rt'] == 0


def test_watchdog_silent_client_released_others_kept():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[11] = True
    clients[0].last_seen = 0
    clients[1].active = True
    clients[1].btn[12] = True
    clients[1].last_seen = 10000

    now = 10000
    released = False
    for c in clients:
        if not c.active:
            continue
        if now - c.last_seen > 5000:
            held = any(c.btn) or (c.lx or c.ly or c.rx or c.ry) or c.dpad_dir != CENTERED or c.lt_val or c.rt_val or c.lt_cc or c.rt_cc
            if held:
                c.btn = [False] * 16
                released = True
    assert released is True
    state = recompute(clients, now=now)
    assert state['buttons'][11] is False
    assert state['buttons'][12] is True


def _apply_watchdog(clients, now, timeout=5000):
    released = False
    for c in clients:
        if not c.active:
            continue
        if now - c.last_seen > timeout:
            held = any(c.btn) or (c.lx or c.ly or c.rx or c.ry) or c.dpad_dir != CENTERED or c.lt_val or c.rt_val or c.lt_cc or c.rt_cc
            if held:
                c.btn = [False] * 16
                c.axis_ts = c.dpad_ts = 0
                c.lt_val = c.rt_val = 0
                c.lt_cc = c.rt_cc = 0
                released = True
            else:
                c.last_seen = now
    return released


def test_watchdog_keeps_heartbeating_client():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[4] = True   # START
    now = 100000
    for t in range(now, now + 20000, 2000):
        clients[0].last_seen = t
        assert _apply_watchdog(clients, t + 1) is False
    state = recompute(clients, now=now + 20000)
    assert state['buttons'][4] is True


def test_watchdog_releases_silent_client():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[4] = True
    clients[0].last_seen = 0
    assert _apply_watchdog(clients, 10000) is True
    state = recompute(clients, now=10000)
    assert state['buttons'][4] is False


def test_watchdog_releases_silent_cc_lock():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].rt_cc_on = True
    clients[0].rt_cc = 12000
    clients[0].last_seen = 0
    assert _apply_watchdog(clients, 10000) is True
    state = recompute(clients, now=10000)
    assert state['rt'] == 0


def test_two_clients_same_button_stuck_regression():
    clients = fresh_clients()
    clients[0].active = True
    clients[1].active = True

    clients[0].btn[11] = True
    state = recompute(clients, now=1000)
    assert state['buttons'][11] is True

    clients[1].btn[11] = True
    state = recompute(clients, now=1001)
    assert state['buttons'][11] is True
    assert state['btn_ref'][11] == 2

    clients[1].btn[11] = False
    state = recompute(clients, now=1002)
    assert state['buttons'][11] is True
    assert state['btn_ref'][11] == 1

    clients[0].btn[11] = False
    state = recompute(clients, now=1003)
    assert state['buttons'][11] is False
    assert state['btn_ref'][11] == 0
