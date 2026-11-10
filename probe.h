#pragma once
#include <ArduinoJson.h>
#include "util.h"
#include "devices.h"
#include "settings.h"

// ---------------------------------------------------------------------------
// Low-level TCP helpers (blocking, used from worker tasks and web handlers)
// ---------------------------------------------------------------------------

static bool tcpOpen(uint32_t ip, uint16_t port, uint32_t timeoutMs) {
  WiFiClient c;
  bool ok = c.connect(u32Ip(ip), port, (int32_t)timeoutMs);
  c.stop();
  return ok;
}

// Sends req, reads until the peer closes, the byte limit is hit, or the line goes
// idle after some data. keepNl=false strips \0 \r \n (cgminer-style JSON).
static String rawRequest(uint32_t ip, uint16_t port, const char* req, uint32_t connTo,
                         uint32_t totalTo, size_t maxBytes, bool keepNl, uint32_t idleMs) {
  String body;
  WiFiClient c;
  if (!c.connect(u32Ip(ip), port, (int32_t)connTo)) {
    return body;
  }
  c.print(req);
  c.flush();
  body.reserve(1024);
  uint8_t buf[256];
  char out[256];
  uint32_t t0 = millis();
  uint32_t last = t0;
  while (millis() - t0 < totalTo && body.length() < maxBytes) {
    int n = c.available();
    if (n > 0) {
      if (n > (int)sizeof(buf)) {
        n = sizeof(buf);
      }
      n = c.read(buf, n);
      int m = 0;
      for (int i = 0; i < n; i++) {
        char ch = (char)buf[i];
        if (ch == '\0') {
          continue;
        }
        if (!keepNl && (ch == '\r' || ch == '\n')) {
          continue;
        }
        out[m++] = ch;
      }
      if (m > 0) {
        body.concat(out, m);
      }
      last = millis();
    } else {
      if (!c.connected()) {
        break;
      }
      if (body.length() > 0 && millis() - last > idleMs) {
        break;
      }
      delay(5);
    }
  }
  c.stop();
  return body;
}

// HTTP/1.0 GET on port 80. Returns the status code (0 = no answer).
static int httpGet(uint32_t ip, const char* path, String& hdr, String& body, size_t maxBytes) {
  String req = String("GET ") + path + " HTTP/1.0\r\nHost: " + ipStr(ip) +
               "\r\nUser-Agent: MinerScan\r\nAccept: */*\r\nConnection: close\r\n\r\n";
  String raw = rawRequest(ip, 80, req.c_str(), 1200, 3500, maxBytes, true, 600);
  hdr = "";
  body = "";
  if (raw.length() < 12) {
    return 0;
  }
  int code = raw.substring(9, 12).toInt();
  int sp = raw.indexOf("\r\n\r\n");
  if (sp < 0) {
    hdr = raw;
    return code;
  }
  hdr = raw.substring(0, sp);
  body = raw.substring(sp + 4);
  return code;
}

// ---------------------------------------------------------------------------
// Identification: which kind of miner is this?
// ---------------------------------------------------------------------------

