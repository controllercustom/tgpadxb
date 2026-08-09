/*
 * MIT License
 *
 * Copyright (c) 2026 controllercustom@myyahoo.com
 */

#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <Update.h>
#include <ESP32XInput.h>

#ifdef ARDUINO_M5STACK_ATOMS3
#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_GC9A01.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <lgfx/v1/platforms/esp32/Light_PWM.hpp>
#endif
#include <esp_wifi.h>
#include "tusb.h"
#include <cstdarg>

#define VERSION "1.0.2"

// Uncomment next line and change the password to enable OTA authentication:
// #define OTA_PASS "your-password-here"

#ifdef ARDUINO_M5STACK_ATOMS3
M5GFX display;
#endif
WebServer server(80);
WebSocketsServer webSocket(81);

#define MAX_WS_CLIENTS WEBSOCKETS_SERVER_CLIENT_MAX

// Non-dpad buttons start at START (=4); dpad handled separately via setHat().
#define BUTTON_LO 4

struct ClientState {
  bool   active = false;
  uint32_t lastSeen = 0;
  bool   btn[16] = {false};
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
  uint8_t dpadDir = 8;            // 0-7 direction, 8=centered
  bool   ltCCon = false;          // LT cruise control active?
  bool   rtCCon = false;          // RT cruise control active?
  uint16_t ltVal = 0;             // live LT analog 0..32768
  uint16_t rtVal = 0;             // live RT analog 0..32768
  uint16_t ltCC = 0;              // locked LT cruise value
  uint16_t rtCC = 0;              // locked RT cruise value
  uint32_t axisTs = 0, dpadTs = 0;
};

static ClientState clients[MAX_WS_CLIENTS];
uint8_t btnRef[16];

char hostname[33];
bool portalConfigSaved = false;
WiFiManagerParameter customHostnameParam("hostname", "Device hostname", "tgpadxb", 32);

#ifdef ARDUINO_M5STACK_ATOMS3
#define RESET_BUTTON_PIN 41
#else
#define RESET_BUTTON_PIN 0
#endif
unsigned long resetPressStart = 0;
bool resetButtonWasLow = false;
int wsClientCount = 0;
char buttonNamingMode[4] = "xs";

static void debugPrint(const char* s) {
#ifdef ARDUINO_M5STACK_ATOMS3
  Serial.print(s);
#else
  Serial0.print(s);
#endif
}

static void debugPrintf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
#ifdef ARDUINO_M5STACK_ATOMS3
  Serial.vprintf(fmt, args);
#else
  Serial0.vprintf(fmt, args);
#endif
  va_end(args);
}

static void resetAllState() {
  ESP32XInput.releaseAll();
  memset(btnRef, 0, sizeof(btnRef));
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    ClientState c;
    clients[i] = c;
  }
}

static void recomputeAndSend(unsigned long now) {
  // Buttons: OR across all active clients (dpad buttons 0-3 unused here).
  memset(btnRef, 0, sizeof(btnRef));
  for (uint8_t b = BUTTON_LO; b < 16; b++) {
    for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
      if (clients[i].active && clients[i].btn[b]) btnRef[b]++;
    }
    ESP32XInput.setButton((ESP32XInputClass::Button)b, btnRef[b] > 0);
  }

  // Sticks / d-pad: freshest active contributor.
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
  uint8_t dpadDir = 8;
  uint32_t bestAxis = 0, bestDpad = 0;

  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    ClientState& c = clients[i];
    if (!c.active) continue;
    if (c.axisTs >= bestAxis) {
      bestAxis = c.axisTs;
      lx = (int16_t)c.lx * 258; ly = (int16_t)c.ly * 258;
      rx = (int16_t)c.rx * 258; ry = (int16_t)c.ry * 258;
    }
    if (c.dpadTs >= bestDpad) {
      bestDpad = c.dpadTs; dpadDir = c.dpadDir;
    }
  }

  // Triggers: CC-locked value wins over any live value; else freshest live.
  uint16_t ltFinal = 0, rtFinal = 0;
  uint32_t ltTs = 0, rtTs = 0;
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    ClientState& c = clients[i];
    if (!c.active) continue;
    if (c.ltCCon && c.ltCC > 0) { ltFinal = c.ltCC; ltTs = UINT32_MAX; }
    else if (c.axisTs >= ltTs) { ltTs = c.axisTs; ltFinal = c.ltVal; }
    if (c.rtCCon && c.rtCC > 0) { rtFinal = c.rtCC; rtTs = UINT32_MAX; }
    else if (c.axisTs >= rtTs) { rtTs = c.axisTs; rtFinal = c.rtVal; }
  }

  ESP32XInput.setStickLeft(lx, ly);
  ESP32XInput.setStickRight(rx, ry);
  ESP32XInput.setHat(dpadDir);
  ESP32XInput.setLeftTrigger(ltFinal);
  ESP32XInput.setRightTrigger(rtFinal);
}

