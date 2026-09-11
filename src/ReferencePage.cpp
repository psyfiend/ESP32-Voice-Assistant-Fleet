#include "ReferencePage.h"
#include "UITokens.h"
#include "UIToolkit.h"
#include <Arduino.h>

namespace {

lv_obj_t *s_screen   = nullptr;
lv_obj_t *s_previous = nullptr;

// Measured while the sample grid is built. Card cost comes from LVGL's pool,
// which is the budget that actually constrains us - see the header.
uint32_t s_lvUsedBefore = 0;
uint32_t s_perCard      = 0;
const int SAMPLE_CARDS  = 8;

lv_obj_t *section(lv_obj_t *parent, const char *title) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font (lbl, UI::type().TAG, 0);
    lv_obj_set_style_text_color(lbl, UI::c(UI::pal().TEXT_DIM), 0);
    lv_obj_set_style_pad_top   (lbl, UI::sc(10), 0);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width              (row, lv_pct(100));
    lv_obj_set_height             (row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow          (row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_bg_opa       (row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (row, 0, 0);
    lv_obj_set_style_pad_all      (row, 0, 0);
    lv_obj_set_style_pad_gap      (row, UI::sc(6), 0);
    lv_obj_clear_flag             (row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

// One labelled colour chip. The label sits *under* the chip rather than on it,
// so a token can be judged without text contrast getting in the way.
void chip(lv_obj_t *parent, const char *name, uint32_t hex) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size               (box, UI::sc(62), UI::sc(52));
    lv_obj_set_style_bg_opa       (box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (box, 0, 0);
    lv_obj_set_style_pad_all      (box, 0, 0);
    lv_obj_clear_flag             (box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sw = lv_obj_create(box);
    lv_obj_set_size               (sw, lv_pct(100), UI::sc(30));
    lv_obj_align                  (sw, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color     (sw, UI::c(hex), 0);
    lv_obj_set_style_radius       (sw, UI::sc(UI::met().RADIUS / 2), 0);
    lv_obj_set_style_border_width (sw, 1, 0);
    lv_obj_set_style_border_color (sw, UI::border(), 0);
    lv_obj_clear_flag             (sw, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text             (lbl, name);
    lv_obj_set_style_text_font    (lbl, UI::type().TAG, 0);
    lv_obj_set_style_text_color   (lbl, UI::c(UI::pal().TEXT_DIM), 0);
    lv_obj_align                  (lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// A card drawn entirely from current metrics — the thing every real card will
// look like. Used both as a sample and as the unit of measurement.
lv_obj_t *sampleCard(lv_obj_t *parent, const char *name, const char *value, uint32_t tint) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size               (card, UI::grid().cellW, UI::grid().cellH);
    lv_obj_set_style_bg_color     (card, UI::c(UI::pal().SURFACE), 0);
    lv_obj_set_style_radius       (card, UI::sc(UI::met().RADIUS), 0);
    lv_obj_set_style_pad_all      (card, UI::sc(UI::met().PAD), 0);
    lv_obj_set_style_border_width (card, UI::met().BORDER_W, 0);
    lv_obj_set_style_border_color (card, UI::border(), 0);
    lv_obj_set_style_shadow_width (card, UI::sc(UI::met().SHADOW), 0);
    lv_obj_set_style_shadow_opa   (card, LV_OPA_40, 0);
    lv_obj_clear_flag             (card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *n = lv_label_create(card);
    lv_label_set_text             (n, name);
    lv_obj_set_style_text_font    (n, UI::type().NAME, 0);
    lv_obj_set_style_text_color   (n, UI::c(tint), 0);
    lv_obj_align                  (n, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *v = lv_label_create(card);
    lv_label_set_text             (v, value);
    lv_obj_set_style_text_font    (v, UI::type().VALUE, 0);
    lv_obj_set_style_text_color   (v, UI::c(UI::pal().TEXT), 0);
    lv_obj_center                 (v);
    return card;
}

void schemeCb(lv_event_t *e) {
    const int which = (int)(intptr_t)lv_event_get_user_data(e);
    switch (which) {
        case 0: UI::setScheme(UI_PAL_FLEET, UI_MET_DARK);  break;
        case 1: UI::setScheme(UI_PAL_SLATE, UI_MET_DARK);  break;
        default: UI::setScheme(UI_PAL_PAPER, UI_MET_LIGHT); break;
    }
    ReferencePage::show();   // rebuild against the new tokens
}

void backCb(lv_event_t *e) { (void)e; ReferencePage::close(); }

lv_obj_t *button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *ud) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height             (b, UI::minTouch());   // 9 mm, derived
    lv_obj_set_style_bg_color     (b, UI::c(UI::pal().ACCENT), 0);
    lv_obj_set_style_radius       (b, UI::sc(UI::met().RADIUS / 2), 0);
    lv_obj_add_event_cb           (b, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text             (l, text);
    lv_obj_set_style_text_font    (l, UI::type().TAG, 0);
    lv_obj_set_style_text_color   (l, UI::c(UI::pal().GROUND), 0);
    lv_obj_center                 (l);
    return b;
}

} // namespace

namespace ReferencePage {

void close() {
    if (s_previous) lv_screen_load(s_previous);
    if (s_screen) { lv_obj_delete(s_screen); s_screen = nullptr; }
}

void show() {
    lv_obj_t *old = s_screen;
    if (!s_previous) s_previous = lv_screen_active();   // only on first entry

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI::c(UI::pal().GROUND), 0);

    lv_obj_t *col = lv_obj_create(s_screen);
    lv_obj_set_size               (col, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow          (col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa       (col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (col, 0, 0);
    lv_obj_set_style_pad_all      (col, UI::sc(UI::grid().INSET), 0);
    lv_obj_set_style_pad_gap      (col, UI::sc(4), 0);

    // --- Top bar: identity, schemes, back -----------------------------------
    lv_obj_t *bar = lv_obj_create(col);
    lv_obj_set_width              (bar, lv_pct(100));
    lv_obj_set_height             (bar, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow          (bar, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_bg_opa       (bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (bar, 0, 0);
    lv_obj_set_style_pad_all      (bar, 0, 0);
    lv_obj_set_style_pad_gap      (bar, UI::sc(6), 0);
    lv_obj_clear_flag             (bar, LV_OBJ_FLAG_SCROLLABLE);

    button(bar, "Back",  backCb,   nullptr);
    button(bar, "Fleet", schemeCb, (void *)(intptr_t)0);
    button(bar, "Slate", schemeCb, (void *)(intptr_t)1);
    button(bar, "Paper", schemeCb, (void *)(intptr_t)2);

    // --- The numbers --------------------------------------------------------
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);

    lv_obj_t *stats = lv_label_create(col);
    lv_obj_set_style_text_font (stats, UI::type().TAG, 0);
    lv_obj_set_style_text_color(stats, UI::c(UI::pal().TEXT_DIM), 0);
    lv_label_set_text_fmt(stats,
        "%s  ·  %u PPI  ·  scale %d.%02dx  ·  grid %ux%u of %ux%u  ·  touch %upx\n"
        "lv_mem %u/%u KB used (%u%%)  ·  per card ~%u B  ·  internal heap %u KB",
        UI::pal().name,
        (unsigned)bspPixelDensity(),
        (int)bspUiScale(), (int)((bspUiScale() - (int)bspUiScale()) * 100),
        (unsigned)UI::grid().cols, (unsigned)UI::grid().rows,
        (unsigned)UI::grid().cellW, (unsigned)UI::grid().cellH,
        (unsigned)UI::minTouch(),
        (unsigned)((mon.total_size - mon.free_size) / 1024), (unsigned)(mon.total_size / 1024),
        (unsigned)mon.used_pct,
        (unsigned)s_perCard,
        (unsigned)(ESP.getFreeHeap() / 1024));

    // --- Surfaces -----------------------------------------------------------
    lv_obj_t *r = section(col, "SURFACES");
    chip(r, "ground",  UI::pal().GROUND);
    chip(r, "surface", UI::pal().SURFACE);
    chip(r, "alt",     UI::pal().SURFACE_ALT);
    chip(r, "text",    UI::pal().TEXT);
    chip(r, "dim",     UI::pal().TEXT_DIM);
    chip(r, "accent",  UI::pal().ACCENT);

    // --- State: the palette that carries meaning ----------------------------
    r = section(col, "STATE  ·  meaning, not decoration");
    chip(r, "active", UI::pal().ST_ACTIVE);
    chip(r, "idle",   UI::pal().ST_IDLE);
    chip(r, "ok",     UI::pal().ST_OK);
    chip(r, "warn",   UI::pal().ST_WARN);
    chip(r, "bad",    UI::pal().ST_BAD);

    // --- Sensor tints -------------------------------------------------------
    r = section(col, "SENSOR TINTS");
    chip(r, "temp",  UI::pal().TINT_TEMP);
    chip(r, "humid", UI::pal().TINT_HUMID);
    chip(r, "light", UI::pal().TINT_LIGHT);
    chip(r, "air",   UI::pal().TINT_AIR);
    chip(r, "power", UI::pal().TINT_POWER);

    // --- Type ---------------------------------------------------------------
    r = section(col, "TYPE");
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
    struct { const lv_font_t *f; const char *s; } faces[] = {
        { UI::type().VALUE, "23.4  value" },
        { UI::type().NAME,  "Deck  ·  name" },
        { UI::type().TAG,   "OUTDOOR  ·  tag / status row" },
    };
    for (auto &f : faces) {
        lv_obj_t *l = lv_label_create(r);
        lv_label_set_text          (l, f.s);
        lv_obj_set_style_text_font (l, f.f, 0);
        lv_obj_set_style_text_color(l, UI::c(UI::pal().TEXT), 0);
    }

    // --- Cards, and the measurement -----------------------------------------
    r = section(col, "CARDS  ·  live grid metrics");

    lv_mem_monitor(&mon);
    s_lvUsedBefore = mon.total_size - mon.free_size;

    const char *names[]  = { "Deck", "Kitchen", "Bath", "Bedroom", "Garage", "Porch", "Office", "Hall" };
    const uint32_t tints[] = { UI::pal().TINT_TEMP, UI::pal().TINT_HUMID, UI::pal().TINT_LIGHT,
                               UI::pal().TINT_AIR,  UI::pal().TINT_POWER, UI::pal().TINT_TEMP,
                               UI::pal().TINT_HUMID, UI::pal().TINT_LIGHT };
    for (int i = 0; i < SAMPLE_CARDS; i++) sampleCard(r, names[i], "21.4", tints[i]);

    lv_mem_monitor(&mon);
    const uint32_t after = mon.total_size - mon.free_size;
    if (after > s_lvUsedBefore) s_perCard = (after - s_lvUsedBefore) / SAMPLE_CARDS;

    Serial.printf("[Ref] %s | %u cards cost %u B of lv_mem (%u B each) | pool %u%% used\n",
                  UI::pal().name, (unsigned)SAMPLE_CARDS,
                  (unsigned)(after - s_lvUsedBefore), (unsigned)s_perCard,
                  (unsigned)mon.used_pct);

    lv_screen_load(s_screen);
    if (old) lv_obj_delete(old);   // after the load, never before
}

} // namespace ReferencePage
