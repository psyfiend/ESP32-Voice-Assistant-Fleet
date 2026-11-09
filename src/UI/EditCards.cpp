// Edit cards (DECISIONS K148, K149) - see include/UI/EditCards.h.
#include "UI/EditCards.h"
#include "UI/HaPicker.h"      // stateWords(): an HA row's state as a card says it
#include "HaValue.h"          // haDescriptor(): a row's kind - a sensor can go in the header (K168)
#include "Cards/CardAdded.h"  // the made cards' limit (K176)
#include "UI/Kit.h"
#include "UI/UITokens.h"
#include "UI/UIToolkit.h"     // show_toast()
#include "UI/UIIcons.h"       // MDI_PIN (K160)
#include "HaCatalog.h"        // AREAS_MAX
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

using KS   = Kit::KitStyle;
using Item = EditCards::Item;
using Tick = EditCards::Tick;
using From = EditCards::From;
using Act  = EditCards::Act;

namespace {

constexpr uint8_t ROWS_MAX = 64, CHOSEN_MAX = 32;

EditCards::Spec   s_spec;
lv_obj_t         *s_root = nullptr, *s_nav = nullptr, *s_body = nullptr, *s_foot = nullptr;
int8_t            s_list = 0;              // the list showing; -1: the HA area s_area
char              s_area[32] = "";
Item             *s_rows = nullptr;        // the rows showing
uint8_t           s_n = 0;
lv_obj_t         *s_rowObj[ROWS_MAX] = {};
lv_obj_t         *s_tick[ROWS_MAX] = {};
Item             *s_chosen = nullptr;      // every list's
HaOffer::Row     *s_chosenRow = nullptr;   // ...an HA one's row, at the same index
uint8_t           s_nChosen = 0;
Item             *s_count = nullptr;       // a list read for its count
HaOffer::AreaRow *s_areas = nullptr;
uint8_t           s_nAreas = 0;
HaOffer::Row     *s_haRows = nullptr;      // the HA area showing, as HaOffer gives it
bool              s_move = false;

template <typename T> T *psram(size_t n) {
    T *p = (T *)heap_caps_calloc(n, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : (T *)calloc(n, sizeof(T));
}

int32_t gap() { return UI::mm(UI::shape().S5); }

// A NAME FITTED TO ITS ROOM (K152): the name's face, else the tag's; cut only
// when neither fits (the 4B's narrow column).
void fitName(lv_obj_t *l, const char *text, int32_t room) {
    const lv_font_t *ladder[] = { UI::type().NAME, UI::type().TAG };
    Kit::fitLabel(l, text, room, ladder, 2);
    // Every row as tall as one in the name's face, so a smaller name keeps the list even.
    if (lv_obj_t *row = lv_obj_get_parent(l))
        lv_obj_set_style_min_height(row, lv_font_get_line_height(UI::type().NAME) +
                                         lv_obj_get_style_pad_top(row, LV_PART_MAIN) +
                                         lv_obj_get_style_pad_bottom(row, LV_PART_MAIN), 0);
}

// What a row's words beside its name take: each tag and a gap before it.
int32_t besideW(const char *a, const char *b, int32_t tickW) {
    const lv_font_t *t = UI::type().TAG;
    int32_t w = tickW ? tickW + gap() : 0;
    if (a && a[0]) w += Kit::textW(a, t) + gap();
    if (b && b[0]) w += Kit::textW(b, t) + gap();
    return w;
}

int chosenAt(const char *key) {
    for (uint8_t i = 0; i < s_nChosen; i++) if (!strcmp(s_chosen[i].key, key)) return i;
    return -1;
}

Tick kindNow() { return s_nChosen ? s_chosen[0].tick : Tick::TICK_NONE; }

// --- A group's tap cycle (K161) ---------------------------------------------------
uint8_t stepOf(const Item &it) {
    const int k = chosenAt(it.key);
    return k >= 0 ? s_chosen[k].step : 0;
}

uint8_t addsChosen() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nChosen; i++) n += s_chosen[i].tick == Tick::TICK_ADD;
    return n;
}

// THE SQUARE A ROW ENDS IN: what will be on the page (K160) - a member's header
// glyph or pin, its first tap flipping it; a card being added pinned on its second.
const char *signOf(const Item &it, uint8_t step, uint32_t &tint) {
    // The pin is the accent color (K178); a header glyph keeps its own tint.
    tint = UI::pal().ACCENT;
    if (it.from == From::FROM_GROUP && !it.coming) {
        if (step == 1) return it.pinned ? nullptr : MDI_PIN;
        if (!it.signIcon || strcmp(it.signIcon, MDI_PIN)) tint = it.signTint;
        return it.signIcon;
    }
    if (step == 3) { tint = it.signTint; return it.signIcon; }   // into the header (K168)
    return step == 2 ? MDI_PIN : nullptr;
}