// Map WS tokens (with '*' prefix) to XInput button enum index.
static uint8_t btnIndex(const char* key) {
  if (key[0] != '*') return 255;
  if (strcmp(key, "*A") == 0) return ESP32XInputClass::Button::A;
  if (strcmp(key, "*B") == 0) return ESP32XInputClass::Button::B;
  if (strcmp(key, "*X") == 0) return ESP32XInputClass::Button::X;
  if (strcmp(key, "*Y") == 0) return ESP32XInputClass::Button::Y;
  if (strcmp(key, "*LB") == 0) return ESP32XInputClass::Button::LEFT_SHOULDER;
  if (strcmp(key, "*RB") == 0) return ESP32XInputClass::Button::RIGHT_SHOULDER;
  if (strcmp(key, "*Start") == 0) return ESP32XInputClass::Button::START;
  if (strcmp(key, "*Back") == 0) return ESP32XInputClass::Button::BACK;
  if (strcmp(key, "*LThumb") == 0) return ESP32XInputClass::Button::LEFT_THUMB;
  if (strcmp(key, "*RThumb") == 0) return ESP32XInputClass::Button::RIGHT_THUMB;
  if (strcmp(key, "*Xbox") == 0) return ESP32XInputClass::Button::XBOX;
  return 255;
}

static void handleKeyDown(uint8_t num, const char* key, unsigned long now) {
  if (num >= MAX_WS_CLIENTS) return;
  ClientState& c = clients[num];
  c.active = true;
  c.lastSeen = now;

  uint8_t b = btnIndex(key);
  if (b != 255) { c.btn[b] = true; recomputeAndSend(now); return; }

  // CC toggles — press = lock ON, release = unlock (explicit, resync-safe).
  if (strcmp(key, "*LTC") == 0) {
    c.ltCCon = true;
    if (c.ltVal > 0) c.ltCC = c.ltVal;   // lock current live value
    debugPrintf("[CC] Client %u LT cruise control ON\n", num);
    recomputeAndSend(now); return;
  }
  if (strcmp(key, "*RTC") == 0) {
    c.rtCCon = true;
    if (c.rtVal > 0) c.rtCC = c.rtVal;
    debugPrintf("[CC] Client %u RT cruise control ON\n", num);
    recomputeAndSend(now); return;
  }

  // D-pad (WS value 0..7 direction, anything else = centered)
  if (strncmp(key, "*DPAD:", 6) == 0) {
    int v = atoi(key + 6);
    c.dpadDir = (v >= 0 && v <= 7) ? (uint8_t)v : 8;
    c.dpadTs = now; recomputeAndSend(now); return;
  }

  // Analog sticks (-127..127, scaled to int16 in recomputeAndSend)
  if (strncmp(key, "*LX:", 4) == 0) { c.lx = constrain(atoi(key + 4), -127, 127); c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*LY:", 4) == 0) { c.ly = constrain(atoi(key + 4), -127, 127); c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RX:", 4) == 0) { c.rx = constrain(atoi(key + 4), -127, 127); c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RY:", 4) == 0) { c.ry = constrain(atoi(key + 4), -127, 127); c.axisTs = now; recomputeAndSend(now); return; }

  // Analog triggers (0..32768). When CC is locked, refresh the locked value.
  if (strncmp(key, "*LT:", 4) == 0) {
    int v = constrain(atoi(key + 4), 0, 32768);
    c.ltVal = v;
    if (c.ltCCon && v > 0) c.ltCC = v;
    c.axisTs = now; recomputeAndSend(now); return;
  }
  if (strncmp(key, "*RT:", 4) == 0) {
    int v = constrain(atoi(key + 4), 0, 32768);
    c.rtVal = v;
    if (c.rtCCon && v > 0) c.rtCC = v;
    c.axisTs = now; recomputeAndSend(now); return;
  }
}

