/*
 * MIT License
 *
 * Copyright (c) 2026 controllercustom@myyahoo.com
 */

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
    <meta name="mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="default">
    <title>TGPad-XB 1.1.0 XInput Gamepad</title>
    <style>
        :root {
            --bg-chassis: #3a3f47;
            --deep-navy: #1a2b4c;
            --xbox-green: #52b043;
            --border-color: #20242a;
            --key-shadow-inset: inset -2px -2px 4px rgba(0,0,0,0.25), 2px 2px 5px rgba(0,0,0,0.35);
        }

        * {
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            -webkit-tap-highlight-color: transparent;
            user-select: none;
        }
        html, body {
            margin: 0; padding: 0;
            width: 100%; height: 100%;
            overflow: hidden;
            font-family: Helvetica, Arial, sans-serif;
            background-color: var(--bg-chassis);
            display: flex;
            flex-direction: column;
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            user-select: none;
            touch-action: none;
            overscroll-behavior-x: none;
            overscroll-behavior-y: none;
        }

        #pad {
            flex: 1;
            position: relative;
            margin: 4px;
            box-sizing: border-box;
            min-height: 0;
            touch-action: none;
        }

        .key {
            background-color: #4a4f57;
            border: 2px solid var(--border-color);
            box-shadow: var(--key-shadow-inset);
            display: flex; align-items: center; justify-content: center;
            font-weight: bold; cursor: pointer;
            transition: all 0.05s ease-in-out;
            user-select: none; -webkit-user-select: none; -webkit-touch-callout: none;
            border-radius: 8px;
            color: #eee; font-size: 3.2vmin;
            line-height: 1; text-align: center;
            box-sizing: border-box; padding: 0;
            touch-action: none;
            overflow: hidden;
            position: absolute;
        }
        .key:active {
            transform: translateY(1px);
            box-shadow: inset 2px 2px 4px rgba(0,0,0,0.3), 0 1px 2px rgba(0,0,0,0.4);
        }

        .dpad8 {
            background:
                conic-gradient(from -22.5deg,
                    rgba(26,43,76,0.95) 0deg 45deg,
                    rgba(46,70,120,0.95) 45deg 90deg,
                    rgba(26,43,76,0.95) 90deg 135deg,
                    rgba(46,70,120,0.95) 135deg 180deg,
                    rgba(26,43,76,0.95) 180deg 225deg,
                    rgba(46,70,120,0.95) 225deg 270deg,
                    rgba(26,43,76,0.95) 270deg 315deg,
                    rgba(46,70,120,0.95) 315deg 360deg);
            border: 3px solid #1a2b4c;
            box-shadow: inset 0 0 8px rgba(0,0,0,0.35), 2px 2px 6px rgba(0,0,0,0.25);
            border-radius: 50%;
            position: absolute;
            box-sizing: border-box;
        }
        .dpad8-sector {
            fill: rgba(207, 224, 255, 0.75);
            transition: fill 0.1s;
        }
        .dpad8-sector.btn-active {
            fill: #fff;
            filter: drop-shadow(0 0 6px #ffd86b) drop-shadow(0 0 3px #ffd86b);
        }
        .dpad8-hub {
            position: absolute;
            left: 50%; top: 50%;
            width: 26%; height: 26%;
            transform: translate(-50%, -50%);
            background: rgba(255,255,255,0.18);
            border-radius: 50%;
            border: 2px solid rgba(255,255,255,0.35);
            pointer-events: none;
        }

        .round {
            border-radius: 50%;
            aspect-ratio: 1 / 1;
            height: auto !important;
        }

        .face {
            font-size: 7vmin;
            border: 2px solid #20242a !important;
        }
        .face.btn-active { filter: brightness(1.4); }

        .btn-active {
            box-shadow: inset 2px 2px 10px rgba(0,0,0,0.5) !important;
            transform: translateY(1px);
        }

        .bumper {
            background-color: #7a7f8a !important;
            color: #fff;
        }
        .menu-key {
            background-color: #8a8a8a !important;
            color: #fff;
            font-size: 2.4vmin;
        }
        .xbox-key {
            background-color: #222 !important;
            color: #7fc95e;
            border-radius: 50%;
            font-size: 5vmin;
        }
        .sticky {
            background-color: #c77d4a !important;
            color: #fff;
            font-size: 2.2vmin;
            border: 2px solid #8a5a2a !important;
        }
        .sticky.btn-active { background-color: var(--xbox-green) !important; }

        /* ---- Analog trigger sliders ---- */
        .trig {
            position: absolute;
            box-sizing: border-box;
            touch-action: none;
        }
        .trig-track {
            position: absolute;
            top: 0; bottom: 0;
            background: #222;
            border: 2px solid #111;
            border-radius: 10px;
            box-shadow: inset 0 0 6px rgba(0,0,0,0.6);
            overflow: hidden;
            touch-action: none;
        }
        .trig-fill {
            position: absolute;
            left: 0; top: 0; bottom: 0;
            width: 0%;
            background: linear-gradient(90deg, #2e6b1f, var(--xbox-green));
            border-radius: 8px;
            pointer-events: none;
        }
        .trig-thumb {
            position: absolute;
            top: 50%; left: 0%;
            width: 12px; height: 70%;
            transform: translate(-50%, -50%);
            background: #eee;
            border: 2px solid #111;
            border-radius: 6px;
            pointer-events: none;
            box-shadow: 0 0 4px rgba(0,0,0,0.5);
        }
        .trig-cc {
            position: absolute;
            top: 0; bottom: 0;
            display: flex; align-items: center; justify-content: center;
            background: #555;
            border: 2px solid #222;
            border-radius: 8px;
            color: #ddd;
            font-size: 2.2vmin;
            font-weight: bold;
            box-sizing: border-box;
            touch-action: none;
        }
        .trig-cc.on {
            background: var(--xbox-green);
            color: #fff;
            box-shadow: 0 0 8px rgba(82,176,67,0.8);
        }
        .trig-label {
            position: absolute;
            left: 0; right: 0; top: -52%;
            text-align: center;
            font-size: 3.2vmin;
            color: #bbb;
            font-weight: bold;
            pointer-events: none;
        }

        /* ---- XInput LED panel + VU-meter rumble indicators (top-center) ---- */
        #led-panel {
            position: absolute;
            left: 50%; top: 2%;
            transform: translateX(-50%);
            display: flex;
            align-items: center;
            gap: 6px;
            padding: 4px 10px;
            background: rgba(0,0,0,0.35);
            border-radius: 10px;
            z-index: 10;
        }
        .xinput-led {
            width: 10px; height: 10px;
            border-radius: 50%;
            background: #555;
            border: 1px solid #111;
            transition: background 0.15s;
        }
        .xinput-led.on {
            background: #3ddc2f;
            box-shadow: 0 0 8px rgba(61,220,47,0.9);
        }
        /* VU-meter rumble bars — linear scale, anchored at LED panel center */
        .vu-track {
            width: 13vw; height: 8px;
            background: #333;
            border-radius: 4px;
            overflow: hidden;
            position: relative;
        }
        .vu-fill {
            position: absolute;
            top: 0; bottom: 0;
            width: 0%;
            transition: none;
            background: linear-gradient(90deg, #3ddc2f, #ffd600);
        }
        /* Left meter fills rightward (base at LED side) */
        .vu-left .vu-fill { left: auto; right: 0; }
        /* Right meter fills leftward (base at LED side) — mirrored via transform */
        .vu-right .vu-fill { left: 0; right: auto; background: linear-gradient(270deg, #3ddc2f, #ffd600); }

        .stick-base {
            background-color: #cfcfcf;
            border: 3px solid #999;
            box-shadow: inset -2px -2px 6px rgba(0,0,0,0.2), 2px 2px 6px rgba(0,0,0,0.2);
            border-radius: 50%;
            position: absolute;
            display: flex; align-items: center; justify-content: center;
            touch-action: none;
        }
        .stick-thumb {
            background-color: #2a2a2a;
            border-radius: 50%;
            box-shadow: inset -1px -1px 3px rgba(255,255,255,0.15), 2px 2px 4px rgba(0,0,0,0.4);
            position: absolute;
            left: 50%; top: 50%;
            transform: translate(-50%, -50%);
            pointer-events: none;
        }
        .stick-label {
            position: absolute;
            bottom: 2px;
            font-size: 2vmin; color: #555;
            pointer-events: none;
        }

        .edge-bar {
            position: fixed; top: 0; bottom: 0; left: 0; width: 40px; pointer-events: auto; touch-action: none;
        }

        .rotate-overlay {
            display: none;
            position: fixed;
            inset: 0;
            z-index: 9999;
            background: var(--deep-navy);
            color: #fff;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            font-size: 5vmin;
            text-align: center;
        }
        .rotate-overlay .icon { font-size: 12vmin; margin-bottom: 4vmin; }

        /* ---- Gear button + config overlay ---- */
        #gear-btn {
            position: absolute; right: max(2.5%, env(safe-area-inset-right, 0px)); top: 1.5%; width: 4%; height: 5%; z-index: 100;
            background-color: rgba(0,0,0,0.4); border: 2px solid #555; border-radius: 50%;
            color: #ccc; font-size: 3vmin; display: flex; align-items: center; justify-content: center; cursor: pointer;
        }
        .config-overlay {
            display: none; position: fixed; inset: 0; z-index: 200; background: rgba(10,15,30,0.92);
            flex-direction: column; align-items: center; justify-content: center;
        }
        .config-overlay.active { display: flex; }
        .config-panel {
            background: #2a3040; border: 2px solid #556; border-radius: 14px; padding: 8vmin; text-align: center; color: #eee;
        }
        .config-panel h2 { margin: 0 0 5vmin 0; font-size: 3.5vmin; }
        .config-option {
            display: flex; align-items: center; justify-content: center; gap: 2vmin; padding: 2vmin 4vmin; margin: 1.5vmin 0;
            background: #3a4060; border-radius: 8px; cursor: pointer; font-size: 3vmin; font-weight: bold;
        }
        .config-option.selected { background: var(--xbox-green); color: #fff; }
    </style>
</head>
<body>
    <div class="edge-bar"></div>
    <div class="edge-bar" style="right:0;"></div>
    <div id="rotate-overlay" class="rotate-overlay">
        <div class="icon">&#8635;</div>
        <div>Rotate device to landscape</div>
    </div>
    <div id="led-panel">
        <div class="vu-track vu-left"><div class="vu-fill" data-rumble="left"></div></div>
        <div class="xinput-led" data-led="0"></div>
        <div class="xinput-led" data-led="1"></div>
        <div class="xinput-led" data-led="2"></div>
        <div class="xinput-led" data-led="3"></div>
        <div class="vu-track vu-right"><div class="vu-fill" data-rumble="right"></div></div>
    </div>
    <button id="gear-btn">&#9881;</button>
    <div id="config-overlay" class="config-overlay">
        <div class="config-panel">
            <h2>Button Labels</h2>
            <div class="config-option" data-mode="xs">Xbox X|S (VIEW / MENU)</div>
            <div class="config-option" data-mode="x360">Xbox 360 (BACK / START)</div>
        </div>
    </div>
    <div id="pad"></div>

<script>
let ws;

// Neutralize Safari's swipe-back (left edge) — on iPhone this gesture fires at the
// system level before page touch handlers can preventDefault(). Adding a history
// entry and re-pushing on popstate/pageshow cancels the navigation without leaving.
(function() {
    function pin() { history.pushState(null, '', location.href); }
    history.pushState(null, '', location.href);
    window.addEventListener('popstate', pin);
    window.addEventListener('pageshow', pin);
})();

// Block ALL touch gestures page-wide (this is a full-screen gamepad; no scrolling,
// pinch-zoom or native swipe is ever wanted). Capture-phase preventDefault on
// touchstart/touchmove is the strongest signal Safari accepts that the page owns
// every gesture — this covers edge swipes starting in the notch/safe-area too.
document.addEventListener('touchstart', function(e) { e.preventDefault(); }, {passive: false, capture: true});
document.addEventListener('touchmove',  function(e) { e.preventDefault(); }, {passive: false, capture: true});

function checkOrientation() {
    const el = document.getElementById('rotate-overlay');
    if (!el) return;
    const portrait = window.matchMedia('(orientation: portrait)').matches;
    el.style.display = portrait ? 'flex' : 'none';
}

function applyLed(index) {
    const leds = document.querySelectorAll('.xinput-led');
    leds.forEach((l) => {
        const n = parseInt(l.dataset.led, 10);
        l.classList.toggle('on', index < 4 && n === index);
    });
}

function applyRumble(leftVal, rightVal) {
    var vL = Math.max(0, Math.min(255, leftVal));
    var vR = Math.max(0, Math.min(255, rightVal));

    _vuSetFill('left', vL);
    _vuSetFill('right', vR);
}

function _vuSetFill(side, val) {
    var el = document.querySelector('[data-rumble="' + side + '"]');
    if (!el) return;
    var pct = (val / 255 * 100).toFixed(1);
    el.style.width = pct + '%';
}

function connectWS() {
    ws = new WebSocket('ws://' + location.hostname + ':81/');
    ws.onopen  = () => { resyncSticky(); resyncCC(); };
    ws.onmessage = (e) => {
        const d = e.data;
        if (d.startsWith('#HOST:')) {
        } else if (d.startsWith('#LED:')) {
            applyLed(parseInt(d.substring(5), 10) || 4);
        } else if (d.startsWith('#RUMBLE:')) {
            var parts = d.substring(8).split(',');
            applyRumble(parseInt(parts[0], 10) || 0, parseInt(parts[1], 10) || 0);
        } else if (d.startsWith('#NAMING:')) {
            setButtonNaming(d.substring(8), true);
        }
    };
    ws.onclose   = () => { setTimeout(connectWS, 2000); };
    ws.onerror   = () => { };
}

/* ---- Button naming toggle (Xbox X/S vs Xbox 360) ---- */
const btnLabels = { xs: 'VIEW', x360: 'BACK' }, stLabels = { xs: 'MENU', x360: 'START' };
let buttonNaming = localStorage.getItem('tgpadxb_naming') || 'xs';

function applyButtonNaming(mode) {
    if (keyEls['*Back']) keyEls['*Back'].innerHTML = btnLabels[mode] || btnLabels.xs;
    if (keyEls['*Start']) keyEls['*Start'].innerHTML = stLabels[mode] || stLabels.xs;
}

function setButtonNaming(mode, fromFirmware) {
    buttonNaming = mode;
    localStorage.setItem('tgpadxb_naming', mode);
    applyButtonNaming(mode);
    if (!fromFirmware && ws && ws.readyState === 1) ws.send('#NAMING:' + mode);
    updateConfigSelection();
}

function updateConfigSelection() {
    document.querySelectorAll('.config-option').forEach((opt) => {
        opt.classList.toggle('selected', opt.dataset.mode === buttonNaming);
    });
}

connectWS();

setInterval(() => { if (ws && ws.readyState === 1) ws.send('#PING'); }, 2000);

window.matchMedia('(orientation: portrait)').addListener(checkOrientation);
checkOrientation();

// Read safe-area insets (notch / home indicator) so edge controls can be nudged
// inward on notched phones. Returns px; 0 on devices without insets.
function safeInset(side) {
    const el = document.createElement('div');
    el.style.cssText = 'position:fixed;top:0;left:0;width:1px;height:1px;' +
        'padding-' + side + ':env(safe-area-inset-' + side + ');' +
        'box-sizing:content-box;visibility:hidden;pointer-events:none;';
    document.body.appendChild(el);
    const inset = el.clientWidth - 1;
    el.remove();
    return inset;
}
const safeLeft  = safeInset('left');
const safeRight = safeInset('right');

// Push controls that sit near an edge inward past the notch (in px via calc()).
// Halved so they don't crowd center controls.
function nudgedLeft(x) {
    const dx = x <= 15 ? safeLeft / 2 : (x >= 85 ? -safeRight / 2 : 0);
    return dx ? 'calc(' + x + '% + ' + dx + 'px)' : x + '%';
}

const pad = document.getElementById('pad');

const keyEls = {};
function mkKey(o) {
    const b = document.createElement('button');
    b.className = 'key ' + (o.cls || '');
    b.style.left   = nudgedLeft(o.x);
    b.style.top    = o.y + '%';
    b.style.width  = o.w + '%';
    b.style.height = o.h + '%';
    b.style.transform = 'translate(-50%, -50%)';
    if (o.round) b.classList.add('round');
    b.innerHTML = o.label || '';
    if (o.k !== undefined) { b.dataset.k = o.k; keyEls[o.k] = b; }
    pad.appendChild(b);
    return b;
}

const buttons = [
    // Bumpers
    {k:'*LB', label:'LB', x:9.5, y:28, w:18, h:10, cls:'bumper'},
    {k:'*RB', label:'RB', x:90.5, y:28, w:18, h:10, cls:'bumper'},

    // Face buttons (Xbox layout: A bottom-right, B right, X left, Y top)
    {k:'*Y', round:true, label:'Y', x:90.5, y:46, w:8, h:9, cls:'round face', color:'#d9b31a'},
    {k:'*X', round:true, label:'X', x:85.5, y:62, w:8, h:9, cls:'round face', color:'#2d6fbe'},
    {k:'*B', round:true, label:'B', x:95.5, y:62, w:8, h:9, cls:'round face', color:'#c1352d'},
    {k:'*A', round:true, label:'A', x:90.5, y:78, w:8, h:9, cls:'round face', color:'#3c9e2f'},

    // Menu buttons (Back/View, Xbox guide, Start)
    {k:'*Back',  label:'BACK', x:34, y:24, w:18, h:10, cls:'menu-key'},
    {k:'*Xbox',  label:'XB', x:50, y:26, w:12, h:7, cls:'xbox-key round'},
    {k:'*Start', label:'START', x:66, y:24, w:18, h:10, cls:'menu-key'},
];

buttons.forEach((o) => {
    const b = mkKey(o);
    if (o.color) b.style.backgroundColor = o.color;
});

/* Apply saved naming + wire gear button */
applyButtonNaming(buttonNaming);
updateConfigSelection();
document.getElementById('gear-btn').addEventListener('pointerdown', (e) => {
    e.preventDefault();
    document.getElementById('config-overlay').classList.toggle('active');
});
document.querySelectorAll('.config-option').forEach((opt) => {
    opt.addEventListener('pointerdown', (e) => {
        e.stopPropagation();
        setButtonNaming(opt.dataset.mode);
        document.getElementById('config-overlay').classList.remove('active');
    });
});

// ---- XInput LED panel handled above; LEDs updated via ws.onmessage ----

// ---- Analog trigger sliders with cruise control ----
function mkTrigger(o) {
    const wrap = document.createElement('div');
    wrap.className = 'trig';
    wrap.style.left = nudgedLeft(o.x);
    wrap.style.top  = o.y + '%';
    wrap.style.width  = o.w + '%';
    wrap.style.height = o.h + '%';
    wrap.style.transform = 'translate(-50%, -50%)';
    pad.appendChild(wrap);

    const lbl = document.createElement('div');
    lbl.className = 'trig-label';
    lbl.textContent = o.label;
    wrap.appendChild(lbl);

    const track = document.createElement('div');
    track.className = 'trig-track';
    if (o.reverse) {
        track.style.right = '0';
    } else {
        track.style.left = '0';
    }
    track.style.width = '76%';
    wrap.appendChild(track);

    const fill = document.createElement('div');
    fill.className = 'trig-fill';
    if (o.reverse) {
        fill.style.left = 'auto';
        fill.style.right = '0';
    } else {
        fill.style.left = '0';
    }
    track.appendChild(fill);

    const thumb = document.createElement('div');
    thumb.className = 'trig-thumb';
    if (o.reverse) {
        thumb.style.left = 'auto';
        thumb.style.right = '0%';
        thumb.style.transform = 'translate(50%, -50%)';
    } else {
        thumb.style.left = '0%';
        thumb.style.transform = 'translate(-50%, -50%)';
    }
    track.appendChild(thumb);

    const cc = document.createElement('div');
    cc.className = 'trig-cc';
    cc.textContent = 'CC';
    if (o.ccLeft) {
        cc.style.right = '78%';
    } else {
        cc.style.left = '78%';
    }
    cc.style.width = '22%';
    wrap.appendChild(cc);

    let val = 0, ccOn = false, active = false;
    function setVal(v, send) {
        val = Math.max(0, Math.min(32768, Math.round(v)));
        fill.style.width = (val / 32768 * 100) + '%';
        if (o.reverse) {
            thumb.style.left = 'auto';
            thumb.style.right = (val / 32768 * 100) + '%';
        } else {
            thumb.style.left = (val / 32768 * 100) + '%';
        }
        if (send && ws && ws.readyState === 1) ws.send(o.token + ':' + val);
    }
    function update(e) {
        const r = track.getBoundingClientRect();
        let nx = o.reverse ? (r.right - e.clientX) / r.width : (e.clientX - r.left) / r.width;
        nx = Math.max(0, Math.min(1, nx));
        setVal(nx * 32768, true);
    }
    function end() {
        active = false;
        if (!ccOn) {
            setVal(0, false);
            if (ws && ws.readyState === 1) ws.send('~' + o.token);
        }
    }
    track.addEventListener('pointerdown', (e) => {
        e.preventDefault(); e.stopPropagation();
        track.setPointerCapture(e.pointerId);
        active = true;
        update(e);
    });
    track.addEventListener('pointermove', (e) => {
        if (active) { e.preventDefault(); update(e); }
    });
    track.addEventListener('pointerup', end);
    track.addEventListener('pointercancel', end);

    cc.addEventListener('pointerdown', (e) => {
        e.preventDefault(); e.stopPropagation();
        ccOn = !ccOn;
        cc.classList.toggle('on', ccOn);
        if (ccOn) {
            if (val > 0 && ws && ws.readyState === 1) ws.send(o.token + ':' + val);
            if (ws && ws.readyState === 1) ws.send(o.ccToken);
        } else {
            setVal(0, true);
            if (ws && ws.readyState === 1) {
                ws.send('~' + o.token);
                ws.send('~' + o.ccToken);
            }
        }
    });
    wrap._setCC = (on) => {
        ccOn = on;
        cc.classList.toggle('on', on);
    };
    wrap._getCC = () => ccOn;
    return wrap;
}

const ccControls = [];
function mkCC(o) {
    const t = mkTrigger(o);
    ccControls.push(t);
    return t;
}

mkCC({x:9.5,  y:14, w:18, h:8, label:'LT', token:'*LT', ccToken:'*LTC'});
mkCC({x:90.5, y:14, w:18, h:8, label:'RT', reverse:true, ccLeft:true, token:'*RT', ccToken:'*RTC'});

function resyncCC() {
    if (!ws || ws.readyState !== 1) return;
    for (const t of ccControls) {
        if (t._getCC()) ws.send(t._getCC() ? t.ccToken : '~' + t.ccToken);
    }
}

// ---- 8-way directional pad ----
function mkDpad8(o) {
    const el = document.createElement('div');
    el.className = 'dpad8';
    el.style.left = nudgedLeft(o.x);
    el.style.top  = o.y + '%';
    el.style.width = 'min(' + o.size + 'vw, 34vmin)';
    el.style.aspectRatio = '1 / 1';
    el.style.transform = 'translate(-50%, -50%)';
    el.style.touchAction = 'none';
    pad.appendChild(el);

    const svgNS = 'http://www.w3.org/2000/svg';
    const svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.style.position = 'absolute';
    svg.style.top = '0';
    svg.style.left = '0';
    svg.style.width = '100%';
    svg.style.height = '100%';
    svg.style.pointerEvents = 'none';
    el.appendChild(svg);

    const paths = [];
    const cx = 12, cy = 12, rOut = 12, rIn = 3.2;
    for (let i = 0; i < 8; i++) {
        const a = (i * 45 - 90) * Math.PI / 180;
        const hp = (22.5 + 0.5) * Math.PI / 180;
        const p = document.createElementNS(svgNS, 'path');
        p.setAttribute('d',
            `M${cx + rOut * Math.cos(a - hp)} ${cy + rOut * Math.sin(a - hp)}` +
            `L${cx + rOut * Math.cos(a + hp)} ${cy + rOut * Math.sin(a + hp)}` +
            `L${cx + rIn  * Math.cos(a + hp)} ${cy + rIn  * Math.sin(a + hp)}` +
            `L${cx + rIn  * Math.cos(a - hp)} ${cy + rIn  * Math.sin(a - hp)}Z`);
        p.classList.add('dpad8-sector');
        svg.appendChild(p);
        paths.push(p);
    }

    const hub = document.createElement('div');
    hub.className = 'dpad8-hub';
    el.appendChild(hub);

    function sliceFor(dx, dy) {
        let a = Math.atan2(dy, dx) * 180 / Math.PI;
        a = (a + 90 + 360) % 360;
        return Math.round(a / 45) % 8;
    }

    let cur = -1;
    function apply(idx) {
        if (idx === cur) return;
        if (cur >= 0 && paths[cur]) paths[cur].classList.remove('btn-active');
        cur = idx;
        if (cur >= 0) {
            if (paths[cur]) paths[cur].classList.add('btn-active');
            if (ws && ws.readyState === 1) ws.send('*DPAD:' + cur);
        } else {
            if (ws && ws.readyState === 1) ws.send('*DPAD:8');
        }
    }

    function update(e) {
        const r = el.getBoundingClientRect();
        const cx = r.left + r.width / 2, cy = r.top + r.height / 2;
        const dx = e.clientX - cx, dy = e.clientY - cy;
        const rad = r.width / 2;
        if (Math.hypot(dx, dy) < rad * 0.18) { apply(-1); return; }
        apply(sliceFor(dx, dy));
    }

    el.addEventListener('pointerdown', (e) => {
        e.preventDefault(); e.stopPropagation();
        el.setPointerCapture(e.pointerId);
        update(e);
    });
    el.addEventListener('pointermove', (e) => {
        if (el.hasPointerCapture && el.hasPointerCapture(e.pointerId)) {
            e.preventDefault(); update(e);
        }
    });
    function end() { apply(-1); }
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
    el.addEventListener('lostpointercapture', end);
}
mkDpad8({x:11, y:55.5, size:20});

// ---- Analog sticks ----
function mkStick(o) {
    const base = document.createElement('div');
    base.className = 'stick-base';
    base.style.left = o.x + '%';
    base.style.top  = o.y + '%';
    base.style.width = 'min(' + o.size + 'vw, 34vmin)';
    base.style.aspectRatio = '1 / 1';
    base.style.transform = 'translate(-50%, -50%)';
    const thumb = document.createElement('div');
    thumb.className = 'stick-thumb';
    const ts = o.size * 0.45;
    thumb.style.width = ts + '%';
    thumb.style.height = ts + '%';
    const lbl = document.createElement('div');
    lbl.className = 'stick-label';
    lbl.textContent = o.label;
    base.appendChild(thumb);
    base.appendChild(lbl);
    pad.appendChild(base);
    return {base, thumb};
}

const RANGE = 127;
function buildStick(cfg) {
    const s = mkStick(cfg);
    let active = false;
    function report(dx, dy) {
        let nx = dx, ny = dy;
        const mag = Math.hypot(nx, ny);
        if (mag > 1) { nx /= mag; ny /= mag; }
        const vx = Math.round(nx * RANGE);
        const vy = Math.round(ny * RANGE);
        ws.send(cfg.axes.x + ':' + vx);
        ws.send(cfg.axes.y + ':' + vy);
        s.thumb.style.left = (50 + nx * 50) + '%';
        s.thumb.style.top  = (50 + ny * 50) + '%';
    }
    function center() {
        s.thumb.style.left = '50%';
        s.thumb.style.top  = '50%';
        ws.send(cfg.axes.x + ':0');
        ws.send(cfg.axes.y + ':0');
    }
    s.base.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        s.base.setPointerCapture(e.pointerId);
        active = true;
        move(e);
    });
    function move(e) {
        const r = s.base.getBoundingClientRect();
        const cx = r.left + r.width/2, cy = r.top + r.height/2;
        const dx = (e.clientX - cx) / (r.width/2);
        const dy = (e.clientY - cy) / (r.height/2);
        report(dx, dy);
    }
    s.base.addEventListener('pointermove', (e) => { if (active) { e.preventDefault(); e.stopPropagation(); move(e); } });
    function end(e) {
        active = false;
        center();
    }
    s.base.addEventListener('pointerup', end);
    s.base.addEventListener('pointercancel', end);
    s.base.addEventListener('lostpointercapture', end);
}

buildStick({x:31, y:72, size:20, label:'', axes:{x:'*LX', y:'*LY'}});
buildStick({x:69, y:72, size:20, label:'', axes:{x:'*RX', y:'*RY'}});

// ---- Sticky toggle buttons (LS / RS stick clicks) ----
const stickyControls = [];
function mkToggle(o) {
    const b = mkKey(o);
    let on = false;
    const ctrl = { k: o.k, getOn: () => on };
    stickyControls.push(ctrl);
    b.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        b.setPointerCapture(e.pointerId);
        on = !on;
        if (on) { ws.send(o.k); b.classList.add('btn-active'); }
        else      { ws.send('~' + o.k); b.classList.remove('btn-active'); }
    });
    return b;
}

