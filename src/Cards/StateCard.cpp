#include "Cards/StateCard.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <stdio.h>

StateCardFill StateCard::s_fill = StateCardFill::FILL_SURFACE;

static lv_obj_t *plainCol(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_style_bg_opa       (o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (o, 0, 0);
    lv_obj_set_style_pad_all      (o, 0, 0);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void StateCard::buildBody(lv_obj_t *body) {
    // Same vertical stack as ValueCard, and for the same reason: the disc used
    // to be centred with a hand-tuned offset and the name aligned to the
    // bottom, which left a gap the owner described as the icon being too high
    // and the name too far from it. A column with the disc in a growing middle
    // puts them together and centres the pair.
    lv_obj_set_flex_flow (body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(body, 0, 0);

    // THE SAME THREE BANDS ValueCard uses, reserved even though this layout
    // has nothing to put in either of them yet. It is what makes a state card
    // and a value card side by side put their names at the same height.
    //
    // The top band is also where a small corner icon goes when that lands -
    // the owner wants one on every card, including those that already show a
    // large one in the middle. The space is already here for it.
    _topRow = plainCol(body);
    lv_obj_set_width (_topRow, lv_pct(100));
    lv_obj_set_height(_topRow, Card::topBandHeight());

    _mid = plainCol(body);
    lv_obj_set_width     (_mid, lv_pct(100));
    lv_obj_set_flex_grow (_mid, 1);
    lv_obj_set_flex_flow (_mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(_mid, Card::midGap(), 0);

    // A plain object with a full radius rather than an arc or an image: it is
    // a circle behind a glyph, and the cheapest thing that draws a circle in
    // LVGL is a square whose radius is set past half its side.
    _disc = lv_obj_create(_mid);
    lv_obj_clear_flag             (_disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_disc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width (_disc, 0, 0);
    lv_obj_set_style_pad_all      (_disc, 0, 0);

    _icon = lv_label_create(_disc);
    lv_obj_center(_icon);

    _name = lv_label_create(body);
    lv_label_set_long_mode     (_name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width           (_name, lv_pct(100));

    _statusRow = plainCol(body);
    lv_obj_set_width (_statusRow, lv_pct(100));
    lv_obj_set_height(_statusRow, Card::statusBandHeight());

    // The not-uniform badge. Top-LEFT, out of the flow, because HDR_NONE parks
    // its floating STALE badge in the top-right corner and two things fighting
    // over one corner is a collision you only find on a board.
    _mixed = lv_label_create(body);
    lv_obj_add_flag(_mixed, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align   (_mixed, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(_mixed, LV_OBJ_FLAG_HIDDEN);
}

uint8_t StateCard::activeCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < primaryCount(); i++) {
        const Entity *e = primary(i);
        if (!e || !e->everSet) continue;
        // A state entity is a bool. Anything numeric that reached an actor
        // card - a dimmable light reporting brightness - is on when nonzero,
        // which is the same question asked of a different type.
        switch (e->value.type) {
            case ValueType::BOOL:  if (e->value.b)        n++; break;
            case ValueType::INT:   if (e->value.i != 0)   n++; break;
            case ValueType::FLOAT: if (e->value.f != 0.f) n++; break;
            default: break;
        }
    }
    return n;
}

void StateCard::render() {
    const Entity *e = primary();
    if (!e) return;

    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();

    const uint8_t total  = primaryCount();
    const uint8_t active = activeCount();
    const bool    isOn    = active > 0;
    const bool    isMixed = active > 0 && active < total;

    // --- Disc geometry, derived from the type scale -----------------------
    // ICON is now a FACE rather than a logical pixel count, because the type
    // scale is generated per board and already carries real density - so the
    // disc measures the font instead of scaling a number. It also has to be a
    // full face: VALUE is a digits-only subset on the dense boards and cannot
    // draw an LV_SYMBOL glyph at all.
    const int32_t iconPx = lv_font_get_line_height(t.ICON);
    const int32_t discPx = (iconPx * 9) / 5;
    lv_obj_set_size        (_disc, discPx, discPx);
    lv_obj_set_style_radius(_disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_bottom(_disc, 0, 0);

    // --- State: the whole surface, not a corner ---------------------------
    //
    // cards.md section 0 recorded the owner's reaction to several semantic
    // colours on one screen - "instantly makes it look more attractive and
    // inviting" - and concluded that state colour is not a finishing touch,
    // it is the thing the card library exists to deliver. This is that.
    //
    // ST_ACTIVE and ST_IDLE are shared across every scheme on purpose: a light
    // being on is content, not decoration, so it does not change colour when
    // the dashboard does.
    const uint32_t chrome = stateColor();   // stale/refused override, or 0
    const bool fill = (s_fill == StateCardFill::FILL_SURFACE);

    if (isOn && !chrome && fill) {
        // The whole surface. cards.md section 4 as written.
        lv_obj_set_style_bg_color  (surface(), UI::c(tone(p.ST_ACTIVE)), 0);
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_20, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(tone(p.SURFACE)), 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(p.SURFACE)), 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(p.SURFACE)), 0);
    } else if (isOn && !chrome) {
        // Only the icon lights, in the state colour, with the disc tinted
        // behind it. The card keeps its own surface, which reads as quieter
        // across a page where several things are on at once.
        lv_obj_set_style_bg_color  (surface(), UI::c(tone(p.SURFACE)), 0);
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_30, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(tone(p.ST_ACTIVE)), 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(p.ST_ACTIVE)), 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(p.ST_ACTIVE)), 0);
    } else {
        lv_obj_set_style_bg_color  (surface(), UI::c(tone(p.SURFACE)), 0);
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(tone(p.SURFACE_ALT)), 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(chrome ? chrome : p.ST_IDLE)), 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(p.TEXT)), 0);
    }

    // --- Icon and name ----------------------------------------------------
    lv_label_set_text          (_icon, cardIconForState(e->desc, isOn));
    lv_obj_set_style_text_font (_icon, t.ICON, 0);

    // COMPACT drops the NAME, not the icon. cards.md section 4: "state is the
    // icon and its colour" - so the glyph is the part that cannot go, and a
    // very small actor card is a disc and nothing else. The disc also recentres
    // once there is no name below it to balance against.
    const bool compact = (variant() == CardVariant::VAR_COMPACT);
    if (compact) {
        lv_obj_add_flag  (_name,      LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag  (_statusRow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag  (_topRow,    LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(_statusRow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_topRow,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_name, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text          (_name, label());
    lv_obj_set_style_text_font (_name, t.NAME, 0);

    // --- Mixed: its own indicator, not a guess ----------------------------
    if (isMixed) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u/%u", (unsigned)active, (unsigned)total);
        lv_label_set_text          (_mixed, buf);
        lv_obj_set_style_text_font (_mixed, t.TAG, 0);
        lv_obj_set_style_text_color(_mixed, UI::c(tone(fill && isOn ? p.SURFACE : p.TEXT_DIM)), 0);
        lv_obj_clear_flag          (_mixed, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_mixed, LV_OBJ_FLAG_HIDDEN);
    }
}

void StateCard::commandAll(bool on) {
    for (uint8_t i = 0; i < primaryCount(); i++) {
        command(i, EntityValue::makeBool(on));
    }
}
