#include "Widget_ConnStatus.h"
#include "UIToolkit.h"

// Palette - semantic, not themeable: these colours ARE the information.
//
// The greens are brighter than the original spec's #34d399/#4ade80. Those read
// as washed out on the WS_P4_7B panel in person - these displays have poor
// gamma and mid-saturation colours lose a lot of punch, which a mockup on a
// desktop monitor cannot show you. Matched to the pure green the old dummy
// icon used, which was legible. Tune per-board if another panel disagrees.
static const uint32_t COL_STRONG  = 0x00ff00;  // excellent
static const uint32_t COL_GOOD    = 0x2ee65a;  // good
static const uint32_t COL_WEAK    = 0xfbbf24;  // marginal / AP mode
static const uint32_t COL_BAD     = 0xf87171;  // no link / degraded
static const uint32_t COL_OFF     = 0x64748b;  // deliberately disabled
static const uint32_t COL_BUSY    = 0x60a5fa;  // connecting

// Arc sweep. LVGL angles run clockwise from 3 o'clock, so 210->330 passes
// through 270 (12 o'clock) and gives an upward-opening fan.
static const int32_t ARC_START = 210;
static const int32_t ARC_END   = 330;

static const uint32_t POLL_MS = 400;   // 2.5 Hz is plenty for a status icon

