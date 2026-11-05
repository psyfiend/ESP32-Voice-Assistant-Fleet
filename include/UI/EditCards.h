#pragma once
//
// EditCards - one list of cards to add, hide, remove or restore (2.13 round 2,
// DECISIONS K148, K149; artifact "2.13 Round 2 Mocks", screens 7-9).
//
// A column on the left - the caller's lists ("In this group", "On the page",
// "Hidden"), then Home Assistant's areas by floor - and the rows of the one
// chosen on the right, each a tick. Under them, the buttons for what is ticked.
//
// THE LISTS SHOW THE RESULT: the cards being added appear in the caller's target
// list dotted, "From Office" or "From the page", before its hidden rows; a card
// ticked to hide or remove is red.
//
// ONE KIND OF TICK AT A TIME (K148): add, hide (hide or remove) or restore. While
// one kind is ticked the other kinds' rows are flat, and a tap on one says why.
//
// The buttons follow what is ticked: Add N (with Copy / Move before it when a
// page card is being added to a group), Remove N and Hide N, Restore N. Each
// calls the caller's act(), which asks its go-or-no-go question (K149) and does
// it. One list at a time (one sheet shows at a time). LVGL thread.
//
// A GROUP'S LIST IS A TAP CYCLE (Spec::cycle; K160, K161): a member's first tap
// flips its pin, its second marks it for removal (red; only one made on the
// board), its third puts it back; a card not in the group is added, then added
// and pinned, then - a sensor only - added to the header (K168), then not. Kinds mix. Each row ends in a square saying what will be
// on the page - a header glyph, or a pin - and one button, Save N, calls act(ACT_SAVE).
//
#include <lvgl.h>
#include "UI/HaOffer.h"

namespace EditCards {

constexpr uint8_t LISTS_MAX = 3;

enum class Tick : uint8_t { TICK_NONE, TICK_ADD, TICK_HIDE, TICK_RESTORE };
enum class From : uint8_t { FROM_HA, FROM_PAGE, FROM_GROUP };

// A row. `tick` says what ticking it means; TICK_NONE is information only (a
// member in the header, a card crowded off the page), flat and not tappable.
struct Item {
    char key[72];        // an HA row's entity ref; else a card's id
    char ref[72];        // a card's HA entity ref, when it has one: its row in an HA area is this one (1.5)
    char name[48];
    char state[28];      // as its card says it
    char mark[40];       // words in place of the tick: "In the header", "Hidden"
    char areaId[32];     // an HA row's area ("From Office")
    From from;
    Tick tick;
    bool canRemove;      // a row to hide that may be removed instead
    bool fresh;          // an HA row new to the board (HaOffer): "New"
    bool sensor;         // a reading: added, its third tap puts it in the header (K168)
    // A GROUP'S MEMBER (cycle): pinned now, and the square saying so - its header
    // glyph in `signTint`, or the pin (null: not on the page).
    bool        pinned;
    const char *signIcon;
    uint32_t    signTint;   // 0: the role's
    // EditCards' own: a card being added, shown in the target list; the list a
    // ticked row was ticked in (-1: an HA area); where the cycle is (1, 2).
    bool    coming;
    int8_t  list;
    uint8_t step;
};

struct List {
    const char *words;
    uint8_t   (*fill)(Item *out, uint8_t cap, void *ctx);
    bool        target;  // where the cards being added go: they show here, dotted
};

enum class Act : uint8_t { ACT_ADD, ACT_HIDE, ACT_REMOVE, ACT_RESTORE, ACT_SAVE };

struct Spec {
    List        lists[LISTS_MAX] = {};
    uint8_t     nLists = 0;
    const char *area = nullptr;      // the HA area shown first; null: the first list
    // An HA row the board already holds: the words that say where, or null.
    const char *(*held)(const HaOffer::Row &r, void *ctx) = nullptr;
    void       *ctx = nullptr;
    bool        copyMove = false;    // a page card being added asks Copy or Move (into a group)
    void      (*act)(Act a, void *ctx) = nullptr;
    bool        keepTicks = false;   // made again (back from a step): the ticks and list kept
    bool        cycle = false;       // a group's: the tap cycle and Save N (K161)
    // The glyph a group's header would show for an HA row (cycle, a sensor's third
    // tap: its square). Null: none shown.
    const char *(*haGlyph)(const HaOffer::Row &r, uint32_t &tint) = nullptr;
    const char *saveWord = nullptr;  // cycle's button: "<word> N" - null "Save" (New group: "Create group", K168)
};
// AT THE LIMIT (K176): no card can be chosen to add - every add row is flat. Within
// 25% of it, build() says so in a toast that stays until tapped.

// The list, `w` x `h` in `parent`. Nothing ticked to start with, unless keepTicks.
lv_obj_t *build(lv_obj_t *parent, int32_t w, int32_t h, const Spec &spec);
uint8_t   chosen(Item *out, uint8_t cap);            // the ticked rows, every list's
bool      haRowOf(const char *ref, HaOffer::Row &out);   // a ticked HA row's own
bool      moving();                                  // Move chosen in the foot (else Copy)
void      forget();                                  // the window closed
// "Garage North Door" / "Garage North Door and Garage South Door" /
// "A, B and C" - the ticked rows' names for a dialog, in the accent (Kit::dialog's
// recolour). `from`: only those; else every one.
void      namesOf(char *out, size_t cap, bool onlyFrom = false, From from = From::FROM_HA);
uint8_t   countOf(From from);                        // ticked rows from there

// For /popup (DEBUG_POPUP): a tap on row `i` of the list showing, on list `k`,
// on an HA area by its id ("" No area), or on a foot button.
bool tapRow(uint8_t i);
bool tapList(uint8_t k);
bool tapArea(const char *areaId);
bool press(Act a);

}  // namespace EditCards
