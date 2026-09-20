#include "UI/Widget_MqttStatus.h"
#include "UI/UIToolkit.h"

// Palette - semantic, not themeable: these colours ARE the information, so
// they do not move when the scheme does. Matched to Widget_ConnStatus so the
// two glyphs in the header speak one language; amber means the same thing on
// both, and so does red.
//
// The one value taken straight from the artifact is the violet. The greens
// next door were brightened because mid-saturation colours wash out on these
// panels, but violet against a dark surface survives - to be re-checked on
// glass like everything else here.
static const uint32_t COL_OK    = 0xa78bfa;  // session up
static const uint32_t COL_BUSY  = 0x60a5fa;  // connecting / backing off
static const uint32_t COL_STALE = 0xfbbf24;  // connected, nothing arriving
static const uint32_t COL_BAD   = 0xf87171;  // unreachable
static const uint32_t COL_OFF   = 0x64748b;  // deliberately disabled

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

void Widget_MqttStatus::init(lv_obj_t *parent, MqttManager *mqtt) {
    _mqtt = mqtt;

    // Same reasoning as the WiFi glyph: the QSPI board pays a per-pixel
    // software rotation cost on every draw, so it gets the cheap motion.
    #ifdef CYD_S3_3248
        _motion = Motion::BLINK;
    #endif

    // The artifact draws on a 24x24 viewBox. Everything below is that geometry
    // scaled to S, so the proportions survive a change of size and the numbers
    // can still be checked against the source drawing.
    const int32_t S  = UIToolkit::sc(28);
    const int32_t lw = UIToolkit::sc(2) < 2 ? 2 : UIToolkit::sc(2);
    auto u = [S](float v) -> int32_t { return (int32_t)((v / 24.0f) * (float)S + 0.5f); };

    _root = lv_obj_create(parent);
    lv_obj_remove_style_all(_root);
    lv_obj_set_size(_root, S, S);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    // The rounded square. An outlined lv_obj rather than a drawn rectangle:
    // border + radius is one object where four lines would be four, and the
    // draw walk recurses per level of nesting (docs/design/card-layout.md
    // 1.3 - every crash in milestone 2.4 was loopTask stack depth).
    //
    // NOTE the radius is set from the artifact's rx=4.2 and NOT rounded up to
    // LV_RADIUS_CIRCLE, and clip_corner is never set. A clipped corner costs a
    // whole render layer sized by object WIDTH, which is what froze WS_P4_5 on
    // boot at the end of 2.5. This object is small, but the rule is the rule.
    _box = lv_obj_create(_root);
    lv_obj_remove_style_all(_box);
    lv_obj_set_size(_box, u(17.2f), u(17.2f));
    lv_obj_set_pos(_box, u(3.4f), u(3.4f));
    lv_obj_set_style_radius(_box, u(4.2f), 0);
    lv_obj_set_style_border_width(_box, lw, 0);
    lv_obj_set_style_bg_opa(_box, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(_box, LV_OBJ_FLAG_SCROLLABLE);

    // Two opposing arrows: publish out, subscribe back. Each is one three-point
    // polyline - shaft and half-head in a single object - which is both the
    // artifact's own path shape ("M 7.6 9.6 h 7.2 l -2.4 -2.4") and the
    // cheapest thing to draw.
    _outPts[0] = { (lv_value_precise_t)u(7.6f),  (lv_value_precise_t)u(9.6f)  };
    _outPts[1] = { (lv_value_precise_t)u(14.8f), (lv_value_precise_t)u(9.6f)  };
    _outPts[2] = { (lv_value_precise_t)u(12.4f), (lv_value_precise_t)u(7.2f)  };
    _out = lv_line_create(_root);
    lv_line_set_points(_out, _outPts, 3);
    lv_obj_set_style_line_width(_out, lw, 0);
    lv_obj_set_style_line_rounded(_out, true, 0);

    _inPts[0] = { (lv_value_precise_t)u(16.4f), (lv_value_precise_t)u(14.4f) };
    _inPts[1] = { (lv_value_precise_t)u(9.2f),  (lv_value_precise_t)u(14.4f) };
    _inPts[2] = { (lv_value_precise_t)u(11.6f), (lv_value_precise_t)u(16.8f) };
    _in = lv_line_create(_root);
    lv_line_set_points(_in, _inPts, 3);
    lv_obj_set_style_line_width(_in, lw, 0);
    lv_obj_set_style_line_rounded(_in, true, 0);

    _strikePts[0] = { (lv_value_precise_t)u(4.0f),  (lv_value_precise_t)u(20.0f) };
    _strikePts[1] = { (lv_value_precise_t)u(20.0f), (lv_value_precise_t)u(4.0f)  };
    _strike = lv_line_create(_root);
    lv_line_set_points(_strike, _strikePts, 2);
    lv_obj_set_style_line_width(_strike, lw, 0);
    lv_obj_set_style_line_rounded(_strike, true, 0);
    lv_obj_set_style_line_color(_strike, lv_color_hex(COL_OFF), 0);
    lv_obj_add_flag(_strike, LV_OBJ_FLAG_HIDDEN);

    _lastLook = -1;
    tick();   // paint the real state immediately, not a default frame
}

void Widget_MqttStatus::tint(lv_color_t col) {
    lv_obj_set_style_border_color(_box, col, 0);
    lv_obj_set_style_line_color(_out, col, 0);
    lv_obj_set_style_line_color(_in, col, 0);
}

static void mqtt_opa_anim_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

void Widget_MqttStatus::startMotion() {
    if (_motionRunning || _motion == Motion::OFF) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, _root);
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
    lv_anim_delete(_root, mqtt_opa_anim_cb);
    lv_obj_set_style_opa(_root, LV_OPA_COVER, LV_PART_MAIN);
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
    stopMotion();
    lv_obj_add_flag(_strike, LV_OBJ_FLAG_HIDDEN);

    switch (look) {
    case Look::OK:
        tint(lv_color_hex(COL_OK));
        break;

    case Look::CONNECTING:
        tint(lv_color_hex(COL_BUSY));
        startMotion();
        break;

    case Look::STALE:
        tint(lv_color_hex(COL_STALE));
        break;

    case Look::UNREACHABLE:
        tint(lv_color_hex(COL_BAD));
        break;

    case Look::NO_LINK:
        // Dimmed, static, and NO strike - the strike means a human turned this
        // off, and nobody did.
        tint(lv_color_hex(COL_OFF));
        break;

    case Look::SESSION_OFF:
    default:
        tint(lv_color_hex(COL_OFF));
        lv_obj_remove_flag(_strike, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

void Widget_MqttStatus::tick() {
    if (!_mqtt || !_root) return;

    uint32_t now = millis();
    if (now - _lastPollMs < POLL_MS) return;
    _lastPollMs = now;

    // resolveLook() walks the registry, so the change guard sits AFTER it
    // rather than before - unlike the WiFi glyph, where the inputs are three
    // atomic loads. At 2.5 Hz over at most ENTITY_MAX entries that walk is
    // cheap; reissuing the style calls and invalidating the region is what is
    // not, and that is what the guard prevents.
    Look look = resolveLook();
    if ((int32_t)look == _lastLook) return;
    _lastLook = (int32_t)look;

    applyVisual(look);
}
