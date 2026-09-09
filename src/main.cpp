#ifndef BOARD_HAS_PSRAM
#error "PSRAM Required! Enable in Tools menu."
#endif

#include <Arduino.h>
#include "SystemCore.h"
#include "SystemReport.h"
#include "LVGL_Startup.h"
#include "GUIManager.h"

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

static SystemCore core;
static GUIManager gui(core);

void setup() {
    Serial.begin(115200);
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

    // 4. System Doctor. Last, so it reports the finished state of everything
    //    above. false = automatic boot run, not manually triggered.
    SystemReport::run(core, false);

    #ifdef DEBUG_DISPLAY
    Serial.println("[Loop] Entering loop() for the first time.");
    #endif
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

    delay(2);
}
