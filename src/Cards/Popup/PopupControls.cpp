// The controls view (2.12 P1, split from CardPopup.cpp): the hero beside what it shows - the
// toggle and its knob, a sensor's disc, the column of words, and where the slider goes in a wide
// view. A light's slider itself is PopupLight.cpp.
#include "PopupState.h"

namespace popup {

void knobExec(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

// The switch's knob SLIDES to its new end (owner, 2026-10-04: "absolutely" on
// the control giving feedback), except when the view is first built, where it
// is simply placed. The knob is inside the window, which covers the page, so
// each frame redraws only the toggle. The animation's var is the knob, so
// LVGL deletes it with the window.
void placeKnob(int32_t y) {
    if (!s.knob) return;
    if (!s.knobPlaced) {
        lv_obj_set_y(s.knob, y);
        s.knobPlaced = true;
        s.knobTarget = y;
        return;
    }
    if (y == s.knobTarget) return;
    s.knobTarget = y;
    lv_anim_delete(s.knob, knobExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, s.knob);
    lv_anim_set_values  (&a, lv_obj_get_y(s.knob), y);
    lv_anim_set_duration(&a, KNOB_SLIDE_MS);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb (&a, knobExec);
    lv_anim_start(&a);
}

// "Changed 3m ago", "Paused", "No reading yet" - under the value, every view.
// A group with members left out says how many (owner, 2.10b): a group must
// never quietly do less than it seems to.
void renderAgo(const Agg &a) {
    if (!s.lblAgo) return;
    char buf[64];
    if (a.paused) {
        setText(s.lblAgo, "Paused");
    } else if (!a.everSet) {
        setText(s.lblAgo, "No reading yet");
    } else {
        char age[16], tail[20] = "";
        cardFormatAge(millis() - a.lastChangeMs, age, sizeof(age));
        if (a.nPaused) snprintf(tail, sizeof(tail), ", %u paused", (unsigned)a.nPaused);
        if (strcmp(age, "now") == 0) snprintf(buf, sizeof(buf), "Changed just now%s", tail);
        else                         snprintf(buf, sizeof(buf), "Changed %s ago%s", age, tail);
        setText(s.lblAgo, buf);
    }
}

// The column's first line: what it is ("Brightness"), and the PAUSED pill
// BESIDE it - on the same line, the same height, so nothing below moves when
// it comes or goes (owner, round 3: it used to sit above and push the rest down).
void makeLabelRow(lv_obj_t *col) {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    lv_obj_t *row = plain(col);
    lv_obj_set_size     (row, LV_SIZE_CONTENT, lv_font_get_line_height(t.TAG));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, mm(UI::shape().S5), 0);
    s.lblWhat = makeLabel(row, t.TAG, p.TEXT_DIM);
    s.pill = makeLabel(row, t.TAG, UI::contrastOf(p.ST_IDLE, p.GROUND, p.TEXT));
    lv_label_set_text(s.pill, "PAUSED");
    lv_obj_set_style_bg_color(s.pill, UI::c(p.ST_IDLE), 0);
    lv_obj_set_style_bg_opa  (s.pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius  (s.pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor (s.pill, mm(UI::shape().S4), 0);
    lv_obj_set_style_text_letter_space(s.pill, mm(UI::shape().TRACK_PILL), 0);
    if (!s.builtPaused) lv_obj_add_flag(s.pill, LV_OBJ_FLAG_HIDDEN);
}

// THE COLUMN BESIDE A TOGGLE OR SLIDER: as wide as THIS light's selector, or
// a fixed allowance for the words, whichever is wider - and as tall as the
// hero - and the row is centred in the window (owner, round 4, "centre by real
// width"). So a toggle and its words sit centred; a light with four or five
// controls is centred as a whole; one with two or three moves in from the
// left edge. For any one light nothing moves - not with the control showing,
// a pause, or changing words, which the allowance covers (a round 3 lesson:
// a column sized from its text moved the slider every time the text changed).
// The control deck's width (buildControlDeck()): Power on a ribbon of its own,
// a break, then the modes' ribbon.
int32_t deckBreak() { return mm(UI::shape().S6); }
int32_t selectorW(uint8_t caps) {
    const int32_t g = mm(UI::shape().S1), slot = UI::minTouch();
    uint8_t n = 0;
    if (caps & LIGHT_CAN_DIM)    n++;
    if (caps & LIGHT_CAN_TEMP)   n++;
    if (caps & LIGHT_CAN_COLOUR) n++;
    const int32_t power = slot + 2 * g;
    return n ? power + deckBreak() + n * slot + (n + 1) * g : power;
}

// `deck`: whether the control deck is in the column (beside the hero) or under
// the hero (a tall window, below), which leaves the column only its words.
int32_t colWidth(bool light, uint8_t caps, bool deck = true) {
    const UIType &t = UI::type();
    const UIShape &k = UI::shape();
    int32_t w = textW(hasMembers() ? "Changed 59m ago, 9 paused" : "Changed 59m ago", t.TAG);
    w = LV_MAX(w, textW("Unavailable", UIToolkit::Font_Hero));
    // The label line with the PAUSED pill beside it (makeLabelRow()).
    const int32_t pill = textW("PAUSED", t.TAG) + 2 * mm(k.S4) + 6 * mm(k.TRACK_PILL);
    w = LV_MAX(w, textW("Temperature", t.TAG) + mm(k.S5) + pill);
    if (light && deck) {
        w = LV_MAX(w, selectorW(caps));
        if (caps & LIGHT_CAN_COLOUR) w = LV_MAX(w, 4 * mm(k.SWATCH) + 3 * mm(k.S5) + 2 * mm(k.S1));
    }
    const int32_t room = lv_area_get_width(&s.winRect) - 2 * s.pad - s.heroW - pm(k.P_GAP);
    return LV_MIN(room, w);
}

// The stage's height: the window less its padding, the header row and the gap
// under it (buildContents()). Known before LVGL lays anything out.
int32_t stageH() {
    return lv_area_get_height(&s.winRect) - 2 * s.pad - UI::minTouch() - mm(UI::shape().S5);
}

// The control deck's height: its switches and its own padding.
int32_t controlDeckH() { return UI::minTouch() + 2 * mm(UI::shape().S1); }

void fixColumn(lv_obj_t *col, int32_t width) {
    lv_obj_set_width     (col, width);
    lv_obj_set_height    (col, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(col, s.heroH, 0);
    lv_obj_set_flex_flow (col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, mm(UI::shape().S2), 0);
}

void rebuildMainAsync(void *unused);   // below

// Called from every render: a pause that came or went since the view was
// built rebuilds it, once.
void checkPausedChange(const Agg &a) {
    if (a.paused == s.builtPaused || s.rebuildQueued) return;
    s.rebuildQueued = true;
    lv_async_call(rebuildMainAsync, nullptr);
}

void showView(PopupView v);   // below
bool pausedBlocks();          // below, with the toggle

// ---------------------------------------------------------------------------
// The main view: the hero beside what it shows (card-sheet 11.1)
// ---------------------------------------------------------------------------
void renderMain() {
    if (s.lightHero) { renderLight(); return; }
    if (!s.lblValue || !s.nEnt || !s.ent[0]) return;
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const Entity    &e = *s.ent[0];
    const Agg a = aggregate();
    char buf[48], wbuf[32];
    checkPausedChange(a);

    // --- What it is --------------------------------------------------------
    setText(s.lblWhat, s.toggleHero ? "Power" : (a.anyBool ? "State" : whatWord(e, wbuf, sizeof(wbuf))));

    // --- The value ---------------------------------------------------------
    // Words in the hero face (a built-in Montserrat the toolkit already links);
    // numbers in VALUE, the card's own digits face, with the unit beside it.
    const char *unit = "";
    if (!a.available) {
        setText(s.lblValue, "Unavailable");
        lv_obj_set_style_text_font (s.lblValue, UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.ST_BAD), 0);
    } else if (a.anyBool) {
        setText(s.lblValue, cardStateWord(e.desc, a.on));
        lv_obj_set_style_text_font (s.lblValue, UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT), 0);
    } else if (!e.everSet) {
        setText(s.lblValue, "--");
        lv_obj_set_style_text_font (s.lblValue, UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT_DIM), 0);
    } else {
        cardFormatValue(e, buf, sizeof(buf), false, s.tempUnit);
        setText(s.lblValue, buf);
        const bool numeric = (e.value.type == ValueType::INT || e.value.type == ValueType::FLOAT);
        lv_obj_set_style_text_font (s.lblValue, numeric ? t.VALUE : UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT), 0);
        if (numeric) unit = cardDisplayUnit(e, s.tempUnit);
    }
    setText(s.lblUnit, unit);
    if (unit[0]) lv_obj_clear_flag(s.lblUnit, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag  (s.lblUnit, LV_OBJ_FLAG_HIDDEN);

    // --- When --------------------------------------------------------------
    renderAgo(a);

    // --- The hero ----------------------------------------------------------
    const bool lit = a.available && a.anyBool && a.on;
    setText(s.heroIcon, cardHeroIcon(e, a.on));
    if (s.toggleHero) {
        // HA's switch dialog, stood upright: a tall track, the knob at the top
        // when on and the bottom when off. Never colour alone (the owner is a
        // little colour-blind): the knob's position says it too.
        lv_obj_set_style_bg_color(s.hero, UI::c(quiet(lit ? UI::mix(p.ST_ACTIVE, p.SURFACE_ALT, 70)
                                                          : UI::mix(p.SURFACE_ALT, p.TEXT, 10))), 0);
        lv_obj_set_style_bg_color(s.knob, UI::c(quiet(lit ? p.ST_ACTIVE
                                                          : UI::mix(p.SURFACE_ALT, p.TEXT, 25))), 0);
        placeKnob(lit ? s.knobInset : s.heroH - s.knobH - s.knobInset);
        lv_obj_set_style_text_color(s.heroIcon,
            UI::c(quiet(lit ? UI::contrastOf(p.ST_ACTIVE, p.GROUND, p.TEXT) : p.TEXT)), 0);
    } else {
        lv_obj_set_style_bg_color(s.hero, UI::c(quiet(lit ? p.ST_ACTIVE : p.SURFACE)), 0);
        lv_obj_set_style_text_color(s.heroIcon,
            UI::c(quiet(lit ? UI::contrastOf(p.ST_ACTIVE, p.GROUND, p.TEXT) : cardTintFor(e.desc))), 0);
    }
}

// Switch every primary to `on` - what a tap on the card itself does with any
// on -> all off, else all on.
void commandAll(bool on) {
    if (!s.reg) return;
    const uint32_t now = millis();
    s.lastScene = -1;   // S5, as lightSend()
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (e && e->desc.writable && !e->paused)   // a paused member is out of the group
            s.reg->commandValue(e->desc.id, EntityValue::makeBool(on), now);
    }
    renderMain();
}

