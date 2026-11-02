#pragma once
//
// HaPicker - a list of Home Assistant's entities to tick (2.13, DECISIONS K140,
// K145; artifact "2.13 Add Cards Mocks", screens 1 and 2).
//
// One area's "fleet" entities at a time, each a row: its name, New when it is
// new (HaOffer), its state, and on the right either where it already is ("In
// this cluster", the caller's words) or a tick when chosen - a chosen row is lit
// glass and shows a tick, so the choice is never told by colour alone. Below
// the rows, the other areas as chips (screen 1); or, with `areaColumn`, every
// area in a column beside the rows, HA's floors as its sections (screen 2).
// Showing an area's rows makes them known (HaOffer::seen).
//
// One picker at a time (one sheet shows at a time). LVGL thread.
//
#include <lvgl.h>
#include "UI/HaOffer.h"

namespace HaPicker {

struct Spec {
    const char *area = nullptr;      // the area shown first (a cluster's); null: the first with entities
    bool        areaColumn = false;  // screen 2: every area beside the rows
    // A row the board already holds: the words that say where, or null when it
    // can be chosen. Asked as each row is built.
    const char *(*held)(const HaOffer::Row &r, void *ctx) = nullptr;
    void       *ctx = nullptr;
    void      (*changed)(uint8_t chosen, void *ctx) = nullptr;   // a tick came or went
    // Ticked to start with (New cluster's K92 tap set and header sensors), in
    // the area shown first. Null: nothing.
    bool      (*preselect)(const HaOffer::Row &r) = nullptr;
};

// The picker, `w` x `h` in `parent`. Rebuilt in place when another area is
// chosen; the ticks are kept across areas.
lv_obj_t *build(lv_obj_t *parent, int32_t w, int32_t h, const Spec &spec);
uint8_t   chosen(HaOffer::Row *out, uint8_t cap);   // the ticked rows, every area's
uint8_t   chosenCount();
void      forget();   // the window closed
// For /popup (DEBUG_POPUP): a tap on row `i` of the area showing, or on
// another area (screen 1's chip, screen 2's column) by its id ("" No area).
bool      tapRow(uint8_t i);
bool      tapArea(const char *areaId);

}  // namespace HaPicker
