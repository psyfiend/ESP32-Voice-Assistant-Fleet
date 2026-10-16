// The page header: three places of slots (2.8). See include/UI/Panel_Header.h;
// the slots' data is UI/HeaderSlots.h, where they stand Layout/HeaderPlan.h,
// their panels SlotPanels.cpp.
#include "UI/Panel_Header.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "UI/Kit.h"
#include "UI/UIIcons.h"           // MDI_THERMOMETER
#include "UI/UIToolkit.h"
#include "Cards/CardIcons.h"      // a sensor's icon, tint and value, as its card draws them
#include "Alarms.h"
#include "EntityRegistry.h"
#include "TimeService.h"

namespace {
constexpr uint32_t TICK_MS   = 250;    // how often the slots are asked; each redraws only on a change
constexpr uint32_t SENSOR_MS = 1000;   // a sensor is read at most once a second

// UTF-8 for the degree sign, as bytes (as CardIcons.cpp writes it).
const char *DEG = "\xC2\xB0";

int32_t hashText(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return (int32_t)(h & 0x3FFFFFFF);
}

// "6:56" and "am", as the owner wrote the time: 12-hour, no seconds.
void clockText(time_t when, char *hm, size_t cap, const char **ampm) {
    struct tm t;
    localtime_r(&when, &t);
    const int h12 = t.tm_hour % 12 ? t.tm_hour % 12 : 12;
    snprintf(hm, cap, "%d:%02d", h12, t.tm_min);
    *ampm = t.tm_hour < 12 ? "am" : "pm";
}
}  // namespace

// THE KINDS. A new kind of slot is a row here and its two functions, and a row
// in HeaderSlots.cpp; nothing else in the header changes (header-slots.md
// section 8).
struct Panel_Header::KindOps {
    SlotKind kind;
    void (Panel_Header::*build)(Slot &);
    void (Panel_Header::*update)(Slot &, bool force);
};

const Panel_Header::KindOps &Panel_Header::ops(SlotKind k) {
    static const KindOps TABLE[] = {
        { SlotKind::SLOT_CLOCK,  &Panel_Header::buildClock,  &Panel_Header::updateClock  },
        { SlotKind::SLOT_PAGE,   &Panel_Header::buildPage,   &Panel_Header::updatePage   },
        { SlotKind::SLOT_STATUS, &Panel_Header::buildStatus, &Panel_Header::updateStatus },
        { SlotKind::SLOT_SENSOR, &Panel_Header::buildSensor, &Panel_Header::updateSensor },
        { SlotKind::SLOT_ALARM,  &Panel_Header::buildAlarm,  &Panel_Header::updateAlarm  },
        { SlotKind::SLOT_AP,     &Panel_Header::buildAp,     &Panel_Header::updateAp     },
    };
    for (const KindOps &o : TABLE) if (o.kind == k) return o;
    return TABLE[0];
}

void Panel_Header::init(lv_obj_t *parent, ConnectivityManager *conn, MqttManager *mqtt, EntityRegistry *reg) {
    _conn = conn;
    _mqtt = mqtt;
    _reg  = reg;
    _bar  = Kit::headerBar(parent);
    _spec  = HEADER_DEFAULT;
    _specN = HEADER_DEFAULT_COUNT;
    buildSlots();
}

void Panel_Header::buildSlots() {
    if (!_bar) return;
    lv_obj_clean(_bar);
    _n = _specN < HEADER_SLOTS_MAX ? _specN : HEADER_SLOTS_MAX;
    for (uint8_t i = 0; i < _n; i++) {
        Slot &s = _slots[i];
        s = Slot();
        s.kind   = _spec[i].kind;
        s.place  = _spec[i].place;
        s.entity = _spec[i].entity;
        s.box    = Kit::headerSlot(_bar, slotClickCb, this);
        (this->*ops(s.kind).build)(s);
    }
    for (uint8_t i = 0; i < _n; i++) (this->*ops(_slots[i].kind).update)(_slots[i], true);
    // The open tab's coves, over the slots (D-23).
    _cove[0] = Kit::joinCorner(_bar, false);
    _cove[1] = Kit::joinCorner(_bar, false);
    plan();
    if (_open >= 0 && _open < _n) lv_obj_add_state(_slots[_open].box, LV_STATE_CHECKED);
    else _open = -1;
}

void Panel_Header::tick() {
    const uint32_t now = millis();
    if (now - _lastTickMs < TICK_MS) return;
    _lastTickMs = now;
    const bool sensorDue = now - _lastSensorMs >= SENSOR_MS;
    if (sensorDue) _lastSensorMs = now;
    for (uint8_t i = 0; i < _n; i++) {
        Slot &s = _slots[i];
        if (s.kind == SlotKind::SLOT_SENSOR && !sensorDue) continue;
        (this->*ops(s.kind).update)(s, false);
    }
    if (_dirty) plan();
}

