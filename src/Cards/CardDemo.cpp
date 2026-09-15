#include "Cards/CardDemo.h"
#include "Cards/CardPage.h"
#include "Cards/CardCatalog.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include "SystemReport.h"
#include "SystemEntities.h"
#include "ExternalEntities.h"
#include "VirtualEntities.h"
#include <Arduino.h>
#include <functional>

namespace {

lv_obj_t *s_screen   = nullptr;
lv_obj_t *s_previous = nullptr;
CardPage *s_page     = nullptr;

EntityRegistry *s_reg    = nullptr;
CardBinder     *s_binder = nullptr;

// Which header treatment every card on the page is currently wearing. One knob
// for the whole page rather than per card, because the question cards.md is
// asking is "which of these two do you prefer", and that is easiest to answer
// when the whole page switches at once.
CardHeaderStyle s_hdr = CardHeaderStyle::HDR_TAG;

int s_scheme = 0;

// Which actor treatment the page is wearing. Same idea as s_hdr: a knob rather
// than a decision, because the question is "which of these do you prefer" and
// that is answered by looking at both.
StateCardFill s_actorStyle = StateCardFill::FILL_SURFACE;

// Whether cards display their area at all. INDEPENDENT of the header style -
// two build-sheet settings, not one, which is what the owner asked for. With
// this off, HDR_TAG has nothing to put in its left pill and the tags simply do
// not appear; the row spacing does not change, so nothing on the page moves.
bool s_showArea = true;

// Colour-code the header band or tag by AREA, so a group of cards reads as a
// group before you read a word of it. Independent of s_showArea: the owner
// wants the colour available whether or not the name is displayed.
bool s_areaColor = true;

// Testing: walk every card on the page through each state in turn.
//
// 0 is "derive normally"; 1..5 pin all cards to one state so the five visual
// treatments can be judged side by side, on every card type, in every header
// mode, in seconds. Without this the only way to see a long-stale card is to
// wait an hour for one.
int s_forced = 0;
const CardState FORCED[] = {
    CardState::ST_LIVE,        // index 0 is unused - 0 means "release"
    CardState::ST_STALE,
    CardState::ST_LONG_STALE,
    CardState::ST_REFUSED,
    CardState::ST_PARTIAL,
    CardState::ST_PAUSED,
};
const char *FORCED_LABEL[] = { "Live", "Stale", "Long", "Failed", "Partial", "Paused" };

// Measured across the page build, in LVGL's own pool. ESP.getFreeHeap() is the
// wrong instrument here and 2.3 established why: LV_USE_STDLIB_MALLOC is
// LV_STDLIB_BUILTIN with LV_MEM_ADR 0, so every widget comes out of a 128 KB
// static array in internal DRAM and the system heap barely moves.
uint32_t s_lvBefore = 0;
lv_obj_t *s_btnState = nullptr;

// Set by GUIManager. The card page cannot reach SystemCore, and the full
// report needs it.
std::function<void()> s_onDump = nullptr;

lv_obj_t *topButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height          (b, UI::minTouch());   // 9 mm, derived per board
    lv_obj_set_width           (b, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color  (b, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_radius    (b, UI::sc(UI::met().RADIUS / 2), 0);
    lv_obj_add_event_cb        (b, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text          (l, text);
    lv_obj_set_style_text_font (l, UI::type().TAG, 0);
    lv_obj_set_style_text_color(l, UI::c(UI::pal().TEXT), 0);
    lv_obj_center              (l);
    return b;
}

void backCb  (lv_event_t *e) { (void)e; CardDemo::close(); }

void schemeCb(lv_event_t *e) {
    (void)e;
    s_scheme = (s_scheme + 1) % 4;
    switch (s_scheme) {
        case 0: UI::setScheme(UI_PAL_FLEET,    UI_MET_DARK);  break;
        case 1: UI::setScheme(UI_PAL_SLATE,    UI_MET_DARK);  break;
        case 2: UI::setScheme(UI_PAL_MIDNIGHT, UI_MET_DARK);  break;
        default:UI::setScheme(UI_PAL_PAPER,    UI_MET_LIGHT); break;
    }
    // The page is rebuilt rather than restyled ONLY because the top bar and
    // the page background are built here by hand and have no restyle hook of
    // their own. The cards themselves would survive a bare restyleAll(), and
    // that is the property worth having - it is what "never cache a colour"
    // buys. Rebuilding also re-runs the grid, which a metrics change needs.
    CardDemo::show(*s_reg, *s_binder);
}

void actorCb(lv_event_t *e) {
    (void)e;
    s_actorStyle = (s_actorStyle == StateCardFill::FILL_SURFACE)
                 ? StateCardFill::LIGHT_ICON : StateCardFill::FILL_SURFACE;
    CardDemo::show(*s_reg, *s_binder);
}

void colorCb(lv_event_t *e) {
    (void)e;
    s_areaColor = !s_areaColor;
    CardDemo::show(*s_reg, *s_binder);
}

void areaCb(lv_event_t *e) {
    (void)e;
    s_showArea = !s_showArea;
    CardDemo::show(*s_reg, *s_binder);
}

void stateCb(lv_event_t *e) {
    (void)e;
    s_forced = (s_forced + 1) % 6;
    // Applied straight to the live cards rather than rebuilding the page: this
    // is the one control where a rebuild would hide what is being tested,
    // because a rebuilt card derives its own state again immediately.
    if (s_binder) s_binder->debugForceAll(FORCED[s_forced], s_forced != 0);
    if (s_btnState) {
        lv_obj_t *l = lv_obj_get_child(s_btnState, 0);
        if (l) lv_label_set_text(l, FORCED_LABEL[s_forced]);
    }
}

void headerCb(lv_event_t *e) {
    (void)e;
    // A header bar is created in Card::build(), not styled in restyle(), so
    // changing the treatment means rebuilding. That is correct: this is a
    // structural choice a card makes once, not a live style.
    s_hdr = (s_hdr == CardHeaderStyle::HDR_TAG) ? CardHeaderStyle::HDR_BAR
          : (s_hdr == CardHeaderStyle::HDR_BAR) ? CardHeaderStyle::HDR_NONE
                                                     : CardHeaderStyle::HDR_TAG;
    CardDemo::show(*s_reg, *s_binder);
}

const Entity *ent(const char *id) { return s_reg ? s_reg->find(id) : nullptr; }

} // namespace

namespace CardDemo {

void setDumpHandler(std::function<void()> cb) { s_onDump = cb; }

void close() {
    if (s_previous) lv_screen_load(s_previous);
    if (s_page)   { delete s_page;          s_page = nullptr; }
    if (s_screen) { lv_obj_delete(s_screen); s_screen = nullptr; }

    // Put the grid back. show() derives it from this page's host, which is the
    // screen minus a top bar - correct there, and quietly wrong for everyone
    // else the moment we leave. UI::grid() is global state and this page
    // borrows it.
    if (s_previous) {
        UI::setViewport(lv_obj_get_width(s_previous), lv_obj_get_height(s_previous));
    }
}

void show(EntityRegistry &reg, CardBinder &binder) {
    s_reg    = &reg;
    s_binder = &binder;

    // Registered once, on first open. The System Doctor is where spans and
    // priorities become checkable on a real board - priority especially, since
    // nothing reads it until 2.5 and an unread field that is also invisible is
    // one nobody notices is wrong.
    static bool s_sectionAdded = false;
    if (!s_sectionAdded) {
        SystemReport::addSection("CARDS", []() {
            if (s_page) s_page->report();
            else        SystemReport::line("  (card page not open)");
        });
        s_sectionAdded = true;
    }

    if (!s_previous) s_previous = lv_screen_active();   // only on first entry

    // THE OLD PAGE COMES DOWN FIRST.
    //
    // It used to be built-then-swapped, which meant eighteen cards alive at
    // once: double the widgets, double the lv_mem, and double the intermediate
    // layer buffers LVGL wants for them. That is what exhausted the P4's draw
    // buffers mid-rebuild - "Allocating layer buffer failed", forever - and
    // what blew the 3248's loopTask stack, which surfaced as a stack canary
    // panic with a backtrace full of recursive LVGL frames.
    //
    // The cost is a frame of the dashboard with no cards on it. The
    // alternative is a board that reboots when you press a button.
    // Load the previous screen BEFORE deleting this one. Deleting the active
    // screen and then replacing it works, but LVGL rightly complains about it
    // every time ("lv_obj_delete: the active screen was deleted"), and a log
    // full of warnings that are fine is how a real one gets missed.
    if (s_previous) lv_screen_load(s_previous);
    if (s_page)   { delete s_page;           s_page   = nullptr; }
    if (s_screen) { lv_obj_delete(s_screen); s_screen = nullptr; }

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    s_lvBefore = mon.total_size - mon.free_size;

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI::c(UI::pal().GROUND), 0);
    lv_obj_clear_flag        (s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *col = lv_obj_create(s_screen);
    lv_obj_set_size               (col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow          (col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa       (col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (col, 0, 0);
    lv_obj_set_style_pad_all      (col, 0, 0);
    lv_obj_set_style_pad_gap      (col, 0, 0);
    lv_obj_clear_flag             (col, LV_OBJ_FLAG_SCROLLABLE);

    // --- Top bar ----------------------------------------------------------
    lv_obj_t *bar = lv_obj_create(col);
    lv_obj_set_width              (bar, lv_pct(100));
    lv_obj_set_height             (bar, LV_SIZE_CONTENT);
    // ONE ROW THAT SCROLLS SIDEWAYS, rather than wrapping onto two.
    //
    // Wrapping made every button reachable on the 3248's 320 px screen, but it
    // cost a second row of chrome - and on a 2x3 grid that is a third of the
    // page, which is why only four cards survived there. Scrolling keeps the
    // buttons full size and gives the row back to the grid. This is demo
    // chrome; a real page has none of it.
    lv_obj_set_flex_flow          (bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir         (bar, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode     (bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa       (bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (bar, 0, 0);
    lv_obj_set_style_pad_all      (bar, UI::sc(6), 0);
    lv_obj_set_style_pad_gap      (bar, UI::sc(6), 0);

    topButton(bar, LV_SYMBOL_LEFT " Back", backCb);
    topButton(bar, UI::pal().name, schemeCb);
    topButton(bar, s_hdr == CardHeaderStyle::HDR_TAG ? "Tag"
                 : s_hdr == CardHeaderStyle::HDR_BAR ? "Bar" : "No hdr", headerCb);
    topButton(bar, s_showArea ? "Area on" : "Area off", areaCb);
    topButton(bar, s_areaColor ? "Colour" : "Mono", colorCb);
    topButton(bar, s_actorStyle == StateCardFill::FILL_SURFACE ? "Fill" : "Icon", actorCb);
    s_btnState = topButton(bar, FORCED_LABEL[s_forced], stateCb);
    // The System panel's Dump Config is unreachable while this screen is up,
    // and the CARDS section of that report is where spans and variants live.
    topButton(bar, "Dump", [](lv_event_t *e) {
        (void)e;
        // Through the registered handler, which runs the whole report with
        // Serial echo ON. Calling s_page->report() straight only reached the
        // System panel's sink - SystemReport::line() mirrors to Serial only
        // during a run() that asked for it, so the button looked dead.
        if (s_onDump) s_onDump();
        else if (s_page) s_page->report();
    });

    // --- The grid ---------------------------------------------------------
    lv_obj_t *host = lv_obj_create(col);
    lv_obj_set_width              (host, lv_pct(100));
    lv_obj_set_flex_grow          (host, 1);
    lv_obj_set_style_bg_opa       (host, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (host, 0, 0);
    lv_obj_set_style_pad_all      (host, 0, 0);
    lv_obj_clear_flag             (host, LV_OBJ_FLAG_SCROLLABLE);

    // The grid derives from the live viewport, which on this screen is what is
    // left under the top bar - not the whole panel. Deriving it from
    // bsp_display.WIDTH/HEIGHT would be wrong on every rotated board AND wrong
    // here, which is the same mistake GUIManager::begin() already documents.
    lv_obj_update_layout(col);
    UI::setViewport(lv_obj_get_content_width(host), lv_obj_get_content_height(host));

    s_page = new CardPage();
    // In tag mode the page has to carve the clearance the tags hang into.
    s_page->begin(host, &binder);


    // --- Two small builders -----------------------------------------------
    //
    // Every card here is created by DOMAIN, through cardForEntity(), and never
    // by naming a layout. That is the whole point of the catalog: this demo is
    // the closest thing to a build sheet that exists yet, so if it had to pick
    // between a value layout and a state layout, the real build sheet would
    // have to as well - and that is not a user's decision to make.
    //
    // Note every label: "Deck", never "Temperature". cards.md section 4 - the
    // tinted icon says what the quantity is, so the name says WHERE.
    StateCard::setFill(s_actorStyle);
    Card::setShowAreaDefault(s_showArea);

    auto place = [&](const char *id, const char *label, const char *area,
                     const Entity *sec, const CardPlacement *pl, bool paused) {
        const Entity *e = ent(id);
        if (!e) return;
        Card *c = cardForEntity(e);   // EntityKind picks the class
        if (!c) return;               // a kind with no card type yet
        c->setLabel(label).setHeaderStyle(s_hdr);
        if (area) {
            c->setArea(area);
            // Derived from the NAME, so two cards in one area always agree and
            // the colour survives a reboot. See cardAreaColor().
            if (s_areaColor) c->setAreaColor(cardAreaColor(area));
        }
        if (sec)    c->bindSecondary(sec);
        if (pl)     c->setPlacement(*pl);
        if (paused) c->setPaused(true);
        s_page->add(c);
    };

    // The aggregate: ONE card, two entities, one tap for both. Same class as a
    // single switch - only the binding differs, which is what cards.md section
    // 4's "groupable by room" asks for and why it is not a group card.
    auto placeGroup = [&](const char *idA, const char *idB, const char *label,
                          const char *area) {
        const Entity *a = ent(idA);
        if (!a) return;
        Card *c = cardForEntity(a);
        if (!c) return;
        c->bindPrimary(ent(idB));
        c->setLabel(label).setHeaderStyle(s_hdr);
        if (area) {
            c->setArea(area);
            if (s_areaColor) c->setAreaColor(cardAreaColor(area));
        }
        s_page->add(c);
    };

    CardPlacement wide;  wide.prefSpanX = 2;  wide.minSpanX = 1;  wide.priority = 200;

    // sensor -> value layout, chosen by the catalog and not by this file
    place("deck_temp", "Deck", "Outdoor", ent("deck_battery"), nullptr, false);
    place("deck_lux",  "Deck", "Outdoor", nullptr,             nullptr, false);

    // binary_sensor -> state layout, and NOTHING happens when it is tapped.
    // Before the catalog existed this was an ActorCard whose tap tried to
    // command a read-only entity and was saved only by a writable check.
    place("deck_motion", "Deck", "Outdoor", nullptr, nullptr, false);

    // switch -> state layout, and a tap actually commands. Both halves of the
    // optimistic write: one entity answers, one is deliberately ignored.
    place(VIRT_ENT_SWITCH, "Obeys",   "Office", nullptr, nullptr, false);
    place(VIRT_ENT_STUCK,  "Ignores", "Office", nullptr, nullptr, false);
    placeGroup(VIRT_ENT_SWITCH, VIRT_ENT_STUCK, "Both", "Office");

    // Four more tappable cards, so a board with seven columns has enough to
    // show what more than two rows looks like. They are lights rather than
    // switches to exercise the other domain type in CardCatalog.
    place(VIRT_ENT_L1, "Lamp 1", "Kitchen", nullptr, nullptr, false);
    place(VIRT_ENT_L2, "Lamp 2", "Kitchen", nullptr, nullptr, false);
    place(VIRT_ENT_L3, "Lamp 3", "Lounge",  nullptr, nullptr, false);
    place(VIRT_ENT_L4, "Lamp 4", "Lounge",  nullptr, nullptr, false);

    place(SYS_ENT_RSSI,   "RSSI",      "Panel", nullptr, nullptr, false);
    place(SYS_ENT_HEAP,   "Free Heap", "Panel", nullptr, &wide,   false);

    // Paused, permanently, so the distinction cards.md section 3 insists on is
    // visible rather than described: this card is dim BECAUSE THE USER CHOSE
    // IT, and no stale card anywhere on this page dims.
    place(SYS_ENT_UPTIME, "Paused", "Panel", nullptr, nullptr, true);

    // A rebuild creates fresh cards, which derive their own state - so a
    // pinned state has to be re-applied or the button would silently lie.
    if (s_forced) binder.debugForceAll(FORCED[s_forced], true);

    lv_mem_monitor(&mon);
    const uint32_t used  = mon.total_size - mon.free_size;
    const uint8_t  cards = s_page->count();
    const int32_t  delta = (int32_t)used - (int32_t)s_lvBefore;
    Serial.printf("[Cards] %u cards, lv_mem %u -> %u (%+ld, %ld B/card), frag %u%%\n",
                  (unsigned)cards, (unsigned)s_lvBefore, (unsigned)used,
                  (long)delta, cards ? (long)(delta / cards) : 0L,
                  (unsigned)mon.frag_pct);

    lv_screen_load(s_screen);
}

} // namespace CardDemo