const char *verb(Tick t) {
    return t == Tick::TICK_ADD ? "add" : t == Tick::TICK_HIDE ? "hide" : "restore";
}

const char *areaWords(const HaOffer::AreaRow &a) { return a.id[0] ? a.name : "No area"; }   // K145

bool targetShowing() { return s_list >= 0 && s_spec.lists[s_list].target; }

// A card being added, as its target list shows it: where it comes from.
void comingMark(const Item &c, char *out, size_t cap) {
    if (c.from == From::FROM_PAGE)  { snprintf(out, cap, "From the page"); return; }
    const char *a = c.areaId[0] ? HaOffer::areaName(c.areaId) : "";
    if (a && a[0]) snprintf(out, cap, "From %s", a);          // the mock's "From Office"
    else           snprintf(out, cap, "From Home Assistant");   // to confirm: an entity with no area
}

// The row of `ref` as a caller's list has it (its card's), into `it`; unchanged
// when no list holds it. Lists are read into s_count, the counts' buffer.
void asListed(Item &it, const char *ref) {
    if (!ref || !ref[0] || !s_count) return;
    for (uint8_t k = 0; k < s_spec.nLists; k++) {
        const EditCards::List &L = s_spec.lists[k];
        const uint8_t n = L.fill ? L.fill(s_count, ROWS_MAX, s_spec.ctx) : 0;
        for (uint8_t i = 0; i < n; i++) {
            if (strcmp(s_count[i].ref, ref)) continue;
            const char *area = it.areaId;
            char keepArea[sizeof(it.areaId)];
            snprintf(keepArea, sizeof(keepArea), "%s", area);
            it = s_count[i];
            snprintf(it.areaId, sizeof(it.areaId), "%s", keepArea);
            it.coming = false;
            it.list = (int8_t)k;   // ticked as from its own list
            return;
        }
    }
}

// AT THE LIMIT (K176): nothing more can be made, so no row can be chosen to add;
// a card already chosen stays so (it was counted when chosen).
void atLimit() {
    if (CardAdded::count() + addsChosen() < CardAdded::MAX) return;
    for (uint8_t i = 0; i < s_n; i++)
        if (s_rows[i].tick == Tick::TICK_ADD && !s_rows[i].coming && chosenAt(s_rows[i].key) < 0)
            s_rows[i].tick = Tick::TICK_NONE;
}

// THE ROWS SHOWING: a caller's list - in its target, the cards being added
// before its first row to restore (the hidden ones last) - or an HA area's.
void readRows() {
    s_n = 0;
    if (s_list >= 0) {
        const EditCards::List &L = s_spec.lists[s_list];
        uint8_t n = L.fill ? L.fill(s_rows, ROWS_MAX, s_spec.ctx) : 0;
        for (uint8_t i = 0; i < n; i++) { s_rows[i].coming = false; s_rows[i].list = s_list; }
        // A group's (cycle): its additions last, among its other changes.
        if (L.target && (s_spec.cycle ? addsChosen() > 0 : kindNow() == Tick::TICK_ADD)) {
            uint8_t at = n;
            if (!s_spec.cycle)
                for (uint8_t i = 0; i < n; i++) if (s_rows[i].tick == Tick::TICK_RESTORE) { at = i; break; }
            const uint8_t add = (uint8_t)LV_MIN(addsChosen(), ROWS_MAX - n);
            memmove(s_rows + at + add, s_rows + at, sizeof(Item) * (n - at));
            uint8_t j = 0;
            for (uint8_t k = 0; k < s_nChosen && j < add; k++) {
                if (s_chosen[k].tick != Tick::TICK_ADD) continue;
                Item &c = s_rows[at + j++];
                c = s_chosen[k];
                c.coming = true;
                comingMark(c, c.mark, sizeof(c.mark));
            }
            n = (uint8_t)(n + add);
        }
        s_n = n;
        atLimit();
        return;
    }
    const uint8_t n = HaOffer::rows(s_area, s_haRows, ROWS_MAX);
    for (uint8_t i = 0; i < n; i++) {
        const HaOffer::Row &r = s_haRows[i];
        Item &it = s_rows[i];
        it = Item{};
        snprintf(it.key, sizeof(it.key), "%s", r.ref);
        snprintf(it.name, sizeof(it.name), "%s", r.name);
        snprintf(it.areaId, sizeof(it.areaId), "%s", r.areaId);
        HaPicker::stateWords(r, it.state, sizeof(it.state));
        it.from  = From::FROM_HA;
        it.fresh = r.isNew;
        it.list  = -1;
        const char *held = s_spec.held ? s_spec.held(r, s_spec.ctx) : nullptr;
        if (held) snprintf(it.mark, sizeof(it.mark), "%s", held);
        it.tick = held ? Tick::TICK_NONE : Tick::TICK_ADD;
        EntityDescriptor d;
        it.sensor = haDescriptor(r.ref, r.dc, r.unit, d) &&
                    (d.kind == EntityKind::SENSOR || d.kind == EntityKind::BINARY_SENSOR);
        if (s_spec.cycle && it.sensor && s_spec.haGlyph) it.signIcon = s_spec.haGlyph(r, it.signTint);
        // A CARD ALREADY HERE, IN A GROUP'S LIST (1.5, K161): its row as its own list
        // has it - the square, the tap cycle - so the area's rows say the same thing.
        if (held && s_spec.cycle) asListed(it, r.ref);
    }
    s_n = n;
    atLimit();
}

