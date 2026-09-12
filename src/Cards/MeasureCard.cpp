#include "Cards/MeasureCard.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

void MeasureCard::buildBody(lv_obj_t *body) {
    // Absolute alignment rather than a flex column. The value has to be
    // centred in the CARD, not centred in whatever space the other rows leave
    // over - and with flex it would drift up or down as the status row comes
    // and goes, which is exactly the twitch a dashboard must not have.
    _icon = lv_label_create(body);
    lv_obj_align(_icon, LV_ALIGN_TOP_LEFT, 0, 0);

    _name = lv_label_create(body);
    lv_label_set_long_mode(_name, LV_LABEL_LONG_DOT);
    lv_obj_align(_name, LV_ALIGN_TOP_LEFT, 0, 0);   // x set in render(), from
                                                    // the icon's real width

    // Value and unit are TWO labels, not one string. The design bench draws
    // the unit markedly smaller and baseline-aligned beside the number, which
    // a single label cannot do - and splitting them is also what lets the
    // VALUE face be a digits-only subset, which is the only reason a 68 px
    // face is affordable on WS_P4_5. See scripts/gen_type_scale.py.
    _value = lv_label_create(body);
    _unit  = lv_label_create(body);

    _status = lv_label_create(body);
    lv_obj_align(_status, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void MeasureCard::render() {
    const Entity *e = primary();
    if (!e) return;

    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();

    // --- Icon, tinted by what this measures -------------------------------
    lv_label_set_text          (_icon, cardIconFor(e->desc));
    lv_obj_set_style_text_font (_icon, t.NAME, 0);
    lv_obj_set_style_text_color(_icon, UI::c(cardTintFor(e->desc)), 0);

    // --- Name, which is the LOCATION --------------------------------------
    lv_label_set_text          (_name, label());
    lv_obj_set_style_text_font (_name, t.NAME, 0);
    lv_obj_set_style_text_color(_name, UI::c(p.TEXT_DIM), 0);

    // Positioned from the icon's measured width rather than a guessed offset,
    // because the icon is a glyph whose width changes with the face and will
    // change again when the MDI subset replaces it.
    lv_obj_t *par = lv_obj_get_parent(_name);
    lv_obj_update_layout(par);
    const int32_t gap    = UI::sc(6);
    const int32_t iconW  = lv_obj_get_width(_icon);
    lv_obj_align(_name, LV_ALIGN_TOP_LEFT, iconW + gap, 0);

    // An explicit width, not a percentage: lv_pct() encodes its argument in a
    // sentinel range, so subtracting the icon's width from it would produce an
    // arbitrary number rather than "the rest of the row". LV_LABEL_LONG_DOT
    // needs a real width to ellipsise against.
    const int32_t avail = lv_obj_get_content_width(par) - iconW - gap;
    if (avail > 0) lv_obj_set_width(_name, avail);

    // --- The value, dominant, with its unit smaller beside it -------------
    char buf[40];
    cardFormatValue(*e, buf, sizeof(buf), false);   // no unit in this string
    lv_label_set_text          (_value, buf);
    lv_obj_set_style_text_font (_value, t.VALUE, 0);

    // A stale number is still the number - it is the chrome that shouts, not
    // the value. cards.md section 3 rejects dimming precisely so the reading
    // stays legible while the card makes clear it cannot be trusted.
    lv_obj_set_style_text_color(_value, UI::c(p.TEXT), 0);

    lv_label_set_text          (_unit, e->desc.unit);
    lv_obj_set_style_text_font (_unit, t.UNIT, 0);
    lv_obj_set_style_text_color(_unit, UI::c(p.TEXT_DIM), 0);

    // Centre the PAIR, then sit the unit on the number's baseline. Centring
    // the number alone and hanging the unit off it would shift the number
    // left or right as the unit's width changed, which is the sort of twitch
    // that makes a wall of cards look unsettled.
    lv_obj_update_layout(_value);
    lv_obj_update_layout(_unit);
    const int32_t vW = lv_obj_get_width(_value);
    const int32_t uW = e->desc.unit[0] ? lv_obj_get_width(_unit) : 0;
    const int32_t kern = e->desc.unit[0] ? UI::sc(2) : 0;
    const int32_t pairW = vW + kern + uW;

    lv_obj_align(_value, LV_ALIGN_CENTER, -(pairW - vW) / 2, 0);
    if (e->desc.unit[0]) {
        lv_obj_clear_flag(_unit, LV_OBJ_FLAG_HIDDEN);
        // Baseline, approximated by the difference in line height - LVGL does
        // not expose a baseline offset on a label.
        const int32_t drop = (lv_font_get_line_height(t.VALUE) -
                              lv_font_get_line_height(t.UNIT)) / 2;
        lv_obj_align_to(_unit, _value, LV_ALIGN_OUT_RIGHT_BOTTOM, kern, -drop);
    } else {
        lv_obj_add_flag(_unit, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Status row: battery and/or last-seen, or nothing at all ----------
    //
    // cards.md section 1: if the entity has neither, the row is simply absent
    // - not an empty reserved strip. An always-present strip is how a card
    // ends up with a hole in it.
    char status[32] = {0};
    for (uint8_t i = 0; i < secondaryCount(); i++) {
        const Entity *s = secondary(i);
        if (!s || !s->everSet) continue;
        char one[16];
        cardFormatValue(*s, one, sizeof(one));
        if (status[0]) strncat(status, "  ", sizeof(status) - strlen(status) - 1);
        strncat(status, one, sizeof(status) - strlen(status) - 1);
    }
    if (e->everSet) {
        char age[12];
        cardFormatAge(millis() - e->lastUpdateMs, age, sizeof(age));
        if (status[0]) strncat(status, "  ", sizeof(status) - strlen(status) - 1);
        strncat(status, age, sizeof(status) - strlen(status) - 1);
    }

    if (status[0]) {
        lv_label_set_text          (_status, status);
        lv_obj_set_style_text_font (_status, t.TAG, 0);
        lv_obj_set_style_text_color(_status, UI::c(p.TEXT_DIM), 0);
        lv_obj_clear_flag          (_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_status, LV_OBJ_FLAG_HIDDEN);
    }
}
