#include "Cards/CardDemo.h"
#include "Cards/CardPage.h"
#include "Cards/MeasureCard.h"
#include "Cards/ActorCard.h"
#include "UI/UITokens.h"
#include "SystemReport.h"
#include "SystemEntities.h"
#include "ExternalEntities.h"
#include "VirtualEntities.h"
#include <Arduino.h>

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
CardHeaderStyle s_hdr = CardHeaderStyle::HDR_EXTERNAL;

int s_scheme = 0;

// Which actor treatment the page is wearing. Same idea as s_hdr: a knob rather
// than a decision, because the question is "which of these do you prefer" and
// that is answered by looking at both.
ActorStateStyle s_actorStyle = ActorStateStyle::FILL_SURFACE;

// Measured across the page build, in LVGL's own pool. ESP.getFreeHeap() is the
// wrong instrument here and 2.3 established why: LV_USE_STDLIB_MALLOC is
// LV_STDLIB_BUILTIN with LV_MEM_ADR 0, so every widget comes out of a 128 KB
// static array in internal DRAM and the system heap barely moves.
uint32_t s_lvBefore = 0;

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
    s_actorStyle = (s_actorStyle == ActorStateStyle::FILL_SURFACE)
                 ? ActorStateStyle::LIGHT_ICON : ActorStateStyle::FILL_SURFACE;
    CardDemo::show(*s_reg, *s_binder);
}

void headerCb(lv_event_t *e) {
    (void)e;
    // A header bar is created in Card::build(), not styled in restyle(), so
    // changing the treatment means rebuilding. That is correct: this is a
    // structural choice a card makes once, not a live style.
    s_hdr = (s_hdr == CardHeaderStyle::HDR_EXTERNAL) ? CardHeaderStyle::HDR_INTERNAL
          : (s_hdr == CardHeaderStyle::HDR_INTERNAL) ? CardHeaderStyle::HDR_NONE
                                                     : CardHeaderStyle::HDR_EXTERNAL;
    CardDemo::show(*s_reg, *s_binder);
}

const Entity *ent(const char *id) { return s_reg ? s_reg->find(id) : nullptr; }

} // namespace

namespace CardDemo {

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

    lv_obj_t *old     = s_screen;
    CardPage *oldPage = s_page;
    if (!s_previous) s_previous = lv_screen_active();   // only on first entry

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
    lv_obj_set_flex_flow          (bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa       (bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (bar, 0, 0);
    lv_obj_set_style_pad_all      (bar, UI::sc(6), 0);
    lv_obj_set_style_pad_gap      (bar, UI::sc(6), 0);
    lv_obj_clear_flag             (bar, LV_OBJ_FLAG_SCROLLABLE);

    topButton(bar, LV_SYMBOL_LEFT " Back", backCb);
    topButton(bar, UI::pal().name, schemeCb);
    topButton(bar, s_hdr == CardHeaderStyle::HDR_EXTERNAL ? "Tag"
                 : s_hdr == CardHeaderStyle::HDR_INTERNAL ? "Bar" : "No hdr", headerCb);
    topButton(bar, s_actorStyle == ActorStateStyle::FILL_SURFACE ? "Fill" : "Icon", actorCb);

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
    s_page->begin(host, &binder,
                  s_hdr == CardHeaderStyle::HDR_EXTERNAL ? Card::headerHeight() : 0);

    CardPlacement wide;  wide.prefSpanX = 2;  wide.minSpanX = 1;  wide.priority = 200;

    // --- Two small builders -----------------------------------------------
    //
    // Card's setters chain and return Card&, while CardPage::add() takes
    // ownership of a Card* - so the two do not compose into one expression.
    // A local that configures then hands over is clearer than making the
    // setters return a pointer just to enable a one-liner.
    //
    // Note every label below: "Deck", never "Temperature". cards.md section 4
    // - the icon says what the quantity is, so the name says WHERE.
    auto measure = [&](const char *id, const char *label, const char *area,
                       const Entity *sec, const CardPlacement *pl, bool paused) {
        const Entity *e = ent(id);
        if (!e) return;
        MeasureCard *c = new MeasureCard();
        c->bindPrimary(e).setLabel(label).setHeaderStyle(s_hdr);
        if (area) c->setArea(area);
        if (sec)  c->bindSecondary(sec);
        if (pl)   c->setPlacement(*pl);
        if (paused) c->setPaused(true);
        s_page->add(c);
    };

    auto actor = [&](const char *idA, const char *idB, const char *label) {
        const Entity *a = ent(idA);
        if (!a) return;
        ActorCard *c = new ActorCard();
        c->bindPrimary(a);
        if (idB) c->bindPrimary(ent(idB));
        c->setLabel(label).setHeaderStyle(s_hdr);
        c->setStateStyle(s_actorStyle);
        s_page->add(c);
    };

    measure("deck_temp", "Deck",  "Outdoor", ent("deck_battery"), nullptr, false);
    measure("deck_lux",  "Deck",  "Outdoor", nullptr,             nullptr, false);

    // Motion is an ACTOR, not a measure, even though it is a sensor. cards.md
    // section 4: a binary sensor placed as its own card behaves as a state
    // card - prominent icon, whole card shifts colour - exactly like a
    // non-dimmable light. What it measures is not a number.
    actor("deck_motion", nullptr, "Deck");

    // The two halves of the optimistic write. Tap both.
    actor(VIRT_ENT_SWITCH, nullptr, "Obeys");
    actor(VIRT_ENT_STUCK,  nullptr, "Ignores");

    // The aggregate: one card, two entities, one tap. Tapping this after
    // tapping only one of the two above is what shows the mixed indicator.
    actor(VIRT_ENT_SWITCH, VIRT_ENT_STUCK, "Both");

    measure(SYS_ENT_RSSI,   "Signal",    nullptr, nullptr, nullptr, false);
    measure(SYS_ENT_HEAP,   "Free Heap", nullptr, nullptr, &wide,   false);

    // Paused, permanently, so the distinction cards.md section 3 insists on is
    // visible rather than described: this card is dim BECAUSE THE USER CHOSE
    // IT, and no stale card anywhere on this page dims.
    measure(SYS_ENT_UPTIME, "Paused",    nullptr, nullptr, nullptr, true);

    // MEASURED HERE, before the old page is freed, and that ordering is the
    // whole point. Taken afterwards it reports (new page - old page), which on
    // a rebuild is roughly zero and underflows an unsigned subtraction into
    // the 4294966956-style nonsense the first flash produced. What a page
    // costs is what it ADDS while it is the only new thing in the pool.
    lv_mem_monitor(&mon);
    const uint32_t used  = mon.total_size - mon.free_size;
    const uint8_t  cards = s_page->count();
    const int32_t  delta = (int32_t)used - (int32_t)s_lvBefore;
    Serial.printf("[Cards] %u cards, lv_mem %u -> %u (%+ld, %ld B/card), frag %u%%\n",
                  (unsigned)cards, (unsigned)s_lvBefore, (unsigned)used,
                  (long)delta, cards ? (long)(delta / cards) : 0L,
                  (unsigned)mon.frag_pct);

    lv_screen_load(s_screen);
    if (oldPage) delete oldPage;      // unregisters its cards from the binder
    if (old)     lv_obj_delete(old);
}

} // namespace CardDemo
