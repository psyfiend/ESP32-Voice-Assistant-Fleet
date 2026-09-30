#pragma once

// This macro is defined in platformio.ini per environment
#ifdef BSP_HEADER
    #include BSP_HEADER
#else
    #error "No BSP_HEADER defined! Check your platformio.ini build_flags."
#endif

#include <math.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Derived display geometry
//
// These live here rather than in Fleet_BSP.h because they read `bsp_display`,
// which only exists once the board header above has been included. They are
// pure arithmetic over BSP data - no LVGL, no Arduino_GFX - so any layer may
// call them.
//
// See docs/design/tokens.md section 2 for the measurements behind this.
// ---------------------------------------------------------------------------

// True pixel density in pixels per inch. Returns 0 when the board has not
// declared DIAGONAL_IN, so callers can tell "unknown" from "low".
inline uint16_t bspPixelDensity() {
    if (bsp_display.DIAGONAL_IN == 0) return 0;
    const double w = (double)bsp_display.WIDTH;
    const double h = (double)bsp_display.HEIGHT;
    const double diagPx = sqrt(w * w + h * h);
    const double diagIn = (double)bsp_display.DIAGONAL_IN / 10.0;
    return (uint16_t)(diagPx / diagIn + 0.5);
}

// The reference density everything is authored against - the centre of the
// fleet's low cluster (CYD_S3_3248 165, three boards at 170, CYD_S3_8048 187).
// A token written as 16px renders 16 physical px on a 170 PPI panel and is
// scaled up from there, so a given token is the same PHYSICAL size fleet-wide.
#define BSP_PPI_REFERENCE 170.0f

// Multiplier for logical UI units. 1.0 on a reference-density panel, 1.73 on
// WS_P4_5 (294 PPI), 0.97 on CYD_S3_3248 (165 PPI).
//
// Clamped: below 0.8 text stops being legible at our smallest type size, and
// above 2.5 is beyond anything in the fleet and more likely a bad DIAGONAL_IN
// than a real panel.
inline float bspUiScale() {
    const uint16_t ppi = bspPixelDensity();
    if (ppi == 0) return 1.0f;                 // undeclared - behave as before
    float s = (float)ppi / BSP_PPI_REFERENCE;
    if (s < 0.8f) s = 0.8f;
    if (s > 2.5f) s = 2.5f;
    return s;
}

// ---------------------------------------------------------------------------
// Present mode - how frames reach the panel (esp_lcd path only)
//
// The same pattern as UI scale: DERIVED by one rule, with a per-board override
// (DisplayConfig.PRESENT_MODE) for testing and for the board that turns out to
// be the exception. The codes and what each means are in Fleet_BSP.h. The
// System Doctor prints which mode is running and where it came from.
//
// THE RULE is the fleet's measured choices, and it is meant to change as modes
// are built and measured. Every row that is not "measured" is a plan.
// ---------------------------------------------------------------------------

// The rule alone. `why` (optional) gets a short reason for the log.
inline uint8_t bspPresentModeRule(const char **why = nullptr) {
    const char *w;
    uint8_t m;
#if defined(HAS_MIPI_PANEL)
    // MEASURED: WS_P4_5 at rotation 1 and WS_P4_4B at rotation 0 (display-stack.md
    // s8.5-8.10). A DSI panel has no RAM of its own, so it needs framebuffers,
    // and the P4's PPA makes the copy (and any rotation) cheap. Untested for
    // rotation 0: DOUBLE_DIRECT and TRIPLE_FULL, which skip the copy.
    m = BSP_PRESENT_TRIPLE_PARTIAL;
    w = "MIPI-DSI";
#elif defined(HAS_RGB_PANEL)
    // PLAN (2.9 step 4): two framebuffers, LVGL drawing straight into them. The
    // S3 has no PPA, so a rotated RGB panel would need CPU rotation - none of
    // the fleet's RGB boards is rotated.
    m = bsp_display.ROTATION == 0 ? BSP_PRESENT_DOUBLE_DIRECT : BSP_PRESENT_TRIPLE_PARTIAL;
    w = bsp_display.ROTATION == 0 ? "RGB, rotation 0" : "RGB, rotated";
#elif defined(HAS_QSPI_PANEL)
    // PLAN (2.9 step 5): the panel keeps its own picture, so no framebuffer is
    // needed; a wired TE pin lets whole frames be timed to it. GPIO 0 is a
    // strapping pin and never a TE line, so 0 (the zero-fill) reads as "none".
    m = bsp_display.TE > 0 ? BSP_PRESENT_TE_SYNC : BSP_PRESENT_NONE;
    w = bsp_display.TE > 0 ? "QSPI, TE wired" : "QSPI, no TE";
#else
    m = BSP_PRESENT_NONE;
    w = "no panel bus flag";
#endif
    if (why) *why = w;
    return m;
}

// The mode this board asks for: its BSP override if it sets one, else the rule.
inline uint8_t bspPresentMode() {
    return bsp_display.PRESENT_MODE ? bsp_display.PRESENT_MODE : bspPresentModeRule();
}

inline bool bspPresentModeOverridden() { return bsp_display.PRESENT_MODE != 0; }

// Framebuffers a mode needs. Never set separately, so it cannot disagree.
inline uint8_t bspPresentFrameBuffers(uint8_t mode) {
    switch (mode) {
    case BSP_PRESENT_TRIPLE_PARTIAL:
    case BSP_PRESENT_TRIPLE_FULL:    return 3;
    case BSP_PRESENT_DOUBLE_PARTIAL:
    case BSP_PRESENT_DOUBLE_DIRECT:  return 2;
    case BSP_PRESENT_TE_SYNC:        return 0;   // the panel's own RAM
    default:                         return 1;   // NONE
    }
}

inline const char *bspPresentModeName(uint8_t mode) {
    switch (mode) {
    case BSP_PRESENT_TRIPLE_PARTIAL: return "TRIPLE_PARTIAL";
    case BSP_PRESENT_DOUBLE_PARTIAL: return "DOUBLE_PARTIAL";
    case BSP_PRESENT_DOUBLE_DIRECT:  return "DOUBLE_DIRECT";
    case BSP_PRESENT_TRIPLE_FULL:    return "TRIPLE_FULL";
    case BSP_PRESENT_TE_SYNC:        return "TE_SYNC";
    case BSP_PRESENT_NONE:           return "NONE";
    default:                         return "UNKNOWN";
    }
}