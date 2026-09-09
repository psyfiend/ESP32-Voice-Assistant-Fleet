#pragma once
//
// GUIManager - screen content. The name finally matches the file.
//
// Everything this class used to hold - buffer allocation, lv_init(), driver
// registration, the flush and touch callbacks - now lives in LVGL_Startup.
// What is left is the actual dashboard: root screen, the two decks, the
// z-order sandwich, the header and the panels.
//
// Hardware is owned by SystemCore, not here. GUIManager takes a reference and
// reads from it; it never brings anything up.
//
#include <lvgl.h>
#include "SystemCore.h"
#include "Panel_Header.h"
#include "Panel_Display.h"
#include "Panel_System.h"
#ifdef HAS_AUDIO_HW
#include "Panel_Audio.h"
#endif

class GUIManager {
public:
    explicit GUIManager(SystemCore &core);

    // Builds the dashboard. Call after LVGL_Startup::begin().
    void begin();

    // Per-frame UI work. Does NOT call lv_timer_handler() - that is
    // LVGL_Startup::tick()'s job, and keeping them separate is what lets a
    // future LVGL task own the engine without touching this class.
    void tick();

    Panel_Header &header()      { return _header; }
    Panel_System &systemPanel() { return _pnlSystem; }

private:
    static void headerIconClickCb(lv_event_t *e);
    static void closeSystemPanelCb();
    static void reportUiSection();   // [UI STATE] in the System Doctor

    SystemCore   &_core;
    Panel_Header  _header;
    Panel_System  _pnlSystem;
    Panel_Display _pnlDisplay;
#ifdef HAS_AUDIO_HW
    Panel_Audio   _pnlAudio;
#endif
};