// A ROW'S LOOK: ticked to add or restore, a lit chip's; ticked to hide or remove,
// red; one being added, dashed; one that can be ticked, satin; information, or
// a row of another kind while one kind is ticked, flat - the latter dimmed. The
// tick says it too, never colour alone.
void paint(uint8_t i) {
    lv_obj_t *r = s_rowObj[i];
    if (!r || i >= s_n) return;
    const Item &it = s_rows[i];
    Kit::itemMark(r, Kit::ItemMark::MARK_NONE);
    lv_obj_set_style_opa(r, LV_OPA_COVER, 0);
    const int32_t rw = lv_obj_get_width(r), rh = lv_obj_get_height(r), rr = lv_obj_get_style_radius(r, LV_PART_MAIN);
    if (s_spec.cycle) {   // K161: changed, lit; to be removed, red; else satin
        const uint8_t st = stepOf(it);
        if (s_tick[i]) lv_label_set_text(s_tick[i], st ? LV_SYMBOL_OK : "");
        if (it.coming)                  { Kit::itemMark(r, Kit::ItemMark::MARK_COMING); return; }
        if (it.tick == Tick::TICK_NONE) { Kit::unfinish(r); return; }
        if (st == 2 && it.from == From::FROM_GROUP) { Kit::itemMark(r, Kit::ItemMark::MARK_DOOMED); return; }
        if (st)                         { Kit::itemMark(r, Kit::ItemMark::MARK_CHOSEN); return; }
        Kit::finish(r, rw, rh, rr, Kit::Finish::FINISH_SATIN);
        return;
    }
    const bool on = it.coming || (it.tick != Tick::TICK_NONE && chosenAt(it.key) >= 0);
    if (s_tick[i]) lv_label_set_text(s_tick[i], on ? LV_SYMBOL_OK : "");
    const int32_t w = lv_obj_get_width(r), h = lv_obj_get_height(r), rad = lv_obj_get_style_radius(r, LV_PART_MAIN);
    if (it.coming)                     { Kit::itemMark(r, Kit::ItemMark::MARK_COMING); return; }
    if (it.tick == Tick::TICK_NONE)    { Kit::unfinish(r); return; }
    if (on && it.tick == Tick::TICK_HIDE) { Kit::itemMark(r, Kit::ItemMark::MARK_DOOMED); return; }
    if (on)                            { Kit::itemMark(r, Kit::ItemMark::MARK_CHOSEN); return; }
    const Tick now = kindNow();
    if (now != Tick::TICK_NONE && now != it.tick) {   // another kind is ticked: flat, dimmed
        Kit::unfinish(r);
        lv_obj_set_style_opa(r, LV_OPA_60, 0);
        return;
    }
    Kit::finish(r, w, h, rad, Kit::Finish::FINISH_SATIN);
}

void paintAll() { for (uint8_t i = 0; i < s_n && i < ROWS_MAX; i++) paint(i); }

void fillNav();
void fillFoot();
void fillBody(bool keepScroll = false);

// Everything that follows the ticks: the rows (made again where the target list
// shows the cards being added, in place elsewhere), the column's counts, the foot.
void ticksChanged(bool rebuild) {
    lv_async_call([](void *p) {
        if (!s_body) return;
        if (p) fillBody(true);
        else   paintAll();
        fillNav();
        fillFoot();
    }, rebuild ? (void *)1 : nullptr);
}

