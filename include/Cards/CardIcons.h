#pragma once
#ifndef CARD_ICONS_H
#define CARD_ICONS_H

#include <stdint.h>
#include "Entity.h"

// ---------------------------------------------------------------------------
// Icons and tints, by what an entity MEASURES rather than by card type.
//
// EVERY GLYPH BELOW IS A PLACEHOLDER, and deliberately so. cards.md section 9
// is explicit that the MDI subset is generated LAST, once the card types have
// settled which glyphs they need, because regenerating it means regenerating
// every board's font blob and one referenced font face costs ~96 KB of flash -
// measured, not estimated. Picking icons before the card types exist is how
// that 96 KB gets spent twice.
//
// So until then these are LVGL's built-in symbols, which live in the Montserrat
// faces UIType already references and therefore cost nothing at all. Several
// are frankly wrong (a droplet for temperature); they are stand-ins for
// position, size and colour, not for meaning. cardIconFor() is the single
// function the generated MDI subset replaces.
//
// The TINTS are not placeholders. UIPalette's sensor tints are real design
// tokens with a real job - cards.md section 0: a wall of sensor cards should be
// scannable by colour before you read a single number - and they key off
// device_class, which is the same string Home Assistant uses and which the
// registry already carries.
// ---------------------------------------------------------------------------

// An LVGL built-in symbol for this entity. Never null.
const char *cardIconFor(const EntityDescriptor &d);

// The icon's tint, as a 0xRRGGBB from the ACTIVE palette. Call it at render
// time and never cache the result - the active palette is a mutable copy that
// UI::setScheme() edits live.
uint32_t cardTintFor(const EntityDescriptor &d);

// Compact age, written into `out`: "now", "45s", "12m", "3h", "2d".
//
// ASCII only, no exceptions and no cleverness. CLAUDE.md: LVGL's stock
// Montserrat covers ASCII plus a small symbol set, and the middle dot and em
// dash that a designer would reach for here render as empty boxes on the
// panels. The degree sign is the one non-ASCII character in range, and it
// arrives from the entity's unit string rather than from here.
void cardFormatAge(uint32_t ageMs, char *out, size_t cap);

// The entity's value as text, with its unit appended when it has one.
// Booleans deliberately produce no words - cards.md section 4 forbids
// "On"/"Off"/"Open"/"Closed" text anywhere, because state is the icon and its
// colour. A bool renders as an empty string here and the card draws the icon.
void cardFormatValue(const Entity &e, char *out, size_t cap, bool withUnit = true);

#endif // CARD_ICONS_H