static void handleKeyUp(uint8_t num, const char* key, unsigned long now) {
  if (num >= MAX_WS_CLIENTS) return;
  ClientState& c = clients[num];
  c.lastSeen = now;

  uint8_t b = btnIndex(key);
  if (b != 255) { c.btn[b] = false; recomputeAndSend(now); return; }

  // CC toggle release → unlock, live value stays zero (or reverts to live)
  if (strcmp(key, "*LTC") == 0) { c.ltCCon = false; c.ltCC = 0; recomputeAndSend(now); return; }
  if (strcmp(key, "*RTC") == 0) { c.rtCCon = false; c.rtCC = 0; recomputeAndSend(now); return; }

  // D-pad release → centered
  if (strncmp(key, "*DPAD", 5) == 0) { c.dpadDir = 8; c.dpadTs = now; recomputeAndSend(now); return; }

  // Stick release → center (locked CC values unaffected)
  if (strncmp(key, "*LX", 3) == 0) { c.lx = 0; c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*LY", 3) == 0) { c.ly = 0; c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RX", 3) == 0) { c.rx = 0; c.axisTs = now; recomputeAndSend(now); return; }
  if (strncmp(key, "*RY", 3) == 0) { c.ry = 0; c.axisTs = now; recomputeAndSend(now); return; }

  // Trigger release → zero live value (CC lock persists if active)
  if (strcmp(key, "*LT") == 0) { c.ltVal = 0; c.axisTs = now; recomputeAndSend(now); return; }
  if (strcmp(key, "*RT") == 0) { c.rtVal = 0; c.axisTs = now; recomputeAndSend(now); return; }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    const char* msg = (const char*)payload;
    if (strncmp(msg, "#HOST:", 6) == 0) {
      const char* h = msg + 6;
      size_t hlen = strlen(h);
      if (h && hlen > 0 && hlen < 32) {
        snprintf(hostname, sizeof(hostname), "%s", h);
        Preferences p; p.begin("tgpadxb", false); p.putString("hostname", hostname); p.end();
        webSocket.broadcastTXT(msg);
      }
    } else if (strncmp(msg, "#NAMING:", 8) == 0) {
      const char* mode = msg + 8;
      size_t mlen = strlen(mode);
      if ((mlen == 2 && strncmp(mode, "xs", 3) == 0) || (mlen == 4 && strncmp(mode, "x360", 5) == 0)) {
        strncpy(buttonNamingMode, mode, sizeof(buttonNamingMode)); buttonNamingMode[sizeof(buttonNamingMode)-1] = '\0';
        Preferences p; p.begin("tgpadxb", false); p.putString("namingmode", buttonNamingMode); p.end();
        webSocket.broadcastTXT(msg);
      }
    } else if (strncmp(msg, "#PING", 5) == 0) {
      if (num < MAX_WS_CLIENTS) clients[num].lastSeen = millis();
    } else if (length > 1 && msg[0] == '~') {
      handleKeyUp(num, msg + 1, millis());
    } else {
      handleKeyDown(num, msg, millis());
    }

  } else if (type == WStype_CONNECTED) {
    debugPrintf("[WS] Client %u connected\n", num);
    wsClientCount++;
    updateDisplay();
    clients[num] = ClientState();
    clients[num].active = true;
    webSocket.sendTXT(num, "Connected to TGPad-XB XInput");
    char buf[40];
    snprintf(buf, sizeof(buf), "#HOST:%s", hostname);
    webSocket.sendTXT(num, buf);
    snprintf(buf, sizeof(buf), "#NAMING:%s", buttonNamingMode);
    webSocket.sendTXT(num, buf);

  } else if (type == WStype_DISCONNECTED) {
    debugPrintf("[WS] Client %u disconnected\n", num);
    if (wsClientCount > 0) wsClientCount--;
    updateDisplay();
    clients[num] = ClientState();
    recomputeAndSend(millis());
  }
}

#include "webpage.h"

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "text/html", index_html);
}

