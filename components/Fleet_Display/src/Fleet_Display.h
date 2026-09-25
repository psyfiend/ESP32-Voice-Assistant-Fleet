#pragma once
//
// Fleet_Display - the board's display on raw esp_lcd. Milestone 2.9 (#67):
// docs/design/esplcd-step2.md, whose §7 A is the owner's decision behind this
// library existing at all.
//
// Built only with -D DISPLAY_ESPLCD. Every other board uses DisplayManager
// (Arduino_GFX), which this does not touch; include/BoardDisplay.h picks one.
// Step 6 deletes DisplayManager and leaves this.
//
// For everything above it, the same small surface DisplayManager offers:
// begin(), setBacklight(), setBrightness(), getBrightness(). The rest is what
// the esp_lcd flush (src/LVGL_Flush_EspLcd.cpp) needs: the panel, its frame
// buffers, and which of them the panel is showing.
//
// No LVGL here, on purpose: SystemCore owns this object, and SystemCore
// includes no LVGL header (CLAUDE.md).
//
#if defined(DISPLAY_ESPLCD)

#include <stdint.h>
#include "esp_lcd_types.h"

class Fleet_Display {
public:
    // Three frame buffers: one being shown, one handed over and waiting for
    // the next frame, one free to draw into - so a draw never waits.
    static constexpr uint8_t NUM_FBS = 3;

    bool begin();

    // Backlight - identical behaviour to DisplayManager's.
    void setBacklight(bool on);
    void setBrightness(uint8_t pct);   // 0-100
    int  getBrightness();

    // The panel as built: physical (unrotated) size, and its frame buffers.
    uint16_t panelWidth()  const { return _w; }
    uint16_t panelHeight() const { return _h; }
    esp_lcd_panel_handle_t panel() const { return _panel; }
    void *frameBuffer(uint8_t i) const { return i < NUM_FBS ? _fb[i] : nullptr; }
    size_t frameBufferBytes() const { return (size_t)_w * _h * 2; }

    // Hand frame buffer `i` to the panel. It is shown from the start of the
    // next frame; no copy is made (IDF recognises its own buffer).
    bool present(uint8_t i);

    // Which buffer the panel is scanning now, and which was handed over last.
    // A buffer that is neither is free to draw into. Updated from the
    // frame-complete interrupt; see Fleet_Display.cpp for why the order of
    // present()'s two steps makes this safe.
    uint8_t scanning()  const { return _scanning; }
    uint8_t submitted() const { return _submitted; }

    // Frames the panel has finished since boot (from the same interrupt).
    uint32_t framesScanned() const { return _frames; }

private:
    esp_lcd_panel_handle_t _panel = nullptr;
    void    *_fb[NUM_FBS] = {};
    uint16_t _w = 0, _h = 0;

    volatile uint8_t  _scanning  = 0;
    volatile uint8_t  _submitted = 0;
    volatile uint32_t _frames    = 0;

    int _currentBrightness = 0;
    static constexpr int DEFAULT_BRIGHTNESS = 75;   // as DisplayManager
    void initBacklightPWM();

    friend struct FleetDisplayIsr;
};

#endif // DISPLAY_ESPLCD
