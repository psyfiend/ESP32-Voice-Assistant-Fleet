#pragma once
#ifndef CARD_POPUP_H
#define CARD_POPUP_H

#include <lvgl.h>

class Card;

// ---------------------------------------------------------------------------
// CardPopup - the window a long press opens.
//
// The design is docs/design/card-sheet.md sections 11-13. In short:
//
//   - USABLE FIRST: the window appears complete, contents and all, in the
//     first frame after the long press, and closes in one. A growing window
//     costs too much on these panels - LESSONS, "Effects that cover the screen"
//   - nothing behind it: no dim (speed wins)
//   - the way in: the held card is pressed in while its border fades to the
//     accent, and stays so while its window is open (CardPopup.cpp, "The
//     hold"); now and then a spark runs along the window's edge
//     ("Interference")
//   - header: X top left on the controls, a back arrow on every other view -
//     each with one fixed place to go back to; "Area > name" in the
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
    // How long a press must be held to open a window: 250 ms (LVGL's default
    // is 400). GUIManager gives it to the input
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

    // Where the History view asks HA for an entity's past: SystemCore's
    // HaHistory, handed down by GUIManager - UI asks a lower layer, never the
    // reverse. Without it, History says it has nothing to show.
    static void setHistory(class HaHistory *history);

    // Open the window for this card. Ignored while one is already open, and
    // when the finger has moved since it went down.
    static void open(Card &card);

    // Open it from a TAP (Tap action): on its controls, its History or
    // its Members (Members only where the card has some; else the controls).
    // The finger is already up, so the touch is not held as open() holds it.
    static constexpr uint8_t OPEN_CONTROLS = 0, OPEN_HISTORY = 1, OPEN_MEMBERS = 2;
    static constexpr uint8_t OPEN_GROUP = 3;   // a cluster's group view (K154)
    static void openOn(Card &card, uint8_t view);
    // A LONG PRESS'S VIEW (K156): open() - the finger still down -
    // on one of the views above. Saved by name as the card's "long".
    static void openHeldOn(Card &card, uint8_t view);
    static const char *openViewName(uint8_t view);
    static bool openViewFromName(const char *name, uint8_t &view);

    // Close it, by whichever of the four routes. Safe to call from an event
    // callback of an object inside the window: the work is deferred to
    // lv_async_call, because deleting the object whose event is running is
    // the one thing an event callback must not do.
    static void close();

    // A GROUP CARD'S CLUSTER, from the page (K154, K155): the group
    // view draws it, a solo group card's GROUP panel reads it. GUIManager hands the
    // page's maker down (UI registers with the layers below it). See
    // CardPage::makeGroup(); the window deletes what it is given.
    static void setGroupMaker(class ClusterCard *(*maker)(const char *id, bool allTiles));

    // A choice in Card UI wants the page built again (Card density): true once,
    // after the window has gone - it holds the page's cards.
    static bool takeRebuildWanted();

    // True from the long press until the window has gone. GUIManager asks
    // this before acting on a gesture: the popup is modal.
    static bool isOpen();
    // A card drawn inside the open window - the group view's cluster or one of
    // its tiles (K154) - which takes its taps while every other card is locked.
    static bool inWindow(const Card &card);

#ifdef DEBUG_POPUP
    // GET /popup - open, drive and measure a window from a PC, with nobody at
    // the panel. Debug builds only (-D DEBUG_POPUP, never committed):
    // The keys are listed in docs/HTTP_REFERENCE.md.
    // The handler hands the work to the LVGL thread and waits for it, as
    // /screenshot does - it never touches LVGL itself.
    static void beginDebug(class HttpServer &http);
    // drawer=: the screen's to do, so GUIManager hands it over (UI registers
    // with the layers below it, never the reverse). `open` is what is wanted;
    // the hook says whether it was already so.
    static void setDrawerHook(bool (*hook)(bool open));
    // The header's keys (slot, glyphs, pagehdr, alarm, alarmin): docs/
    // HTTP_REFERENCE.md. Answers one line saying what it did.
    static void setHeaderHook(const char *(*hook)(const char *slot, int glyphs, int pageHdr, int alarm, int alarmIn));
private:
    static void debugService(lv_timer_t *t);   // the LVGL-thread half
#endif
};

#endif // CARD_POPUP_H
