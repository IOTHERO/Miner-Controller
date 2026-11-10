#pragma once
#include <ArduinoJson.h>
#include "probe.h"

// Read-only detail fetch. Fills `out` with:
//   rows:   [[label, value], ...]
//   boards: [[name, temp, ths, status], ...]
//   pools:  [[url, worker, status], ...]
//   errors: [text, ...]
// and the short summary `s` used by the device list.
// No command that changes the miner is ever sent.

static float asThs(float v) {
  if (v <= 0) return 0;
  if (v >= 100000.0f) return v / 1000000.0f;  // MH/s -> TH/s
  if (v >= 500.0f) return v / 1000.0f;        // GH/s -> TH/s
  return v;
}

static float asVolts(float v) {
  if (v >= 100000.0f) return v / 1000.0f;
  if (v >= 1000.0f) return v / 100.0f;
  return v;
}

static float asWatts(float v) {
  return v >= 20000.0f ? v / 1000.0f : v;
}

static String num(float v, int dec, const char* unit) {
  char b[40];
  if (unit && unit[0]) {
    snprintf(b, sizeof(b), "%.*f %s", dec, v, unit);
  } else {
    snprintf(b, sizeof(b), "%.*f", dec, v);
  }
  return String(b);
}

static String upStr(uint32_t sec) {
  char b[32];
  snprintf(b, sizeof(b), "%lud %02lu:%02lu:%02lu", (unsigned long)(sec / 86400UL),
           (unsigned long)((sec % 86400UL) / 3600UL), (unsigned long)((sec % 3600UL) / 60UL),
           (unsigned long)(sec % 60UL));
  return String(b);
}

static void addRow(JsonArray rows, const char* k, const String& v) {
  JsonArray r = rows.add<JsonArray>();
  r.add(String(k));  // copy: some keys come from a document that is freed later
  r.add(v);
}

static bool numAt(JsonObject o, const char* k, float& out) {
  if (o.isNull() || o[k].isNull()) return false;
  out = o[k].as<float>();
  return true;
}

static int firstInt(JsonObject o, const char* const* keys, int n, int def) {
  for (int i = 0; i < n; i++) {
    if (!o[keys[i]].isNull()) return o[keys[i]].as<int>();
  }
  return def;
}

static String strAt(JsonObject o, const char* k1, const char* k2) {
  if (o.isNull()) return String();
  if (!o[k1].isNull()) return o[k1].as<String>();
  if (k2 && !o[k2].isNull()) return o[k2].as<String>();
  return String();
}

// ---------------------------------------------------------------------------
// Whatsminer ({"cmd": ...} on 4028)
// ---------------------------------------------------------------------------

static bool wmCmd(uint32_t ip, const char* cmd, JsonDocument& doc) {
  char req[64];
  snprintf(req, sizeof(req), "{\"cmd\":\"%s\"}\n", cmd);
  delay(WM_CMD_GAP_MS);
  String raw = rawRequest(ip, 4028, req, API_CONNECT_MS, API_TOTAL_MS, API_MAX_BYTES_WM, false, API_IDLE_MS);
  raw.replace(",}", "}");
  raw.replace(",]", "]");
  if (raw.length() == 0) return false;
  doc.clear();
  if (deserializeJson(doc, jsonSlice(raw))) return false;
  int code = doc["Code"] | 0;
  if (code == 14 || code == 23 || code == 45 || code == 132) return false;
  if (doc["STATUS"].is<const char*>()) {
    const char* st = doc["STATUS"].as<const char*>();
    if (st && (st[0] == 'E' || st[0] == 'e')) return false;
  }
  return true;
}

