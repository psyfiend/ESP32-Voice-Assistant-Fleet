#include "UI/Widget_ConnStatus.h"
#include "UI/UIToolkit.h"

// Colours - semantic: these ARE the information, and since 2.8 round 3 they
// are the scheme's own (UIPalette SIG_*, D-27). History: the greens were once
// brighter than the "Fleet Status Glyphs" spec (00FF00 - the spec's looked
// washed out on the 7B); the owner chose the spec's softer pair again on the
// round-3 canvas, and Linen has a darker set of its own.

static const uint32_t POLL_MS = 400;   // 2.5 Hz is plenty for a status icon

using Glyphs::WIFI_ARC1;
using Glyphs::WIFI_DOT;
using Glyphs::WIFI_STALK;

void Widget_ConnStatus::init(lv_obj_t *parent, ConnectivityManager *mgr, GlyphStyle style, int32_t px) {
    _mgr   = mgr;
    _style = style;
    // A fresh start: the header rebuilds its slots, and the old objects (and
    // any pulse running on them) went with the old slot.
    _motionRunning = false;
    _lastKey       = -1;
    _lastPollMs    = 0;

    // The QSPI board pays a per-pixel software rotation cost on every draw, so
    // it gets the cheap motion by default. See Motion in the header.
    #ifdef CYD_S3_3248
        _motion = Motion::BLINK;
    #endif

    // THE SAME SIZE IN EVERY STYLE (D-24): the frame is a little larger than
    // the glyph; it never makes the glyph smaller.
    const int32_t S = px > 0 ? px : UIToolkit::sc(UI::hdr().GLYPH);
    _frame = Kit::glyphFrame(parent, style);
    lv_obj_remove_flag(_frame, LV_OBJ_FLAG_CLICKABLE);   // touches are the slot's
    Kit::strokeGlyph(_g, _frame, Glyphs::Shape::WIFI, S);
    lv_obj_center(_g.box);

    _lastKey = -1;
    tick();   // paint the real state immediately, not a default frame
}

lv_color_t Widget_ConnStatus::glyphCol(lv_color_t state) {
    if (_style != GlyphStyle::GLYPH_CIRCLE) return state;
    lv_obj_set_style_bg_color(_frame, state, 0);
    return UI::c(UI::pal().SURFACE_ALT);
}

// A strike's halo is the ground the glyph stands on: the bar, the square's
// shade, or the circle's colour.
lv_color_t Widget_ConnStatus::groundCol(lv_color_t state) const {
    const UIPalette &p = UI::pal();
    switch (_style) {
        case GlyphStyle::GLYPH_CIRCLE: return state;
        case GlyphStyle::GLYPH_SQUARE: return UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, UI::hdr().FRAME_MIX));
        default:                       return UI::c(p.SURFACE_ALT);
    }
}

void Widget_ConnStatus::setArcs(int litCount, uint32_t col) {
    // In a circle the unlit arcs are the lit ones' colour, faded: grey on a
    // coloured disc would read as a second state.
    const bool       circle = _style == GlyphStyle::GLYPH_CIRCLE;
    const lv_color_t c      = glyphCol(UI::c(col));
    for (int i = 0; i < 3; i++) {
        const bool lit = i < litCount;
        Kit::strokeGlyphPaint(_g, (uint8_t)(WIFI_ARC1 + i), lit || circle ? c : UI::c(UI::pal().SIG_OFF),
                              lit ? LV_OPA_COVER : circle ? LV_OPA_40 : LV_OPA_50);
    }
    Kit::strokeGlyphPaint(_g, WIFI_DOT, c);
    Kit::strokeGlyphPaint(_g, WIFI_STALK, c);
    Kit::strokeGlyphShow(_g, WIFI_STALK, litCount == 0);
}

static void opa_anim_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

void Widget_ConnStatus::startMotion() {
    if (_motionRunning || _motion == Motion::OFF) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _g.box);
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
    lv_anim_delete(_g.box, opa_anim_cb);
    lv_obj_set_style_opa(_g.box, LV_OPA_COVER, LV_PART_MAIN);
    _motionRunning = false;
}

void Widget_ConnStatus::setMotion(Motion m) {
    if (_motion == m) return;
    bool wasRunning = _motionRunning;
    stopMotion();
    _motion = m;
    if (wasRunning) startMotion();
}

void Widget_ConnStatus::setArcsUnknown(uint32_t col) {
    // Connected, strength unknown. Every arc ghosted, dot solid: the dot says
    // "there is a link", the ghosted arcs say "how strong is not known".
    const lv_color_t c = glyphCol(UI::c(col));
    for (int i = 0; i < 3; i++) Kit::strokeGlyphPaint(_g, (uint8_t)(WIFI_ARC1 + i), c, LV_OPA_30);
    Kit::strokeGlyphPaint(_g, WIFI_DOT, c);
    Kit::strokeGlyphShow(_g, WIFI_STALK, false);
}

