#ifndef BOARD_HAS_PSRAM
#error "PSRAM Required! Enable in Tools menu."
#endif

#include <Arduino.h>
#include "SystemCore.h"
#include "SystemReport.h"
#include "TimeService.h"
#include "LVGL_Startup.h"
#include "GUIManager.h"
#include "HangWatch.h"

// --= FORCE DEPENDENCIES =--
// PlatformIO's LDF only builds libraries something #includes. These are here to
// keep that true - see LESSONS.md, "SUCCESS can mean your library was never
// compiled."
#include <bb_captouch.h>
#include "Fleet_BSP.h"
#ifdef BSP_HEADER
    #include BSP_HEADER
#endif
// --------------------------

// THE LVGL THREAD NEEDS MORE THAN 8 KB OF STACK.
//
// loopTask is where LVGL runs - lv_timer_handler(), the layout pass and the
// whole draw walk - and arduino-esp32 gives it ARDUINO_LOOP_STACK_SIZE, which
// defaults to 8192 (cores/esp32/main.cpp:17). LVGL's refresh recurses once per
// level of widget nesting, and a card is nested deep: screen, column, host,
// page, cell, surface, body, band, row, label.
//
// That is the real cause of EVERY crash in milestone 2.4. All of them reported
// as "Stack canary watchpoint triggered (loopTask)" with a backtrace full of
// one alternating pair of addresses repeating a dozen times - LVGL walking
// itself down the tree. CYD_S3_3248 hit it first each time simply because it
// is the board where everything is tightest, and the fixes that helped before
// helped by making the tree shallower or the page smaller, never by addressing
// the depth itself.
//
// 16 KB is ordinary for an LVGL application. The System Doctor reports the
// high-water mark so this is a measured number rather than a hopeful one.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static SystemCore core;
static GUIManager gui(core);

// How long to wait for the link before reporting anyway. Generous: a P4 joins
// through the C6 over SDIO and is slower than an S3, and an over-eager timeout
// would reintroduce the very "WiFi down" report this exists to avoid.
static constexpr uint32_t BOOT_REPORT_MAX_MS = 20000;

void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
    // Serial is the chip's own USB port here (HWCDC), not a UART bridge. With
    // a PC attached that has stopped reading the port, every write retries for
    // up to 20 x the TX timeout (100 ms by default: HWCDC::write) - up to 2 s
    // of loop() per print. That was the CYD_P4_4880's freezing on every page
    // swipe, deck swipe and tap (2026-09-29): gone on a wall adapter, back
    // whenever Windows stopped draining the port. 0 = never wait: output
    // flows while someone is reading and is dropped when nobody is.
    Serial.setTxTimeoutMs(0);
#endif
    delay(1000);
    Serial.println("\n=== Fleet Hardware Dashboard (Modular) ===");

    // 1. Hardware, then the data layer. Ordering and its reasons live in
    //    SystemCore::begin(), not in line order here.
    if (!core.begin()) {
        Serial.println("[Main] Core init failed (display) - halting.");
        while (1) delay(100);
    }

    // 2. LVGL engine. Takes the already-initialised managers; see
    //    docs/design/startup.md section 3.2.
    if (!LVGL_Startup::begin(core.display(), core.touch())) {
        Serial.println("[Main] LVGL init failed - halting.");
        while (1) delay(100);
    }

    // 3. Screen content.
    gui.begin();

    // 4. The System Doctor runs from loop(), NOT here. See bootReport() below.
    //
    // It used to run on this line, "last, so it reports the finished state of
    // everything above". That was true of everything synchronous and false of
    // the only parts anyone reads it for: the network. `_conn.begin()` starts
    // an association and returns immediately, so a report taken here always
    // said WiFi down, MQTT down, no IP - on a board that was seconds away from
    // being fully online. The owner: "the initial log ALWAYS shows WIFI is down
    // and MQTT is down".
    //
    // A boot report that is wrong every single time is worse than no boot
    // report, because it trains you to skip the first page of every dump.

    #ifdef DEBUG_DISPLAY
    Serial.println("[Loop] Entering loop() for the first time.");
    #endif
}

// The boot report, fired once the board has had a chance to become what it is
// going to be.
//
// Two ways out, and it needs both. It fires as soon as the link is up, which on
// a healthy board is a second or two and gives a report with a real IP, real
// RSSI and a live broker. It ALSO fires on a timeout, because a board that
// never joins is exactly the board whose boot report matters most - waiting for
// an event that will not happen would mean no report at all on the one occasion
// you need one.
//
// MQTT is deliberately not waited for. A board can be perfectly online with no
// broker configured, and #49 is a long argument for not treating one subsystem
// as a proxy for another.
//
// THE CLOCK IS waited for, briefly (#74). SNTP starts the moment the link is up
// - the same loop() that would fire this report - so a report taken then said
// "Time: not set" on a board whose clock was a second away from being right.
// The same trap the link itself was, one step later. The owner caught it.
// Once the link is up the report holds for the first sync, but no longer than
// BOOT_REPORT_CLOCK_WAIT_MS: a board whose SNTP never answers still reports,
// and says why its clock is not set.
static constexpr uint32_t BOOT_REPORT_CLOCK_WAIT_MS = 10000;

static void bootReport() {
    static bool     done     = false;
    static uint32_t onlineAt = 0;
    if (done) return;

    const uint32_t now = millis();
    const bool online  = core.conn().isOnline();
    const bool expired = now >= BOOT_REPORT_MAX_MS;

    if (!online && !expired) return;

    if (online && !expired) {
        if (!onlineAt) onlineAt = now ? now : 1;
        if (!TimeService::isSet() && (now - onlineAt) < BOOT_REPORT_CLOCK_WAIT_MS) return;
    }

    done = true;
    Serial.printf("[Main] Boot report at %lu ms (%s, clock %s)\n",
                  (unsigned long)now,
                  online ? "link up" : "timed out waiting for the link",
                  TimeService::isSet() ? "set" : "not set");
    SystemReport::run(core, false);   // false = automatic, not user-requested
}

void loop() {
    #ifdef DEBUG_DISPLAY
    // Heartbeat: if this stops incrementing (or the whole boot log repeats
    // from the top), the board is hanging/reboot-looping somewhere in or
    // just after this point, not silently succeeding.
    static uint32_t loopCount = 0;
    loopCount++;
    if (loopCount <= 5 || loopCount % 500 == 0) {
        Serial.printf("[Loop] Heartbeat #%lu (uptime %lu ms)\n", (unsigned long)loopCount, millis());
    }
    #endif

    LVGL_Startup::tick();  // lv_timer_handler()
    gui.tick();            // header, display panel, audio panel
    core.loop();           // connectivity, MQTT, providers, registry staleness

    bootReport();
#ifdef DEBUG_HANG
    HangWatch::beat();
#endif

    delay(2);
}
