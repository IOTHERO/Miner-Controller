#pragma once
#include "detail.h"

// ---------------------------------------------------------------------------
// LAN scan: N worker tasks take addresses from a shared counter. They only
// make outgoing connections, so the web server (loop task) is never blocked.
// ---------------------------------------------------------------------------

#define MAX_SCAN_HOSTS 1024
#define MAX_SCAN_PORTS 6

struct ScanState {
  volatile bool running;
  volatile bool stop;
  volatile uint32_t base;
  volatile uint32_t total;
  volatile uint32_t next;
  volatile uint32_t done;
  volatile uint32_t found;
  volatile int active;
  uint16_t ports[MAX_SCAN_PORTS];
  int nports;
  uint32_t timeoutMs;
  uint32_t startedMs;
  uint32_t finishedMs;
};

static ScanState scanSt;
static volatile bool scanJustFinished = false;
static volatile bool pollJustFinished = false;

static void refreshScanParams() {
  scanSt.nports = parsePorts(cfg.ports, scanSt.ports, MAX_SCAN_PORTS);
  if (scanSt.nports == 0) {
    scanSt.ports[0] = 4028;
    scanSt.nports = 1;
  }
  scanSt.timeoutMs = cfg.timeoutMs;
}

static void printTable() {
  int idx[MAX_DEV];
  int n = devSorted(idx);
  Serial.println();
  Serial.println(F("=============================================================================="));
  Serial.printf("MINERS: %d\n", n);
  Serial.println(F("------------------------------------------------------------------------------"));
  Serial.println(F("IP               Brand       Model            TH/s     Temp   Power  Up   Ports"));
  for (int i = 0; i < n; i++) {
    Dev d;
    if (!devGet(idx[i], d)) continue;
    char ths[12] = "-", tmp[12] = "-", pw[12] = "-";
    if (d.hasStats && d.ths >= 0) snprintf(ths, sizeof(ths), "%.2f", d.ths);
    if (d.hasStats && d.temp >= 0) snprintf(tmp, sizeof(tmp), "%.1fC", d.temp);
    if (d.hasStats && d.power >= 0) snprintf(pw, sizeof(pw), "%.0fW", d.power);
    Serial.printf("%-16s %-11s %-16s %-8s %-6s %-6s %-4s %s\n", ipStr(d.ip).c_str(), d.brand,
                  d.model[0] ? d.model : "-", ths, tmp, pw, d.online ? "UP" : "DOWN", d.ports);
  }
  Serial.println(F("=============================================================================="));
}

// Probes one host. manual=true keeps it in the list even if nothing answers.
static void probeHost(uint32_t ip, bool manual) {
  Dev d;
  memset(&d, 0, sizeof(d));
  d.ip = ip;
  d.used = true;
  d.manual = manual;
  bool has4028 = false, has80 = false;
  int cnt = 0;
  size_t pl = 0;
  d.ports[0] = 0;

  for (int i = 0; i < scanSt.nports; i++) {
    if (!manual && scanSt.stop) return;
    uint16_t p = scanSt.ports[i];
    if (tcpOpen(ip, p, scanSt.timeoutMs)) {
      cnt++;
      if (p == 4028) has4028 = true;
      if (p == 80) has80 = true;
      char t[8];
      snprintf(t, sizeof(t), "%s%u", cnt > 1 ? "," : "", (unsigned)p);
      if (pl + strlen(t) < sizeof(d.ports)) {
        strcat(d.ports, t);
        pl += strlen(t);
      }
    }
  }

  if (cnt == 0) {
    if (!manual) return;
    strcpy(d.brand, "?");
    strcpy(d.info, "no response");
    strcpy(d.ports, "-");
    d.online = false;
    devUpsert(d);
    return;
  }

  identify(ip, has4028, has80, d);
  if (!manual && d.brand[0] == '?' && !has4028 && !cfg.listUnknown) {
    return;  // some other device (router, camera, ...)
  }
  d.online = true;
  d.lastSeen = millis();
  devUpsert(d);
  __atomic_fetch_add(&scanSt.found, 1, __ATOMIC_SEQ_CST);
  Serial.printf("[scan] %s  %s %s\n", ipStr(ip).c_str(), d.brand, d.model);
}

