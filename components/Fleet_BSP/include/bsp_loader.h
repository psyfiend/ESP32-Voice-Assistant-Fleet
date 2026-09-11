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