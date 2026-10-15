// The page header: a bar of slots (2.8). See include/UI/Panel_Header.h; the
// slots' data is UI/HeaderSlots.h, their panels SlotPanels.cpp.
#include "UI/Panel_Header.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "UI/Kit.h"
#include "UI/UIIcons.h"           // MDI_THERMOMETER
#include "UI/UIToolkit.h"
#include "Cards/CardIcons.h"      // a sensor's icon, tint and value, as its card draws them
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
}  // namespace

// THE KINDS. A new kind of slot is a row here and its two functions; nothing
// else in the header changes (header-slots.md section 8).
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
        s.entity = _spec[i].entity;
        s.box    = Kit::headerSlot(_bar, slotClickCb, this);
        (this->*ops(s.kind).build)(s);
    }
    // Laid out once, so a page slot can measure its room and a panel can find
    // its slot's centre; then each slot draws what it has.
    lv_obj_update_layout(_bar);
    for (uint8_t i = 0; i < _n; i++) (this->*ops(_slots[i].kind).update)(_slots[i], true);
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
}

void Panel_Header::restyle() {
    for (uint8_t i = 0; i < _n; i++) (this->*ops(_slots[i].kind).update)(_slots[i], true);
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

int32_t Panel_Header::slotCentreX(uint8_t i) const {
    if (i >= _n || !_slots[i].box) return 0;
    lv_area_t a;
    lv_obj_get_coords(_slots[i].box, &a);
    return (a.x1 + a.x2) / 2;
}

void Panel_Header::setOpenSlot(int i) {
    if (_open >= 0 && _open < _n) lv_obj_remove_state(_slots[_open].box, LV_STATE_CHECKED);
    _open = (i >= 0 && i < _n) ? i : -1;
    if (_open >= 0) lv_obj_add_state(_slots[_open].box, LV_STATE_CHECKED);
}

void Panel_Header::slotClickCb(lv_event_t *e) {
    Panel_Header *h = (Panel_Header *)lv_event_get_user_data(e);
    lv_obj_t *box = (lv_obj_t *)lv_event_get_current_target(e);
    if (!h || !h->_onTap) return;
    for (uint8_t i = 0; i < h->_n; i++)
        if (h->_slots[i].box == box) { h->_onTap(i); return; }
}

// --- The clock ------------------------------------------------------------------
// "1:55am", as the owner wrote it: 12-hour, am / pm small beside it, no seconds
// (header-slots.md section 7 - Claude's view, open). Dashes until the clock
// holds a real time (D-15). Redrawn when the minute changes, and only then.
void Panel_Header::buildClock(Slot &s) {
    s.c = Kit::plain(s.box);
    Kit::add(s.c, Kit::KitStyle::KS_HDR_CLOCK);
    s.a = Kit::label(s.c, Kit::KitStyle::KS_HDR_TEXT);
    s.b = Kit::label(s.c, Kit::KitStyle::KS_HDR_TEXT_DIM);
}

void Panel_Header::updateClock(Slot &s, bool force) {
    const time_t now = time(nullptr);
    const int32_t key = TimeService::isSet() ? (int32_t)(now / 60) : -1;
    if (!force && key == s.key) return;
    s.key = key;
    s.redraws++;
    if (key < 0) {
        lv_label_set_text(s.a, "--:--");
        lv_label_set_text(s.b, "");
        return;
    }
    struct tm t;
    localtime_r(&now, &t);
    const int h12 = t.tm_hour % 12 ? t.tm_hour % 12 : 12;
    lv_label_set_text_fmt(s.a, "%d:%02d", h12, t.tm_min);
    lv_label_set_text(s.b, t.tm_hour < 12 ? "am" : "pm");
}

// --- The page -------------------------------------------------------------------
// Its name and a dot per page: this page's larger and filled, the others rings.
// Where the slot is too narrow for both, the name goes and the dots stay (the
// page's panel names it) - measured, never by board.
void Panel_Header::buildPage(Slot &s) {
    s.a = Kit::label(s.box, Kit::KitStyle::KS_HDR_TEXT);
    s.c = Kit::plain(s.box);
    Kit::add(s.c, Kit::KitStyle::KS_HDR_DOTS);
}

void Panel_Header::updatePage(Slot &s, bool force) {
    if (!force) return;   // it changes with the page, never on its own
    s.redraws++;
    lv_label_set_text(s.a, _title);
    lv_obj_clean(s.c);
    if (_pageCount > 1 && _pageCount <= PAGE_DOTS_MAX) {
        for (uint8_t i = 0; i < _pageCount; i++)
            Kit::add(Kit::plain(s.c), i == _pageIndex ? Kit::KitStyle::KS_HDR_DOT_ON : Kit::KitStyle::KS_HDR_DOT_OFF);
    } else if (_pageCount > PAGE_DOTS_MAX) {
        lv_obj_t *l = Kit::label(s.c, Kit::KitStyle::KS_HDR_TEXT_DIM);
        lv_label_set_text_fmt(l, "%u / %u", (unsigned)_pageIndex + 1, (unsigned)_pageCount);
    }
    lv_obj_remove_flag(s.a, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(s.box);
    const int32_t room = lv_obj_get_content_width(s.box);
    const int32_t need = lv_obj_get_width(s.a) + lv_obj_get_width(s.c) + UI::sc(UI::hdr().SLOT_GAP);
    if (need > room) lv_obj_add_flag(s.a, LV_OBJ_FLAG_HIDDEN);
}

// --- Status -----------------------------------------------------------------------
// The HA / MQTT link, then Wi-Fi - the order of the Fleet Status Glyphs mock, and
// a dependency chain read from the edge in. Each is its own widget and throttles
// itself; framed by the glyph style.
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
// source says it is unavailable or it has gone stale - never an old value.
void Panel_Header::buildSensor(Slot &s) {
    s.frame = Kit::glyphFrame(s.box, _style);
    s.a = Kit::label(s.frame, Kit::KitStyle::KS_HDR_ICON);
    s.b = Kit::label(s.box, Kit::KitStyle::KS_HDR_TEXT);
    s.c = Kit::label(s.box, Kit::KitStyle::KS_HDR_MARK);
    lv_label_set_text(s.c, "!");
    lv_obj_add_flag(s.c, LV_OBJ_FLAG_HIDDEN);
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
    s.redraws++;

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
