#include "UI/HaPicker.h"
#include "UI/Kit.h"
#include "UI/UITokens.h"
#include "Cards/CardIcons.h"   // cardStateWord(): the words a card uses for on and off
#include "HaValue.h"           // haDescriptor(): what kind of entity a row is
#include "HaCatalog.h"
#include <esp_heap_caps.h>
#include <string.h>
#include <stdio.h>

using KS = Kit::KitStyle;

namespace {

constexpr uint8_t ROWS_MAX = 48, CHOSEN_MAX = 32;

HaPicker::Spec   s_spec;
lv_obj_t        *s_root = nullptr, *s_body = nullptr, *s_nav = nullptr;
char             s_area[32] = "";
HaOffer::Row    *s_rows = nullptr;     // the area showing
uint8_t          s_n = 0;
lv_obj_t        *s_rowObj[ROWS_MAX] = {};
lv_obj_t        *s_tick[ROWS_MAX] = {};
HaOffer::Row    *s_chosen = nullptr;   // every area's
uint8_t          s_nChosen = 0;
HaOffer::AreaRow *s_areas = nullptr;
uint8_t          s_nAreas = 0;
uint8_t          s_chipArea[HaCatalog::AREAS_MAX];
char             s_chipWords[HaCatalog::AREAS_MAX][48];

template <typename T> T *psram(size_t n) {
    T *p = (T *)heap_caps_calloc(n, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : (T *)calloc(n, sizeof(T));
}

int32_t gap() { return UI::mm(UI::shape().S5); }

int chosenAt(const char *ref) {
    for (uint8_t i = 0; i < s_nChosen; i++) if (!strcmp(s_chosen[i].ref, ref)) return i;
    return -1;
}

// What a row's state says, as a card would say it: "Closed", "Detected",
// "74.5°F", "Unavailable".
void stateWords(const HaOffer::Row &r, char *out, size_t cap) {
    if (!strcmp(r.state, "unavailable")) { snprintf(out, cap, "Unavailable"); return; }
    if (!strcmp(r.state, "unknown") || !r.state[0]) { snprintf(out, cap, "Unknown"); return; }
    EntityDescriptor d;
    if (!haDescriptor(r.ref, r.dc, r.unit, d)) { snprintf(out, cap, "%s", r.state); return; }
    if (d.valueType == ValueType::BOOL) {
        const bool on = !strcmp(r.state, "on") || !strcmp(r.state, "open");
        snprintf(out, cap, "%s", cardStateWord(d, on));
    } else if (d.valueType == ValueType::FLOAT) {
        snprintf(out, cap, "%s%s", r.state, r.unit);
    } else {
        snprintf(out, cap, "%s", r.state);
        if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');   // "heat" -> "Heat"
    }
}

void paint(uint8_t i) {
    lv_obj_t *r = s_rowObj[i];
    if (!r) return;
    const bool on = chosenAt(s_rows[i].ref) >= 0;
    Kit::finish(r, lv_obj_get_width(r), lv_obj_get_height(r), lv_obj_get_style_radius(r, LV_PART_MAIN),
                on ? Kit::Finish::FINISH_LIT : Kit::Finish::FINISH_SATIN);
    if (s_tick[i]) lv_label_set_text(s_tick[i], on ? LV_SYMBOL_OK : "");
}

void rowCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (i >= s_n) return;
    const int k = chosenAt(s_rows[i].ref);
    if (k >= 0) {
        memmove(s_chosen + k, s_chosen + k + 1, sizeof(HaOffer::Row) * (s_nChosen - k - 1));
        s_nChosen--;
    } else if (s_nChosen < CHOSEN_MAX) {
        s_chosen[s_nChosen++] = s_rows[i];
    }
    paint(i);
    if (s_spec.changed) s_spec.changed(s_nChosen, s_spec.ctx);
}

void fillBody();
void fillNav();

void showArea(uint8_t a) {
    if (a >= s_nAreas) return;
    snprintf(s_area, sizeof(s_area), "%s", s_areas[a].id);
    lv_async_call([](void *) {
        if (!s_body) return;
        fillBody();
        if (s_nav) fillNav();
    }, nullptr);
}

void chipCb(lv_event_t *ev) {
    const uint8_t c = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (c < HaCatalog::AREAS_MAX) showArea(s_chipArea[c]);
}

void navCb(lv_event_t *ev) { showArea((uint8_t)(uintptr_t)lv_event_get_user_data(ev)); }

const char *areaWords(const HaOffer::AreaRow &a) { return a.id[0] ? a.name : "No area"; }   // K145

void fillBody() {
    lv_obj_clean(s_body);
    for (uint8_t i = 0; i < ROWS_MAX; i++) { s_rowObj[i] = nullptr; s_tick[i] = nullptr; }
    int cur = -1;
    for (uint8_t a = 0; a < s_nAreas; a++) if (!strcmp(s_areas[a].id, s_area)) cur = a;

    Kit::drawerTitle(s_body, cur >= 0 ? areaWords(s_areas[cur]) : "");
    s_n = HaOffer::rows(s_area, s_rows, ROWS_MAX);
    const lv_font_t *tagF = UI::type().TAG;
    const int32_t tickW = Kit::textW(LV_SYMBOL_OK, tagF) + UI::sc(4);
    // TWO COLUMNS where the window is wide enough for two names side by side
    // (the mock's), one on a narrow one: the card window is a fixed size, and
    // its rows are what there is room for.
    const int32_t bodyW = lv_obj_get_content_width(s_body);
    const bool two = bodyW >= UI::mm(80);
    const int32_t rowW = two ? (bodyW - gap()) / 2 : bodyW;
    for (uint8_t i = 0; i < s_n; i++) {
        const HaOffer::Row &row = s_rows[i];
        const char *held = s_spec.held ? s_spec.held(row, s_spec.ctx) : nullptr;
        lv_obj_t *r = Kit::item(s_body);
        lv_obj_set_size(r, rowW, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(r, gap(), 0);
        lv_obj_set_style_pad_ver(r, UI::mm(UI::shape().S4), 0);
        lv_obj_set_style_pad_column(r, gap(), 0);
        lv_obj_t *n = Kit::label(r, held ? KS::KS_LABEL_TAG_DIM : KS::KS_LABEL_NAME);
        lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(n, 1);
        lv_obj_set_height(n, lv_font_get_line_height(lv_obj_get_style_text_font(n, LV_PART_MAIN)));   // one line, "..."
        lv_label_set_text(n, row.name);
        if (row.isNew) lv_label_set_text(Kit::label(r, KS::KS_LABEL_TAG_ACTIVE), "New");   // K145
        char st[40];
        stateWords(row, st, sizeof(st));
        lv_label_set_text(Kit::label(r, KS::KS_LABEL_TAG), st);
        if (held) {
            lv_label_set_text(Kit::label(r, KS::KS_LABEL_TAG_DIM), held);
            lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_t *t = Kit::label(r, KS::KS_LABEL_TAG_ACTIVE);
            lv_obj_set_width(t, tickW);
            if (i < ROWS_MAX) { s_rowObj[i] = r; s_tick[i] = t; }
            lv_obj_add_event_cb(r, rowCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        }
    }

    // THE OTHER AREAS, as chips (screen 1): a tap shows that area's rows here.
    if (!s_spec.areaColumn) {
        const char *words[HaCatalog::AREAS_MAX];
        uint8_t nw = 0;
        for (uint8_t a = 0; a < s_nAreas; a++) {
            if (a == cur) continue;
            snprintf(s_chipWords[nw], sizeof(s_chipWords[0]), "%s (%u)", areaWords(s_areas[a]), (unsigned)s_areas[a].items);
            s_chipArea[nw] = a;
            words[nw] = s_chipWords[nw];
            nw++;
        }
        if (nw) {
            Kit::drawerTitle(s_body, "Other areas");   // K145
            Kit::chipGrid(s_body, lv_obj_get_content_width(s_body), words, nw, nw, chipCb);
        }
    }

    lv_obj_update_layout(s_body);
    for (uint8_t i = 0; i < s_n && i < ROWS_MAX; i++) paint(i);
    lv_obj_scroll_to_y(s_body, 0, LV_ANIM_OFF);
    HaOffer::seen(s_area);   // shown: no longer new (after the rows said so)
}

// SCREEN 2's COLUMN: every area, HA's floors as its sections, No area last.
void fillNav() {
    lv_obj_clean(s_nav);
    const char *floor = nullptr;
    lv_obj_t *items[HaCatalog::AREAS_MAX] = {};
    for (uint8_t a = 0; a < s_nAreas; a++) {
        const HaOffer::AreaRow &ar = s_areas[a];
        const char *fl = ar.id[0] ? ar.floorName : "";
        if (!floor || strcmp(fl, floor)) {
            Kit::drawerTitle(s_nav, fl);
            floor = fl;
        }
        lv_obj_t *r = Kit::item(s_nav);
        lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(r, gap(), 0);
        lv_obj_t *n = Kit::label(r, KS::KS_LABEL_NAME);
        lv_label_set_text(n, areaWords(ar));
        char c[8];
        snprintf(c, sizeof(c), "%u", (unsigned)ar.items);
        lv_label_set_text(Kit::label(r, ar.fresh ? KS::KS_LABEL_TAG_ACTIVE : KS::KS_LABEL_TAG_DIM), c);
        lv_obj_add_event_cb(r, navCb, LV_EVENT_CLICKED, (void *)(uintptr_t)a);
        items[a] = r;
    }
    lv_obj_update_layout(s_nav);
    for (uint8_t a = 0; a < s_nAreas; a++) {
        if (!items[a]) continue;
        const bool on = !strcmp(s_areas[a].id, s_area);
        Kit::finish(items[a], lv_obj_get_width(items[a]), lv_obj_get_height(items[a]),
                    lv_obj_get_style_radius(items[a], LV_PART_MAIN), on ? Kit::Finish::FINISH_LIT : Kit::Finish::FINISH_SATIN);
    }
}

lv_obj_t *column(lv_obj_t *parent, bool wrap = false) {
    lv_obj_t *c = Kit::plain(parent);
    // The rows' column wraps, two rows to a line where they fit; a section
    // title and the chips are the whole width, a line of their own.
    lv_obj_set_flex_flow(c, wrap ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
    if (wrap) lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(c, gap(), 0);
    lv_obj_set_style_pad_column(c, gap(), 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(c);
    return c;
}

}  // namespace

namespace HaPicker {

lv_obj_t *build(lv_obj_t *parent, int32_t w, int32_t h, const Spec &spec) {
    if (!s_rows)   s_rows   = psram<HaOffer::Row>(ROWS_MAX);
    if (!s_chosen) s_chosen = psram<HaOffer::Row>(CHOSEN_MAX);
    if (!s_areas)  s_areas  = psram<HaOffer::AreaRow>(HaCatalog::AREAS_MAX);
    if (!s_rows || !s_chosen || !s_areas) return nullptr;
    s_spec = spec;
    s_nAreas = HaOffer::areas(s_areas, HaCatalog::AREAS_MAX);
    s_area[0] = '\0';
    bool found = false;
    for (uint8_t a = 0; a < s_nAreas && spec.area; a++)
        if (!strcmp(s_areas[a].id, spec.area)) { found = true; break; }
    if (found) snprintf(s_area, sizeof(s_area), "%s", spec.area);
    // A floor (Outside): its first area with entities.
    for (uint8_t a = 0; a < s_nAreas && !found && spec.area && spec.area[0]; a++)
        if (HaOffer::inPlace(s_areas[a].id, spec.area)) { found = true; snprintf(s_area, sizeof(s_area), "%s", s_areas[a].id); }
    if (!found && s_nAreas) snprintf(s_area, sizeof(s_area), "%s", s_areas[0].id);

    s_root = Kit::plain(parent);
    lv_obj_set_size(s_root, w, h);
    s_nav = nullptr;
    if (spec.areaColumn) {
        lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(s_root, gap() * 2, 0);
        s_nav = column(s_root);
        lv_obj_set_size(s_nav, w * 27 / 100, lv_pct(100));
        s_body = column(s_root, true);
        lv_obj_set_height(s_body, lv_pct(100));
        lv_obj_set_flex_grow(s_body, 1);
        fillNav();
    } else {
        s_body = column(s_root, true);
        lv_obj_set_size(s_body, lv_pct(100), lv_pct(100));
    }
    lv_obj_update_layout(s_root);   // the rows are sized from the body's width
    if (spec.preselect) {
        s_n = HaOffer::rows(s_area, s_rows, ROWS_MAX);
        for (uint8_t i = 0; i < s_n && s_nChosen < CHOSEN_MAX; i++)
            if (spec.preselect(s_rows[i]) && chosenAt(s_rows[i].ref) < 0) s_chosen[s_nChosen++] = s_rows[i];
    }
    lv_obj_add_event_cb(s_root, [](lv_event_t *) { s_root = s_body = s_nav = nullptr; }, LV_EVENT_DELETE, nullptr);
    fillBody();
    return s_root;
}

uint8_t chosen(HaOffer::Row *out, uint8_t cap) {
    uint8_t n = 0;
    for (; n < s_nChosen && n < cap; n++) out[n] = s_chosen[n];
    return n;
}

uint8_t chosenCount() { return s_nChosen; }

void forget() { s_nChosen = 0; s_area[0] = '\0'; }

bool tapRow(uint8_t i) {
    if (!s_body || i >= s_n || !s_rowObj[i]) return false;
    lv_obj_send_event(s_rowObj[i], LV_EVENT_CLICKED, nullptr);
    return true;
}

bool tapArea(const char *areaId) {
    for (uint8_t a = 0; a < s_nAreas && s_body; a++)
        if (!strcmp(s_areas[a].id, areaId ? areaId : "")) { showArea(a); return true; }
    return false;
}

}  // namespace HaPicker
