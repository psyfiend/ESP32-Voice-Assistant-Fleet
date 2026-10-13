// A light's controls (2.12 P1, split from CardPopup.cpp): the slider, what it sends, and the
// column beside it.
#include "PopupState.h"

namespace popup {

// ---------------------------------------------------------------------------
// The light's controls (2.10b; card-sheet 11.1, Card Popup Mock v3)
//
// The hero is a tall slider for whichever control is chosen below it:
// brightness (filled from the bottom, in the light's own colour), colour
// temperature (warm at the bottom, cool at the top) or colour (a hue strip).
// Beside it: what it controls, the value, when it changed, and the selector -
// Power | Brightness, Temperature, Colour - showing only what the light (or,
// for a group, ANY member) can do. Colour shows eight swatches in place of the
// value line: a hue in degrees says nothing a swatch does not.
//
// A TAP JUMPS THERE, A DRAG FOLLOWS THE FINGER (owner, 2026-10-05). While a
// finger is on it, the slider and the value follow the finger every frame,
// and a command goes out at most every LIGHT_SEND_MS - "enough so that a user
// could adjust brightness and see the level change" - plus once on release,
// with where it ended. Power is the selector's first button.
//
// A GROUP ACTS AS HA'S LIGHT GROUP (owner, 2026-10-05): each member is sent
// what it can take. A brightness to a member that only switches turns it on;
// a temperature to a member with colour but no temperature becomes the
// nearest hue (HA's conversion); a colour to a member without one turns it
// on. Levels shown are the mean over the members that are on.
//
// ONLY WHAT IS SHOWING IS BUILT: choosing another control rebuilds the view,
// so the swatches exist only in Colour (owner: memory first).
//
// Cheap to move because it is small: a drag redraws the slider and the value
// - tens of thousands of pixels - never the window.
// ---------------------------------------------------------------------------

constexpr uint32_t LIGHT_SEND_MS = 300;   // owner: "300ms sounds about right"

// Eight defaults, like HA's (card-sheet 11.2). Saving the current colour into
// one with a long press needs the settings store (2.10d).
struct Swatch { int16_t hue; int8_t sat; };
constexpr Swatch SWATCHES[8] = {
    {   0, 100 }, {  30, 100 }, {  55, 100 }, { 120,  90 },
    { 180,  90 }, { 225, 100 }, { 275,  90 }, { 320,  85 },
};

bool canSlide(uint8_t caps) { return caps & (LIGHT_CAN_DIM | LIGHT_CAN_TEMP | LIGHT_CAN_COLOUR); }

// D3: open on the control - brightness first, as HA's dialog does. Nothing
// offered (a group with every member paused): brightness, greyed.
uint8_t firstCtl(uint8_t caps) {
    if (!caps)                 return LCTL_DIM;
    if (caps & LIGHT_CAN_DIM)  return LCTL_DIM;
    if (caps & LIGHT_CAN_TEMP) return LCTL_TEMP;
    return LCTL_COLOUR;
}

// The slider's range for the control showing.
void ctlRange(const LightAgg &L, int32_t &lo, int32_t &hi) {
    switch (s.lightCtl) {
        case LCTL_DIM:  lo = 1;      hi = 100;    break;
        case LCTL_TEMP: lo = L.minK; hi = L.maxK; break;
        default:        lo = 0;      hi = 359;    break;
    }
}

// Where `v` sits on the slider: 0 at the bottom, 1000 at the top.
int32_t permilleOf(const LightAgg &L, int32_t v) {
    int32_t lo, hi;
    ctlRange(L, lo, hi);
    return LV_CLAMP(0, (v - lo) * 1000 / LV_MAX(1, hi - lo), 1000);
}

int32_t stripInset();   // below, with the strip

// The value under the finger.
int32_t valueAtFinger(const LightAgg &L) {
    lv_indev_t *indev = lv_indev_active();
    lv_point_t pt = {0, 0};
    if (indev) lv_indev_get_point(indev, &pt);
    lv_area_t a;
    lv_obj_get_coords(s.hero, &a);
    const int32_t in = stripInset();
    const int32_t h  = lv_area_get_height(&a) - 2 * in;
    const int32_t permille = LV_CLAMP(0, 1000 - (pt.y - a.y1 - in) * 1000 / LV_MAX(1, h - 1), 1000);
    int32_t lo, hi;
    ctlRange(L, lo, hi);
    int32_t v = lo + (int32_t)((int64_t)(hi - lo) * permille / 1000);
    if (s.lightCtl == LCTL_TEMP) v = (v + 5) / 10 * 10;   // 10 K: finer than any eye
    return v;
}

// Send `v` for the control showing to every member, each what it can take.
// `sat` for a swatch; the strip itself is drawn - and sent - at full colour.
void lightSend(int32_t v, int8_t sat) {
    if (!s.reg) return;
    const uint32_t now = millis();
    s.lastScene = -1;   // the light is no longer as a scene left it (S5)
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e || !e->desc.writable || e->paused) continue;
        const EntityAttrs &at = e->attrs;
        LightCommand c;
        if (s.lightCtl == LCTL_DIM) {
            if (at.lightCaps & LIGHT_CAN_DIM) c.brightness = (int16_t)LV_MAX(1, (v * 255 + 50) / 100);
        } else if (s.lightCtl == LCTL_TEMP) {
            if (at.lightCaps & LIGHT_CAN_TEMP) {
                int32_t k = v;
                if (at.minTempK) k = LV_MAX(k, (int32_t)at.minTempK);
                if (at.maxTempK) k = LV_MIN(k, (int32_t)at.maxTempK);
                c.colorTempK = (int16_t)k;
            } else if (at.lightCaps & LIGHT_CAN_COLOUR) {
                lightKelvinToHs(v, c.hue, c.sat);
            }
        } else if (at.lightCaps & LIGHT_CAN_COLOUR) {
            c.hue = (int16_t)v;
            c.sat = sat >= 0 ? sat : 100;
        }
        if (c.brightness > 0 || c.colorTempK > 0 || c.hue >= 0) {
            s.reg->commandLight(e->desc.id, c, now);
        } else if (!(e->value.type == ValueType::BOOL && e->value.b)) {
            // What it cannot take, it ignores - and is turned on (HA).
            s.reg->commandValue(e->desc.id, EntityValue::makeBool(true), now);
        }
    }
    s.sentVal = v;
    s.sentMs  = now;
}


