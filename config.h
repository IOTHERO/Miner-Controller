#pragma once
// ===========================================================================
//  Miner Monitoring - the ONE place for settings and setup values.
//  Compile-time values live here. Values the user changes from the web page
//  (WiFi, scan range, ...) start from the DEF_* defaults below and are then
//  stored in flash.
// ===========================================================================

// ---------------------------------------------------------------------------
// 1. Hardware: W5500 Ethernet module -> ESP32-WROOM-32 (VSPI)
// ---------------------------------------------------------------------------
#define PIN_W5500_SCLK 18
#define PIN_W5500_MISO 19
#define PIN_W5500_MOSI 23
#define PIN_W5500_CS   5
#define PIN_W5500_INT  4
#define PIN_W5500_RST  16

// ---------------------------------------------------------------------------
// 2. General
// ---------------------------------------------------------------------------
#define SERIAL_BAUD        115200
#define WEB_PORT           80
#define NVS_NAMESPACE      "minerscan"   // flash namespace of the saved settings
#define LOOP_STACK_BYTES   (16 * 1024)   // detail requests run in the web handler
#define SCAN_STACK_BYTES   8192
#define POLL_STACK_BYTES   10240

// ---------------------------------------------------------------------------
// 3. Defaults for the first boot (changed later from the web page)
// ---------------------------------------------------------------------------
#define DEF_WIFI_SSID      ""
#define DEF_WIFI_PASS      ""
#define DEF_AP_PASS        "minerscan"   // setup hotspot password (min 8 chars)
#define DEF_ETH_STATIC     false
#define DEF_ETH_IP         "192.168.1.60"
#define DEF_ETH_GW         "192.168.1.1"
#define DEF_ETH_MASK       "255.255.255.0"
#define DEF_SCAN_FROM      "192.168.1.1"
#define DEF_SCAN_TO        "192.168.1.254"
#define DEF_PORTS          "4028,80"
#define DEF_TIMEOUT_MS     400           // TCP connect timeout while scanning
#define DEF_PARALLEL       4             // parallel connections while scanning
#define DEF_AUTO_SCAN      false
#define DEF_AUTO_SCAN_MIN  30            // minutes between automatic scans
#define DEF_AUTO_POLL      true
#define DEF_POLL_SEC       30            // seconds between readings of one miner
#define DEF_LIST_UNKNOWN   false         // also list non-miner devices
#define DEF_AUTH_USER      "admin"
#define DEF_AUTH_PASS      "admin"       // change it in Settings!

// ---------------------------------------------------------------------------
// 4. Limits (what the web page may set)
// ---------------------------------------------------------------------------
#define LIM_TIMEOUT_MIN    150
#define LIM_TIMEOUT_MAX    3000
#define LIM_PARALLEL_MAX   6             // lwIP has ~10 sockets, the web server needs some
#define LIM_AUTO_SCAN_MIN  1
#define LIM_AUTO_SCAN_MAX  1440
#define LIM_POLL_MIN       10
#define LIM_POLL_MAX       3600
#define LIM_PASS_MIN       8             // WiFi / hotspot passwords
#define LIM_AUTH_PASS_MIN  4             // web page password

// ---------------------------------------------------------------------------
// 5. Capacity
// ---------------------------------------------------------------------------
#define MAX_DEV            48            // devices kept in the list
#define MAX_SCAN_HOSTS     1024          // addresses per scan
#define MAX_SCAN_PORTS     6             // ports probed per host

// ---------------------------------------------------------------------------
// 6. WiFi
// ---------------------------------------------------------------------------
#define AP_NAME_PREFIX     "MinerMonitor-"
#define WIFI_FALLBACK_MS   25000         // no WiFi after this long -> open the hotspot

// ---------------------------------------------------------------------------
// 7. Timing of the miner requests (milliseconds / bytes)
// ---------------------------------------------------------------------------
// identification probes
#define PROBE_CONNECT_MS   1000
#define PROBE_TOTAL_MS     2500
#define PROBE_MAX_BYTES    4000
#define PROBE_IDLE_MS      300
// HTTP (port 80)
#define HTTP_CONNECT_MS    1200
#define HTTP_TOTAL_MS      3500
#define HTTP_IDLE_MS       600
#define HTTP_PROBE_BYTES   3000
#define HTTP_DETAIL_BYTES  4000
// miner API on port 4028 (details)
#define API_CONNECT_MS     1500
#define API_TOTAL_MS       6000
#define API_IDLE_MS        400
#define API_MAX_BYTES_WM   24000         // Whatsminer
#define API_MAX_BYTES_CG   20000         // cgminer family
#define WM_CMD_GAP_MS      300           // the miner dislikes back-to-back connections
#define CG_CMD_GAP_MS      200
// background reading
#define POLL_TICK_MS       1000
#define POLL_PORT_CHECK_MS 500