function resyncSticky() {
    if (!ws || ws.readyState !== 1) return;
    for (const c of stickyControls) {
        ws.send(c.getOn() ? c.k : '~' + c.k);
    }
}

mkToggle({k:'*LThumb', label:'LS', x:45, y:72, w:8, h:6, cls:'sticky'});
mkToggle({k:'*RThumb', label:'RS', x:55, y:72, w:8, h:6, cls:'sticky'});

// ---- Generic button press/release with slide support ----
document.addEventListener('contextmenu', function(e) { e.preventDefault(); });

const pointers = new Map();

function findBtnByKey(k) { return keyEls[k] || null; }

function sendDown(k, btn) {
    if (!(ws && ws.readyState === 1)) return;
    ws.send(k);
    if (btn) btn.classList.add('btn-active');
}
function sendUp(k, btn) {
    if (!(ws && ws.readyState === 1)) return;
    ws.send('~' + k);
    if (btn) btn.classList.remove('btn-active');
}

function keysUnder(x, y) {
    let els;
    try { els = document.elementsFromPoint(x, y); }
    catch (_) { els = []; }
    const out = [];
    for (const el of els) {
        if (!el || !el.closest) continue;
        const btn = el.closest('.key');
        if (!btn) continue;
        if (btn.classList.contains('stick-base') || btn.classList.contains('sticky')) continue;
        if (btn.dataset.k === undefined) continue;
        out.push(btn);
    }
    return out;
}

