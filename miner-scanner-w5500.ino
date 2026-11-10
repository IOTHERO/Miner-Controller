/*
 * Miner Monitoring  -  ESP32-WROOM-32 + W5500 (Ethernet) + WiFi (web page)
 *
 *  - Scans an IP range on the Ethernet side and lists miners it finds
 *    (Whatsminer, Antminer / cgminer family, Avalon, Bitaxe / NerdAxe, ...).
 *  - Shows the list and the details of the selected miner on a web page
 *    (served on the Ethernet IP and on the WiFi IP at the same time).
 *  - Every setting is changed from the web page and stored in flash.
 *  - READ ONLY: no command that changes a miner is ever sent.
 *
 * Needs: Arduino-ESP32 core 3.x, library "ArduinoJson" 7.x
 * Wiring: see config.h / README.md      Serial Monitor: 115200
 */

#include <WiFi.h>
#include <ETH.h>
#include <SPI.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "config.h"
#include "util.h"
#include "settings.h"
#include "devices.h"
#include "probe.h"
#include "detail.h"
#include "scanner.h"
#include "webui.h"
#include "web.h"

SET_LOOP_TASK_STACK_SIZE(LOOP_STACK_BYTES);

static uint32_t wifiStartMs = 0;
static uint32_t lastScanEndMs = 0;
static uint32_t lastEthIp = 0;
static bool autoScanPending = true;

static void startAp() {
  if (apOn) return;
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String name = String(AP_NAME_PREFIX) + mac.substring(8);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(name.c_str(), cfg.apPass.c_str());
  apOn = true;
  Serial.printf("[wifi] setup hotspot \"%s\"  password \"%s\"  http://%s/\n", name.c_str(),
                cfg.apPass.c_str(), WiFi.softAPIP().toString().c_str());
}

static void netBegin() {
  SPI.begin(PIN_W5500_SCLK, PIN_W5500_MISO, PIN_W5500_MOSI);
  if (!ETH.begin(ETH_PHY_W5500, 1, PIN_W5500_CS, PIN_W5500_INT, PIN_W5500_RST, SPI)) {
    Serial.println(F("[eth] W5500 init failed - check wiring (config.h)"));
  }
  if (cfg.ethStatic) {
    uint32_t ip, gw, mask;
    if (parseIp(cfg.ethIp, ip) && parseIp(cfg.ethGw, gw) && parseIp(cfg.ethMask, mask)) {
      ETH.config(u32Ip(ip), u32Ip(gw), u32Ip(mask), u32Ip(gw));
    } else {
      Serial.println(F("[eth] bad static IP settings, using DHCP"));
    }
  }

  wifiStartMs = millis();
  if (cfg.wifiSsid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    Serial.printf("[wifi] connecting to \"%s\" ...\n", cfg.wifiSsid.c_str());
  } else {
    startAp();
  }
}

static void wifiLoop() {
  static bool printed = false;
  if (WiFi.status() == WL_CONNECTED) {
    if (!printed) {
      printed = true;
      Serial.printf("[wifi] connected, web page: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    return;
  }
  printed = false;
  // No WiFi after 25 s -> open the setup hotspot so the page stays reachable
  if (!apOn && cfg.wifiSsid.length() > 0 && millis() - wifiStartMs > WIFI_FALLBACK_MS) {
    startAp();
  }
}

static void ethLoop() {
  uint32_t ip = ipU32(ETH.localIP());
  if (ip != lastEthIp) {
    lastEthIp = ip;
    if (ip != 0) {
      Serial.printf("[eth] up, IP %s, web page: http://%s/\n", ipStr(ip).c_str(), ipStr(ip).c_str());
    } else {
      Serial.println(F("[eth] no IP (cable? DHCP?)"));
    }
  }
}

static void serialCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'p') {
      printTable();
    } else if (c == 's') {
      uint32_t a, b;
      String err;
      if (parseIp(cfg.scanFrom, a) && parseIp(cfg.scanTo, b) && startScan(a, b, err)) {
        Serial.println(F("[scan] started"));
      } else {
        Serial.printf("[scan] not started: %s\n", err.c_str());
      }
    }
  }
}

static void autoScanLoop() {
  if (!cfg.autoScan || scanSt.running || ipU32(ETH.localIP()) == 0) return;
  bool due = autoScanPending || (millis() - lastScanEndMs > (uint32_t)cfg.autoScanMin * 60000UL);
  if (!due) return;
  uint32_t a, b;
  String err;
  autoScanPending = false;
  lastScanEndMs = millis();
  if (parseIp(cfg.scanFrom, a) && parseIp(cfg.scanTo, b)) {
    startScan(a, b, err);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println(F("Miner Monitoring  (read only)"));
  Serial.println(F("Serial: 's' = start scan, 'p' = print table"));

  loadSettings();
  devInit();
  refreshScanParams();
  netBegin();
  webBegin();
  xTaskCreate(pollTask, "poll", POLL_STACK_BYTES, NULL, 1, NULL);
  if (cfg.authPass == DEF_AUTH_PASS) {
    Serial.println(F("[web] login admin / admin  - change the password in Settings!"));
  }
}

void loop() {
  server.handleClient();
  wifiLoop();
  ethLoop();
  serialCommands();
  autoScanLoop();

  if (scanJustFinished) {
    scanJustFinished = false;
    lastScanEndMs = millis();
    Serial.printf("[scan] finished in %lu s, %u found\n",
                  (unsigned long)((scanSt.finishedMs - scanSt.startedMs) / 1000), (unsigned)scanSt.found);
    printTable();
  }
  if (pollJustFinished) {
    pollJustFinished = false;
    printTable();
  }
  delay(2);
}