// THE TAP CYCLE (K161): a member flips its pin, then (one made here) is to be
// removed, then is back as it was; a card not in the group is added, then added and
// pinned, then not. The rows made again: their squares say the result.
void cycleTap(uint8_t i) {
    const Item &it = s_rows[i];
    const int k = chosenAt(it.key);
    if (k < 0) {
        if (it.tick == Tick::TICK_NONE || s_nChosen >= CHOSEN_MAX) return;
        Item &c = s_chosen[s_nChosen];
        c = it;
        c.list = s_list;
        c.step = (s_spec.pinFirst && it.from != From::FROM_GROUP) ? 2 : 1;   // K190: New group pins
        c.coming = false;
        if (it.from == From::FROM_HA && s_list < 0) s_chosenRow[s_nChosen] = s_haRows[i];
        s_nChosen++;
    } else {
        Item &c = s_chosen[k];
        // A sensor being added: added, pinned, in the header, off (K168).
        const uint8_t last = (c.from == From::FROM_GROUP) ? (c.canRemove ? 2 : 1) : (c.sensor && c.signIcon ? 3 : 2);
        if (s_spec.pinFirst && c.from != From::FROM_GROUP) {
            // K190: pinned, in the header (a sensor), unpinned, off.
            if (c.step == 2)      c.step = (c.sensor && c.signIcon) ? 3 : 1;
            else if (c.step == 3) c.step = 1;
            else {
                memmove(s_chosen + k, s_chosen + k + 1, sizeof(Item) * (s_nChosen - k - 1));
                memmove(s_chosenRow + k, s_chosenRow + k + 1, sizeof(HaOffer::Row) * (s_nChosen - k - 1));
                s_nChosen--;
            }
        } else if (c.step < last) {
            c.step++;
        } else {
            memmove(s_chosen + k, s_chosen + k + 1, sizeof(Item) * (s_nChosen - k - 1));
            memmove(s_chosenRow + k, s_chosenRow + k + 1, sizeof(HaOffer::Row) * (s_nChosen - k - 1));
            s_nChosen--;
        }
    }
    if (!EditCards::countOf(From::FROM_PAGE)) s_move = false;
    ticksChanged(true);
}

void rowCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (i >= s_n) return;
    if (s_spec.cycle) { cycleTap(i); return; }
    const Item &it = s_rows[i];
    const Tick before = kindNow();
    const int k = chosenAt(it.key);
    if (it.coming || k >= 0) {   // unticked - a card being added, from its target list too
        if (k < 0) return;
        memmove(s_chosen + k, s_chosen + k + 1, sizeof(Item) * (s_nChosen - k - 1));
        memmove(s_chosenRow + k, s_chosenRow + k + 1, sizeof(HaOffer::Row) * (s_nChosen - k - 1));
        s_nChosen--;
    } else {
        if (it.tick == Tick::TICK_NONE) return;
        // ONE KIND OF TICK AT A TIME (K148): the other kinds say why they are flat.
        if (before != Tick::TICK_NONE && before != it.tick) {
            char msg[96];
            snprintf(msg, sizeof(msg), "Cards cannot be selected to %s while cards are selected to %s.",
                     verb(it.tick), verb(before));
            UIToolkit::show_toast(msg, 3000);
            return;
        }
        if (s_nChosen >= CHOSEN_MAX) return;
        s_chosen[s_nChosen] = it;
        s_chosen[s_nChosen].list = s_list;
        if (it.from == From::FROM_HA && s_list < 0) s_chosenRow[s_nChosen] = s_haRows[i];
        s_nChosen++;
    }
    if (kindNow() == Tick::TICK_NONE) s_move = false;
    ticksChanged(targetShowing() || before != kindNow());
}

void showList(int8_t k) {
    s_list = k;
    lv_async_call([](void *) { if (s_body) { fillBody(); fillNav(); } }, nullptr);
}

void showArea(uint8_t a) {
    if (a >= s_nAreas) return;
    snprintf(s_area, sizeof(s_area), "%s", s_areas[a].id);
    showList(-1);
}

// The column's items: a caller's list (0x100 | k), or an area by its index.
void navCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    if (v & 0x100) showList((int8_t)(v & 0xFF));
    else           showArea((uint8_t)v);
}

