#include "Cards/StateCard.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <stdio.h>

StateCardFill StateCard::s_fill = StateCardFill::FILL_SURFACE;

void StateCard::buildBody(lv_obj_t *body) {
    // The disc. A plain object with a full radius rather than an arc or an
    // image: it is a circle behind a glyph, and the cheapest thing that draws
    // a circle in LVGL is a square with radius set past half its side.
    _disc = lv_obj_create(body);
    lv_obj_clear_flag             (_disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_disc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width (_disc, 0, 0);
    lv_obj_set_style_pad_all      (_disc, 0, 0);

    _icon = lv_label_create(_disc);
    lv_obj_center(_icon);

    _name = lv_label_create(body);
    lv_label_set_long_mode(_name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(_name, LV_TEXT_ALIGN_CENTER, 0);

    _mixed = lv_label_create(body);
    lv_obj_align(_mixed, LV_ALIGN_TOP_RIGHT, 0, 0);
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
    lv_obj_align           (_disc, LV_ALIGN_CENTER, 0, -UI::sc(8));

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
        lv_obj_set_style_bg_color  (surface(), UI::c(p.ST_ACTIVE), 0);
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_20, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(p.SURFACE), 0);
        lv_obj_set_style_text_color(_icon, UI::c(p.SURFACE), 0);
        lv_obj_set_style_text_color(_name, UI::c(p.SURFACE), 0);
    } else if (isOn && !chrome) {
        // Only the icon lights, in the state colour, with the disc tinted
        // behind it. The card keeps its own surface, which reads as quieter
        // across a page where several things are on at once.
        lv_obj_set_style_bg_color  (surface(), UI::c(p.SURFACE), 0);
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_30, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(p.ST_ACTIVE), 0);
        lv_obj_set_style_text_color(_icon, UI::c(p.ST_ACTIVE), 0);
        lv_obj_set_style_text_color(_name, UI::c(p.ST_ACTIVE), 0);
    } else {
        lv_obj_set_style_bg_color  (surface(), UI::c(p.SURFACE), 0);
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(p.SURFACE_ALT), 0);
        lv_obj_set_style_text_color(_icon, UI::c(chrome ? chrome : p.ST_IDLE), 0);
        lv_obj_set_style_text_color(_name, UI::c(p.TEXT), 0);
    }

    // --- Icon and name ----------------------------------------------------
    lv_label_set_text          (_icon, cardIconForState(e->desc, isOn));
    lv_obj_set_style_text_font (_icon, t.ICON, 0);

    lv_label_set_text          (_name, label());
    lv_obj_set_style_text_font (_name, t.NAME, 0);

    lv_obj_t *par = lv_obj_get_parent(_name);
    lv_obj_update_layout(par);
    lv_obj_set_width(_name, lv_obj_get_content_width(par));
    lv_obj_align    (_name, LV_ALIGN_BOTTOM_MID, 0, 0);

    // --- Mixed: its own indicator, not a guess ----------------------------
    if (isMixed) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u/%u", (unsigned)active, (unsigned)total);
        lv_label_set_text          (_mixed, buf);
        lv_obj_set_style_text_font (_mixed, t.TAG, 0);
        lv_obj_set_style_text_color(_mixed, UI::c(fill && isOn ? p.SURFACE : p.TEXT_DIM), 0);
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