static bool detailWhatsminer(uint32_t ip, JsonDocument& out, Summ& s) {
  JsonArray rows = out["rows"].to<JsonArray>();
  JsonArray boards = out["boards"].to<JsonArray>();
  JsonArray pools = out["pools"].to<JsonArray>();
  JsonArray errs = out["errors"].to<JsonArray>();
  JsonDocument doc;
  bool any = false;

  bool haveHash = false, haveUp = false, haveRej = false, haveFi = false, haveFo = false;
  bool haveVolt = false, havePow = false;
  float hash = 0, rej = 0, volt = 0, pow = 0;
  long fi = 0, fo = 0;
  uint32_t up = 0;
  String mode;
  float env = -1;

  if (wmCmd(ip, "summary", doc)) {
    any = true;
    JsonObject msg = doc["Msg"].as<JsonObject>();
    if (msg.isNull()) msg = doc["SUMMARY"][0].as<JsonObject>();
    JsonObject sm = msg["summary"].is<JsonObject>() ? msg["summary"].as<JsonObject>() : msg;
    if (!sm.isNull()) {
      float v = 0;
      if (numAt(sm, "Uptime", v) || numAt(sm, "Elapsed", v)) {
        haveUp = true;
        up = (uint32_t)v;
      }
      float hr = 0;
      if (numAt(sm, "HS RT", hr) || numAt(sm, "MHS av", hr) || numAt(sm, "GHS av", hr)) {
        if (hr > 0) {
          haveHash = true;
          hash = asThs(hr);
        }
      }
      if (numAt(sm, "Pool Rejected%", v)) {
        haveRej = true;
        rej = v;
      }
      if (!sm["Fan Speed In"].isNull()) {
        haveFi = true;
        fi = sm["Fan Speed In"].as<long>();
      }
      if (!sm["Fan Speed Out"].isNull()) {
        haveFo = true;
        fo = sm["Fan Speed Out"].as<long>();
      }
      if (numAt(sm, "Power", v) || numAt(sm, "power", v)) {
        havePow = true;
        pow = asWatts(v);
      }
      mode = strAt(sm, "Power Mode", "power_mode");
      if (numAt(sm, "Env Temp", v)) env = v;
    }
  }

  if (wmCmd(ip, "get_psu", doc)) {
    any = true;
    JsonObject msg = doc["Msg"].as<JsonObject>();
    JsonObject pw = msg["power"].is<JsonObject>() ? msg["power"].as<JsonObject>() : msg;
    float v = 0;
    if (numAt(pw, "vin", v) || numAt(pw, "Voltage", v) || numAt(pw, "voltage", v)) {
      haveVolt = true;
      volt = asVolts(v);
    }
    if (!havePow && (numAt(pw, "pin", v) || numAt(pw, "Power", v))) {
      havePow = true;
      pow = asWatts(v);
    }
  }

  float sumBoards = 0, maxT = -1;
  if (wmCmd(ip, "edevs", doc)) {
    any = true;
    JsonArray devsA = doc["DEVS"].as<JsonArray>();
    if (devsA.isNull()) {
      JsonObject m = doc["Msg"].as<JsonObject>();
      devsA = m["DEVS"].as<JsonArray>();
      if (devsA.isNull()) devsA = m["edevs"].as<JsonArray>();
    }
    static const char* const SLOT_KEYS[] = {"Slot", "slot", "ASC", "ID"};
    int n = 0;
    for (JsonObject d : devsA) {
      int slot = firstInt(d, SLOT_KEYS, 4, n);
      float t = -1;
      numAt(d, "Temperature", t);
      if (t < 0) numAt(d, "temperature", t);
      float hr = 0;
      if (!numAt(d, "HS RT", hr)) numAt(d, "MHS av", hr);
      float ths = asThs(hr);
      String st = "n/a";
      if (!d["Upfreq Complete"].isNull()) st = d["Upfreq Complete"].as<int>() ? "UP" : "DOWN";
      JsonArray b = boards.add<JsonArray>();
      b.add(slot);
      b.add(t >= 0 ? num(t, 1, "C") : String("-"));
      b.add(ths);
      b.add(st);
      sumBoards += ths;
      if (t > maxT) maxT = t;
      n++;
    }
    if (sumBoards > 0) {
      haveHash = true;
      hash = sumBoards;
    }
  }

  if (wmCmd(ip, "pools", doc)) {
    any = true;
    JsonArray pa = doc["POOLS"].as<JsonArray>();
    if (pa.isNull()) {
      JsonObject m = doc["Msg"].as<JsonObject>();
      pa = m["POOLS"].as<JsonArray>();
      if (pa.isNull()) pa = m["pools"].as<JsonArray>();
      if (pa.isNull()) pa = doc["Msg"].as<JsonArray>();
    }
    int n = 0;
    for (JsonObject p : pa) {
      if (n++ >= 4) break;
      String active = "";
      if (!p["Stratum Active"].isNull()) {
        active = (p["Stratum Active"].as<bool>() || p["Stratum Active"].as<int>() != 0) ? " / ON" : " / OFF";
      }
      JsonArray r = pools.add<JsonArray>();
      r.add(strAt(p, "URL", "url"));
      r.add(strAt(p, "User", "user"));
      r.add(strAt(p, "Status", "status") + active);
    }
  }

  if (wmCmd(ip, "get_error_code", doc)) {
    any = true;
    JsonObject msg = doc["Msg"].as<JsonObject>();
    JsonVariant codes = msg["error_code"];
    if (codes.is<JsonArray>()) {
      for (JsonVariant it : codes.as<JsonArray>()) errs.add(it.as<String>());
    } else if (codes.is<JsonObject>()) {
      for (JsonPair kv : codes.as<JsonObject>()) {
        errs.add(String(kv.key().c_str()) + " " + kv.value().as<String>());
      }
    } else if (codes.is<const char*>()) {
      errs.add(codes.as<String>());
    }
  }

  if (haveUp) addRow(rows, "Uptime", upStr(up));
  if (haveHash) addRow(rows, "Hashrate", num(hash, 2, "TH/s"));
  if (haveRej) addRow(rows, "Reject", num(rej, 2, "%"));
  if (haveFi) addRow(rows, "Fan In", String(fi) + " rpm");
  if (haveFo) addRow(rows, "Fan Out", String(fo) + " rpm");
  if (haveVolt) addRow(rows, "Voltage", num(volt, 1, "V"));
  if (havePow) addRow(rows, "Power", num(pow, 0, "W"));
  if (mode.length()) addRow(rows, "Power Mode", mode);
  if (env >= 0) addRow(rows, "Env Temp", num(env, 1, "C"));

  s.ok = haveHash || boards.size() > 0;
  s.ths = haveHash ? hash : -1;
  s.temp = maxT;
  s.power = havePow ? pow : -1;
  s.uptime = up;
  s.hasUp = haveUp;
  return any;
}

