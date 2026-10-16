#pragma once
// "UI Tweaks" (2.8 round 3, D-22 / D-25 / D-27): the development window of the
// card looks - a Sheet client like "About this board". Every row is one choice
// made outright on the kit's compact selector, all of them the same width a
// choice so the rows line up; Tokens and Reset all below them. What a row
// sets, and where it is saved, is its owner's (GUIManagerTweaks.cpp): this
// file builds the window and says which choice was made.
// docs/design/components.md, "Sheet".
#include <lvgl.h>
#include <stdint.h>

namespace TweaksSheet {

constexpr uint8_t ROWS_MAX = 10;

// A row: what it sets, its choices, which is chosen now, and what choosing one does.
struct Row {
    const char        *what;
    const char *const *words;
    uint8_t            n;
    uint8_t          (*chosen)();
    void             (*choose)(uint8_t i);
    bool               rebuild;     // the window is built again after it (the selector's own look)
};

struct Spec {
    const Row *rows;
    uint8_t    nRows;
    void     (*tokens)();                     // after the window has gone: the token page
    void     (*resetAll)();                   // confirmed; the rows are read again after it
    void     (*opened)(const lv_area_t &win); // the window is up, here (the deck rises under it)
    void     (*closing)();                    // it is going (the deck goes)
};

bool open(const Spec &spec);   // false when a sheet is already showing
void close();
bool isOpen();
void refresh();                // every row's selector from its chosen()

} // namespace TweaksSheet
