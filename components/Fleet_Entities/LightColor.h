#pragma once
#ifndef LIGHT_COLOR_H
#define LIGHT_COLOR_H

#include <stdint.h>
#include <math.h>
#include "Entity.h"

// ---------------------------------------------------------------------------
// A light's colour as a screen colour, and colour temperature as a hue. 2.10b.
//
// Header-only and dependency-free, like the rest of this library (ROADMAP Q9).
// Used by VirtualProvider (which plays a light, and must report rgb the way HA
// does - rgb_color for every colour mode), by a group's commands (HA turns a
// colour temperature into a hue for a member that has no temperature), and by
// the popup, which draws the temperature strip with it.
//
// These are approximations for DRAWING and for a light that cannot do better,
// not colorimetry: Tanner Helland's fit of the black-body curve, good to a few
// percent from 1000 K to 40000 K, which is the whole range a lamp offers.
// ---------------------------------------------------------------------------

static inline uint8_t lightClamp8(float v) {
    return (uint8_t)(v < 0.f ? 0.f : v > 255.f ? 255.f : v + 0.5f);
}

// A hue (0-359) and saturation (0-100) at full value, as 0xRRGGBB.
static inline uint32_t lightHsToRgb(int hue, int sat) {
    const float h = (float)(((hue % 360) + 360) % 360) / 60.f;
    const float s = (float)(sat < 0 ? 0 : sat > 100 ? 100 : sat) / 100.f;
    const int   i = (int)h;
    const float f = h - (float)i;
    const float p = 1.f - s, q = 1.f - s * f, t = 1.f - s * (1.f - f);
    float r, g, b;
    switch (i) {
        case 0:  r = 1; g = t; b = p; break;
        case 1:  r = q; g = 1; b = p; break;
        case 2:  r = p; g = 1; b = t; break;
        case 3:  r = p; g = q; b = 1; break;
        case 4:  r = t; g = p; b = 1; break;
        default: r = 1; g = p; b = q; break;
    }
    return ((uint32_t)lightClamp8(r * 255.f) << 16) | ((uint32_t)lightClamp8(g * 255.f) << 8) |
           lightClamp8(b * 255.f);
}

// A colour temperature in kelvin, as 0xRRGGBB.
static inline uint32_t lightKelvinToRgb(int kelvin) {
    const float t = (float)(kelvin < 1000 ? 1000 : kelvin > 40000 ? 40000 : kelvin) / 100.f;
    float r, g, b;
    if (t <= 66.f) {
        r = 255.f;
        g = 99.4708025861f * logf(t) - 161.1195681661f;
        b = (t <= 19.f) ? 0.f : 138.5177312231f * logf(t - 10.f) - 305.0447927307f;
    } else {
        r = 329.698727446f * powf(t - 60.f, -0.1332047592f);
        g = 288.1221695283f * powf(t - 60.f, -0.0755148492f);
        b = 255.f;
    }
    return ((uint32_t)lightClamp8(r) << 16) | ((uint32_t)lightClamp8(g) << 8) | lightClamp8(b);
}

// 0xRRGGBB to hue (0-359) and saturation (0-100).
static inline void lightRgbToHs(uint32_t rgb, int16_t &hue, int8_t &sat) {
    const float r = (float)((rgb >> 16) & 0xFF) / 255.f;
    const float g = (float)((rgb >> 8) & 0xFF) / 255.f;
    const float b = (float)(rgb & 0xFF) / 255.f;
    const float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
    float h = 0.f;
    if (d > 0.f) {
        if (mx == r)      h = 60.f * fmodf((g - b) / d, 6.f);
        else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
        else              h = 60.f * ((r - g) / d + 4.f);
    }
    if (h < 0.f) h += 360.f;
    hue = (int16_t)((int)(h + 0.5f) % 360);
    sat = (int8_t)(mx > 0.f ? (d / mx) * 100.f + 0.5f : 0);
}

// What HA does for a light asked for a colour temperature it cannot show but
// that has a hue: the nearest hue and saturation (color_temperature_to_hs).
static inline void lightKelvinToHs(int kelvin, int16_t &hue, int8_t &sat) {
    lightRgbToHs(lightKelvinToRgb(kelvin), hue, sat);
}

// The colour a light is showing, as HA's rgb_color would give it: from its
// temperature or its hue, whichever mode it is in. 0 when it shows neither.
static inline uint32_t lightShownRgb(const EntityAttrs &a) {
    if (a.lightMode == LightMode::LMODE_TEMP && a.colorTempK > 0) return lightKelvinToRgb(a.colorTempK);
    if (a.lightMode == LightMode::LMODE_COLOUR && a.hue >= 0) return lightHsToRgb(a.hue, a.sat < 0 ? 100 : a.sat);
    return 0;
}

#endif // LIGHT_COLOR_H
