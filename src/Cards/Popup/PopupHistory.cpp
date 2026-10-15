// History (2.12 P1, split from CardPopup.cpp): whose it is, what is asked and kept, the top
// line, drawing what has come, and the taps on it. The chart and activity have files of their own.
#include "PopupState.h"

namespace popup {

// ---------------------------------------------------------------------------
// HISTORY (2.10e, card-sheet section 18). What the view shows follows the
// entity: a NUMBER gets a chart over a span picked right on the view (24 / 12 /
// 6 / 1 h - owner: quick to switch), with now, min and max; anything that turns
// ON AND OFF gets its ACTIVITY - each change, newest first, with what caused it
// (a person, an automation, another entity), 10 rows then "Show more", a tap on
// a row for its details. The left panel is CHART or ACTIVITY to match.
//
// HA entities only, tonight's scope (owner, 2026-10-11): the board's own and
// MQTT entities keep a local log later. A group defined in HA is ITS OWN
// entity here, as in HA (owner): its members' histories are reached through
// Members, from each member's own window.
//
// HaHistory does the asking, on the loop task, and its answer is parsed on the
// websocket task; tickCb() notices it and draws it here.
// ---------------------------------------------------------------------------
HaHistory *s_hist = nullptr;

HistView s_hv;
lv_obj_t *s_histList = nullptr;   // activity's rows, while they show

Card *cardOf(lv_obj_t *sf);
void keepCardSetting(const char *key, const char *value);

// The card whose choices these are; null inside a member.
Card *histCard() {
    if (inMember()) return nullptr;
    Card *c = cardOf(h.surface);
    return (c && c->hasId()) ? c : nullptr;
}

void histKeep(const char *key, const char *value) { if (histCard()) keepCardSetting(key, value); }

// A saved name back to its index in `names`, or `def`.
uint8_t histLoad(const char *key, const char *const *names, uint8_t n, uint8_t def) {
    Card *c = histCard();
    char v[16];
    if (!c || !Settings::card(c->id(), key, v, sizeof(v))) return def;
    for (uint8_t i = 0; i < n; i++) if (!strcmp(v, names[i])) return i;
    return def;
}
bool histLoadOn(const char *key, bool def) {
    static const char *const ONOFF[2] = { "off", "on" };
    return histLoad(key, ONOFF, 2, def ? 1 : 0) == 1;
}

// Whose history the view shows: one member, inside it; an HA group as itself;
// a card's one entity. A card standing for several (All Lamps) has none of its
// own.
const Entity *histSubject() {
    if (inMember()) return s.ent[0];
    if (s.native) return s.native;
    return s.nEnt == 1 ? s.ent[0] : nullptr;
}

bool histFromHa(const Entity *e) {
    return e && e->desc.source == EntitySource::HA && e->desc.externalRef[0];
}

bool histIsChart(const Entity *e) {
    return e && e->desc.kind == EntityKind::SENSOR &&
           (e->value.type == ValueType::FLOAT || e->value.type == ValueType::INT);
}

// The subject changed (another card, a member): its choices start afresh. True
// when they did - the left panel then shows the new subject's.
bool histSync() {
    const Entity *e = histSubject();
    if (e == s_hv.e) return false;
    s_hv = HistView{};
    s_hv.e = e;
    s_hv.chart = histIsChart(e);
    // What the owner chose for this card last time (H3).
    s_hv.span     = histLoad("hist_span", HIST_SPAN_NAME, HIST_SPANS, 0);
    s_hv.period   = histLoad("act_span", HIST_PERIOD_NAME, HIST_PERIODS, 0);
    s_hv.legend   = histLoad("hist_legend", HIST_LEGEND_NAME, 4, LEGEND_BOTH);
    s_hv.duration = histLoadOn("act_duration", false);
    s_hv.minMax   = histLoadOn("hist_minmax", true);
    s_hv.shade    = histLoadOn("hist_shade", true);
    s_hv.chips    = histLoadOn("hist_chips", true);
    s_hv.actChips = histLoadOn("act_chips", true);
    s_hv.causes   = histLoadOn("act_causes", true);
    return true;
}

void histAsk();
void histDraw();
void histChartCb(lv_event_t *ev);
void histFillCb(lv_event_t *ev);
int32_t histX(uint32_t t);
float histNice(float raw);
lv_obj_t *histVLine(lv_obj_t *parent, lv_point_precise_t *pts, int32_t x, uint32_t col, int32_t width);
void histSpanChosen(uint8_t i);
void histMoreCb(lv_event_t *ev);
void histRowCb(lv_event_t *ev);
void histSyncPanel();   // below, with the panels: the left panel's rows follow the view

// A centred line of words in the view - loading, nothing, a failure.
void histNote(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = Kit::label(parent, KS::KS_LABEL_NAME_DIM);
    lv_label_set_text(l, text);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(90));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}