// A PAUSED window's controls are greyed and do nothing (owner, 2.10b round 1);
// a touch on one says why, rather than leaving the finger wondering.
bool pausedBlocks() {
    if (!aggregate().paused) return false;
    UIToolkit::show_toast("Paused - turn it off in SETTINGS");
    return true;
}

void toggleCb(lv_event_t *ev) {
    (void)ev;
    // A drag has already decided (knobDragCb); this is the click that ends it.
    if (s.swallowClick) { s.swallowClick = false; return; }
    if (pausedBlocks()) return;
    commandAll(!aggregate().on);
}

// THE KNOB CAN BE DRAGGED (owner, round 7): a knob that slides on a tap but
// cannot be moved by a finger is "jarring". It follows the finger up and down
// the track; let go, and it goes to whichever half it is in - the rest of the
// way by the same slide a tap uses - and the switch follows it. Dragged all the
// way, or most of the way, the switch changes; less than half, the knob slides
// back. A press that does not move is a tap, and toggleCb() has it.
void knobDragCb(lv_event_t *ev) {
    if (!s.knob || !s.hero) return;
    if (aggregate().paused) return;   // greyed: the click says why (toggleCb)
    const lv_event_code_t code = lv_event_get_code(ev);
    lv_indev_t *in = lv_indev_active();
    lv_point_t p = {0, 0};
    if (in) lv_indev_get_point(in, &p);
    const int32_t topY    = s.knobInset;
    const int32_t bottomY = s.heroH - s.knobH - s.knobInset;

    if (code == LV_EVENT_PRESSED) {
        lv_anim_delete(s.knob, knobExec);
        s.dragStartY   = p.y;
        s.dragKnobY0   = lv_obj_get_y(s.knob);
        s.knobDragging = false;
        s.swallowClick = false;
    } else if (code == LV_EVENT_PRESSING) {
        const int32_t dy = p.y - s.dragStartY;
        if (!s.knobDragging && LV_ABS(dy) > mm(UI::shape().SLOP_KNOB)) s.knobDragging = true;
        if (!s.knobDragging) return;
        const int32_t y = LV_CLAMP(topY, s.dragKnobY0 + dy, bottomY);
        lv_obj_set_y(s.knob, y);
        s.knobTarget = y;   // so the next placeKnob() slides from here
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!s.knobDragging) return;
        s.knobDragging = false;
        s.swallowClick = (code == LV_EVENT_RELEASED);
        const bool wantOn = lv_obj_get_y(s.knob) + s.knobH / 2 < s.heroH / 2;   // top half = on
        if (wantOn != aggregate().on) commandAll(wantOn);
        else                          renderMain();   // slides back where it was
    }
}

