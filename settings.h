#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Everything the user can change from the web page. Stored in flash (NVS),
// so the ESP32 never needs to be re-programmed to change a setting.
struct Settings {
  String wifiSsid, wifiPass, apPass;
  bool ethStatic;
  String ethIp, ethGw, ethMask;
  String scanFrom, scanTo, ports;
  uint16_t timeoutMs;
  uint8_t parallel;
  bool autoScan;
  uint16_t autoScanMin;
  bool autoPoll;
  uint16_t pollSec;
  bool listUnknown;
  String authUser, authPass;
};

static Settings cfg;

static void loadSettings() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  if (RESET_SETTINGS_ON_BOOT) {
    p.clear();  // saved values are dropped, DEF_* from config.h apply
  }
  cfg.wifiSsid = p.getString("wSsid", DEF_WIFI_SSID);
  cfg.wifiPass = p.getString("wPass", DEF_WIFI_PASS);
  cfg.apPass = p.getString("apPass", DEF_AP_PASS);
  cfg.ethStatic = p.getBool("eStatic", DEF_ETH_STATIC);
  cfg.ethIp = p.getString("eIp", DEF_ETH_IP);
  cfg.ethGw = p.getString("eGw", DEF_ETH_GW);
  cfg.ethMask = p.getString("eMask", DEF_ETH_MASK);
  cfg.scanFrom = p.getString("sFrom", DEF_SCAN_FROM);
  cfg.scanTo = p.getString("sTo", DEF_SCAN_TO);
  cfg.ports = p.getString("ports", DEF_PORTS);
  cfg.timeoutMs = p.getUShort("toMs", DEF_TIMEOUT_MS);
  cfg.parallel = p.getUChar("par", DEF_PARALLEL);
  cfg.autoScan = p.getBool("aScan", DEF_AUTO_SCAN);
  cfg.autoScanMin = p.getUShort("aScanMin", DEF_AUTO_SCAN_MIN);
  cfg.autoPoll = p.getBool("aPoll", DEF_AUTO_POLL);
  cfg.pollSec = p.getUShort("pollSec", DEF_POLL_SEC);
  cfg.listUnknown = p.getBool("lUnk", DEF_LIST_UNKNOWN);
  cfg.authUser = p.getString("aUser", DEF_AUTH_USER);
  cfg.authPass = p.getString("aPass", DEF_AUTH_PASS);
  p.end();
}

static void saveSettings() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putString("wSsid", cfg.wifiSsid);
  p.putString("wPass", cfg.wifiPass);
  p.putString("apPass", cfg.apPass);
  p.putBool("eStatic", cfg.ethStatic);
  p.putString("eIp", cfg.ethIp);
  p.putString("eGw", cfg.ethGw);
  p.putString("eMask", cfg.ethMask);
  p.putString("sFrom", cfg.scanFrom);
  p.putString("sTo", cfg.scanTo);
  p.putString("ports", cfg.ports);
  p.putUShort("toMs", cfg.timeoutMs);
  p.putUChar("par", cfg.parallel);
  p.putBool("aScan", cfg.autoScan);
  p.putUShort("aScanMin", cfg.autoScanMin);
  p.putBool("aPoll", cfg.autoPoll);
  p.putUShort("pollSec", cfg.pollSec);
  p.putBool("lUnk", cfg.listUnknown);
  p.putString("aUser", cfg.authUser);
  p.putString("aPass", cfg.authPass);
  p.end();
}
