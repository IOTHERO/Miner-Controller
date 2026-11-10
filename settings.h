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
  p.begin("minerscan", false);
  cfg.wifiSsid = p.getString("wSsid", "");
  cfg.wifiPass = p.getString("wPass", "");
  cfg.apPass = p.getString("apPass", "minerscan");
  cfg.ethStatic = p.getBool("eStatic", false);
  cfg.ethIp = p.getString("eIp", "192.168.1.60");
  cfg.ethGw = p.getString("eGw", "192.168.1.1");
  cfg.ethMask = p.getString("eMask", "255.255.255.0");
  cfg.scanFrom = p.getString("sFrom", "192.168.1.1");
  cfg.scanTo = p.getString("sTo", "192.168.1.254");
  cfg.ports = p.getString("ports", "4028,80");
  cfg.timeoutMs = p.getUShort("toMs", 400);
  cfg.parallel = p.getUChar("par", 4);
  cfg.autoScan = p.getBool("aScan", false);
  cfg.autoScanMin = p.getUShort("aScanMin", 30);
  cfg.autoPoll = p.getBool("aPoll", true);
  cfg.pollSec = p.getUShort("pollSec", 30);
  cfg.listUnknown = p.getBool("lUnk", false);
  cfg.authUser = p.getString("aUser", "admin");
  cfg.authPass = p.getString("aPass", "admin");
  p.end();
}

static void saveSettings() {
  Preferences p;
  p.begin("minerscan", false);
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