void Widget_ConnStatus::init(lv_obj_t *parent, ConnectivityManager *mgr) {
    _mgr = mgr;

    // The QSPI board pays a per-pixel software rotation cost on every draw, so
    // it gets the cheap motion by default. See Motion in the header.
    #ifdef CYD_S3_3248
        _motion = Motion::BLINK;
    #endif

    const int32_t S   = UiToolkit::sc(28);          // fan bounding box
    const int32_t cx  = S / 2;
    const int32_t cy  = (S * 4) / 5;                 // fan origin sits low
    const int32_t aw  = UiToolkit::sc(2) < 2 ? 2 : UiToolkit::sc(2);

    _root = lv_obj_create(parent);
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_root, 0, 0);
    // Touches belong to the header's hotspot wrapper, not to the glyph.
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    _fan = lv_obj_create(_root);
    lv_obj_remove_style_all(_fan);
    lv_obj_set_size(_fan, S, S);
    lv_obj_remove_flag(_fan, LV_OBJ_FLAG_SCROLLABLE);

    // Three concentric arcs, all centred on (cx, cy). Each lv_arc is centred in
    // its own box, so aligning by top-left with a half-size offset puts every
    // arc's centre on the same point. Only the upper sweep is drawn, so the
    // parts that fall below the container are never rendered.
    const int32_t size[3] = { (S * 2) / 5, (S * 7) / 10, S };   // inner -> outer
    for (int i = 0; i < 3; i++) {
        lv_obj_t *a = lv_arc_create(_fan);
        lv_obj_set_size(a, size[i], size[i]);
        lv_obj_set_pos(a, cx - size[i] / 2, cy - size[i] / 2);
        lv_arc_set_bg_angles(a, ARC_START, ARC_END);
        lv_obj_set_style_arc_width(a, aw, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
        // An lv_arc is a control by default; strip the knob and the value
        // indicator so only the background sweep paints.
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
        _arc[i] = a;
    }

    // Optical centring nudge. lv_arc centres its stroke on the arc radius, so
    // the visual centre of the fan sits slightly above and right of the
    // geometric origin; the dot needs to come back down and left to look
    // centred. Confirmed by eye on both the 7B and the 3248.
    const int32_t nx = -UiToolkit::sc(1);
    const int32_t ny =  UiToolkit::sc(2);

    const int32_t dotD = UiToolkit::sc(5) < 4 ? 4 : UiToolkit::sc(5);
    _dot = lv_obj_create(_fan);
    lv_obj_remove_style_all(_dot);
    lv_obj_set_size(_dot, dotD, dotD);
    lv_obj_set_pos(_dot, cx - dotD / 2 + nx, cy - dotD / 2 + ny);
    lv_obj_set_style_radius(_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(_dot, LV_OPA_COVER, 0);

    // Bare antenna stalk: shown only when zero arcs are lit, so "radio on, no
    // link" is a distinct silhouette rather than just a recoloured full fan.
    _stalk = lv_obj_create(_fan);
    lv_obj_remove_style_all(_stalk);
    lv_obj_set_size(_stalk, aw, UiToolkit::sc(7));
    lv_obj_set_pos(_stalk, cx - aw / 2 + nx, cy - UiToolkit::sc(7) + ny);
    lv_obj_set_style_bg_opa(_stalk, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_stalk, aw, 0);
    lv_obj_add_flag(_stalk, LV_OBJ_FLAG_HIDDEN);

    _strikePts[0] = { (lv_value_precise_t)(S / 6),       (lv_value_precise_t)(S - S / 6) };
    _strikePts[1] = { (lv_value_precise_t)(S - S / 6),   (lv_value_precise_t)(S / 6) };
    _strike = lv_line_create(_fan);
    lv_line_set_points(_strike, _strikePts, 2);
    lv_obj_set_style_line_width(_strike, aw, 0);
    lv_obj_set_style_line_rounded(_strike, true, 0);
    lv_obj_set_style_line_color(_strike, lv_color_hex(COL_OFF), 0);
    lv_obj_add_flag(_strike, LV_OBJ_FLAG_HIDDEN);

    // Badge is its own label rather than text appended to the glyph: it needs
    // an independent colour, and mixing sizes inside one label renders badly.
    _badge = lv_label_create(_root);
    lv_label_set_text(_badge, "");
    lv_obj_set_style_text_font(_badge, UiToolkit::Font_Caption, 0);

    _lastKey = -1;
    tick();   // paint the real state immediately, not a default frame
}

void Widget_ConnStatus::setArcs(int litCount, lv_color_t col, bool glow) {
    for (int i = 0; i < 3; i++) {
        bool lit = (i < litCount);
        lv_obj_set_style_arc_color(_arc[i], lit ? col : lv_color_hex(COL_OFF), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(_arc[i], lit ? LV_OPA_COVER : LV_OPA_20, LV_PART_MAIN);
    }
    lv_obj_set_style_bg_color(_dot, col, 0);
    lv_obj_set_style_bg_color(_stalk, col, 0);
    if (litCount == 0) lv_obj_remove_flag(_stalk, LV_OBJ_FLAG_HIDDEN);
    else               lv_obj_add_flag(_stalk, LV_OBJ_FLAG_HIDDEN);
    (void)glow;   // reserved: LVGL has no cheap outer-glow; revisit with the
                  // MDI icon font in Phase 2.2 rather than faking it now.
}

static void opa_anim_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

void Widget_ConnStatus::startMotion() {
    if (_motionRunning || _motion == Motion::OFF) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _fan);
    lv_anim_set_exec_cb(&a, opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    if (_motion == Motion::BLINK) {
        // Step between two opacities instead of ramping: LVGL still redraws on
        // each step, but there are two steps a second rather than one a frame.
        lv_anim_set_duration(&a, 500);
        lv_anim_set_playback_duration(&a, 500);
        lv_anim_set_path_cb(&a, lv_anim_path_step);
    } else {
        lv_anim_set_duration(&a, 700);
        lv_anim_set_playback_duration(&a, 700);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    }
    lv_anim_start(&a);
    _motionRunning = true;
}

void Widget_ConnStatus::stopMotion() {
    if (!_motionRunning) return;
    lv_anim_delete(_fan, opa_anim_cb);
    lv_obj_set_style_opa(_fan, LV_OPA_COVER, LV_PART_MAIN);
    _motionRunning = false;
}

void Widget_ConnStatus::setMotion(Motion m) {
    if (_motion == m) return;
    bool wasRunning = _motionRunning;
    stopMotion();
    _motion = m;
    if (wasRunning) startMotion();
}

void Widget_ConnStatus::applyVisual(ConnState st, SignalBand band, bool apUp) {
    // Always reset the transient bits first, so no previous state can leak
    // through into the next one.
    stopMotion();
    lv_obj_add_flag(_strike, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_badge, "");

    switch (st) {
    case ConnState::RADIO_OFF:
        setArcs(3, lv_color_hex(COL_OFF), false);
        lv_obj_remove_flag(_strike, LV_OBJ_FLAG_HIDDEN);
        break;

    case ConnState::BOOT:
        setArcs(0, lv_color_hex(COL_OFF), false);
        break;

    case ConnState::STA_CONNECTING:
        setArcs(2, lv_color_hex(COL_BUSY), false);
        startMotion();
        break;

    case ConnState::STA_CONNECTED:
    case ConnState::APSTA: {
        int      lit = 1;
        uint32_t c   = COL_WEAK;
        switch (band) {
            case SignalBand::EXCELLENT: lit = 3; c = COL_STRONG; break;
            case SignalBand::GOOD:      lit = 2; c = COL_GOOD;   break;
            case SignalBand::FAIR:      lit = 1; c = COL_WEAK;   break;
            default:                    lit = 1; c = COL_BAD;    break;
        }
        setArcs(lit, lv_color_hex(c), band == SignalBand::EXCELLENT);
        if (st == ConnState::APSTA) {
            // Green arcs, amber badge: each half of the link reports itself.
            lv_label_set_text(_badge, "AP");
            lv_obj_set_style_text_color(_badge, lv_color_hex(COL_WEAK), 0);
        }
        break;
    }

    case ConnState::AP_ACTIVE:
        // Amber, not red: provisioning is a working state, not a fault.
        setArcs(3, lv_color_hex(COL_WEAK), false);
        lv_label_set_text(_badge, "AP");
        lv_obj_set_style_text_color(_badge, lv_color_hex(COL_WEAK), 0);
        break;

    case ConnState::DEGRADED:
    default:
        // Bare stalk, no arcs - the absence of arcs is the message.
        setArcs(0, lv_color_hex(COL_BAD), false);
        break;
    }
    (void)apUp;
}

void Widget_ConnStatus::tick() {
    if (!_mgr || !_root) return;

    uint32_t now = millis();
    if (now - _lastPollMs < POLL_MS) return;
    _lastPollMs = now;

    ConnState  st   = _mgr->getState();
    SignalBand band = _mgr->getSignalBand();
    bool       apUp = _mgr->isApActive();

    // Change guard. Without it every poll would reissue style calls and
    // invalidate the icon region 2.5x/second forever - the redraw-scoping trap
    // from the roadmap's gotcha list.
    int32_t key = ((int32_t)st << 8) | ((int32_t)band << 4) | (apUp ? 1 : 0);
    if (key == _lastKey) return;
    _lastKey = key;

    applyVisual(st, band, apUp);
}