static void identifyHttp(uint32_t ip, Dev& d) {
  String h, b;

  // Bitaxe / NerdAxe style (ESP-Miner firmware)
  int code = httpGet(ip, "/api/system/info", h, b, 3000);
  if (code == 200 && (b.indexOf("hashRate") >= 0 || b.indexOf("ASICModel") >= 0)) {
    JsonDocument doc;
    if (!deserializeJson(doc, jsonSlice(b))) {
      String host = doc["hostname"] | "";
      String asic = doc["ASICModel"] | "";
      String ver = doc["version"] | "";
      String all = host + " " + asic;
      all.toLowerCase();
      strcpy(d.brand, all.indexOf("nerd") >= 0 ? "NerdAxe" : "Bitaxe");
      asic.toCharArray(d.model, sizeof(d.model));
      ver.toCharArray(d.info, sizeof(d.info));
    }
    return;
  }

  // Generic: look at the front page / login challenge
  code = httpGet(ip, "/", h, b, 3000);
  if (code == 0) {
    return;
  }
  String all = h + "\n" + b;
  String lo = all;
  lo.toLowerCase();

  static const char* KW[][2] = {
      {"antminer", "Antminer"}, {"bitmain", "Antminer"}, {"whatsminer", "Whatsminer"},
      {"axeos", "Bitaxe"},      {"bitaxe", "Bitaxe"},    {"nerdaxe", "NerdAxe"},
      {"avalon", "Avalon"},     {"canaan", "Avalon"},    {"braiins", "Braiins OS"},
      {"vnish", "VNish"},       {"luxos", "LuxOS"},      {"innosilicon", "Innosilicon"},
      {"goldshell", "Goldshell"}, {"ipollo", "iPollo"},  {"jasminer", "Jasminer"},
  };
  for (size_t i = 0; i < sizeof(KW) / sizeof(KW[0]); i++) {
    if (lo.indexOf(KW[i][0]) >= 0) {
      strncpy(d.brand, KW[i][1], sizeof(d.brand) - 1);
      break;
    }
  }

  // Page title or auth realm as a human hint
  String hint;
  int ti = lo.indexOf("<title>");
  if (ti >= 0) {
    int te = lo.indexOf("</title>", ti);
    if (te > ti) {
      hint = all.substring(ti + 7, te);
    }
  }
  if (hint.length() == 0) {
    int ri = lo.indexOf("realm=\"");
    if (ri >= 0) {
      int re = lo.indexOf('"', ri + 7);
      if (re > ri) {
        hint = all.substring(ri + 7, re);
      }
    }
  }
  hint.trim();
  if (hint.length() > 38) {
    hint = hint.substring(0, 38);
  }
  if (d.info[0] == 0 && hint.length() > 0) {
    hint.toCharArray(d.info, sizeof(d.info));
  }
}

static void identify(uint32_t ip, bool has4028, bool has80, Dev& d) {
  strcpy(d.brand, "?");
  d.model[0] = 0;
  d.info[0] = 0;

  if (has4028) {
    // cgminer / bmminer style API (Antminer, Avalon, many others)
    String r = rawRequest(ip, 4028, "{\"command\":\"version\"}\n", 1000, 2500, 4000, false, 300);
    String lo = r;
    lo.toLowerCase();
    if (r.indexOf("\"VERSION\"") >= 0) {
      JsonDocument doc;
      if (!deserializeJson(doc, jsonSlice(r))) {
        JsonObject v = doc["VERSION"][0];
        String type = v["Type"] | "";
        String prod = v["Prod"] | "";
        String mn = v["Miner"] | "";
        String api = v["API"] | "";
        String tl = type + " " + prod + " " + mn;
        tl.toLowerCase();
        if (tl.indexOf("avalon") >= 0 || tl.indexOf("canaan") >= 0 || lo.indexOf("avalon") >= 0) {
          strcpy(d.brand, "Avalon");
        } else if (tl.indexOf("antminer") >= 0 || lo.indexOf("bmminer") >= 0 ||
                   lo.indexOf("antminer") >= 0) {
          strcpy(d.brand, "Antminer");
        } else {
          strcpy(d.brand, "cgminer");
        }
        if (type.length() == 0) {
          type = prod;
        }
        type.toCharArray(d.model, sizeof(d.model));
        if (api.length() > 0) {
          String t = "API " + api;
          t.toCharArray(d.info, sizeof(d.info));
        }
      }
    }

    // Whatsminer API: {"cmd": "..."}
    if (d.brand[0] == '?') {
      String r2 = rawRequest(ip, 4028, "{\"cmd\":\"get_version\"}\n", 1000, 2500, 4000, false, 300);
      if (r2.indexOf("\"Msg\"") >= 0) {
        strcpy(d.brand, "Whatsminer");
        JsonDocument doc;
        if (!deserializeJson(doc, jsonSlice(r2))) {
          String fw = doc["Msg"]["fw_ver"] | "";
          if (fw.length() > 0) {
            String t = "FW " + fw;
            t.toCharArray(d.info, sizeof(d.info));
          }
        }
      } else if (r.length() == 0 && r2.length() == 0) {
        strcpy(d.info, "4028 no reply");
      }
    }
  }

  if (has80 && d.brand[0] == '?') {
    identifyHttp(ip, d);
  }
}