void fillBody(bool keepScroll) {
    const int32_t y = keepScroll ? lv_obj_get_scroll_y(s_body) : 0;
    lv_obj_clean(s_body);
    for (uint8_t i = 0; i < ROWS_MAX; i++) { s_rowObj[i] = nullptr; s_tick[i] = nullptr; }
    readRows();
    const lv_font_t *tagF = UI::type().TAG;
    const int32_t tickW = Kit::textW(LV_SYMBOL_OK, tagF) + UI::sc(4);
    // TWO COLUMNS where the window is wide enough for two names side by side
    // (the mock's), one on a narrow one (the 4B, mock 9).
    const int32_t bodyW = lv_obj_get_content_width(s_body);
    const bool two = bodyW >= UI::mm(UI::shape().W_TWO_COL);
    const int32_t cellW = two ? (bodyW - gap()) / 2 : bodyW;
    // A GROUP'S ROWS END IN A SQUARE (K160, K161): shorter, the square beside them
    // as Members has it - a header glyph, a pin, or an empty place.
    int32_t side = 0, sq = 0;
    if (s_spec.cycle) {
        lv_obj_t *probe = Kit::plain(s_body);
        Kit::add(probe, KS::KS_CLUSTER_GLYPH_SQUARE);
        sq = lv_obj_get_style_width(probe, LV_PART_MAIN);
        lv_obj_delete(probe);
        side = sq + gap();
    }
    const int32_t rowW = cellW - side;
    for (uint8_t i = 0; i < s_n; i++) {
        const Item &it = s_rows[i];
        const bool tappable = it.coming || it.tick != Tick::TICK_NONE;
        lv_obj_t *holder = s_body;
        if (s_spec.cycle) {
            holder = Kit::plain(s_body);
            lv_obj_set_size(holder, cellW, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(holder, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(holder, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(holder, gap(), 0);
        }
        lv_obj_t *r = Kit::item(holder);
        lv_obj_set_size(r, rowW, LV_SIZE_CONTENT);
        if (s_spec.cycle) {
            uint32_t tint = 0;
            const char *icon = signOf(it, stepOf(it), tint);
            lv_obj_t *g = Kit::plain(holder);
            Kit::add(g, KS::KS_CLUSTER_GLYPH_SQUARE);
            lv_obj_t *ic = Kit::label(g, KS::KS_CARD_ICON_SM);
            lv_label_set_text(ic, icon ? icon : "");
            if (tint) lv_obj_set_style_text_color(ic, UI::c(tint), 0);
            lv_obj_center(ic);
            if (!icon) lv_obj_add_flag(g, LV_OBJ_FLAG_HIDDEN);
            if (!icon) {   // its place kept, so the rows stay one width
                lv_obj_t *gap_ = Kit::plain(holder);
                lv_obj_set_size(gap_, sq, 1);
            }
        }
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(r, gap(), 0);
        lv_obj_set_style_pad_ver(r, UI::mm(UI::shape().S4), 0);
        lv_obj_set_style_pad_column(r, gap(), 0);
        lv_obj_t *n = Kit::label(r, tappable ? KS::KS_LABEL_NAME : KS::KS_LABEL_NAME_DIM);
        lv_obj_set_flex_grow(n, 1);
        const bool fresh = it.fresh && !it.coming;
        // NO CHECK MARK (K168): the lit chip says it's chosen, and
        // nothing inside a row moves when it is - no room kept for one either.
        (void)tickW;
        fitName(n, it.name, rowW - 2 * gap() - besideW(it.state, it.mark, 0) -
                            (fresh ? Kit::textW("New", tagF) + gap() : 0));
        if (fresh) lv_label_set_text(Kit::label(r, KS::KS_LABEL_TAG_ACTIVE), "New");   // K145
        if (it.state[0]) lv_label_set_text(Kit::label(r, KS::KS_LABEL_TAG), it.state);
        if (it.mark[0]) lv_label_set_text(Kit::label(r, it.coming ? KS::KS_LABEL_TAG_ACCENT : KS::KS_LABEL_TAG_DIM), it.mark);
        if (tappable) {
            if (i < ROWS_MAX) { s_rowObj[i] = r; s_tick[i] = nullptr; }
            lv_obj_add_event_cb(r, rowCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        } else {
            lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
            if (i < ROWS_MAX) s_rowObj[i] = r;
        }
    }
    lv_obj_update_layout(s_body);
    paintAll();
    lv_obj_scroll_to_y(s_body, y, LV_ANIM_OFF);
    if (s_list < 0) HaOffer::seen(s_area);   // shown: no longer new (after the rows said so)
}

lv_obj_t *navItem(const char *words, const char *count, bool countLit, uintptr_t user) {
    lv_obj_t *r = Kit::item(s_nav);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(r, gap(), 0);
    lv_obj_t *n = Kit::label(r, KS::KS_LABEL_NAME);
    lv_obj_set_flex_grow(n, 1);
    fitName(n, words, lv_obj_get_content_width(s_nav) - 2 * gap() - besideW(count, nullptr, 0));
    if (count && count[0]) lv_label_set_text(Kit::label(r, countLit ? KS::KS_LABEL_TAG_ACCENT : KS::KS_LABEL_TAG_DIM), count);
    lv_obj_add_event_cb(r, navCb, LV_EVENT_CLICKED, (void *)user);
    return r;
}

uint8_t chosenIn(int8_t list, const char *areaId) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nChosen; i++)
        if (s_chosen[i].list == list && (list >= 0 || !strcmp(s_chosen[i].areaId, areaId))) n++;
    return n;
}

// THE COLUMN: the caller's lists - the target counted, "8 +2" while cards are
// being added to it; another, how many are ticked there - then every HA area,
// HA's floors as its sections, No area last; an area with ticks says "1 / 11".
void fillNav() {
    lv_obj_clean(s_nav);
    lv_obj_t *items[EditCards::LISTS_MAX + HaCatalog::AREAS_MAX] = {};
    uint8_t ni = 0;
    char c[20];
    for (uint8_t k = 0; k < s_spec.nLists; k++) {
        const EditCards::List &L = s_spec.lists[k];
        const uint8_t ticked = chosenIn((int8_t)k, nullptr);
        bool lit = false;
        c[0] = '\0';
        if (L.target) {
            const uint8_t n = L.fill ? L.fill(s_count, ROWS_MAX, s_spec.ctx) : 0;
            const uint8_t adds = s_spec.cycle ? addsChosen() : (kindNow() == Tick::TICK_ADD ? s_nChosen : 0);
            if (adds) { snprintf(c, sizeof(c), "%u +%u", (unsigned)n, (unsigned)adds); lit = true; }
            else        snprintf(c, sizeof(c), "%u", (unsigned)n);
        } else if (ticked) {
            snprintf(c, sizeof(c), "%u", (unsigned)ticked);
            lit = true;
        }
        items[ni++] = navItem(L.words, c, lit, 0x100 | k);
    }
    const char *floor = nullptr;
    for (uint8_t a = 0; a < s_nAreas; a++) {
        const HaOffer::AreaRow &ar = s_areas[a];
        const char *fl = ar.id[0] ? ar.floorName : "";
        if (!floor || strcmp(fl, floor)) {
            Kit::drawerTitle(s_nav, fl);
            floor = fl;
        }
        const uint8_t ticked = chosenIn(-1, ar.id);
        if (ticked) snprintf(c, sizeof(c), "%u / %u", (unsigned)ticked, (unsigned)ar.items);
        else        snprintf(c, sizeof(c), "%u", (unsigned)ar.items);
        items[ni++] = navItem(areaWords(ar), c, ticked || ar.fresh, a);
    }
    lv_obj_update_layout(s_nav);
    for (uint8_t i = 0; i < ni; i++) {
        const bool on = i < s_spec.nLists ? s_list == (int8_t)i
                                          : (s_list < 0 && !strcmp(s_areas[i - s_spec.nLists].id, s_area));
        Kit::finish(items[i], lv_obj_get_width(items[i]), lv_obj_get_height(items[i]),
                    lv_obj_get_style_radius(items[i], LV_PART_MAIN), on ? Kit::Finish::FINISH_GLASS : Kit::Finish::FINISH_SATIN);   // the list showing is glass
    }
}

void act(Act a) {
    if (!s_spec.act) return;
    lv_async_call([](void *p) { if (s_body && s_spec.act) s_spec.act((Act)(uintptr_t)p, s_spec.ctx); },
                  (void *)(uintptr_t)a);
}
void addCb(lv_event_t *)     { act(Act::ACT_ADD); }
void hideCb(lv_event_t *)    { act(Act::ACT_HIDE); }
void removeCb(lv_event_t *)  { act(Act::ACT_REMOVE); }
void restoreCb(lv_event_t *) { act(Act::ACT_RESTORE); }
void saveCb(lv_event_t *)    { act(Act::ACT_SAVE); }

void moveCb(lv_event_t *ev) {
    s_move = (uintptr_t)lv_event_get_user_data(ev) == 1;
    lv_async_call([](void *) { if (s_foot) fillFoot(); }, nullptr);
}

// THE FOOT: what can be done with what is ticked (K148, Q5 - the logic decides,
// nothing fixed): Copy / Move and Add N; Remove N (when every one ticked can go
// for good) and Hide N; Restore N.
void fillFoot() {
    lv_obj_clean(s_foot);
    const Tick t = kindNow();
    if (t == Tick::TICK_NONE) return;
    const int32_t bw = Kit::textW("Restore 00", UI::type().NAME) + 2 * UI::mm(UI::shape().S7);
    const int32_t bh = Kit::controlH();
    char w[24];
    if (s_spec.cycle) {   // K161: every change at once
        if (s_spec.copyMove && EditCards::countOf(From::FROM_PAGE)) {
            static const char *const CM[] = { "Copy", "Move" };
            Kit::chipRow(s_foot, CM, 2, s_move ? 1 : 0, 0, moveCb);
        }
        // "Save N"; New group's "Create group N" (K168).
        snprintf(w, sizeof(w), "%s %u", s_spec.saveWord ? s_spec.saveWord : "Save", (unsigned)s_nChosen);
        const int32_t sw = LV_MAX(bw, Kit::textW(w, UI::type().NAME) + 2 * UI::mm(UI::shape().S7));
        Kit::button(s_foot, Kit::ButtonRole::BTN_CONFIRM, w, sw, bh, saveCb);
        return;
    }
    if (t == Tick::TICK_ADD) {
        if (s_spec.copyMove && EditCards::countOf(From::FROM_PAGE)) {
            static const char *const CM[] = { "Copy", "Move" };
            Kit::chipRow(s_foot, CM, 2, s_move ? 1 : 0, 0, moveCb);
        }
        snprintf(w, sizeof(w), "Add %u", (unsigned)s_nChosen);   // the mock's "Add 2"
        Kit::button(s_foot, Kit::ButtonRole::BTN_CONFIRM, w, bw, bh, addCb);
    } else if (t == Tick::TICK_HIDE) {
        bool removable = true;
        for (uint8_t i = 0; i < s_nChosen; i++) removable &= s_chosen[i].canRemove;
        if (removable) {
            snprintf(w, sizeof(w), "Remove %u", (unsigned)s_nChosen);
            Kit::button(s_foot, Kit::ButtonRole::BTN_CANCEL, w, bw, bh, removeCb);
        }
        snprintf(w, sizeof(w), "Hide %u", (unsigned)s_nChosen);
        Kit::button(s_foot, Kit::ButtonRole::BTN_DESTRUCTIVE, w, bw, bh, hideCb);
    } else {
        snprintf(w, sizeof(w), "Restore %u", (unsigned)s_nChosen);
        Kit::button(s_foot, Kit::ButtonRole::BTN_CONFIRM, w, bw, bh, restoreCb);
    }
}

lv_obj_t *column(lv_obj_t *parent, bool wrap = false) {
    lv_obj_t *c = Kit::plain(parent);
    // The rows' column wraps, two rows to a line where they fit.
    lv_obj_set_flex_flow(c, wrap ? LV_FLEX_FLOW_ROW_WRAP : LV_FLEX_FLOW_COLUMN);
    if (wrap) lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(c, gap(), 0);
    lv_obj_set_style_pad_column(c, gap(), 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(c);
    return c;
}

}  // namespace

namespace EditCards {

lv_obj_t *build(lv_obj_t *parent, int32_t w, int32_t h, const Spec &spec) {
    if (!s_rows)      s_rows      = psram<Item>(ROWS_MAX);
    if (!s_count)     s_count     = psram<Item>(ROWS_MAX);
    if (!s_chosen)    s_chosen    = psram<Item>(CHOSEN_MAX);
    if (!s_chosenRow) s_chosenRow = psram<HaOffer::Row>(CHOSEN_MAX);
    if (!s_haRows)    s_haRows    = psram<HaOffer::Row>(ROWS_MAX);
    if (!s_areas)     s_areas     = psram<HaOffer::AreaRow>(HaCatalog::AREAS_MAX);
    if (!s_rows || !s_count || !s_chosen || !s_chosenRow || !s_haRows || !s_areas) return nullptr;
    s_spec = spec;
    s_nAreas = HaOffer::ready() ? HaOffer::areas(s_areas, HaCatalog::AREAS_MAX) : 0;
    // NEAR THE LIMIT (K176): every time Edit cards opens; it stays
    // until tapped.
    if (!spec.keepTicks && CardAdded::nearLimit()) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Approaching limit of %u cards: %u currently",
                 (unsigned)CardAdded::MAX, (unsigned)CardAdded::count());
        UIToolkit::show_sticky(msg);
    }
    if (!spec.keepTicks) {
        s_nChosen = 0;
        s_move = false;
    }
    // The area shown first: the caller's, or the first on its floor (Outside),
    // else the first list - or, with none, the first area. Back from a step, as left.
    bool kept = false;
    if (spec.keepTicks) {
        kept = s_list >= 0 && s_list < (int8_t)spec.nLists;
        for (uint8_t a = 0; a < s_nAreas && !kept && s_list < 0; a++) kept = !strcmp(s_areas[a].id, s_area);
    }
    if (!kept) {
        bool found = false;
        s_area[0] = '\0';
        for (uint8_t a = 0; a < s_nAreas && spec.area && !found; a++)
            if (!strcmp(s_areas[a].id, spec.area)) { found = true; snprintf(s_area, sizeof(s_area), "%s", spec.area); }
        for (uint8_t a = 0; a < s_nAreas && !found && spec.area && spec.area[0]; a++)
            if (HaOffer::inPlace(s_areas[a].id, spec.area)) { found = true; snprintf(s_area, sizeof(s_area), "%s", s_areas[a].id); }
        if (!found && s_nAreas) snprintf(s_area, sizeof(s_area), "%s", s_areas[0].id);
        s_list = (found || !spec.nLists) ? -1 : 0;
    }

    s_root = Kit::plain(parent);
    lv_obj_set_size(s_root, w, h);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_root, gap(), 0);
    lv_obj_t *top = Kit::plain(s_root);
    lv_obj_set_width(top, lv_pct(100));
    lv_obj_set_flex_grow(top, 1);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(top, gap() * 2, 0);
    s_nav = column(top);
    // The column a little wider on a narrow window (the 4B), where its names are cut first.
    lv_obj_set_size(s_nav, w * (w < UI::mm(UI::shape().W_NARROW) ? 31 : 27) / 100, lv_pct(100));
    s_body = column(top, true);
    lv_obj_set_height(s_body, lv_pct(100));
    lv_obj_set_flex_grow(s_body, 1);
    s_foot = Kit::plain(s_root);
    lv_obj_set_size(s_foot, lv_pct(100), Kit::controlH());
    lv_obj_set_flex_flow(s_foot, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_foot, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_foot, UI::mm(UI::shape().S7), 0);
    lv_obj_update_layout(s_root);   // the rows are sized from the body's width
    lv_obj_add_event_cb(s_root, [](lv_event_t *) { s_root = s_body = s_nav = s_foot = nullptr; }, LV_EVENT_DELETE, nullptr);
    fillNav();
    fillBody();
    fillFoot();
    return s_root;
}

