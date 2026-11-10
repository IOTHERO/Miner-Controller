#pragma once
#include <Arduino.h>
#include "util.h"

#define MAX_DEV 48

// Short summary used in the list (filled by polling).
struct Summ {
  bool ok = false;
  float ths = -1;
  float temp = -1;
  float power = -1;
  uint32_t uptime = 0;
  bool hasUp = false;
};

struct Dev {
  bool used;
  bool manual;
  bool online;
  uint8_t fails;
  uint32_t ip;
  char brand[16];
  char model[32];
  char info[40];
  char ports[24];
  bool hasStats;
  float ths, temp, power;
  uint32_t uptime;
  uint32_t lastSeen;
  uint32_t lastPoll;
};

static SemaphoreHandle_t devMx = nullptr;
static Dev devs[MAX_DEV];

static void devInit() {
  devMx = xSemaphoreCreateMutex();
  memset(devs, 0, sizeof(devs));
}

static void devLock() { xSemaphoreTake(devMx, portMAX_DELAY); }
static void devUnlock() { xSemaphoreGive(devMx); }

static int devFindLocked(uint32_t ip) {
  for (int i = 0; i < MAX_DEV; i++) {
    if (devs[i].used && devs[i].ip == ip) {
      return i;
    }
  }
  return -1;
}

static void devUpsert(const Dev& n) {
  devLock();
  int i = devFindLocked(n.ip);
  if (i < 0) {
    for (int k = 0; k < MAX_DEV; k++) {
      if (!devs[k].used) {
        i = k;
        break;
      }
    }
    if (i < 0) {
      devUnlock();
      return;
    }
    devs[i] = n;
  } else {
    Dev& d = devs[i];
    d.manual = d.manual || n.manual;
    d.online = n.online;
    if (n.online) {
      d.fails = 0;
      d.lastSeen = n.lastSeen;
    }
    memcpy(d.brand, n.brand, sizeof(d.brand));
    memcpy(d.model, n.model, sizeof(d.model));
    memcpy(d.info, n.info, sizeof(d.info));
    memcpy(d.ports, n.ports, sizeof(d.ports));
  }
  devUnlock();
}

static bool devGet(int i, Dev& out) {
  devLock();
  out = devs[i];
  devUnlock();
  return out.used;
}

static bool devGetByIp(uint32_t ip, Dev& out) {
  devLock();
  int i = devFindLocked(ip);
  if (i >= 0) {
    out = devs[i];
  }
  devUnlock();
  return i >= 0;
}

static void devSetStats(uint32_t ip, const Summ& s) {
  devLock();
  int i = devFindLocked(ip);
  if (i >= 0) {
    Dev& d = devs[i];
    d.online = true;
    d.fails = 0;
    d.lastSeen = millis();
    d.lastPoll = millis();
    d.hasStats = true;
    d.ths = s.ths;
    d.temp = s.temp;
    d.power = s.power;
    d.uptime = s.hasUp ? s.uptime : d.uptime;
  }
  devUnlock();
}

// portOpen: the miner still accepts connections even though we could not read it
// (API locked or busy) -> keep it online, just without numbers.
static void devPollFail(uint32_t ip, bool portOpen) {
  devLock();
  int i = devFindLocked(ip);
  if (i >= 0) {
    Dev& d = devs[i];
    d.lastPoll = millis();
    if (portOpen) {
      d.online = true;
      d.fails = 0;
      d.lastSeen = millis();
    } else {
      if (d.fails < 255) {
        d.fails++;
      }
      if (d.fails >= 3) {
        d.online = false;
      }
    }
  }
  devUnlock();
}

static void devClear() {
  devLock();
  memset(devs, 0, sizeof(devs));
  devUnlock();
}

// Fills idx[] with used slots sorted by IP. Returns the count.
static int devSorted(int* idx) {
  int n = 0;
  devLock();
  for (int i = 0; i < MAX_DEV; i++) {
    if (devs[i].used) {
      idx[n++] = i;
    }
  }
  for (int a = 0; a < n; a++) {
    for (int b = a + 1; b < n; b++) {
      if (devs[idx[b]].ip < devs[idx[a]].ip) {
        int t = idx[a];
        idx[a] = idx[b];
        idx[b] = t;
      }
    }
  }
  devUnlock();
  return n;
}
