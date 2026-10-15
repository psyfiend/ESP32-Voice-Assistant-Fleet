#pragma once
// ---------------------------------------------------------------------------
// THE PAGE HEADER - a bar of slots (2.8, #19; UI/HeaderSlots.h).
//
// Four by default, each an equal share of the bar: the clock, the page (its
// name and dots), status (the HA / MQTT link and Wi-Fi), a sensor. What a page's
// header holds is data - PageSpec::headerSlots, else HEADER_DEFAULT - and each
// kind of slot is a build and an update in Panel_Header.cpp's table. The bar,
// a slot, a glyph's frame are the kit's (Kit::headerBar(), headerSlot(),
// glyphFrame()); their looks are roles (KS_HDR_*), their sizes UI::hdr().
//
// CHEAP TO KEEP (#19): tick() redraws a slot only when what it shows changes -
// the clock on a new minute, a sensor on a new value - so a redraw is that
// slot's labels, never the bar.
//
// A tap on a slot calls the tap callback with its index; GUIManager opens that
// slot's panel (the status slot's is the system panel) and marks the slot open
// (setOpenSlot()), which draws it in the panel's face, like a folder's tab.
// ---------------------------------------------------------------------------
#include <lvgl.h>
#include <Arduino.h>
#include <limits.h>
#include "UI/HeaderSlots.h"
#include "UI/Widget_ConnStatus.h"
#include "UI/Widget_MqttStatus.h"

class ConnectivityManager;
class MqttManager;
class EntityRegistry;

class Panel_Header {
public:
    // conn and mqtt may be null (a build without them): the status slot then
    // omits that glyph rather than showing a made-up state. reg may be null: a
    // sensor slot then shows dashes.
    void init(lv_obj_t *parent, ConnectivityManager *conn, MqttManager *mqtt, EntityRegistry *reg);
    void tick();
    // A scheme change. The roles follow by themselves; the glyphs' state
    // colours (and a circle's bar-coloured strokes) are drawn again here.
    void restyle();

    // THE PAGE: its title, its index among `count` pages, and its own slots
    // (nullptr: HEADER_DEFAULT). The slots are rebuilt only when the list is a
    // different one; otherwise only the page slot changes. `count` 0 or 1 hides
    // the dots; past PAGE_DOTS_MAX they become "3 / 15".
    void setPage(const char *title, uint8_t index, uint8_t count,
                 const SlotSpec *slots = nullptr, uint8_t nSlots = 0);
    static constexpr uint8_t PAGE_DOTS_MAX = 12;

    // How every glyph in the bar is framed (#19). Rebuilds the slots.
    void       setGlyphStyle(GlyphStyle s);
    GlyphStyle glyphStyle() const { return _style; }

    // --- The slots, for their panels (GUIManager) ---
    using SlotTapFn = void (*)(uint8_t slot);
    void        setOnSlotTap(SlotTapFn fn) { _onTap = fn; }
    uint8_t     slotCount() const { return _n; }
    SlotKind    slotKind(uint8_t i) const;
    const char *slotEntity(uint8_t i) const;
    int         slotOf(SlotKind k) const;        // the first slot of that kind, or -1
    int32_t     slotCentreX(uint8_t i) const;    // its centre on the screen
    void        setOpenSlot(int i);              // drawn open while its panel shows; -1 none
    int         openSlot() const { return _open; }
    // How often a slot has redrawn since it was built: the proof that the
    // clock redraws once a minute and a sensor only when its value moves.
    uint32_t    slotRedraws(uint8_t i) const { return i < _n ? _slots[i].redraws : 0; }

    lv_obj_t *getContainer() { return _bar; }
    Widget_ConnStatus &getConnStatus() { return _wifi; }
    Widget_MqttStatus &getMqttStatus() { return _link; }

private:
    // One slot and what it shows. Which object is which depends on the kind:
    //   clock   a: the time, b: am / pm, c: the row they stand in
    //   page    a: the page's name, c: its dots
    //   status  the two glyphs (their own widgets), nothing here
    //   sensor  frame: the icon's frame, a: the icon, b: the value, c: the error mark
    struct Slot {
        SlotKind    kind   = SlotKind::SLOT_CLOCK;
        const char *entity = nullptr;
        lv_obj_t   *box = nullptr, *frame = nullptr, *a = nullptr, *b = nullptr, *c = nullptr;
        int32_t     key = INT32_MIN;   // what it shows now; redrawn only when this changes
        uint32_t    redraws = 0;       // how often it did (the System Doctor's [UI STATE])
    };
    struct KindOps;
    static const KindOps &ops(SlotKind k);

    void buildSlots();
    void buildClock(Slot &s);   void updateClock(Slot &s, bool force);
    void buildPage(Slot &s);    void updatePage(Slot &s, bool force);
    void buildStatus(Slot &s);  void updateStatus(Slot &s, bool force);
    void buildSensor(Slot &s);  void updateSensor(Slot &s, bool force);
    static void slotClickCb(lv_event_t *e);

    lv_obj_t       *_bar = nullptr;
    Slot            _slots[HEADER_SLOTS_MAX];
    uint8_t         _n = 0;
    const SlotSpec *_spec = nullptr;
    uint8_t         _specN = 0;
    GlyphStyle      _style = GlyphStyle::GLYPH_BARE;
    int             _open = -1;
    SlotTapFn       _onTap = nullptr;
    uint32_t        _lastTickMs = 0, _lastSensorMs = 0;

    char    _title[40] = "";
    uint8_t _pageIndex = 0, _pageCount = 0;

    ConnectivityManager *_conn = nullptr;
    MqttManager         *_mqtt = nullptr;
    EntityRegistry      *_reg  = nullptr;
    Widget_ConnStatus    _wifi;   // the status slot's; the two fail independently
    Widget_MqttStatus    _link;
};
