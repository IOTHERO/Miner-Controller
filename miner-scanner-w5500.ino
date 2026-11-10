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
#include <ESPmDNS.h>

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

// The ESP32's own WiFi network. The web page is always reachable on AP_IP.
static void startAp(bool withSta) {
  if (apOn) return;
  String pass = cfg.apPass.length() >= LIM_PASS_MIN ? cfg.apPass : String(DEF_AP_PASS);
  IPAddress aip, amask;
  aip.fromString(AP_IP);
  amask.fromString(AP_MASK);
  WiFi.mode(withSta ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(aip, aip, amask);
  WiFi.softAP(cfg.apSsid.c_str(), pass.c_str());
  apOn = true;
  Serial.printf("[wifi] hotspot \"%s\"  (password: see Settings / config.h)  web page: http://%s/\n",
                cfg.apSsid.c_str(), WiFi.softAPIP().toString().c_str());
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

  WiFi.setHostname(DEVICE_NAME);
  wifiStartMs = millis();
  bool wantSta = (cfg.wifiRole == WIFI_ROLE_STA || cfg.wifiRole == WIFI_ROLE_AP_STA) &&
                 cfg.wifiSsid.length() > 0;
  if (cfg.wifiRole != WIFI_ROLE_STA || !wantSta) {
    startAp(wantSta);  // own hotspot (default), plus the modem connection when requested
  } else {
    WiFi.mode(WIFI_STA);
  }
  if (wantSta) {
    WiFi.setSleep(false);
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    Serial.printf("[wifi] connecting to modem \"%s\" ...\n", cfg.wifiSsid.c_str());
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
  // Modem WiFi did not connect in time -> open our own hotspot so the page stays reachable
  if (!apOn && cfg.wifiSsid.length() > 0 && millis() - wifiStartMs > WIFI_FALLBACK_MS) {
    startAp(true);
  }
}

static void startMdns() {
  MDNS.end();
  if (MDNS.begin(DEVICE_NAME)) {
    MDNS.addService("http", "tcp", WEB_PORT);
  }
}

static void ethLoop() {
  uint32_t ip = ipU32(ETH.localIP());
  if (ip != lastEthIp) {
    lastEthIp = ip;
    if (ip != 0) {
      Serial.printf("[eth] up, IP %s, web page: http://%s/\n", ipStr(ip).c_str(), ipStr(ip).c_str());
      startMdns();
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
      if (resolveRange(a, b) && startScan(a, b, err)) {
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
  if (resolveRange(a, b)) {
    startScan(a, b, err);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println();
  Serial.println(F("Miner Monitoring  (read only)"));
  Serial.printf("Name: %s   (http://%s.local/)\n", DEVICE_NAME, DEVICE_NAME);
  Serial.println(F("Serial: 's' = start scan, 'p' = print table"));

  loadSettings();
  devInit();
  refreshScanParams();
  netBegin();
  webBegin();
  startMdns();
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