// ---------------------------------------------------------------------------
// cgminer / bmminer style ({"command": ...} on 4028): Antminer, Avalon, ...
// ---------------------------------------------------------------------------

static bool cgCmd(uint32_t ip, const char* cmd, JsonDocument& doc) {
  char req[64];
  snprintf(req, sizeof(req), "{\"command\":\"%s\"}\n", cmd);
  delay(CG_CMD_GAP_MS);
  String raw = rawRequest(ip, 4028, req, API_CONNECT_MS, API_TOTAL_MS, API_MAX_BYTES_CG, false, API_IDLE_MS);
  if (raw.length() == 0) return false;
  doc.clear();
  return !deserializeJson(doc, jsonSlice(raw));
}

static bool startsWith(const char* s, const char* p) {
  return strncmp(s, p, strlen(p)) == 0;
}

static bool detailCgminer(uint32_t ip, JsonDocument& out, Summ& s) {
  JsonArray rows = out["rows"].to<JsonArray>();
  JsonArray boards = out["boards"].to<JsonArray>();
  JsonArray pools = out["pools"].to<JsonArray>();
  out["errors"].to<JsonArray>();
  JsonDocument doc;
  bool any = false;

  if (cgCmd(ip, "summary", doc)) {
    any = true;
    JsonObject sm = doc["SUMMARY"][0].as<JsonObject>();
    float v = 0;
    if (numAt(sm, "Elapsed", v)) {
      s.uptime = (uint32_t)v;
      s.hasUp = true;
      addRow(rows, "Uptime", upStr(s.uptime));
    }
    float ths = -1;
    if (numAt(sm, "GHS 5s", v) && v > 0) {
      ths = v / 1000.0f;
    } else if (numAt(sm, "GHS av", v) && v > 0) {
      ths = v / 1000.0f;
    } else if (numAt(sm, "MHS 5s", v) && v > 0) {
      ths = v / 1000000.0f;
    } else if (numAt(sm, "MHS av", v) && v > 0) {
      ths = v / 1000000.0f;
    }
    if (ths >= 0) {
      s.ths = ths;
      addRow(rows, "Hashrate", num(ths, 2, "TH/s"));
    }
    if (numAt(sm, "Accepted", v)) addRow(rows, "Accepted", String((long)v));
    if (numAt(sm, "Rejected", v)) addRow(rows, "Rejected", String((long)v));
    if (numAt(sm, "Hardware Errors", v)) addRow(rows, "HW errors", String((long)v));
  }

  if (cgCmd(ip, "stats", doc)) {
    any = true;
    int shown = 0;
    float maxT = -1;
    JsonArray st = doc["STATS"].as<JsonArray>();
    for (JsonObject item : st) {
      for (JsonPair kv : item) {
        const char* k = kv.key().c_str();
        bool isFan = startsWith(k, "fan");
        bool isTemp = startsWith(k, "temp");
        bool isRate = startsWith(k, "chain_rate");
        if (!(isFan || isTemp || isRate)) continue;
        if (!(kv.value().is<float>() || kv.value().is<int>() || kv.value().is<const char*>())) continue;
        float v = kv.value().as<float>();
        if (isTemp && v > 10 && v < 150 && v > maxT) maxT = v;
        if (shown < 30 && (v != 0 || isRate)) {
          addRow(rows, k, kv.value().as<String>());
          shown++;
        }
      }
    }
    s.temp = maxT;
  }

  if (cgCmd(ip, "pools", doc)) {
    any = true;
    int n = 0;
    for (JsonObject p : doc["POOLS"].as<JsonArray>()) {
      if (n++ >= 4) break;
      JsonArray r = pools.add<JsonArray>();
      r.add(strAt(p, "URL", nullptr));
      r.add(strAt(p, "User", nullptr));
      r.add(strAt(p, "Status", nullptr));
    }
  }

  s.ok = s.ths >= 0;
  (void)boards;
  return any;
}