void Panel_Header::refresh() {
    for (uint8_t i = 0; i < _n; i++) (this->*ops(_slots[i].kind).update)(_slots[i], false);
    if (_dirty) plan();
}

void Panel_Header::restyle() {
    for (uint8_t i = 0; i < _n; i++) (this->*ops(_slots[i].kind).update)(_slots[i], true);
    plan();
}

// --- The plan ---------------------------------------------------------------------
// Each slot's forms are measured from what it holds now: its pads, the things
// it always shows, and the extras each form keeps, parted by its gap. An
// object hidden for a reason of its own (the error mark while nothing is
// wrong) takes no room; an extra the last plan shed is still measured, since
// LVGL keeps a hidden object's size up to date.
void Panel_Header::claimOf(const Slot &s, Layout::HeaderClaim &c) const {
    const SlotKindInfo &k = slotKindInfo(s.kind);
    c = Layout::HeaderClaim();
    c.place  = s.place;
    c.wanted = s.wanted;
    c.forms  = k.forms <= s.nExtra + 1 ? k.forms : s.nExtra + 1;
    for (uint8_t f = 0; f < c.forms; f++) c.keep[f] = k.keep[f];

    const int32_t gap  = lv_obj_get_style_pad_column(s.box, LV_PART_MAIN);
    const int32_t pads = lv_obj_get_style_pad_left(s.box, LV_PART_MAIN) +
                         lv_obj_get_style_pad_right(s.box, LV_PART_MAIN);
    int32_t base = 0;
    uint8_t nBase = 0;
    const uint32_t cnt = lv_obj_get_child_count(s.box);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *ch = lv_obj_get_child(s.box, (int32_t)i);
        bool isExtra = false;
        for (uint8_t e = 0; e < s.nExtra; e++) if (s.extra[e] == ch) isExtra = true;
        if (isExtra || lv_obj_has_flag(ch, LV_OBJ_FLAG_HIDDEN)) continue;
        const int32_t w = lv_obj_get_width(ch);
        if (w <= 0) continue;
        base += w;
        nBase++;
    }
    for (uint8_t f = 0; f < c.forms; f++) {
        int32_t w = base;
        uint8_t n = nBase;
        for (uint8_t e = f; e < s.nExtra; e++) {
            if (!s.extraOn[e]) continue;   // off (the bell with no alarm): no room
            const int32_t ew = lv_obj_get_width(s.extra[e]);
            if (ew > 0) { w += ew; n++; }
        }
        c.width[f] = pads + w + (n ? gap * (n - 1) : 0);
    }
}

