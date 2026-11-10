#pragma once
#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include <ETH.h>

// IPv4 as a host-order integer (a.b.c.d -> 0xAABBCCDD)
static uint32_t ipU32(const IPAddress& ip) {
  return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | (uint32_t)ip[3];
}

static IPAddress u32Ip(uint32_t v) {
  return IPAddress((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
}

static String ipStr(uint32_t v) {
  return u32Ip(v).toString();
}

static bool parseIp(const String& s, uint32_t& out) {
  IPAddress ip;
  String t = s;
  t.trim();
  if (!ip.fromString(t)) {
    return false;
  }
  out = ipU32(ip);
  return true;
}

static int parsePorts(const String& s, uint16_t* out, int maxn) {
  int n = 0;
  int start = 0;
  int len = s.length();
  while (start <= len && n < maxn) {
    int c = s.indexOf(',', start);
    if (c < 0) {
      c = len;
    }
    int v = s.substring(start, c).toInt();
    if (v > 0 && v < 65536) {
      out[n++] = (uint16_t)v;
    }
    start = c + 1;
  }
  return n;
}

static String jsonSlice(const String& s) {
  int a = s.indexOf('{');
  int b = s.lastIndexOf('}');
  if (a < 0 || b < a) {
    return String();
  }
  return s.substring(a, b + 1);
}
