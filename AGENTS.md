# AGENTS.md — TGPad-XB

## Build & Upload

Build profiles are defined in `sketch.yaml` with local library paths. The default profile is **atoms3**.

### Compile
```bash
# Default target: M5Stack AtomS3 (8MB flash)
arduino-cli compile .

# Generic ESP32-S3
arduino-cli compile --profile esp32s3 .

# LilyGo T-Dongle-S3 (160x80 ST7735 via vendored TFT_eSPI)
arduino-cli compile --profile tdongle_s3 .

# LilyGo T-Dongle-S3-Plus (same FQBN as base; define T_DONGLE_S3_PLUS for the banner)
arduino-cli compile --profile tdongle_s3_plus . --build-property compiler.cpp.extra_flags=-DT_DONGLE_S3_PLUS

# Waveshare ESP32-S3-Touch-LCD-1.54 (240x240 ST7789 via Arduino_GFX; core 3.2.0;
# the esp32 core 3.2.0 is downloaded into the profile cache on first build)
arduino-cli compile --profile waveshare . --build-property compiler.cpp.extra_flags=-DT_WAVESHARE_154
```

### Serial upload (board must be in download mode, BOOT button held)
```bash
# AtomS3
arduino-cli upload -p /dev/ttyACM0 --profile atoms3 .

# Generic ESP32-S3
arduino-cli upload -p /dev/ttyUSB0 --profile esp32s3 .

# T-Dongle-S3 / Plus (native USB CDC, typically /dev/ttyACM0;
# use the --build-property flag for the Plus variant)
arduino-cli upload -p /dev/ttyACM0 --profile tdongle_s3 .

# Waveshare ESP32-S3-Touch-LCD-1.54 (native USB CDC, /dev/ttyACM0;
# use the --build-property flag for -DT_WAVESHARE_154)
arduino-cli upload -p /dev/ttyACM0 --profile waveshare .
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
- **Display (T-Dongle-S3 / Plus)**: Same status on the 160×80 ST7735 via **vendored TFT_eSPI** (`lib/TFT_eSPI`, version 2.5.43). Auto-detected at compile time with `__has_include(<TFT_eSPI.h>)` **and** `ARDUINO_USB_MODE == 1` (i.e. `USBMode=hwcdc`, which all tdongle profiles select) → defines `T_DONGLE_S3`. The USB-mode guard prevents a stray global TFT_eSPI from enabling the T-Dongle path on AtomS3/generic builds. The vendored library's `User_Setup.h` is the T-Dongle-S3 config (ST7735 GREENTAB160x80, BGR, pins MOSI=3 SCLK=5 CS=4 DC=2 RST=1, backlight GPIO38 active-low 0=on). Do not add/remove that marker. The Plus is the same board plus `-DT_DONGLE_S3_PLUS` (only used for a boot banner; display/reset identical).
- **Display (Waveshare ESP32-S3-Touch-LCD-1.54)**: Same status on the 1.54" 240×240 ST7789 via **Arduino_GFX** (GFX Library for Arduino **1.6.0**, core **3.2.0**) — the same proven stack as the Waveshare examples and the touchWASD/iKeys ports. TFT_eSPI 2.5.43 was abandoned because it crashes on ESP32-S3 on both core 3.x and 2.0.x (StoreProhibited in `begin_tft_write`). Selected EXPLICITLY by the `-DT_WAVESHARE_154` build flag (defined before any `__has_include` probe) → defines `WAVESHARE_154` and includes `<Arduino_GFX_Library.h>`. The flag must be checked BEFORE the T-Dongle probe because both boards use `USBMode=hwcdc` — otherwise a waveshare build could mis-detect as `T_DONGLE_S3`. Display is wrapped by the `WS154Display` adapter in `tgpadxb.ino`, which exposes the TFT_eSPI API subset the sketch uses on top of Arduino_GFX (`new Arduino_ST7789(new Arduino_ESP32SPI(45, 21, 38, 39, -1), 40, 0, true, 240, 240)`; `printf()` is buffered via `vsnprintf`). Display config: ST7789 240×240, pins MOSI=39 SCLK=38 CS=21 DC=45 RST=40, backlight GPIO46 (active-high, set HIGH in setup). Do not add/remove that marker. The adapter calls `setTextWrap(false)` in `init()` — Arduino_GFX defaults `wrap=true`, which makes `getTextBounds` under-measure strings wider than the panel (it wraps internally instead of returning the true width), breaking `printWrap`'s overflow detection and truncating long lines like the title. With wrap off, `textWidth()` reports true width and `printWrap` handles line breaks.
- **USB mode + TinyUSB**: T-Dongle/Waveshare run `USBMode=hwcdc`; ArduinoX360's `tinyusb_init()` takes the USB port over from the built-in USB-Serial/JTAG — Serial console is lost at runtime on those boards, use UART0 (Serial0) for debug.
- **Web serial flasher** (`docs/`): the board `<select>` must list all supported boards (`atoms3`, `esp32s3`, `tdongle_s3`, `tdongle_s3_plus`, `waveshare`); each has a matching `#<board>-info` bootloader-instructions block that `app.js` toggles. When adding a board, update: the `<option>`, the `#<board>-info` block, the `els` map in `app.js`, and `updateInstructions()`. Keep the list in sync with the release build loop that emits `firmware/<version>/<board>/` artifacts for `firmware.json`.
- **`lib/WiFiManager`, `lib/WebSockets`, `lib/M5GFX`, `lib/ArduinoX360-tinyusb` are git submodules.** After a fresh clone run `git submodule update --init --recursive`. To bump ArduinoX360-tinyusb: enter the directory and pull from GitHub (`cd lib/ArduinoX360-tinyusb && git fetch --tags && git checkout v1.0.0` — pinned to `v1.0.0`; update tag when bumping). Profiles handle all of them via `dir:` entries — no manual `--library` flags needed when using profiles. **`lib/TFT_eSPI` is vendored** (not a submodule); it is the T-Dongle-configured copy, committed directly. Compatibility shim `lib/ArduinoX360-tinyusb/src/ESP32XInput.h` aliases `ArduinoX360` as `ESP32XInput` but the canonical include is now `<ArduinoX360.h>`.
- **Version string**: the release version exists in **two** places and must match: `#define VERSION` in `tgpadxb.ino` and the `<title>` in `webpage.h`. Bump both together when releasing.
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
- Offline tests: **PASS** (65 tests) — run `python3 -m pytest test/ -v --ignore=test/e2e`
- Compile: **PASS** — `atoms3` (38%), `esp32s3` (89%), `tdongle_s3` / `tdongle_s3_plus` (38%), `waveshare` (38% on core 3.2.0 + GFX 1.6.0)
- Hardware e2e on generic ESP32-S3 at `192.168.1.181`: **PASS** (17 tests incl. multiclient) — run `sudo TGPADXB_HOST=192.168.1.181 TGPADXB_MULTICLIENT=1 python3 -m pytest test/e2e -v`
- Hardware e2e on Waveshare ESP32-S3-Touch-LCD-1.54 at `192.168.1.99`: **PASS** (13 tests; multiclient skipped — board reports slot 0 for every client, like AtomS3). Re-verified after the `setTextWrap(false)` display fix (line-wrap title truncation).
- Hardware e2e on LilyGo T-Dongle-S3 at `192.168.1.225`: **PASS** (17 tests incl. multiclient — distinct client slots work on this board). Also verified offline suite (65) + e2e (13) during the same session; the T-Dongle reports real client slots so `TGPADXB_MULTICLIENT=1` is supported.
- Hardware e2e on LilyGo T-Dongle-S3-Plus at `192.168.1.162`: **PASS** (17 tests incl. multiclient, after re-binding xpad; offline 65 also passed). Built with `-DT_DONGLE_S3_PLUS` — banner string verified in the compiled binary.

## XInput Wire Format (verified against `xpad.c` + live evtest)
20-byte report on interrupt IN ep, starting with a 2-byte `bMessageType(0x00) bMessageSize(0x14)` header so `xpad360_process_packet` accepts it:
`00 14 | wButtons(2 LE) | bLeftTrigger(1) | bRightTrigger(1) | sThumbLX(2 LE) | sThumbLY(2 LE) | sThumbRX(2 LE) | sThumbRY(2 LE) | dwReserved0(4) | wReserved1(2)`
- A/B/X/Y are `wButtons` bits **12/13/14/15** (`xpad.c`: `data[3]` bits 4–7). Sticks/triggers are raw (no +128 offset): center = 0.
- Two firmware gotchas already fixed: (1) `ArduinoX360.isConnected()` MUST be called in `loop()` — otherwise `_usbReady` stays false and every report is dropped; (2) `BUTTON_COUNT` must be 16 so Y (index 15) passes the library's `btn >= BUTTON_COUNT` guard.