void Panel_Header::plan() {
    _dirty = false;
    if (!_bar || !_n) return;
    // The labels' sizes for what they say now.
    lv_obj_update_layout(_bar);
    Layout::HeaderClaim claims[HEADER_SLOTS_MAX];
    for (uint8_t i = 0; i < _n; i++) claimOf(_slots[i], claims[i]);
    const UIHeader &h = UI::hdr();
    Layout::HeaderBar bar;
    bar.width = lv_obj_get_content_width(_bar);
    bar.edge  = UI::sc(h.EDGE);
    bar.space = UI::sc(h.SPACE);
    bar.clear = UI::sc(h.CLEAR);
    Layout::HeaderSpot spot[HEADER_SLOTS_MAX];
    const bool fits = Layout::planHeader(bar, claims, _n, spot);
    if (!fits && _fits) Serial.printf("[UI] header: a side meets the middle with nothing left to give way (%ld px)\n",
                                      (long)bar.width);
    _fits = fits;
    _plans++;
    for (uint8_t i = 0; i < _n; i++) {
        Slot &s = _slots[i];
        s.shown = spot[i].shown;
        s.form  = spot[i].form;
        if (!s.shown) { lv_obj_add_flag(s.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(s.box, LV_OBJ_FLAG_HIDDEN);
        for (uint8_t e = 0; e < s.nExtra; e++) {
            if (e < s.form || !s.extraOn[e]) lv_obj_add_flag   (s.extra[e], LV_OBJ_FLAG_HIDDEN);
            else                             lv_obj_remove_flag(s.extra[e], LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_pos(s.box, spot[i].x, 0);
        lv_obj_set_width(s.box, spot[i].w);
    }
}

void Panel_Header::setPage(const char *title, uint8_t index, uint8_t count,
                           const SlotSpec *slots, uint8_t nSlots) {
    snprintf(_title, sizeof(_title), "%s", title ? title : "");
    _pageIndex = index;
    _pageCount = count;
    const SlotSpec *want  = slots ? slots : HEADER_DEFAULT;
    const uint8_t   wantN = slots ? nSlots : HEADER_DEFAULT_COUNT;
    if (want != _spec || wantN != _specN) {
        _spec  = want;
        _specN = wantN;
        buildSlots();
        return;
    }
    for (uint8_t i = 0; i < _n; i++)
        if (_slots[i].kind == SlotKind::SLOT_PAGE) updatePage(_slots[i], true);
    if (_dirty) plan();
}

void Panel_Header::setGlyphStyle(GlyphStyle s) {
    if (s == _style) return;
    _style = s;
    buildSlots();
}

SlotKind Panel_Header::slotKind(uint8_t i) const {
    return i < _n ? _slots[i].kind : SlotKind::SLOT_KIND_COUNT;
}

const char *Panel_Header::slotEntity(uint8_t i) const {
    return i < _n ? _slots[i].entity : nullptr;
}

int Panel_Header::slotOf(SlotKind k) const {
    for (uint8_t i = 0; i < _n; i++) if (_slots[i].kind == k) return i;
    return -1;
}

bool Panel_Header::slotShown(uint8_t i) const {
    return i < _n && _slots[i].shown;
}

uint8_t Panel_Header::slotForm(uint8_t i) const {
    return i < _n ? _slots[i].form : 0;
}

int32_t Panel_Header::slotCentreX(uint8_t i) const {
    if (i >= _n || !_slots[i].box) return 0;
    lv_area_t a;
    lv_obj_get_coords(_slots[i].box, &a);
    return (a.x1 + a.x2) / 2;
}

void Panel_Header::slotSpan(uint8_t i, int32_t &l, int32_t &r) const {
    l = r = -1;
    if (i >= _n || !_slots[i].box) return;
    lv_area_t a;
    lv_obj_get_coords(_slots[i].box, &a);
    l = a.x1;
    r = a.x2 + 1;
}

void Panel_Header::setOpenSlot(int i, int32_t panelL, int32_t panelR) {
    if (_open >= 0 && _open < _n) lv_obj_remove_state(_slots[_open].box, LV_STATE_CHECKED);
    _open = (i >= 0 && i < _n) ? i : -1;
    for (lv_obj_t *c : _cove) if (c) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
    if (_open < 0) return;
    lv_obj_add_state(_slots[_open].box, LV_STATE_CHECKED);
    // THE COVES (D-23): where the panel reaches past the tab, the tab's side
    // curves out into the line; where they share a side, a straight line.
    int32_t l, r;
    slotSpan((uint8_t)_open, l, r);
    const int32_t barH = lv_obj_get_height(_bar);
    const bool cl = panelL >= 0 && panelL < l;
    const bool cr = panelR >= 0 && panelR > r;
    Kit::coveAt(_cove[0], l, false, barH);
    Kit::coveAt(_cove[1], r, true, barH);
    if (cl) { lv_obj_remove_flag(_cove[0], LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(_cove[0]); }
    if (cr) { lv_obj_remove_flag(_cove[1], LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(_cove[1]); }
}

void Panel_Header::slotClickCb(lv_event_t *e) {
    Panel_Header *h = (Panel_Header *)lv_event_get_user_data(e);
    lv_obj_t *box = (lv_obj_t *)lv_event_get_current_target(e);
    if (!h || !h->_onTap) return;
    for (uint8_t i = 0; i < h->_n; i++)
        if (h->_slots[i].box == box) { h->_onTap(i); return; }
}

// --- A glyph in a slot ------------------------------------------------------------
// Its frame in the bar's style and a stroke glyph inside it (UI/GlyphShapes.h),
// the same size in every style (D-24).
void Panel_Header::glyphIn(Slot &s, lv_obj_t *parent, Glyphs::Shape shape) {
    s.frame = Kit::glyphFrame(parent, _style);
    Kit::strokeGlyph(s.glyph, s.frame, shape, UI::sc(UI::hdr().GLYPH));
}

// The glyph in `col`; in a circle, the circle takes it and the strokes the bar's.
void Panel_Header::paintGlyph(Slot &s, uint32_t col) {
    if (_style == GlyphStyle::GLYPH_CIRCLE) {
        lv_obj_set_style_bg_color(s.frame, UI::c(col), 0);
        Kit::strokeGlyphPaint(s.glyph, Kit::ALL_PARTS, UI::c(UI::pal().SURFACE_ALT));
    } else {
        Kit::strokeGlyphPaint(s.glyph, Kit::ALL_PARTS, UI::c(col));
    }
}

// --- The clock ------------------------------------------------------------------
// "1:55am", as the owner wrote it: 12-hour, am / pm small beside it, no seconds
// (Q2: as built). Dashes until the clock holds a real time (D-15). Redrawn when
// the minute changes, and only then. THE BELL (D-21) stands inside the clock's
// slot while an alarm is within a week - the slot's extra, the first thing the
// bar sheds; a tap on either opens the clock's panel, which says when.
void Panel_Header::buildClock(Slot &s) {
    s.c = Kit::plain(s.box);
    Kit::add(s.c, Kit::KitStyle::KS_HDR_CLOCK);
    s.a = Kit::label(s.c, Kit::KitStyle::KS_HDR_TEXT);
    s.b = Kit::label(s.c, Kit::KitStyle::KS_HDR_TEXT_DIM);
    glyphIn(s, s.box, Glyphs::Shape::BELL);
    s.extra[0] = s.frame;
    s.extraOn[0] = false;
    s.nExtra = 1;
}

void Panel_Header::updateClock(Slot &s, bool force) {
    const time_t now = time(nullptr);
    const bool bell = TimeService::isSet() && Alarms::bell(now);
    const int32_t key = TimeService::isSet() ? (int32_t)(now / 60) * 2 + (bell ? 1 : 0) : -1;
    if (!force && key == s.key) return;
    s.key = key;
    redrawn(s);
    s.extraOn[0] = bell;
    if (bell) paintGlyph(s, UI::pal().TEXT);
    if (key < 0) {
        lv_label_set_text(s.a, "--:--");
        lv_label_set_text(s.b, "");
        return;
    }
    char hm[8];
    const char *ampm;
    clockText(now, hm, sizeof(hm), &ampm);
    lv_label_set_text(s.a, hm);
    lv_label_set_text(s.b, ampm);
}

// --- The page -------------------------------------------------------------------
// Its name and a dot per page: this page's larger and filled, the others rings.
// The name is the slot's extra: where the bar is too narrow, the plan sheds it
// and the dots stay (the page's panel names it) - measured, never by board.
void Panel_Header::buildPage(Slot &s) {
    s.a = Kit::label(s.box, Kit::KitStyle::KS_HDR_TEXT);
    s.c = Kit::plain(s.box);
    Kit::add(s.c, Kit::KitStyle::KS_HDR_DOTS);
    s.extra[0] = s.a;
    s.nExtra = 1;
}

void Panel_Header::updatePage(Slot &s, bool force) {
    if (!force) return;   // it changes with the page, never on its own
    redrawn(s);
    lv_label_set_text(s.a, _title);
    lv_obj_clean(s.c);
    if (_pageCount > 1 && _pageCount <= PAGE_DOTS_MAX) {
        for (uint8_t i = 0; i < _pageCount; i++)
            Kit::add(Kit::plain(s.c), i == _pageIndex ? Kit::KitStyle::KS_HDR_DOT_ON : Kit::KitStyle::KS_HDR_DOT_OFF);
    } else if (_pageCount > PAGE_DOTS_MAX) {
        lv_obj_t *l = Kit::label(s.c, Kit::KitStyle::KS_HDR_TEXT_DIM);
        lv_label_set_text_fmt(l, "%u / %u", (unsigned)_pageIndex + 1, (unsigned)_pageCount);
    }
}

// --- Status -----------------------------------------------------------------------
// The HA / MQTT link, then Wi-Fi - the order of the Fleet Status Glyphs mock, and
// a dependency chain read from the edge in. Each is its own widget and throttles
// itself; framed by the glyph style. Their size never changes, so they never
// ask for a plan.
void Panel_Header::buildStatus(Slot &s) {
    if (_mqtt) _link.init(s.box, _mqtt, _style);
    if (_conn) _wifi.init(s.box, _conn, _style);
}

void Panel_Header::updateStatus(Slot &s, bool force) {
    (void)s;
    if (force) {
        if (_mqtt) _link.repaint();
        if (_conn) _wifi.repaint();
        return;
    }
    if (_conn) _wifi.tick();
    if (_mqtt) _link.tick();
}

// --- A sensor ------------------------------------------------------------------------
// Its icon and its value, as its card would draw them (the corner icon and its
// tint; a temperature to the whole degree). DASHES UNTIL A LIVE VALUE (D-15):
// "--" while it has never reported this boot; "--" and the error mark when the
// source says it is unavailable or it has gone stale - never an old value. The
// icon is the slot's extra: on a narrow bar the value stands alone.
void Panel_Header::buildSensor(Slot &s) {
    s.frame = Kit::glyphFrame(s.box, _style);
    s.a = Kit::label(s.frame, Kit::KitStyle::KS_HDR_ICON);
    s.b = Kit::label(s.box, Kit::KitStyle::KS_HDR_TEXT);
    s.c = Kit::label(s.box, Kit::KitStyle::KS_HDR_MARK);
    lv_label_set_text(s.c, "!");
    lv_obj_add_flag(s.c, LV_OBJ_FLAG_HIDDEN);
    s.extra[0] = s.frame;
    s.nExtra = 1;
}

void Panel_Header::updateSensor(Slot &s, bool force) {
    const Entity *e = (_reg && s.entity) ? _reg->find(s.entity) : nullptr;
    // An entity this board does not know is taken for a temperature - the
    // default slot's - so its dashes carry the degree sign they will keep.
    bool temp = true;
    if (e) {
        const char *u = cardDisplayUnit(*e);
        temp = (uint8_t)u[0] == 0xC2 && (uint8_t)u[1] == 0xB0;
    }
    char text[24];
    uint8_t state = 0;   // 0 live, 1 nothing yet, 2 an error
    if (!e || !e->everSet) state = 1;
    else if (!e->available || _reg->isStale(*e, millis())) state = 2;
    const bool number = e && (e->value.type == ValueType::FLOAT || e->value.type == ValueType::INT);
    if (state) {
        snprintf(text, sizeof(text), "--%s", temp ? DEG : "");
    } else if (temp && number) {
        const float v = cardDisplayNumber(*e, e->value.type == ValueType::FLOAT ? e->value.f : (float)e->value.i);
        snprintf(text, sizeof(text), "%ld%s", lroundf(v), DEG);
    } else {
        cardFormatValue(*e, text, sizeof(text), true);
    }
    const int32_t key = hashText(text) * 4 + state;
    if (!force && key == s.key) return;
    s.key = key;
    redrawn(s);

    lv_label_set_text(s.b, text);
    if (state == 2) lv_obj_remove_flag(s.c, LV_OBJ_FLAG_HIDDEN);
    else            lv_obj_add_flag   (s.c, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s.a, e ? cardCornerIcon(*e, false) : MDI_THERMOMETER);
    // The icon in its tint while live, dimmed otherwise; in a circle, the circle
    // takes that colour and the icon the bar's.
    const uint32_t tint = (!state && e) ? cardTintFor(e->desc) : UI::pal().TEXT_DIM;
    if (_style == GlyphStyle::GLYPH_CIRCLE) {
        lv_obj_set_style_bg_color(s.frame, UI::c(tint), 0);
        lv_obj_set_style_text_color(s.a, UI::c(UI::pal().SURFACE_ALT), 0);
    } else {
        lv_obj_set_style_text_color(s.a, UI::c(tint), 0);
    }
}

// --- The alarm's notice -------------------------------------------------------------
// A ringing bell among the status icons (D-21): A CLAIM WITH A CONDITION (D-19)
// - the next alarm is within the owner's warning time (12 hours unless set on
// the clock's panel) and its notice has not been dismissed. Amber: a notice,
// not a fault. Its panel says when, dismisses it, and leads to the clock's.
void Panel_Header::buildAlarm(Slot &s) {
    glyphIn(s, s.box, Glyphs::Shape::BELL_RING);
}

void Panel_Header::updateAlarm(Slot &s, bool force) {
    time_t when = 0;
    const bool on = TimeService::isSet() && Alarms::warning(time(nullptr), when);
    const int32_t key = on ? (int32_t)(when / 60) : -1;
    if (!force && key == s.key) return;
    s.key = key;
    s.wanted = on;
    redrawn(s);
    if (on) paintGlyph(s, UI::pal().SIG_WEAK);
}

// --- The access point ---------------------------------------------------------------
// The mast (D-24, D-27 "B") while the board hosts its setup AP - amber,
// provisioning being a working state, not a fault. Its own claim beside the
// Wi-Fi fan, where it used to be a badge under it. A tap opens the system
// panel, as the status glyphs' does.
void Panel_Header::buildAp(Slot &s) {
    glyphIn(s, s.box, Glyphs::Shape::AP_MAST);
}

void Panel_Header::updateAp(Slot &s, bool force) {
    const bool on = _conn && _conn->isApActive();
    const int32_t key = on ? 1 : 0;
    if (!force && key == s.key) return;
    s.key = key;
    s.wanted = on;
    redrawn(s);
    if (on) paintGlyph(s, UI::pal().SIG_WEAK);
}