uint8_t chosen(Item *out, uint8_t cap) {
    uint8_t n = 0;
    for (; n < s_nChosen && n < cap; n++) out[n] = s_chosen[n];
    return n;
}

bool haRowOf(const char *ref, HaOffer::Row &out) {
    const int k = chosenAt(ref);
    if (k < 0 || s_chosen[k].from != From::FROM_HA) return false;
    out = s_chosenRow[k];
    return true;
}

bool moving() { return s_move; }

void forget() { s_nChosen = 0; s_area[0] = '\0'; s_move = false; s_list = 0; }

uint8_t countOf(From from) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nChosen; i++) n += s_chosen[i].from == from;
    return n;
}

// "A", "A and B", "A, B and C" - each in the accent, as the dialogs name things.
void namesOf(char *out, size_t cap, bool onlyFrom, From from) {
    out[0] = '\0';
    const unsigned long acc = (unsigned long)(UI::pal().ACCENT & 0xFFFFFF);
    uint8_t total = 0;
    for (uint8_t i = 0; i < s_nChosen; i++) total += !onlyFrom || s_chosen[i].from == from;
    uint8_t k = 0;
    size_t used = 0;
    for (uint8_t i = 0; i < s_nChosen && used + 1 < cap; i++) {
        if (onlyFrom && s_chosen[i].from != from) continue;
        const char *sep = k == 0 ? "" : (k + 1 == total ? " and " : ", ");
        const int wrote = snprintf(out + used, cap - used, "%s#%06lX %s#", sep, acc, s_chosen[i].name);
        if (wrote < 0 || used + (size_t)wrote >= cap) { out[used] = '\0'; break; }
        used += (size_t)wrote;
        k++;
    }
}

bool tapRow(uint8_t i) {
    if (!s_body || i >= s_n || !s_rowObj[i]) return false;
    lv_obj_send_event(s_rowObj[i], LV_EVENT_CLICKED, nullptr);
    return true;
}

bool tapList(uint8_t k) {
    if (!s_body || k >= s_spec.nLists) return false;
    showList((int8_t)k);
    return true;
}

bool tapArea(const char *areaId) {
    for (uint8_t a = 0; a < s_nAreas && s_body; a++)
        if (!strcmp(s_areas[a].id, areaId ? areaId : "")) { showArea(a); return true; }
    return false;
}

bool press(Act a) {
    if (!s_body || !s_spec.act || kindNow() == Tick::TICK_NONE) return false;
    act(a);
    return true;
}

}  // namespace EditCards