const char *histFailText(HistFail f) {
    switch (f) {
        case HistFail::FAIL_NO_LINK:   return "Not connected to Home Assistant";
        case HistFail::FAIL_NO_CLOCK:  return "Waiting for the clock to be set";
        case HistFail::FAIL_TIMEOUT:   return "Home Assistant did not answer";
        case HistFail::FAIL_TOO_BIG:   return "Too much history to show here";
        case HistFail::FAIL_REFUSED:   return "Home Assistant could not give this history";
        case HistFail::FAIL_NO_MEMORY: return "No room to show history";
        case HistFail::FAIL_NO_DATA:   return "Home Assistant keeps no history for this";
        default:                       return "No history";
    }
}

void buildHistory() {
    histSync();
    s_hv.body = nullptr;
    const Entity *e = s_hv.e;
    if (!histFromHa(e)) {
        histNote(s.stage, e ? "History for this card comes later: only Home Assistant's is shown so far"
                            : "A group made here has no history of its own. Open Members for each one's");
        return;
    }
    s_hv.body = plain(s.stage);
    lv_obj_set_size(s_hv.body, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(s_hv.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_hv.body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_hv.body, mm(UI::shape().S4), 0);
    histAsk();
    histDraw();
}

void histAsk() {
    if (!s_hist || !histFromHa(s_hv.e)) return;
    s_hv.openRow = -1;
    if (s_hv.chart) s_hist->requestChart(s_hv.e->desc.externalRef, HIST_SPAN_SEC[s_hv.span]);
    else            s_hist->requestActivity(s_hv.e->desc.externalRef, HIST_PERIOD_SEC[s_hv.period]);
    s_hv.gen = s_hist->gen();
}

// --- The chart -------------------------------------------------------------

// The span buttons: the chart's four spans, or activity's two (owner, round
// 2). THE CONTROL DECK'S SELECTOR, COMPACT (owner, 2026-10-14; #92): the same
// eye candy as the light's controls, a span chip's height, so the chart and
// the log keep their room. `room`: the most width it may take (0, any).
lv_obj_t *histChips(lv_obj_t *parent, int32_t room) {
    Kit::SelectorSpec sp = {};
    const char *const *words = s_hv.chart ? HIST_SPAN_CHIP : HIST_PERIOD_CHIP;
    sp.n = s_hv.chart ? HIST_SPANS : HIST_PERIODS;
    for (uint8_t i = 0; i < sp.n && i < Kit::SELECTOR_MAX; i++) sp.glyph[i] = words[i];
    sp.chosen   = s_hv.chart ? s_hv.span : s_hv.period;
    sp.compact  = true;
    sp.room     = room;
    sp.onChosen = histSpanChosen;
    Kit::selector(s_hv.spanSel, parent, sp);
    return s_hv.spanSel.obj;
}

// THE SELECTOR KEEPS ITS SIZE (owner, T9, 2026-10-14). The chart's corners
// set its room, and they are not drawn while the next span loads - it widened
// there and narrowed again when the answer came. So the room only ever
// narrows while the view shows one subject (a wider Min / Max narrows it once),
// and a new subject or Show min/max starts it afresh.
int32_t histChipRoom(int32_t topW, int32_t leftW, int32_t rightW) {
    const int32_t half = LV_MIN(topW / 2 - leftW, topW / 2 - rightW) - mm(UI::shape().S5);
    const int32_t room = 2 * LV_MAX(half, 1);
    s_hv.chipRoom = s_hv.chipRoom ? LV_MIN(s_hv.chipRoom, room) : room;
    return s_hv.chipRoom;
}

// Before the chart's first answer: its corners as they will be, the numbers
// taken as wide as Now's.
int32_t histChipRoomBefore(lv_obj_t *row) {
    if (s_hv.chipRoom) return s_hv.chipRoom;
    const lv_font_t *f = UI::type().TAG;
    char now[32], buf[48];
    cardFormatValue(*s_hv.e, now, sizeof(now), true, s.tempUnit);
    snprintf(buf, sizeof(buf), "Now: %s", now);
    const int32_t leftW = s_hv.minMax ? LV_MAX(textW("Min:", f), textW("Max:", f)) + mm(UI::shape().S3) + textW(now, f) : 0;
    lv_obj_update_layout(row);
    return histChipRoom(lv_obj_get_width(row), leftW, textW(buf, f));
}

// THE TOP LINE (owner, rounds 5 and 6), one shape for the chart and activity,
// loading or drawn, so nothing moves when the answer arrives: the span chips
// or "Showing: ..." in the middle; on the chart, Min / Max in the left corner
// and Now in the right. History's stage REACHES UP INTO THE HEADER ROW's empty
// lower half (showView() gives the window a negative gap there), so the middle
// sits a small gap under the title. The corners start `histLift()` lower, under
// the back and chart buttons. Nothing in the stage's top strip but the middle
// is clickable, so a tap there still reaches those buttons. Without the chips
// the line is only as tall as Min / Max (or, on activity, "Showing:"), so the
// chart or the list below gets the room.
//
// Not LV_OBJ_FLAG_OVERFLOW_VISIBLE: LVGL draws in strips and skips a child
// whose parent is outside the strip, and never hit-tests outside the parent -
// the first try was cut off at the stage's top.

// How far the stage reaches up into the header row.
int32_t histLift() {
    const int32_t lh = lv_font_get_line_height(UI::type().NAME);
    return LV_MAX(0, (UI::minTouch() - lh) / 2 - mm(UI::shape().S6));
}

bool histChipsOn() { return s_hv.chart ? s_hv.chips : s_hv.actChips; }

lv_obj_t *histTopRow(lv_obj_t *body) {
    const UIType &t = UI::type();
    const int32_t corners = s_hv.chart ? histLift() + 2 * lv_font_get_line_height(t.TAG) + mm(UI::shape().S3) : 0;
    const int32_t middle  = histChipsOn() ? mm(UI::shape().H_CHIP) : lv_font_get_line_height(t.NAME);
    lv_obj_t *row = plain(body);
    lv_obj_set_size(row, lv_pct(100), LV_MAX(corners, middle));
    return row;
}

// A corner of the top line: out of the buttons' way, in the middle of the rest.
void histCorner(lv_obj_t *o, lv_align_t where) { lv_obj_align(o, where, 0, histLift() / 2); }

// The span in words (owner, round 4) where the chips were when they are
// hidden. A TAP STEPS THE SPAN (owner, rounds 5 and 6): 24 -> 12 -> 6 -> 1 hour
// on the chart, 24 hours <-> 7 days on activity, round again, as the chips do.
void histShowingCb(lv_event_t *ev);
void histShowing(lv_obj_t *row) {
    static const char *const CHART_WORDS[HIST_SPANS] = { "last 24 hours", "last 12 hours", "last 6 hours", "last hour" };
    static const char *const ACT_WORDS[HIST_PERIODS] = { "last 24 hours", "last 7 days" };
    const UIType    &t = UI::type();
    const UIPalette &p = UI::pal();
    char buf[40];
    snprintf(buf, sizeof(buf), "Showing: %s", s_hv.chart ? CHART_WORDS[s_hv.span] : ACT_WORDS[s_hv.period]);
    lv_obj_t *l = Kit::label(row, KS::KS_LABEL_NAME);
    lv_label_set_text(l, buf);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);
    // The whole line, and a little round it - not so much that it takes taps
    // meant for the title just above.
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(l, mm(UI::shape().S3));
    lv_obj_set_style_text_color(l, UI::c(p.ACCENT), UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_add_event_cb(l, histShowingCb, LV_EVENT_CLICKED, nullptr);
}

// The middle of the top line. `room`: the width the chips may take (0, any).
void histTopMiddle(lv_obj_t *row, int32_t room) {
    if (!histChipsOn()) { histShowing(row); return; }
    lv_obj_t *chips = histChips(row, room);
    lv_obj_align(chips, LV_ALIGN_TOP_MID, 0, 0);
}

// Whatever HaHistory has for this view now: loading, the answer, or why not.
void histDraw() {
    if (!s_hv.body) return;
    s_hv.area = s_hv.scrubLine = s_hv.scrubLbl = nullptr;   // going with the clean
    s_hv.ys = nullptr;
    s_histList = nullptr;
    lv_obj_clean(s_hv.body);
    const HistState st = s_hist ? s_hist->state() : HistState::HIST_FAILED;
    s_hv.drawnSeq = s_hist ? s_hist->doneSeq() : 0;
    const bool ours = s_hist && s_hist->gen() == s_hv.gen;
    if (st == HistState::HIST_READY && ours) {
        if (s_hv.chart) histDrawChart(s_hv.body);
        else            histDrawActivity(s_hv.body);
        return;
    }
    // Still asking, or no answer: the chips stay, so another span can be tried
    // - in the same top line as the answer's, so nothing moves when it comes.
    lv_obj_t *top = histTopRow(s_hv.body);
    histTopMiddle(top, (s_hv.chart && histChipsOn()) ? histChipRoomBefore(top) : 0);
    lv_obj_t *mid = plain(s_hv.body);
    lv_obj_set_width(mid, lv_pct(100));
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    histNote(mid, (st == HistState::HIST_WAITING || !ours) ? "Loading..."
                                                          : histFailText(s_hist ? s_hist->fail() : HistFail::FAIL_NO_LINK));
}

// tickCb(): the answer arrived (or failed) since the last draw.
void histPoll() {
    if (s.view != PopupView::VIEW_HISTORY || !s_hv.body || !s_hist) return;
    if (s_hist->gen() != s_hv.gen) return;
    if (s_hist->doneSeq() != s_hv.drawnSeq) histDraw();
}

// The selector has arrived on span i (it glides there on a tap, or snaps
// there when let go).
void histSpanChosen(uint8_t i) {
    if (s_hv.chart) {
        if (i == s_hv.span || i >= HIST_SPANS) return;
        s_hv.span = i;
        histKeep("hist_span", HIST_SPAN_NAME[i]);
    } else {
        if (i == s_hv.period || i >= HIST_PERIODS) return;
        s_hv.period = i;
        s_hv.more = false;
        histKeep("act_span", HIST_PERIOD_NAME[i]);
    }
    histSyncPanel();
    histAsk();
    lv_async_call([](void *) { histDraw(); }, nullptr);   // the selector's own row is redrawn
}

// "Showing: ..." tapped: the next span, round again after the last.
void histShowingCb(lv_event_t *ev) {
    (void)ev;
    if (s_hv.chart) {
        s_hv.span = (uint8_t)((s_hv.span + 1) % HIST_SPANS);
        histKeep("hist_span", HIST_SPAN_NAME[s_hv.span]);
    } else {
        s_hv.period = (uint8_t)((s_hv.period + 1) % HIST_PERIODS);
        s_hv.more = false;
        histKeep("act_span", HIST_PERIOD_NAME[s_hv.period]);
    }
    histSyncPanel();
    histAsk();
    lv_async_call([](void *) { histDraw(); }, nullptr);   // its own row is redrawn
}

void histMoreCb(lv_event_t *ev) {
    (void)ev;
    s_hv.more = true;
    lv_async_call([](void *) { histDraw(); }, nullptr);
}

// A row's details, one row at a time; a second tap folds them.
void histRowCb(lv_event_t *ev) {
    const int16_t i = (int16_t)(uintptr_t)lv_event_get_user_data(ev);
    s_hv.openRow = (s_hv.openRow == i) ? -1 : i;
    lv_async_call([](void *) {
        // Redrawn whole; the list keeps its place.
        const int32_t y = s_histList ? lv_obj_get_scroll_y(s_histList) : 0;
        histDraw();
        if (s_histList && y) { lv_obj_update_layout(s_histList); lv_obj_scroll_to_y(s_histList, y, LV_ANIM_OFF); }
    }, nullptr);
}

} // namespace popup
