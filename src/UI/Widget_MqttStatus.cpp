#include "UI/Widget_MqttStatus.h"
#include "UI/UIToolkit.h"

// Colours - semantic: these ARE the information. Since 2.8 round 3 they are
// the scheme's own (UIPalette SIG_*, D-27), shared with the Wi-Fi glyph so the
// two speak one language: amber means the same thing on both, and so does red.
// The violet is the "Fleet Status Glyphs" mock's.

static const uint32_t POLL_MS = 400;   // matched to the WiFi glyph

// Connected, but NOTHING has arrived on ANY subscribed topic for this long.
//
// This is a statement about the PIPE and nothing else. The owner's rule,
// 2026-09-18: this icon "should never reflect the status of individual
// entities and the conditions for it turning orange should be entirely
// independent from what makes a card go STALE."
//
// Two earlier attempts got this wrong in the same direction and are worth
// recording so a third does not:
//
//   1. A blanket ten minutes measured against the newest ENTITY update. Wrong
//      within two hours of reaching hardware - two boards sat amber all
//      evening because his outdoor Zigbee sensors are simply quiet.
//   2. Asking EntityRegistry::isStale() per entity instead. Better, and still
//      wrong by the rule above: that is literally the call that makes a card
//      grey out, so the header would have been a second opinion on the same
//      question rather than a different question.
//
// Half an hour, because it is not trying to catch a quiet sensor - the cards
// already do that, in place, far better. It is trying to catch a session that
// is nominally CONNECTED and receiving nothing at all, which is a fault no
// card can show because every card would be stale at once and none of them
// would say why.
static const uint32_t FEED_SILENT_MS = 30UL * 60UL * 1000UL;

void Widget_MqttStatus::init(lv_obj_t *parent, MqttManager *mqtt, GlyphStyle style, int32_t px) {
    _mqtt  = mqtt;
    _style = style;
    // A fresh start, as the Wi-Fi glyph's (the header rebuilds its slots).
    _motionRunning = false;
    _lastLook      = -1;
    _lastPollMs    = 0;

    // Same reasoning as the WiFi glyph: the QSPI board pays a per-pixel
    // software rotation cost on every draw, so it gets the cheap motion.
    #ifdef CYD_S3_3248
        _motion = Motion::BLINK;
    #endif

    // The two arrows (UI/GlyphShapes.h), THE SAME SIZE in every style (D-24).
    // No square of their own any more: a frame is the square, and bare they
    // are larger for it.
    const int32_t S = px > 0 ? px : UIToolkit::sc(UI::hdr().GLYPH);
    _frame = Kit::glyphFrame(parent, style);
    lv_obj_remove_flag(_frame, LV_OBJ_FLAG_CLICKABLE);   // touches are the slot's
    Kit::strokeGlyph(_g, _frame, Glyphs::Shape::LINK, S);
    lv_obj_center(_g.box);

    _lastLook = -1;
    tick();   // paint the real state immediately, not a default frame
}

lv_color_t Widget_MqttStatus::glyphCol(lv_color_t state) {
    if (_style != GlyphStyle::GLYPH_CIRCLE) return state;
    lv_obj_set_style_bg_color(_frame, state, 0);
    return UI::c(UI::pal().SURFACE_ALT);
}

void Widget_MqttStatus::tint(uint32_t col, bool struck) {
    const UIPalette &p = UI::pal();
    const lv_color_t state = UI::c(col);
    const lv_color_t c = glyphCol(state);
    Kit::strokeGlyphPaint(_g, Kit::ALL_PARTS, c);
    // The strike's halo is the ground it stands on (the bar, the square's
    // shade, the circle's colour).
    const lv_color_t halo = _style == GlyphStyle::GLYPH_CIRCLE ? state
                          : _style == GlyphStyle::GLYPH_SQUARE ? UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, UI::hdr().FRAME_MIX))
                          : UI::c(p.SURFACE_ALT);
    Kit::strokeGlyphStrike(_g, struck, c, halo);
}

void Widget_MqttStatus::repaint() {
    _lastLook = -1;
    _lastPollMs = 0;
    tick();
}