void Widget_ConnStatus::applyVisual(ConnState st, SignalBand band, LinkHealth health, bool retrying) {
    const UIPalette &p = UI::pal();
    // Always reset the transient bits first, so no previous state can leak
    // through into the next one.
    stopMotion();
    Kit::strokeGlyphStrike(_g, false, UI::c(p.SIG_OFF), UI::c(p.SIG_OFF));

    // Issue #49 - THE OVERRIDE, and it comes before the switch on purpose.
    //
    // A board whose link has been confirmed dead is still sitting in
    // STA_CONNECTED as far as the driver is concerned, so the switch below
    // would paint it green with full arcs. That is the precise lie this whole
    // change exists to stop: full bars for six hours while nothing on the LAN
    // could reach the panel. The evidence outranks the driver's own story.
    if (health == LinkHealth::LINK_DEAD &&
        (st == ConnState::STA_CONNECTED || st == ConnState::APSTA)) {
        // Bare stalk, red - identical to a frank disconnect, because that is
        // what it is. The device just has not been told yet.
        setArcs(0, p.SIG_BAD);
        return;
    }

    switch (st) {
    case ConnState::RADIO_OFF: {
        setArcs(3, p.SIG_OFF);
        const lv_color_t c = glyphCol(UI::c(p.SIG_OFF));
        Kit::strokeGlyphStrike(_g, true, c, groundCol(UI::c(p.SIG_OFF)));
        break;
    }

    case ConnState::BOOT:
        setArcs(0, p.SIG_OFF);
        break;

    case ConnState::STA_CONNECTING:
        // Two readings of the same state: a first join is blue and unworried;
        // a board on its second or third try has been failing, and the amber
        // says so (the panel's CONNECTIVITY row says how often). Both pulse -
        // motion still means "working".
        setArcs(2, retrying ? p.SIG_WEAK : p.SIG_BUSY);
        startMotion();
        break;

    case ConnState::STA_CONNECTED:
    case ConnState::APSTA: {
        // Strength unknown - the RSSI sample has aged out. Only reachable when
        // the periodic re-read has been failing, which is itself informative,
        // so it gets its own silhouette rather than being folded into "weak".
        if (band == SignalBand::NONE) {
            setArcsUnknown(health == LinkHealth::LINK_SUSPECT ? p.SIG_WEAK : p.SIG_GOOD);
            break;
        }
        int      lit = 1;
        uint32_t c   = p.SIG_WEAK;
        switch (band) {
            case SignalBand::EXCELLENT: lit = 3; c = p.SIG_STRONG; break;
            case SignalBand::GOOD:      lit = 2; c = p.SIG_GOOD;   break;
            case SignalBand::FAIR:      lit = 1; c = p.SIG_WEAK;   break;
            default:                    lit = 1; c = p.SIG_BAD;    break;
        }
        // A link under suspicion keeps its arc COUNT - the radio really is
        // hearing the AP that well - but loses its colour. The count is a
        // measurement and still true; the green is a reassurance and no longer
        // earned.
        if (health == LinkHealth::LINK_SUSPECT) c = p.SIG_WEAK;
        setArcs(lit, c);
        break;
    }

    case ConnState::AP_ACTIVE:
        // Hosting the setup AP with no network joined: no link (the stalk), in
        // amber, not red - provisioning is a working state, not a fault. The
        // AP mast beside it says why.
        setArcs(0, p.SIG_WEAK);
        break;

    case ConnState::DEGRADED:
    default:
        // Bare stalk, no arcs - the absence of arcs is the message.
        setArcs(0, p.SIG_BAD);
        break;
    }
}

void Widget_ConnStatus::repaint() {
    _lastKey = -1;
    _lastPollMs = 0;
    tick();
}

void Widget_ConnStatus::tick() {
    if (!_mgr || !_frame) return;

    uint32_t now = millis();
    if (now - _lastPollMs < POLL_MS) return;
    _lastPollMs = now;

    ConnState  st       = _mgr->getState();
    SignalBand band     = _mgr->getSignalBand();
    LinkHealth health   = _mgr->getLinkHealth();
    bool       retrying = _mgr->isRetrying();

    // Change guard. Without it every poll would reissue style calls and
    // invalidate the icon region 2.5x/second forever - the redraw-scoping trap
    // from the roadmap's gotcha list. Health and retrying are IN the key
    // because they are in the visual.
    int32_t key = ((int32_t)st << 12) | ((int32_t)band << 8) |
                  ((int32_t)health << 4) | (retrying ? 1 : 0);
    if (key == _lastKey) return;
    _lastKey = key;

    applyVisual(st, band, health, retrying);
}
