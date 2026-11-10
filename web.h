#pragma once
#include <WebServer.h>
#include "scanner.h"
#include "webui.h"

static WebServer server(WEB_PORT);
static bool apOn = false;

static bool auth() {
  if (server.authenticate(cfg.authUser.c_str(), cfg.authPass.c_str())) return true;
  server.requestAuthentication();
  return false;
}

static void sendJson(JsonDocument& d, int code = 200) {
  String s;
  serializeJson(d, s);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", s);
}

static void sendResult(bool ok, const char* err = nullptr, bool reboot = false) {
  JsonDocument d;
  d["ok"] = ok;
  if (err) d["error"] = err;
  if (reboot) d["reboot"] = true;
  sendJson(d, ok ? 200 : 400);
}

static bool bodyJson(JsonDocument& d) {
  if (!server.hasArg("plain")) return false;
  return !deserializeJson(d, server.arg("plain"));
}

static void handleRoot() {
  if (!auth()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}

static void handleStatus() {
  if (!auth()) return;
  JsonDocument d;
  bool up = ETH.linkUp();
  uint32_t eip = ipU32(ETH.localIP());
  uint32_t wip = ipU32(WiFi.localIP());
  d["eth"]["up"] = up && eip != 0;
  d["eth"]["ip"] = ipStr(eip);
  d["wifi"]["ip"] = ipStr(wip);
  d["wifi"]["ssid"] = WiFi.SSID();
  d["wifi"]["ap"] = apOn;
  d["wifi"]["apIp"] = WiFi.softAPIP().toString();
  d["heap"] = ESP.getFreeHeap() / 1024;
  d["uptime"] = millis() / 1000;
  d["scan"]["running"] = (bool)scanSt.running;
  d["scan"]["total"] = (uint32_t)scanSt.total;
  d["scan"]["done"] = (uint32_t)scanSt.done;
  d["scan"]["found"] = (uint32_t)scanSt.found;
  d["scan"]["lastSec"] = scanSt.finishedMs > scanSt.startedMs ? (scanSt.finishedMs - scanSt.startedMs) / 1000 : 0;
  if (eip != 0) {
    uint32_t net = eip & 0xFFFFFF00UL;
    d["suggest"]["from"] = ipStr(net + 1);
    d["suggest"]["to"] = ipStr(net + 254);
  }
  d["sameSubnet"] = (eip != 0 && wip != 0 && (eip & 0xFFFFFF00UL) == (wip & 0xFFFFFF00UL));
  sendJson(d);
}

static void handleDevices() {
  if (!auth()) return;
  JsonDocument d;
  JsonArray arr = d.to<JsonArray>();
  int idx[MAX_DEV];
  int n = devSorted(idx);
  for (int i = 0; i < n; i++) {
    Dev v;
    if (!devGet(idx[i], v)) continue;
    JsonObject o = arr.add<JsonObject>();
    o["ip"] = ipStr(v.ip);
    o["brand"] = String(v.brand);
    o["model"] = String(v.model);
    o["info"] = String(v.info);
    o["ports"] = String(v.ports);
    o["online"] = v.online;
    o["manual"] = v.manual;
    o["hasStats"] = v.hasStats;
    if (v.hasStats) {
      o["ths"] = v.ths;
      o["temp"] = v.temp;
      o["power"] = v.power;
      o["uptime"] = v.uptime;
    }
  }
  sendJson(d);
}

static void handleScan() {
  if (!auth()) return;
  JsonDocument b;
  if (!bodyJson(b)) {
    sendResult(false, "bad request");
    return;
  }
  uint32_t from, to;
  String f = b["from"] | "";
  String t = b["to"] | "";
  if (!parseIp(f, from) || !parseIp(t, to)) {
    sendResult(false, "invalid IP");
    return;
  }
  String err;
  if (!startScan(from, to, err)) {
    sendResult(false, err.c_str());
    return;
  }
  cfg.scanFrom = f;
  cfg.scanTo = t;
  saveSettings();
  sendResult(true);
}

static void handleScanStop() {
  if (!auth()) return;
  stopScan();
  sendResult(true);
}

static void handleAdd() {
  if (!auth()) return;
  JsonDocument b;
  if (!bodyJson(b)) {
    sendResult(false, "bad request");
    return;
  }
  uint32_t ip;
  String s = b["ip"] | "";
  if (!parseIp(s, ip)) {
    sendResult(false, "invalid IP");
    return;
  }
  manualAdd(ip);
  sendResult(true);
}

static void handleClear() {
  if (!auth()) return;
  devClear();
  sendResult(true);
}

static void handleDetail() {
  if (!auth()) return;
  uint32_t ip;
  if (!parseIp(server.arg("ip"), ip)) {
    sendResult(false, "invalid IP");
    return;
  }
  Dev d;
  if (!devGetByIp(ip, d)) {
    sendResult(false, "unknown device");
    return;
  }
  JsonDocument out;
  Summ s;
  bool ok = fetchDetail(ip, d, out, s);
  if (ok && s.ok) {
    devSetStats(ip, s);
  }
  sendJson(out);
}

static void handleSettingsGet() {
  if (!auth()) return;
  JsonDocument d;
  d["wifiSsid"] = cfg.wifiSsid;
  d["ethStatic"] = cfg.ethStatic;
  d["ethIp"] = cfg.ethIp;
  d["ethGw"] = cfg.ethGw;
  d["ethMask"] = cfg.ethMask;
  d["scanFrom"] = cfg.scanFrom;
  d["scanTo"] = cfg.scanTo;
  d["ports"] = cfg.ports;
  d["timeoutMs"] = cfg.timeoutMs;
  d["parallel"] = cfg.parallel;
  d["autoScan"] = cfg.autoScan;
  d["autoScanMin"] = cfg.autoScanMin;
  d["autoPoll"] = cfg.autoPoll;
  d["pollSec"] = cfg.pollSec;
  d["listUnknown"] = cfg.listUnknown;
  d["authUser"] = cfg.authUser;
  sendJson(d);
}

// Partial updates are accepted: only the keys present are changed.
// Passwords are never sent back to the browser.
static void handleSettingsPost() {
  if (!auth()) return;
  JsonDocument b;
  if (!bodyJson(b)) {
    sendResult(false, "bad request");
    return;
  }
  bool net = false;

  if (b["wifiSsid"].is<const char*>()) {
    String v = b["wifiSsid"].as<String>();
    if (v != cfg.wifiSsid) net = true;
    cfg.wifiSsid = v;
  }
  if (b["wifiPass"].is<const char*>()) {
    String v = b["wifiPass"].as<String>();
    if (v.length() >= LIM_PASS_MIN || v.length() == 0) {
      cfg.wifiPass = v;
      net = true;
    }
  }
  if (b["apPass"].is<const char*>()) {
    String v = b["apPass"].as<String>();
    if (v.length() >= LIM_PASS_MIN) {
      cfg.apPass = v;
      net = true;
    }
  }
  if (b["ethStatic"].is<bool>()) {
    bool v = b["ethStatic"].as<bool>();
    if (v != cfg.ethStatic) net = true;
    cfg.ethStatic = v;
  }
  uint32_t tmp;
  const char* netKeys[3] = {"ethIp", "ethGw", "ethMask"};
  String* netDst[3] = {&cfg.ethIp, &cfg.ethGw, &cfg.ethMask};
  for (int i = 0; i < 3; i++) {
    if (b[netKeys[i]].is<const char*>()) {
      String v = b[netKeys[i]].as<String>();
      if (parseIp(v, tmp)) {
        if (v != *netDst[i]) net = true;
        *netDst[i] = v;
      }
    }
  }
  if (b["scanFrom"].is<const char*>()) {
    String v = b["scanFrom"].as<String>();
    if (parseIp(v, tmp)) cfg.scanFrom = v;
  }
  if (b["scanTo"].is<const char*>()) {
    String v = b["scanTo"].as<String>();
    if (parseIp(v, tmp)) cfg.scanTo = v;
  }
  if (b["ports"].is<const char*>()) {
    String v = b["ports"].as<String>();
    uint16_t p[MAX_SCAN_PORTS];
    if (parsePorts(v, p, MAX_SCAN_PORTS) > 0) cfg.ports = v;
  }
  if (b["timeoutMs"].is<int>()) {
    int v = b["timeoutMs"].as<int>();
    if (v >= LIM_TIMEOUT_MIN && v <= LIM_TIMEOUT_MAX) cfg.timeoutMs = v;
  }
  if (b["parallel"].is<int>()) {
    int v = b["parallel"].as<int>();
    if (v >= 1 && v <= LIM_PARALLEL_MAX) cfg.parallel = v;
  }
  if (b["autoScan"].is<bool>()) cfg.autoScan = b["autoScan"].as<bool>();
  if (b["autoScanMin"].is<int>()) {
    int v = b["autoScanMin"].as<int>();
    if (v >= LIM_AUTO_SCAN_MIN && v <= LIM_AUTO_SCAN_MAX) cfg.autoScanMin = v;
  }
  if (b["autoPoll"].is<bool>()) cfg.autoPoll = b["autoPoll"].as<bool>();
  if (b["pollSec"].is<int>()) {
    int v = b["pollSec"].as<int>();
    if (v >= LIM_POLL_MIN && v <= LIM_POLL_MAX) cfg.pollSec = v;
  }
  if (b["listUnknown"].is<bool>()) cfg.listUnknown = b["listUnknown"].as<bool>();
  if (b["authUser"].is<const char*>()) {
    String v = b["authUser"].as<String>();
    if (v.length() >= 1) cfg.authUser = v;
  }
  if (b["authPass"].is<const char*>()) {
    String v = b["authPass"].as<String>();
    if (v.length() >= LIM_AUTH_PASS_MIN) cfg.authPass = v;
  }

  saveSettings();
  refreshScanParams();
  sendResult(true, nullptr, net);
}

static void handleReboot() {
  if (!auth()) return;
  sendResult(true);
  delay(300);
  ESP.restart();
}

static void webBegin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/devices", HTTP_GET, handleDevices);
  server.on("/api/detail", HTTP_GET, handleDetail);
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/scan", HTTP_POST, handleScan);
  server.on("/api/scan/stop", HTTP_POST, handleScanStop);
  server.on("/api/add", HTTP_POST, handleAdd);
  server.on("/api/clear", HTTP_POST, handleClear);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
}