#ifdef ARDUINO_M5STACK_ATOMS3
static bool initAtomS3Display() {
  static lgfx::Bus_SPI bus;
  static lgfx::Panel_GC9107 panel;

  auto busCfg = bus.config();
  busCfg.pin_mosi = GPIO_NUM_21;
  busCfg.pin_miso = (gpio_num_t)-1;
  busCfg.pin_sclk = GPIO_NUM_17;
  busCfg.pin_dc   = GPIO_NUM_33;
  busCfg.spi_mode = 0;
  busCfg.spi_3wire = true;
  busCfg.spi_host = SPI3_HOST;
  busCfg.freq_write = 40000000;
  busCfg.freq_read  = 16000000;
  bus.config(busCfg);
  bus.init();

  auto panelCfg = panel.config();
  panelCfg.pin_cs  = GPIO_NUM_15;
  panelCfg.pin_rst = GPIO_NUM_34;
  panelCfg.panel_width  = 128;
  panelCfg.panel_height = 128;
  panelCfg.offset_y = 32;
  panelCfg.readable = false;
  panelCfg.bus_shared = false;
  panel.config(panelCfg);
  panel.bus(&bus);

  static lgfx::Light_PWM light;
  auto lightCfg = light.config();
  lightCfg.pin_bl = GPIO_NUM_16;
  lightCfg.pwm_channel = 7;
  lightCfg.freq = 256;
  lightCfg.invert = false;
  lightCfg.offset = 48;
  light.config(lightCfg);
  light.init(128);
  panel.setLight(&light);

  display.init(&panel);
  display.setBrightness(128);
  return true;
}

static void bootMsg(const char* s1, const char* s2) {
  display.fillScreen(TFT_BLACK);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.printf("TGPad-XB v%s", VERSION);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  int y = 18;
  if (s1) { display.setCursor(0, y); display.println(s1); y += 18; }
  if (s2) { display.setCursor(0, y); display.println(s2); }
}

static void updateDisplay() {
  display.fillScreen(TFT_BLACK);
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.printf("TGPad-XB v%s\n", VERSION);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    display.println(WiFi.localIP());
    char buf[32];
    snprintf(buf, sizeof(buf), "%s.local", hostname);
    display.println(buf);
  } else {
    display.println("No WiFi");
  }
  display.printf("Clients: %d", wsClientCount);
}
#else
static void bootMsg(const char*, const char*) {}
static void updateDisplay() {}
#endif

