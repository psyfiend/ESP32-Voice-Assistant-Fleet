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
//     hold"); now and then a spark runs along the window's edge
//     ("Interference")
//   - header: X top left on the controls, a back arrow on every other view -
//     each with one fixed place to go back to (2.10c); "Area > name" in the
//     middle, its first part a link up; history and members icons top right -
//     tabs, lit on their own view - and scenes under the chart
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

    // Open it from a TAP (2.10d, Tap action): on its controls, its History or
    // its Members (Members only where the card has some; else the controls).
    // The finger is already up, so the touch is not held as open() holds it.
    static constexpr uint8_t OPEN_CONTROLS = 0, OPEN_HISTORY = 1, OPEN_MEMBERS = 2;
    static void openOn(Card &card, uint8_t view);

    // Close it, by whichever of the four routes. Safe to call from an event
    // callback of an object inside the window: the work is deferred to
    // lv_async_call, because deleting the object whose event is running is
    // the one thing an event callback must not do.
    static void close();

    // The Selector look for the scheme showing (K31; moved to the system
    // panel, K45): the next one - the scheme's own, then the four looks - kept
    // in the settings file at once; and a short name for a button.
    static void cycleSelectorLook();
    static const char *selectorLookShort();

    // True from the long press until the window has gone. GUIManager asks
    // this before acting on a gesture: the popup is modal.
    static bool isOpen();

#ifdef DEBUG_POPUP
    // GET /popup - open, drive and measure a window from a PC, with nobody at
    // the panel. Debug builds only (-D DEBUG_POPUP, never committed):
    //   /popup              the cards on this page, numbered
    //   /popup?card=N       open card N's window; replies with LVGL's pool
    //   /popup?deck=1       open (or fold, deck=0) the settings deck
    //   /popup?ctl=N        a light's control: 0 brightness, 1 temperature, 2 colour
    //   /popup?scene=N      as a tap on scene N; lists the light's scenes (2.10c)
    //   /popup?set=V        as a tap on that control's slider at V (%, K, hue)
    //   /popup?view=N       as the corner icons: 0 the controls (a member's, inside
    //                       one), 1 history, 2 members, 4 scenes; 3 only lists. Each
    //                       lists the entities' levels (and an HA group's members)
    //   /popup?power=1      as a tap on Power
    //   /popup?pause=1|0    pause / resume what the window shows
    //   /popup?member=N     as a tap on row N of Members
    //   /popup?close=1      close it; replies with the pool afterwards
    // The handler hands the work to the LVGL thread and waits for it, as
    // /screenshot does - it never touches LVGL itself.
    static void beginDebug(class HttpServer &http);
private:
    static void debugService(lv_timer_t *t);   // the LVGL-thread half
#endif
};

#endif // CARD_POPUP_H