// WHERE THE SLIDER GOES IN A WIDE VIEW (owner, round 7). Where the controls
// put it, whenever that works: one column of scenes, or the swatches, sit
// just beside it. Only what does not fit moves it, and only as far as it must:
//  - wider than the room to the right (extra scene columns): the slider goes
//    left by the difference;
//  - running under the clapperboard chip in the top corner: the content goes
//    DOWN first, to the bottom of the slider's height, if that clears it;
//    otherwise the slider goes left until the content clears the chip.
// The row is a flex item, centred; it is moved with translate_x, which LVGL
// applies to the coordinates (taps land where it is drawn).
void placeWide(lv_obj_t *row, lv_obj_t *col, int32_t colW, int32_t stageW) {
    lv_obj_update_layout(s.win);
    lv_area_t st;  lv_obj_get_content_coords(s.stage, &st);
    lv_area_t rw;  lv_obj_get_coords(row, &rw);
    // The stage as LVGL laid it out: the controls' row was centred in this,
    // and a width a pixel off moved the slider a pixel.
    stageW = lv_area_get_width(&st);

    const int32_t gapH  = pm(UI::shape().P_GAP);
    const int32_t rowW  = s.heroW + gapH + colW;
    const int32_t homeX = (stageW - s.mainRowW) / 2;   // the controls' slider
    int32_t x = homeX;
    if (x + rowW > stageW) x = stageW - rowW;
    if (x < 0) x = 0;

    // The content's real extent, top to bottom, from what the column holds.
    int32_t top = LV_COORD_MAX, bot = LV_COORD_MIN;
    for (uint32_t i = 0; i < lv_obj_get_child_count(col); i++) {
        lv_obj_t *c = lv_obj_get_child(col, (int32_t)i);
        if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t a; lv_obj_get_coords(c, &a);
        if (a.y2 - a.y1 < 2) continue;   // a spacer
        top = LV_MIN(top, a.y1); bot = LV_MAX(bot, a.y2);
    }
    if (s.btnScenes && !lv_obj_has_flag(s.btnScenes, LV_OBJ_FLAG_HIDDEN) && top <= bot) {
        lv_area_t chip; lv_obj_get_coords(s.btnScenes, &chip);
        const int32_t m = mm(UI::shape().S3);
        const int32_t right = st.x1 + x + s.heroW + gapH + colW;
        if (right > chip.x1 - m && top < chip.y2 + m) {
            const int32_t downTop = rw.y1 + s.heroH - (bot - top + 1);
            if (downTop >= chip.y2 + m) {
                lv_obj_set_flex_align(col, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            } else {
                x -= right - (chip.x1 - m);
                if (x < 0) x = 0;
            }
        }
    }
    lv_obj_set_style_translate_x(row, st.x1 + x - rw.x1, 0);
}

void buildMain() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const Entity    &e = *s.ent[0];

    lv_obj_t *row = plain(s.stage);
    lv_obj_set_size     (row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, pm(UI::shape().P_GAP), 0);

    // THE SAME SHARE OF THE WINDOW ON EVERY BOARD (owner, 2026-10-05). It used
    // to be the window's height less the header row and margins, which on a
    // 7" panel made a toggle twice the P4_5's (60 mm on calipers) and on the 4B
    // nearly 40 mm.
    s.heroH = pm(HERO_H_MM);
    if (s.heroH < UI::minTouch()) s.heroH = UI::minTouch();

    // D3: every writable thing opens on its control. A light that can dim, or
    // has a white range or a colour, gets its slider (2.10b - "The light's
    // controls"); a switch, an on/off light, and a light whose source has not
    // said what it can do (an HA light until 2.10c) get the big toggle, which
    // is HA's own dialog for those.
    s.builtPaused   = aggregate().paused;
    s.rebuildQueued = false;
    const bool writableLight = (e.desc.kind == EntityKind::LIGHT && e.desc.writable);
    const LightAgg LA = lightAggregate();
    const uint8_t caps = LA.caps;   // paused members offer nothing
    // The slider stays for a group whose members are all paused (greyed, no
    // controls offered): what the window IS does not change with a pause.
    s.lightHero  = writableLight && canSlide(LA.allCaps);
    s.toggleHero = !s.lightHero && e.desc.writable &&
                   (e.desc.kind == EntityKind::SWITCH || writableLight);
    if (s.lightHero) {
        // A control no counting member has any more (its lamp was paused)
        // falls back to the first one there is.
        const uint8_t bit = s.lightCtl == LCTL_DIM ? LIGHT_CAN_DIM : s.lightCtl == LCTL_TEMP ? LIGHT_CAN_TEMP
                                                                                          : LIGHT_CAN_COLOUR;
        if (!(caps & bit)) s.lightCtl = firstCtl(caps);
        s.builtCaps = caps;

        // THE CONTROL DECK UNDER THE HERO WHERE THE WINDOW HAS THE HEIGHT
        // (owner, 2026-10-07): the 4B and the 7" panels. The P4_5 is the one
        // board too short for it - its screen is the widest shape - and keeps
        // the deck beside the slider. Worked out from the window, not a list
        // of boards. The slider, its words and the deck are centred.
        //
        // SCENES, AND COLOUR UNDER A TALL WINDOW, ARE "WIDE": the buttons or
        // swatches take the column beside the slider, as wide as they need,
        // and placeWide() keeps the slider where the controls put it unless
        // they do not fit. The control deck stays exactly where it is in
        // every control.
        const bool    scenes = (s.view == PopupView::VIEW_SCENES);
        const int32_t gapV   = pm(UI::shape().P_STACK_GAP);
        const int32_t stageW = lv_area_get_width(&s.winRect) - 2 * s.pad;
        s.heroW   = s.heroH * 42 / 100;
        s.stacked = stageH() >= s.heroH + gapV + controlDeckH();
        const bool wide = scenes || (s.stacked && s.lightCtl == LCTL_COLOUR);
        lv_obj_set_style_pad_row(s.stage, s.stacked ? gapV : 0, 0);

        buildLightHero(row);
        lv_obj_t *col = plain(row);
        // Sized for everything the members CAN do, paused or not: a pause
        // hides buttons, and a narrower column re-centred the whole group
        // (owner, round 5 - All Lamps paused).
        // WIDE: the column is as wide as what it holds - the swatches, or the
        // scene buttons - and starts where the controls' column starts.
        const int32_t ctlColW = colWidth(true, LA.allCaps, !s.stacked);
        s.mainRowW = s.heroW + pm(UI::shape().P_GAP) + ctlColW;
        int32_t colW = ctlColW;
        if (wide) {
            int32_t maxW = stageW - s.heroW - pm(UI::shape().P_GAP);
            // THE THIRD WAY CLEAR OF THE CHIP (round 7, the 4B's "show all"):
            // when the scenes can neither drop below the clapperboard (their
            // rows fill the slider's height) nor clear it with the slider at
            // the edge, they get fewer columns - those that fit beside the
            // chip - and scroll. Chip and stage share a top and a right edge
            // (buildContents()), so this is known before anything is laid out.
            if (scenes && s.btnScenes && !lv_obj_has_flag(s.btnScenes, LV_OBJ_FLAG_HIDDEN)) {
                const SceneStack k = sceneStack(maxW);
                const int32_t m      = mm(UI::shape().S3);
                const int32_t chipL  = stageW - UI::minTouch() - m;
                const int32_t chipB  = UI::minTouch() + m;
                const int32_t rowTop = (stageH() - s.heroH - (s.stacked ? gapV + controlDeckH() : 0)) / 2;
                const bool downOK = rowTop + s.heroH - k.h >= chipB;
                const bool leftOK = s.heroW + pm(UI::shape().P_GAP) + k.w <= chipL;
                if (!downOK && !leftOK) maxW = chipL - s.heroW - pm(UI::shape().P_GAP);
            }
            colW = scenes ? sceneGridW(maxW)
                          : LV_MIN(maxW, 4 * mm(UI::shape().SWATCH) + 3 * mm(UI::shape().S5) + 2 * mm(UI::shape().S1));
        }
        fixColumn(col, colW);
        if (wide) lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        if (scenes) {
            buildSceneGrid(col, colW);
            // No deck in Scenes, but its room is kept, so the slider stays at
            // the same height as on the controls.
            if (s.stacked) { lv_obj_t *keep = plain(s.stage); lv_obj_set_size(keep, 1, controlDeckH()); }
        } else {
            lv_obj_t *deckParent = col;
            if (s.stacked) {
                // THE STAGE'S FULL WIDTH, the deck centred in it (L5, measured
                // on the 1060): content-sized, it was centred inside a "track"
                // as wide as the widest child, itself centred in the stage -
                // two roundings, and Color's wider row moved the deck a pixel.
                deckParent = plain(s.stage);
                lv_obj_set_size(deckParent, lv_pct(100), LV_SIZE_CONTENT);
                lv_obj_set_flex_flow(deckParent, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(deckParent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            }
            buildLightColumn(col, deckParent);
        }
        if (wide) placeWide(row, col, colW, stageW);
        renderLight();
        return;
    }
    if (s.toggleHero) {
        s.heroW     = s.heroH * 42 / 100;
        s.knobInset = s.heroW * 6 / 100;
        s.knobH     = s.heroH * 48 / 100;
        s.hero = plain(row);
        lv_obj_set_size(s.hero, s.heroW, s.heroH);
        lv_obj_set_style_radius(s.hero, s.heroW * 30 / 100, 0);
        lv_obj_set_style_bg_opa(s.hero, LV_OPA_COVER, 0);
        lv_obj_add_flag(s.hero, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s.hero, toggleCb,   LV_EVENT_CLICKED,    nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_PRESSED,    nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_PRESSING,   nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_RELEASED,   nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_PRESS_LOST, nullptr);
        s.knob = plain(s.hero);
        lv_obj_set_size(s.knob, s.heroW - 2 * s.knobInset, s.knobH);
        lv_obj_set_x   (s.knob, s.knobInset);
        lv_obj_set_style_radius(s.knob, (s.heroW - 2 * s.knobInset) * 30 / 100, 0);
        lv_obj_set_style_bg_opa(s.knob, LV_OPA_COVER, 0);
        s.heroIcon = makeLabel(s.knob, t.ICON, p.TEXT);
        lv_obj_center(s.heroIcon);
    } else {
        // A share of the hero's height, like the toggle. The icon inside is a
        // fixed face, so on a 7" panel the disc grows around the same glyph.
        const int32_t d = s.heroH * 60 / 100;
        s.heroW = s.heroH = d;
        s.hero = plain(row);
        lv_obj_set_size(s.hero, d, d);
        lv_obj_set_style_radius(s.hero, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s.hero, LV_OPA_COVER, 0);
        s.heroIcon = makeLabel(s.hero, t.ICON, p.TEXT);
        lv_obj_center(s.heroIcon);
    }

    lv_obj_t *col = plain(row);
    lv_obj_set_size     (col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, mm(UI::shape().S2), 0);
    // Never wider than what is left beside the hero, so a long text value
    // ellipsises instead of running out of the window.
    const int32_t colMax = lv_area_get_width(&s.winRect) - 2 * s.pad - s.heroW - pm(UI::shape().P_GAP);
    lv_obj_set_style_max_width(col, colMax, 0);
    // AND NEVER NARROWER THAN ITS LONGEST ORDINARY WORDS. The row is centred,
    // so a column that changed width moved the whole group - the owner's B1:
    // the first tap turned "Changed 12m ago" into "Changed just now" and the
    // hero jumped left. Sized for the longest lines it will normally show.
    int32_t colMin = textW("Changed just now", t.TAG);
    if (textW("No reading yet", t.TAG) > colMin) colMin = textW("No reading yet", t.TAG);
    if (textW("Unavailable", UIToolkit::Font_Hero) > colMin) colMin = textW("Unavailable", UIToolkit::Font_Hero);
    lv_obj_set_style_min_width(col, colMin < colMax ? colMin : colMax, 0);
    // A toggle's column is the slider's: the same size, the lines in the
    // same places (owner, round 3).
    if (s.toggleHero) fixColumn(col, colWidth(false, 0));
    s.mainRowW = s.toggleHero ? s.heroW + pm(UI::shape().P_GAP) + colWidth(false, 0) : 0;

    makeLabelRow(col);

    lv_obj_t *vrow = plain(col);
    lv_obj_set_size     (vrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(vrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(vrow, mm(UI::shape().S2), 0);
    lv_obj_set_style_max_width(vrow, colMax, 0);
    s.lblValue = makeLabel(vrow, UIToolkit::Font_Hero, p.TEXT);
    lv_label_set_long_mode(s.lblValue, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(s.lblValue, colMax, 0);
    s.lblUnit  = makeLabel(vrow, t.UNIT, p.TEXT_DIM);

    s.lblAgo = makeLabel(col, t.TAG, p.TEXT_DIM);
    if (s.toggleHero) {
        lv_label_set_long_mode(s.lblAgo, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s.lblAgo, lv_pct(100));
    }

    s.knobPlaced = false;
    renderMain();
}
} // namespace popup