void setup() {
#ifdef ARDUINO_M5STACK_ATOMS3
  Serial.begin(115200);
#else
  Serial0.begin(115200);
#endif
  setCpuFrequencyMhz(240);
  delay(500);

#ifdef ARDUINO_M5STACK_ATOMS3
  initAtomS3Display();
#endif
  bootMsg("Starting...", nullptr);

  ESP32XInput.begin(0x045E, 0x028E);

  {
    Preferences p;
    p.begin("tgpadxb", true);
    String h = p.getString("hostname", "tgpadxb");
    snprintf(hostname, sizeof(hostname), "%s", h.c_str());
    strncpy(buttonNamingMode, p.getString("namingmode", "xs").c_str(), sizeof(buttonNamingMode)); buttonNamingMode[sizeof(buttonNamingMode)-1] = '\0';
    p.end();
  }

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  bootMsg("WiFi connecting...", nullptr);
  WiFiManager wm;
  wm.setHostname(hostname);
  wm.addParameter(&customHostnameParam);
  wm.setSaveConfigCallback([]() { portalConfigSaved = true; });
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  if (!wm.autoConnect("TGPad-XB-Config")) {
    debugPrint("[WARN] WiFi timeout! Proceeding anyway.\n");
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (portalConfigSaved) {
      const char* h = customHostnameParam.getValue();
      if (h && strlen(h) > 0) {
        snprintf(hostname, sizeof(hostname), "%s", h);
        Preferences p; p.begin("tgpadxb", false); p.putString("hostname", hostname); p.end();
      }
    }

    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    char mdnsHostname[40];
    snprintf(mdnsHostname, sizeof(mdnsHostname), "%s.local", hostname);
    bootMsg(ipStr, mdnsHostname);

    debugPrintf("[WiFi] Connected! IP=%s\n", ipStr);
    esp_wifi_set_ps(WIFI_PS_NONE);

    ArduinoOTA.setHostname(hostname);
#ifdef OTA_PASS
    ArduinoOTA.setPassword(OTA_PASS);
#endif
    ArduinoOTA.onStart([]() { debugPrint("[OTA] Start"); });
    ArduinoOTA.onEnd([]() { debugPrint("[OTA] End"); });
    ArduinoOTA.begin();

    if (MDNS.begin(hostname)) {
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("ws", "tcp", 81);
    }

    WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        esp_netif_t *sta = WiFi.STA.netif();
        if (sta) mdns_netif_action(sta, (mdns_event_actions_t)(MDNS_EVENT_ANNOUNCE_IP4 | MDNS_EVENT_ANNOUNCE_IP6));
      }
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  } else {
    snprintf(hostname, sizeof(hostname), "tgpadxb");
    bootMsg("WiFi failed!", nullptr);
  }

  // LED callback → broadcast to web UI light panel
  ESP32XInput.onLed([](uint8_t ledIndex) {
    char buf[16];
    snprintf(buf, sizeof(buf), "#LED:%d", ledIndex);
    webSocket.broadcastTXT((const uint8_t*)buf, strlen(buf));
  });

  // Rumble callback → broadcast to web UI for haptic feedback
  ESP32XInput.onRumble([](uint8_t leftMotor, uint8_t rightMotor) {
    char buf[32];
    snprintf(buf, sizeof(buf), "#RUMBLE:%d,%d", leftMotor, rightMotor);
    webSocket.broadcastTXT((const uint8_t*)buf, strlen(buf));
  });

  server.on("/", handleRoot);
  server.on("/favicon.ico", [](){server.send(204, "text/plain", "");});
  server.on("/update", HTTP_GET, []() {
#ifdef OTA_PASS
    if (!server.authenticate("admin", OTA_PASS)) return server.requestAuthentication(BASIC_AUTH, "TGPad-XB OTA");
#endif
    server.sendHeader("Connection", "close");
    server.send(200, "text/html",
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='firmware'><br><br>"
      "<input type='submit' value='Update Firmware'>"
      "</form>");
  });
  server.on("/update", HTTP_POST, []() {
#ifdef OTA_PASS
    if (!server.authenticate("admin", OTA_PASS)) { server.send(401, "text/plain", "Unauthorized"); return; }
#endif
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
  }, []() {
    HTTPUpload &upload = server.upload();
    static bool uploadAborted = false;
    if (upload.status == UPLOAD_FILE_START) {
      uploadAborted = false;
#ifdef OTA_PASS
      if (!server.authenticate("admin", OTA_PASS)) { uploadAborted = true; return; }
#endif
      debugPrintf("[OTA Web] Start: %s\n", upload.filename.c_str());
      Update.begin(upload.totalSize, U_FLASH);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (!uploadAborted) Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (!uploadAborted) Update.end(true);
    }
  });
  server.begin();

  webSocket.onEvent(webSocketEvent);
  webSocket.begin();

  updateDisplay();
}

static void handleWdt(unsigned long now) {
  bool released = false;
  for (uint8_t i = 0; i < MAX_WS_CLIENTS; i++) {
    ClientState& c = clients[i];
    if (!c.active) continue;
    if (now - c.lastSeen > 5000) {
      bool held = false;
      for (uint8_t b = BUTTON_LO; b < 16; b++) if (c.btn[b]) held = true;
      if (held || c.lx || c.ly || c.rx || c.ry || c.dpadDir != 8 || c.ltVal || c.rtVal || c.ltCC || c.rtCC) {
        debugPrintf("[WDT] Client %u silent >5s — releasing\n", i);
        c = ClientState();
        released = true;
      } else {
        c.lastSeen = now;
      }
    }
  }
  if (released) recomputeAndSend(now);
}

static void handleResetButton(unsigned long now) {
  bool resetPressed = digitalRead(RESET_BUTTON_PIN) == LOW;
  if (resetPressed && !resetButtonWasLow) {
    resetPressStart = now;
    resetButtonWasLow = true;
  } else if (resetPressed && resetButtonWasLow) {
    if (now - resetPressStart >= 5000) {
      debugPrint("[WiFi] Button held 5s — erasing credentials\n");
#ifdef ARDUINO_M5STACK_ATOMS3
      bootMsg("Resetting WiFi...", nullptr);
#endif
      delay(100);
      WiFiManager wm;
      wm.resetSettings();
      delay(500);
      ESP.restart();
    }
  } else {
    resetButtonWasLow = false;
  }
}

void loop() {
  webSocket.loop();
  server.handleClient();
  ArduinoOTA.handle();
  unsigned long now = millis();
  handleWdt(now);
  handleResetButton(now);

  ESP32XInput.isConnected();
  ESP32XInput.send();
  ESP32XInput.pollRumble();
}