static void scanWorker(void*) {
  uint32_t self1 = ipU32(ETH.localIP());
  uint32_t self2 = ipU32(WiFi.localIP());
  while (!scanSt.stop) {
    uint32_t i = __atomic_fetch_add(&scanSt.next, 1, __ATOMIC_SEQ_CST);
    if (i >= scanSt.total) break;
    uint32_t ip = scanSt.base + i;
    if (ip != self1 && ip != self2) {
      probeHost(ip, false);
    }
    __atomic_fetch_add(&scanSt.done, 1, __ATOMIC_SEQ_CST);
  }
  if (__atomic_sub_fetch(&scanSt.active, 1, __ATOMIC_SEQ_CST) == 0) {
    scanSt.finishedMs = millis();
    scanSt.running = false;
    scanJustFinished = true;
  }
  vTaskDelete(NULL);
}

static bool startScan(uint32_t from, uint32_t to, String& err) {
  if (scanSt.running) {
    err = "scan already running";
    return false;
  }
  if (to < from) {
    err = "range end is before start";
    return false;
  }
  if ((to - from) + 1 > MAX_SCAN_HOSTS) {
    err = "range too large (max 1024 addresses)";
    return false;
  }
  if (ipU32(ETH.localIP()) == 0) {
    err = "Ethernet has no IP yet";
    return false;
  }
  refreshScanParams();
  int workers = cfg.parallel;
  if (workers < 1) workers = 1;
  if (workers > 6) workers = 6;

  scanSt.base = from;
  scanSt.total = (to - from) + 1;
  scanSt.next = 0;
  scanSt.done = 0;
  scanSt.found = 0;
  scanSt.stop = false;
  scanSt.active = workers;
  scanSt.startedMs = millis();
  scanSt.running = true;
  Serial.printf("[scan] %s - %s  (%u addresses, %d workers)\n", ipStr(from).c_str(),
                ipStr(to).c_str(), (unsigned)scanSt.total, workers);
  for (int i = 0; i < workers; i++) {
    xTaskCreate(scanWorker, "scan", 8192, NULL, 1, NULL);
  }
  return true;
}

static void stopScan() {
  scanSt.stop = true;
}

// Manual add: probe once, keep even when silent.
static void manualAdd(uint32_t ip) {
  refreshScanParams();
  probeHost(ip, true);
}

// ---------------------------------------------------------------------------
// Poll task: refreshes numbers (hashrate, temperature, power) of known miners.
// Skipped while a scan is running to save sockets.
// ---------------------------------------------------------------------------

static void pollOne(uint32_t ip) {
  Dev d;
  if (!devGetByIp(ip, d)) return;
  JsonDocument out;
  Summ s;
  bool ok = fetchDetail(ip, d, out, s);
  if (ok && s.ok) {
    devSetStats(ip, s);
  } else {
    int p = atoi(d.ports);
    bool open = p > 0 && tcpOpen(ip, (uint16_t)p, 500);
    devPollFail(ip, open);
  }
}

static void pollTask(void*) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (!cfg.autoPoll || scanSt.running) continue;
    bool did = false;
    for (int i = 0; i < MAX_DEV; i++) {
      Dev d;
      if (!devGet(i, d)) continue;
      if (d.lastPoll != 0 && millis() - d.lastPoll < (uint32_t)cfg.pollSec * 1000UL) continue;
      pollOne(d.ip);
      did = true;
      if (scanSt.running || !cfg.autoPoll) break;
    }
    if (did) pollJustFinished = true;
  }
}
