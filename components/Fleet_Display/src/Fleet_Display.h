#pragma once
//
// Fleet_Display - the board's display on raw esp_lcd. Milestone 2.9 (#67):
// docs/archive/display/esplcd-step2.md, whose §7 A is the owner's decision behind this
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
    // The most frame buffers any present mode uses (TRIPLE_*), and so the
    // size of every per-buffer array. How many THIS board has follows from
    // its present mode: numFrameBuffers().
    static constexpr uint8_t MAX_FBS = 3;

    bool begin();

    // How frames reach the panel (a BSP_PRESENT_* code, Fleet_BSP.h), decided
    // in begin(): the BSP's override if it sets one, else bspPresentMode()'s
    // rule. A mode that is not built yet is refused, loudly, in favour of
    // TRIPLE_PARTIAL - presentModeRequested() keeps what was asked for.
    uint8_t presentMode() const          { return _mode; }
    uint8_t presentModeRequested() const { return _modeRequested; }
    bool    presentModeFromBsp() const   { return _modeFromBsp; }   // false = the rule
    const char *presentModeReason() const { return _modeReason; }
    uint8_t numFrameBuffers() const      { return _numFbs; }

    // The rotation the flush applies between LVGL and the frame buffer (0-3,
    // the BSP's meaning). Today always the BSP's ROTATION: no panel turns the
    // picture itself. TRIED 2026-09-28 on WS_P4_7B (EK79007, rotation 2): the
    // driver's mirror() - MADCTL 0x36 - with both bit pairs after init, and
    // 0x36 inside the init sequence before sleep-out; the picture never moved.
    // In DSI video mode this panel ignores MADCTL (docs/LESSONS.md). Kept as
    // its own value so a panel that CAN do it only has to change this.
    uint8_t softwareRotation() const     { return _swRot; }

    // Backlight - identical behaviour to DisplayManager's.
    void setBacklight(bool on);
    void setBrightness(uint8_t pct);   // 0-100
    int  getBrightness();

    // The panel as built: physical (unrotated) size, and its frame buffers.
    uint16_t panelWidth()  const { return _w; }
    uint16_t panelHeight() const { return _h; }
    esp_lcd_panel_handle_t panel() const { return _panel; }
    void *frameBuffer(uint8_t i) const { return i < _numFbs ? _fb[i] : nullptr; }
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

    // Frames the panel has finished since boot (from the same interrupt), and
    // when it started scanning (esp_timer, us) - divide the one by the time
    // since the other for its real refresh rate. NOT by uptime: the panel
    // starts seconds into boot.
    uint32_t framesScanned() const { return _frames; }
    int64_t  scanStartUs() const   { return _scanStartUs; }

    // For the System Doctor: the vendored driver and the link as brought up
    // (after BSP defaults are applied). "none" / 0 until begin() succeeds.
    const char *driverName() const { return _driver; }
    uint8_t  lanes() const        { return _lanes; }      // 0 on an RGB panel
    uint32_t laneMbps() const     { return _laneMbps; }
    uint32_t pixelClockHz() const { return _pclkHz; }
    uint32_t bouncePixels() const { return _bounce; }     // RGB only: the bounce buffer, 0 = none

private:
    esp_lcd_panel_handle_t _panel = nullptr;
    const char *_driver   = "none";
    uint8_t     _lanes    = 0;
    uint32_t    _laneMbps = 0;
    uint32_t    _pclkHz   = 0;
    uint32_t    _bounce   = 0;
    void       *_expander = nullptr;   // RGB boards with a TCA9554 (esp_io_expander_handle_t)
    bool beginRgb();                   // defined on RGB boards only (Fleet_Display.cpp)
    void    *_fb[MAX_FBS] = {};
    uint8_t  _numFbs = 0;
    uint8_t  _swRot = 0;
    uint8_t  _mode = 0, _modeRequested = 0;
    bool     _modeFromBsp = false;
    const char *_modeReason = "";
    uint16_t _w = 0, _h = 0;

    volatile uint8_t  _scanning  = 0;
    volatile uint8_t  _submitted = 0;
    volatile uint32_t _frames    = 0;
    int64_t  _scanStartUs = 0;

    int _currentBrightness = 0;
    static constexpr int DEFAULT_BRIGHTNESS = 75;   // as DisplayManager
    void initBacklightPWM();

    friend struct FleetDisplayIsr;
};

#endif // DISPLAY_ESPLCD