document.addEventListener('pointerdown', function(e) {
    const btns = keysUnder(e.clientX, e.clientY);
    if (btns.length === 0) return;
    e.preventDefault();
    const set = new Set();
    for (const b of btns) {
        const k = b.dataset.k;
        set.add(k);
        sendDown(k, b);
    }
    pointers.set(e.pointerId, set);
}, {capture: true});

document.addEventListener('pointermove', function(e) {
    const set = pointers.get(e.pointerId);
    if (!set) return;
    e.preventDefault();
    const btns = keysUnder(e.clientX, e.clientY);
    const newKeys = new Set(btns.map(b => b.dataset.k));
    for (const k of Array.from(set)) {
        if (!newKeys.has(k)) {
            sendUp(k, findBtnByKey(k));
            set.delete(k);
        }
    }
    for (const b of btns) {
        const k = b.dataset.k;
        if (!set.has(k)) { sendDown(k, b); set.add(k); }
    }
}, {capture: true});

function pointerEnd(e) {
    const set = pointers.get(e.pointerId);
    if (!set) return;
    for (const k of set) sendUp(k, findBtnByKey(k));
    pointers.delete(e.pointerId);
}
document.addEventListener('pointerup', pointerEnd, {capture: true});
document.addEventListener('pointercancel', pointerEnd, {capture: true});
</script>
</body>
</html>
)rawliteral";

#endif