// ---------------------------------------------------------------------------
// Bitaxe / NerdAxe (HTTP JSON)
// ---------------------------------------------------------------------------

static bool detailAxe(uint32_t ip, JsonDocument& out, Summ& s) {
  JsonArray rows = out["rows"].to<JsonArray>();
  out["boards"].to<JsonArray>();
  JsonArray pools = out["pools"].to<JsonArray>();
  out["errors"].to<JsonArray>();
  String h, b;
  int code = httpGet(ip, "/api/system/info", h, b, HTTP_DETAIL_BYTES);
  if (code != 200) return false;
  JsonDocument doc;
  if (deserializeJson(doc, jsonSlice(b))) return false;

  String host = doc["hostname"] | "";
  if (host.length()) addRow(rows, "Hostname", host);
  if (!doc["hashRate"].isNull()) {
    s.ths = doc["hashRate"].as<float>() / 1000.0f;  // GH/s -> TH/s
    addRow(rows, "Hashrate", num(s.ths, 3, "TH/s"));
  }
  if (!doc["temp"].isNull()) {
    s.temp = doc["temp"].as<float>();
    addRow(rows, "ASIC temp", num(s.temp, 1, "C"));
  }
  if (!doc["vrTemp"].isNull()) addRow(rows, "VR temp", num(doc["vrTemp"].as<float>(), 1, "C"));
  if (!doc["power"].isNull()) {
    s.power = doc["power"].as<float>();
    addRow(rows, "Power", num(s.power, 1, "W"));
  }
  if (!doc["voltage"].isNull()) addRow(rows, "Input voltage", num(doc["voltage"].as<float>() / 1000.0f, 2, "V"));
  if (!doc["frequency"].isNull()) addRow(rows, "Frequency", num(doc["frequency"].as<float>(), 0, "MHz"));
  if (!doc["fanrpm"].isNull()) addRow(rows, "Fan", String(doc["fanrpm"].as<int>()) + " rpm");
  if (!doc["uptimeSeconds"].isNull()) {
    s.uptime = doc["uptimeSeconds"].as<uint32_t>();
    s.hasUp = true;
    addRow(rows, "Uptime", upStr(s.uptime));
  }
  if (!doc["sharesAccepted"].isNull()) {
    addRow(rows, "Shares OK / Rej",
           String(doc["sharesAccepted"].as<long>()) + " / " + String(doc["sharesRejected"].as<long>()));
  }
  if (!doc["wifiRSSI"].isNull()) addRow(rows, "WiFi RSSI", String(doc["wifiRSSI"].as<int>()) + " dBm");
  String ver = doc["version"] | "";
  if (ver.length()) addRow(rows, "Firmware", ver);

  JsonArray p = pools.add<JsonArray>();
  p.add(String(doc["stratumURL"] | ""));
  p.add(String(doc["stratumUser"] | ""));
  p.add(String(""));
  s.ok = s.ths >= 0;
  return true;
}

// ---------------------------------------------------------------------------

static bool fetchDetail(uint32_t ip, const Dev& d, JsonDocument& out, Summ& s) {
  out["ip"] = ipStr(ip);
  out["brand"] = String(d.brand);
  out["model"] = String(d.model);
  bool ok = false;
  if (strcmp(d.brand, "Whatsminer") == 0) {
    ok = detailWhatsminer(ip, out, s);
  } else if (strcmp(d.brand, "Bitaxe") == 0 || strcmp(d.brand, "NerdAxe") == 0) {
    ok = detailAxe(ip, out, s);
  } else if (strstr(d.ports, "4028")) {
    ok = detailCgminer(ip, out, s);
  }
  if (!out["rows"].is<JsonArray>()) {
    JsonArray rows = out["rows"].to<JsonArray>();
    out["boards"].to<JsonArray>();
    out["pools"].to<JsonArray>();
    out["errors"].to<JsonArray>();
    addRow(rows, "Ports", String(d.ports));
    if (d.info[0]) addRow(rows, "Info", String(d.info));
  }
  out["ok"] = ok;
  if (!ok) {
    out["message"] = "No data (API disabled, locked, or the device is not a supported miner)";
  }
  return ok;
}
