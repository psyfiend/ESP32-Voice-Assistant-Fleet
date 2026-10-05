#pragma once
#ifndef CARD_POPUP_H
#define CARD_POPUP_H

#include <lvgl.h>

class Card;

// ---------------------------------------------------------------------------
// CardPopup - the window a long press opens. Milestone 2.10a, issue #65.
//
// The design is docs/design/card-sheet.md sections 11-13 and the artifact
// "Card Popup Mock" (v3), which the owner approved. In short:
//
//   - USABLE FIRST (owner, 2026-10-04): the window appears complete, contents
//     and all, in the first frame after the long press, and closes in one.
//     The mock's grow was built and measured four ways and dropped - LESSONS,
//     "Effects that cover the screen"
//   - nothing behind it: no dim (owner, after trying one: speed wins)
//   - the way in: the held card is pressed in while its border fades to the
//     accent, and stays so while its window is open (CardPopup.cpp, "The
//     hold"); the window's edge crackles a moment ("Interference")
//   - header: X top left (a back arrow on inner views), "Area > name" in the
//     middle, history and members icons top right
//   - modal: nothing outside the window takes a touch or a gesture
//   - closes four ways: the X, a tap outside, a drag down on the header row,
//     and 60 s without a touch
//
// THE WINDOW HOLDS THE ENTITIES, NOT THE CARD. A card can be rebuilt
// underneath (a page rebuild, a scheme change), and a pointer to it would
// dangle. Entities live in the registry for the life of the device. The only
// things taken from the card are copied at open: its entities and words.
//
// The window is the SCREEN's topmost object, so LVGL draws only the window
// where it covers the page; a transparent catcher on lv_layer_top() takes
// every touch outside it. LVGL thread only, like every other UI file.
// ---------------------------------------------------------------------------
class CardPopup {
public:
    // How long a press must be held to open a window: 250 ms (owner,
    // 2026-10-04; LVGL's default is 400). GUIManager gives it to the input
    // device, and the card's hold fades to the accent over exactly this long.
    static constexpr uint32_t LONG_PRESS_MS = 250;

    // Marks a card's surface, so a press on one starts the hold (the border
    // fading to the accent). Card::build() sets it; nothing else uses USER_1.
    static constexpr lv_obj_flag_t CARD_SURFACE_FLAG = LV_OBJ_FLAG_USER_1;

    // Once, from GUIManager::begin(), after the input device exists. Hooks every
    // press so the popup knows where a long press began (a finger that moved
    // is a drag, not a long press) and when the user last touched anything
    // (the auto-close), and starts the hold on a card; and makes the tap
    // catcher, which lives for the life of the device.
    static void begin();

    // Open the window for this card. Ignored while one is already open, and
    // when the finger has moved since it went down.
    static void open(Card &card);

    // Close it, by whichever of the four routes. Safe to call from an event
    // callback of an object inside the window: the work is deferred to
    // lv_async_call, because deleting the object whose event is running is
    // the one thing an event callback must not do.
    static void close();

    // True from the long press until the window has gone. GUIManager asks
    // this before acting on a gesture: the popup is modal.
    static bool isOpen();
};

#endif // CARD_POPUP_H