void renderLight() {
    if (!s.hero) return;
    const UIPalette &p = UI::pal();
    const Agg      a = aggregate();
    const LightAgg L = lightAggregate();
    const bool on = a.available && a.on;
    char buf[32];
    checkPausedChange(a);
    // A member paused or resumed changes what the group offers: rebuild once.
    if (L.caps != s.builtCaps && !s.rebuildQueued) {
        s.rebuildQueued = true;
        lv_async_call(rebuildMainAsync, nullptr);
    }

    // US spelling on screen (owner). Color shows no label at all - the
    // swatches say what it is - unless the PAUSED pill needs the line.
    setText(s.lblWhat, s.lightCtl == LCTL_DIM ? "Brightness" : s.lightCtl == LCTL_TEMP ? "Temperature" : "");

    // What the slider shows: the finger while it is down, otherwise the
    // light. -1: nothing to mark (off, or the light has not said).
    int32_t v;
    if (s.sliding)                    v = s.slideVal;
    else if (!on)                     v = -1;
    else if (s.lightCtl == LCTL_DIM)  v = L.bri >= 0 ? LV_MAX(1, (L.bri * 100 + 127) / 255) : 100;
    else if (s.lightCtl == LCTL_TEMP) v = L.kelvin;
    else                              v = L.hue;

    if (s.lblValue) {
        if (!a.available) {
            setText(s.lblValue, "Unavailable");
            lv_obj_set_style_text_color(s.lblValue, UI::c(p.ST_BAD), 0);
        } else {
            // On but no temperature: it is showing a colour, and a light in a
            // colour reports no kelvin (HA the same) - so no ring, and the
            // words say why (owner, round 1, L7).
            if (v < 0)                       setText(s.lblValue, !on ? "Off"
                                                     : L.mode == LightMode::LMODE_COLOUR ? "A color" : "--");
            else if (s.lightCtl == LCTL_DIM) { snprintf(buf, sizeof(buf), "%ld%%", (long)v); setText(s.lblValue, buf); }
            else                             { snprintf(buf, sizeof(buf), "%ld K", (long)v); setText(s.lblValue, buf); }
            lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT), 0);
        }
    }
    renderAgo(a);

    // --- The slider --------------------------------------------------------
    if (s.lightCtl == LCTL_DIM && s.fill) {
        const uint32_t track = UI::mix(p.SURFACE_ALT, p.TEXT, 10);
        lv_obj_set_style_bg_color(s.hero, UI::c(quiet(track)), 0);
        // In the light's own colour when it has one that shows against the
        // track; the scheme's "on" colour otherwise.
        EntityAttrs shown;
        shown.lightMode = L.mode; shown.colorTempK = (int16_t)L.kelvin;
        shown.hue = (int16_t)L.hue; shown.sat = (int8_t)L.sat;
        uint32_t fillHex = lightShownRgb(shown);
        if (!fillHex || LV_ABS(lumOf(fillHex) - lumOf(track)) < 60) fillHex = p.ST_ACTIVE;
        fillHex = quiet(fillHex);
        // Never shorter than its own rounded end: at 1% a 3 px fill was a flat
        // line wider than the slider's rounded bottom. The value says 1%.
        const int32_t fh = v > 0 ? LV_MAX(s.heroH * v / 100, 2 * s.heroR) : 0;
        lv_obj_set_style_bg_color(s.fill, UI::c(fillHex), 0);
        lv_obj_set_y     (s.fill, s.heroH - fh);
        lv_obj_set_height(s.fill, fh);
        // The grip: a short bar near the top of the fill, where a finger takes it.
        // Small, so HIDDEN costs only its own few pixels (the HIDDEN lesson is
        // about screen-sized objects). Its x from the width it is GIVEN: read
        // back with lv_obj_get_width() before LVGL had laid it out, it was 0
        // on the first draw, and the grip sat half a slider to the right until
        // the next redraw a second later (owner, round 1).
        if (s.grip) {
            const int32_t gw = s.heroW * 34 / 100;
            lv_obj_set_style_bg_color(s.grip, UI::c(UI::contrastOf(fillHex, p.GROUND, p.TEXT)), 0);
            lv_obj_set_pos  (s.grip, (s.heroW - gw) / 2, s.heroH - fh + pm(UI::shape().P_GRIP_INSET));
            lv_obj_set_width(s.grip, gw);
            if (fh >= pm(UI::shape().P_GRIP_MIN)) lv_obj_clear_flag(s.grip, LV_OBJ_FLAG_HIDDEN);
            else             lv_obj_add_flag  (s.grip, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (s.mark) {
        // HIDDEN, not 0 wide: a 0-wide ring still drew its outline, the
        // stray vertical line beside the strip (owner, round 1, L7/L9).
        const int32_t mh = lv_obj_get_height(s.mark);
        if (v < 0) {
            lv_obj_add_flag(s.mark, LV_OBJ_FLAG_HIDDEN);
        } else {
            const int32_t in = stripInset();
            const int32_t y  = in + (1000 - permilleOf(L, v)) * (s.heroH - 2 * in) / 1000 - mh / 2;
            lv_obj_set_pos(s.mark, s.heroW * 10 / 100, LV_CLAMP(0, y, s.heroH - mh));
            lv_obj_clear_flag(s.mark, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // --- The selector ------------------------------------------------------
    // Power says whether the light is on; the chosen switch is the selector's
    // (buildControlDeck()).
    if (s.btnCtl[0])
        if (lv_obj_t *l = lv_obj_get_child(s.btnCtl[0], 0))
            lv_obj_set_style_text_color(l, UI::c(quiet(on ? p.ST_ACTIVE : p.TEXT)), 0);

    // --- The swatches: a ring round the one the light is showing -----------
    for (uint8_t i = 0; i < 8; i++) {
        if (!s.swatch[i]) continue;
        int32_t dh = LV_ABS((int32_t)SWATCHES[i].hue - L.hue) % 360;
        if (dh > 180) dh = 360 - dh;
        const bool here = on && L.mode == LightMode::LMODE_COLOUR && L.hue >= 0 && dh <= 3 &&
                          LV_ABS((int32_t)SWATCHES[i].sat - L.sat) <= 3;
        lv_obj_set_style_outline_width(s.swatch[i], here ? LV_MAX(2, mm(UI::shape().RING_W)) : 0, 0);
        lv_obj_set_style_outline_color(s.swatch[i], UI::c(p.TEXT), 0);
    }

    // --- Scenes: a ring round the one loaded from this window --------------
    // Only that: HA does not say which scene is showing, and the lights may
    // have been changed since (K15).
    for (uint8_t i = 0; i < ENTITY_SCENES_MAX; i++) {
        if (!s.sceneBtn[i]) continue;
        lv_obj_set_style_outline_width(s.sceneBtn[i], (i == s.lastScene) ? LV_MAX(2, mm(UI::shape().RING_W)) : 0, 0);
        lv_obj_set_style_outline_color(s.sceneBtn[i], UI::c(p.ACCENT), 0);
    }
}

// The finger on the slider. See the section comment for the rules.
void lightSlideCb(lv_event_t *ev) {
    if (!s.hero) return;
    const lv_event_code_t code = lv_event_get_code(ev);
    const LightAgg L = lightAggregate();
    const uint32_t now = millis();
    if (code == LV_EVENT_PRESSED) {
        if (pausedBlocks()) return;     // greyed, and says why
        s.sliding  = true;
        s.slideVal = valueAtFinger(L);
        lightSend(s.slideVal);          // a tap jumps there
        renderLight();
    } else if (code == LV_EVENT_PRESSING) {
        if (!s.sliding) return;
        const int32_t v = valueAtFinger(L);
        if (v == s.slideVal) return;
        s.slideVal = v;
        if (v != s.sentVal && now - s.sentMs >= LIGHT_SEND_MS) lightSend(v);
        renderLight();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!s.sliding) return;
        if (s.slideVal != s.sentVal) lightSend(s.slideVal);   // where it ended
        s.sliding = false;
        renderLight();
    }
}

void commandAll(bool on);   // below: what Power does

void rebuildMainAsync(void *unused) {
    (void)unused;
    s.rebuildQueued = false;
    if (s.phase == PopupPhase::PHASE_OPEN && s.stage &&
        (s.view == PopupView::VIEW_MAIN || s.view == PopupView::VIEW_MEMBER ||
         s.view == PopupView::VIEW_SCENES))
        showView(s.view);
}


void swatchCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (i >= 8 || pausedBlocks()) return;
    lightSend(SWATCHES[i].hue, SWATCHES[i].sat);
    renderLight();
}

// The strip's colours run between the rounded ends: a radius in from each.
// The brightness fill uses the whole height.
int32_t stripInset() { return s.lightCtl == LCTL_DIM ? 0 : s.heroR; }

// A strip of `n` colours, bottom to top. LVGL is built with two gradient
// stops (LV_GRADIENT_MAX_STOPS), so it is n-1 square two-stop pieces over the
// straight middle of the slider; the slider itself draws the rounded ends, in
// the end colours, flat. No clip_corner, which would cost a layer (LESSONS).
// (Round 1 of this had the end pieces carry the rounding and reach into their
// neighbours; at a radius 3/4 of a piece tall that hid most of the next hue.)
void stripPieces(const uint32_t *cols, int n) {
    const int32_t in = stripInset();
    const int32_t span = s.heroH - 2 * in;
    lv_obj_set_style_bg_opa       (s.hero, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color     (s.hero, UI::c(quiet(cols[n - 1])), 0);   // top
    lv_obj_set_style_bg_grad_color(s.hero, UI::c(quiet(cols[0])), 0);       // bottom
    lv_obj_set_style_bg_grad_dir  (s.hero, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop (s.hero, (uint8_t)(255 * in / s.heroH), 0);
    lv_obj_set_style_bg_grad_stop (s.hero, (uint8_t)(255 * (s.heroH - in) / s.heroH), 0);
    const int p = n - 1;
    for (int i = 0; i < p; i++) {
        const int32_t yTop = in + span - (int32_t)((int64_t)span * (i + 1) / p);
        const int32_t yBot = in + span - (int32_t)((int64_t)span * i / p);
        lv_obj_t *o = plain(s.hero);
        lv_obj_set_pos (o, 0, yTop);
        lv_obj_set_size(o, s.heroW, yBot - yTop);
        lv_obj_set_style_bg_opa       (o, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color     (o, UI::c(quiet(cols[i + 1])), 0);   // top
        lv_obj_set_style_bg_grad_color(o, UI::c(quiet(cols[i])), 0);       // bottom
        lv_obj_set_style_bg_grad_dir  (o, LV_GRAD_DIR_VER, 0);
    }
}

void buildLightHero(lv_obj_t *row) {
    const LightAgg L = lightAggregate();
    s.heroW = s.heroH * 42 / 100;   // the toggle's proportions
    const int32_t r = s.heroW * 30 / 100;
    s.heroR = r;
    s.hero = plain(row);
    lv_obj_set_size(s.hero, s.heroW, s.heroH);
    lv_obj_set_style_radius(s.hero, r, 0);
    lv_obj_add_flag(s.hero, LV_OBJ_FLAG_CLICKABLE);
    // A drag on the slider is the slider's: no gesture walks up from it to the
    // window or the screen, and no scroll is handed to a parent (LESSONS,
    // "Events do not reach the screen", and #68's scrolling layer).
    lv_obj_clear_flag(s.hero, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(s.hero, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_PRESSED,    nullptr);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_PRESSING,   nullptr);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_RELEASED,   nullptr);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_PRESS_LOST, nullptr);

    if (s.lightCtl == LCTL_DIM) {
        lv_obj_set_style_bg_opa(s.hero, LV_OPA_COVER, 0);
        s.fill = plain(s.hero);
        lv_obj_set_size(s.fill, s.heroW, 0);
        lv_obj_set_style_radius(s.fill, r, 0);
        lv_obj_set_style_bg_opa(s.fill, LV_OPA_COVER, 0);
        s.grip = plain(s.hero);
        lv_obj_set_size(s.grip, s.heroW * 34 / 100, LV_MAX(3, pm(UI::shape().P_GRIP_H)));
        lv_obj_add_flag(s.grip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_radius(s.grip, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s.grip, LV_OPA_COVER, 0);
        return;
    }
    if (s.lightCtl == LCTL_TEMP) {
        // The light's own range, warm at the bottom (HA's dialog the same way).
        const uint32_t cols[3] = { lightKelvinToRgb(L.minK), lightKelvinToRgb((L.minK + L.maxK) / 2),
                                   lightKelvinToRgb(L.maxK) };
        stripPieces(cols, 3);
    } else {
        uint32_t cols[7];
        for (int i = 0; i < 7; i++) cols[i] = lightHsToRgb(i * 60, 100);
        stripPieces(cols, 7);
    }
    // The marker: a white ring with a dark edge, which reads on every colour.
    s.mark = plain(s.hero);
    lv_obj_set_size(s.mark, s.heroW * 80 / 100, LV_MAX(8, pm(UI::shape().P_MARK_H)));
    lv_obj_add_flag(s.mark, LV_OBJ_FLAG_HIDDEN);   // until renderLight() places it
    lv_obj_set_style_radius      (s.mark, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s.mark, LV_MAX(2, pm(UI::shape().P_MARK_LINE)), 0);
    lv_obj_set_style_border_color(s.mark, UI::c(quiet(0xFFFFFF)), 0);
    lv_obj_set_style_outline_width(s.mark, 1, 0);   // one pixel on every board: an edge, not a line
    lv_obj_set_style_outline_color(s.mark, UI::c(0x000000), 0);
    lv_obj_set_style_outline_opa (s.mark, LV_OPA_50, 0);
}

// `deckParent`: the column itself (the control deck at its bottom, level with
// the slider's) or a row under the hero (a tall window, 2.10c).
void buildLightColumn(lv_obj_t *col, lv_obj_t *deckParent) {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const LightAgg   L = lightAggregate();
    const uint8_t    caps = L.caps;
    // Color has no label line (owner, round 6) - except when paused, for the
    // PAUSED pill. Without it the swatches and the deck fit the P4_5's column
    // with room to spare; with it the column was a few pixels too tall and
    // the deck moved down (L5).
    if (s.lightCtl != LCTL_COLOUR || s.builtPaused) makeLabelRow(col);
    if (s.lightCtl != LCTL_COLOUR) {
        s.lblValue = makeLabel(col, UIToolkit::Font_Hero, p.TEXT);
        s.lblAgo   = makeLabel(col, t.TAG, p.TEXT_DIM);
        lv_label_set_long_mode(s.lblAgo, LV_LABEL_LONG_DOT);   // "..., 3 paused" stays one line
        lv_obj_set_width(s.lblAgo, lv_pct(100));
    } else {
        // 6.5 mm: with the label and the selector they fill the P4_5's 30 mm
        // column exactly, so the selector stays where it is in every control.
        const UIShape &k = UI::shape();
        const int32_t sz = mm(k.SWATCH), gap = mm(k.S5);
        lv_obj_t *grid = plain(col);
        lv_obj_set_size     (grid, 4 * sz + 3 * gap + 2 * mm(k.S1), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_all   (grid, mm(k.S1), 0);   // room for the ring
        lv_obj_set_style_pad_column(grid, gap, 0);
        lv_obj_set_style_pad_row   (grid, gap, 0);
        for (uint8_t i = 0; i < 8; i++) {
            lv_obj_t *w = plain(grid);
            lv_obj_set_size(w, sz, sz);
            lv_obj_set_style_radius      (w, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa      (w, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color    (w, UI::c(quiet(lightHsToRgb(SWATCHES[i].hue, SWATCHES[i].sat))), 0);
            lv_obj_set_style_border_width(w, LV_MAX(1, mm(k.LINE_HAIR)), 0);
            lv_obj_set_style_border_color(w, UI::border(), 0);
            lv_obj_set_style_outline_pad (w, LV_MAX(2, mm(k.RING_PAD)), 0);
            lv_obj_add_flag(w, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(w, swatchCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            s.swatch[i] = w;
        }
    }

    // The selector sits at the BOTTOM of the column, level with the bottom of
    // the slider, whatever is above it (owner, round 3).
    if (deckParent == col) {
        lv_obj_t *spacer = plain(col);
        lv_obj_set_size(spacer, 1, 0);
        lv_obj_set_flex_grow(spacer, 1);
    }

    // Power | the controls this light (any member) has, with a divider after
    // Power as in HA. A light with only one of them still shows it, so the
    // control deck always says what the slider is.
    buildControlDeck(deckParent, caps);
}

} // namespace popup
