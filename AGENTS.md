# AGENTS.md — TGPad-XB

## Build & Upload

Build profiles are defined in `sketch.yaml` with local library paths. The default profile is **atoms3**.

### Compile
```bash
# Default target: M5Stack AtomS3 (8MB flash)
arduino-cli compile .

# Generic ESP32-S3
arduino-cli compile --profile esp32s3 .
```

### Serial upload (board must be in download mode, BOOT button held)
```bash
# AtomS3
arduino-cli upload -p /dev/ttyACM0 --profile atoms3 .

# Generic ESP32-S3
arduino-cli upload -p /dev/ttyUSB0 --profile esp32s3 .
```

### OTA upload (board must have WiFi connected and ArduinoOTA running)
```bash
# AtomS3 — by hostname
arduino-cli compile --output-dir /tmp/tgap_ota_build . && \
  arduino-cli upload -p tgpadxb.local --upload-field password="" \
    --protocol network --profile atoms3 --input-dir /tmp/tgap_ota_build .

# Generic ESP32-S3 — verified working
arduino-cli compile --profile esp32s3 --output-dir /tmp/tgap_ota_build . && \
  arduino-cli upload -p <IP> --upload-field password="" \
    --protocol network --profile esp32s3 --input-dir /tmp/tgap_ota_build .
```

Fallback — direct espota.py (bypasses arduino-cli port discovery):
```bash
ESPOTA=~/.arduino15/packages/esp32/hardware/esp32/3.3.10/tools/espota.py
arduino-cli compile --profile esp32s3 --output-dir /tmp/tgap_ota_build . && \
   python3 "$ESPOTA" -r -i <IP> -p 3232 --auth="" \
    -f /tmp/tgap_ota_build/tgpadxb.ino.bin; echo "EXIT=$?"
```

Fallback — web-based OTA (no ArduinoOTA needed, HTTP port 80):
```bash
arduino-cli compile --profile esp32s3 --output-dir /tmp/tgap_ota_build . && \
  curl -F "firmware=@/tmp/tgap_ota_build/tgpadxb.ino.bin" \
    http://<IP>/update; echo "EXIT=$?"
```

## Critical Gotchas
- **OTA authentication**: Uncomment `#define OTA_PASS "your-password-here"` in `tgpadxb.ino` to enable password-protected OTA (both ArduinoOTA and web-based). When enabled, append `--upload-field password=<pass>` to OTA upload commands and use basic auth user `admin` for web OTA.
- **Detecting successful OTA**: Do NOT parse espota.py or curl output — both produce `\r` progress bars that get truncated by tool output limits. Always check exit code (`$?`) instead: 0 = success, non-zero = failure. Append `; echo "EXIT=$?"` to verify.
- **USB enumerates as vendor-class XInput** (no CDC serial at runtime). Use `Serial0` for debug output on boards with CP210x bridge. For AtomS3, internal UART routing may still allow Serial — test both boards.
- **`lib/WiFiManager`, `lib/WebSockets`, `lib/M5GFX` are git submodules.** After a fresh clone run `git submodule update --init --recursive`. `lib/ESP32XInput` is a copy of `~/ESP32XInput/src/`. Profiles handle all of them via `dir:` entries — no manual `--library` flags needed when using profiles.
- **AtomS3 serial port**: `/dev/ttyACM0` only appears when the board is in download mode (BOOT button held). When running normally it disappears from USB and shows up as an XInput gamepad + WiFi device.

## Testing
```bash
pip3 install -r test/requirements.txt

# Offline unit tests (no hardware)
python3 -m pytest test/ -v --ignore=test/e2e

# Hardware e2e — requires root (evdev grab), set board IP via env var
sudo TGPADXB_HOST=192.168.1.x python3 -m pytest test/e2e -v

# Full e2e with multi-client tests enabled (requires generic ESP32-S3, not AtomS3)
sudo TGPADXB_HOST=192.168.1.x TGPADXB_MULTICLIENT=1 python3 -m pytest test/e2e -v
```

- Multi-client e2e tests are **skipped by default**. Enable with `TGAPDXB_MULTICLIENT=1` (requires a generic ESP32-S3 dev module; AtomS3 reports slot 0 for every client so OR-combine cannot be verified there). See `test/conftest.py`.
- e2e tests connect to the board over WebSocket port **81** and read USB HID events via python-evdev.

## Architecture Notes
- Web UI lives in `webpage.h` (embedded HTML/JS/CSS), not a separate web server project.
- Firmware: `tgpadxb.ino`. Entry point is standard Arduino setup()/loop().
- WebSocket protocol on port 81: key-down = token string (`*A`, `*DPAD:0`), key-up = `~<token>`, analog sticks scaled `-127..127 → -32768..+32767`. Analog triggers use `*LT:<0..32768>` / `*RT:<0..32768>`.
- XInput LED state broadcast: firmware sends `#LED:<0..4>` (4 = all off). Web UI updates LED panel accordingly.

## Verified Status
- Offline tests: **PASS** (39 tests) — run `python3 -m pytest test/ -v --ignore=test/e2e`
- Compile: **PASS** — both `atoms3` (38% flash) and `esp32s3` (88% flash) profiles
- Hardware e2e: **PASS** (14 tests incl. multiclient, on generic ESP32-S3 at `192.168.1.181`) — run `sudo TGPADXB_HOST=192.168.1.181 TGPADXB_MULTICLIENT=1 python3 -m pytest test/e2e -v`

## XInput Wire Format (verified against `xpad.c` + live evtest)
20-byte report on interrupt IN ep, starting with a 2-byte `bMessageType(0x00) bMessageSize(0x14)` header so `xpad360_process_packet` accepts it:
`00 14 | wButtons(2 LE) | bLeftTrigger(1) | bRightTrigger(1) | sThumbLX(2 LE) | sThumbLY(2 LE) | sThumbRX(2 LE) | sThumbRY(2 LE) | dwReserved0(4) | wReserved1(2)`
- A/B/X/Y are `wButtons` bits **12/13/14/15** (`xpad.c`: `data[3]` bits 4–7). Sticks/triggers are raw (no +128 offset): center = 0.
- Two firmware gotchas already fixed: (1) `ESP32XInput.isConnected()` MUST be called in `loop()` — otherwise `_usbReady` stays false and every report is dropped; (2) `BUTTON_COUNT` must be 16 so Y (index 15) passes the library's `btn >= BUTTON_COUNT` guard.