static void mqtt_opa_anim_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

void Widget_MqttStatus::startMotion() {
    if (_motionRunning || _motion == Motion::OFF) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _g.box);
    lv_anim_set_exec_cb(&a, mqtt_opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    if (_motion == Motion::BLINK) {
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

void Widget_MqttStatus::stopMotion() {
    if (!_motionRunning) return;
    lv_anim_delete(_g.box, mqtt_opa_anim_cb);
    lv_obj_set_style_opa(_g.box, LV_OPA_COVER, LV_PART_MAIN);
    _motionRunning = false;
}

void Widget_MqttStatus::setMotion(Motion m) {
    if (_motion == m) return;
    bool wasRunning = _motionRunning;
    stopMotion();
    _motion = m;
    if (wasRunning) startMotion();
}

Widget_MqttStatus::Look Widget_MqttStatus::resolveLook() const {
    if (!_mqtt) return Look::SESSION_OFF;

    switch (_mqtt->getState()) {
        case MqttState::SESSION_OFF:
            return Look::SESSION_OFF;

        case MqttState::AUTH_STOPPED:
            // The broker actively refused us, which is a fault and not a
            // setting - so it is red, NOT a strike, however much "we have
            // stopped trying" sounds like one. The artifact is explicit:
            // distinguish auth failure in the detail panel, not the glyph.
            return Look::UNREACHABLE;

        case MqttState::CONNECTING:
        case MqttState::BACKOFF:
            return Look::CONNECTING;

        case MqttState::NO_LINK:
            // NOT "connecting", which is what this used to say and what the
            // owner saw: "the MQTT icon frequently switches from softly
            // blinking (meaning trying to connect?) and stuck in a dimmed
            // state". It was blinking because it was being drawn as busy, and
            // it is not busy - MqttManager::loop() returns immediately while
            // the link is down and makes no attempts at all.
            //
            // Motion means TRANSIENT and "working on it"; animating here
            // claimed effort that is not happening. Not red either: the WiFi
            // glyph beside it already reports the real fault with more detail,
            // and two red icons for one problem sends the reader hunting for a
            // broker failure that does not exist. Dimmed and still is the
            // honest reading - "nothing to say until the link comes back".
            return Look::NO_LINK;

        case MqttState::CONNECTED:
            break;
    }

    // The session is up. The remaining question is whether the PIPE is
    // carrying anything - and that is a question about the pipe, not about any
    // entity on the other end of it.
    const uint32_t since = _mqtt->msSinceLastInbound();

    // Nothing has ever arrived. Not a fault: a board may subscribe to nothing
    // at all, and Zigbee2MQTT does not retain state topics, so a quiet sensor
    // legitimately says nothing for a long while after boot. There is no
    // baseline here to call anything stale against.
    if (since == UINT32_MAX) return Look::OK;

    return (since > FEED_SILENT_MS) ? Look::STALE : Look::OK;
}

void Widget_MqttStatus::applyVisual(Look look) {
    const UIPalette &p = UI::pal();
    stopMotion();

    switch (look) {
    case Look::OK:
        tint(p.SIG_LINK);
        break;

    case Look::CONNECTING:
        tint(p.SIG_BUSY);
        startMotion();
        break;

    case Look::STALE:
        tint(p.SIG_WEAK);
        break;

    case Look::UNREACHABLE:
        tint(p.SIG_BAD);
        break;

    case Look::NO_LINK:
        // Dimmed, static, and NO strike - the strike means a human turned this
        // off, and nobody did.
        tint(p.SIG_OFF);
        break;

    case Look::SESSION_OFF:
    default:
        tint(p.SIG_OFF, true);
        break;
    }
}

void Widget_MqttStatus::tick() {
    if (!_mqtt || !_frame) return;

    uint32_t now = millis();
    if (now - _lastPollMs < POLL_MS) return;
    _lastPollMs = now;

    // resolveLook() is cheap; reissuing the style calls and invalidating the
    // region is not, and that is what the guard prevents.
    Look look = resolveLook();
    if ((int32_t)look == _lastLook) return;
    _lastLook = (int32_t)look;

    applyVisual(look);
}
