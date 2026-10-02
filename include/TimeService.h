#pragma once
#include <stddef.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// TimeService - wall-clock time for the fleet. Issue #74.
//
// Nothing on the fleet knew the time until this existed. The header clock
// (#19), night mode (#75), the alarm clock (#35) and every "8 minutes ago"
// need it, and they all ask the C library (time(), localtime_r()) - so the
// whole job here is to SET that clock and to say where it came from.
//
// ESP-IDF's esp_netif_sntp, not Arduino's configTime(): the fleet prefers
// IDF facilities (CLAUDE.md), and this is the API the ha-dashboard reference
// uses (reference/.../components/time_sync/time_sync.c, MIT).
//
// Owned by SystemCore and started in two halves, like the HTTP server:
// begin() at boot sets the time zone only; loop() starts SNTP the first time
// the link is up, because esp_netif_sntp_init() needs a network interface that
// exists. After that lwIP re-syncs on its own schedule and after every
// reconnect (IP_EVENT_STA_GOT_IP).
//
// A namespace of free functions rather than an object, for the reason
// Card::useRegistry() gives: there is exactly one clock per device, and the
// System Doctor (which must not reach into SystemCore) needs to ask it.
//
// NOT DONE YET: the RTC fallback (PCF85063 on the S3s, the P4's RTC domain on
// its backup cell) - it needs #32's driver and rechargeable cells; and the
// time zone and servers as SETTINGS (ROADMAP 4.x). Both are named in #74.
// ---------------------------------------------------------------------------

// The time zone as a POSIX TZ string; the C library handles daylight saving.
// Pacific by default - the owner's zone. A board elsewhere sets -D FLEET_TZ.
#ifndef FLEET_TZ
#define FLEET_TZ "PST8PDT,M3.2.0,M11.1.0"
#endif

// Public pools; no local NTP server is needed. A server handed out by DHCP is
// accepted as well where the framework was built to ask for one.
#ifndef FLEET_NTP_1
#define FLEET_NTP_1 "pool.ntp.org"
#endif
#ifndef FLEET_NTP_2
#define FLEET_NTP_2 "time.google.com"
#endif
#ifndef FLEET_NTP_3
#define FLEET_NTP_3 "time.windows.com"
#endif

namespace TimeService {

// Applies the time zone. Call once at boot, before anything formats a time.
void begin();

// Starts SNTP the first time `online` is true; reports each new sync on
// Serial from the loop task (the sync callback itself runs on lwIP's task and
// only records it). Cheap; call every loop().
void loop(bool online);

// The clock holds a real date - set by SNTP, or by anything else that called
// settimeofday(). Callers that print a time should check this, never 1970.
bool isSet();

// For the System Doctor, e.g.
//   "2026-10-01 15:02:11 PDT  (SNTP time.google.com, synced 3 min ago)"
//   "not set  (SNTP waiting for the network)"
void describe(char *buf, size_t len);

}  // namespace TimeService
