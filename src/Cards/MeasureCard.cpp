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

    _value = lv_label_create(body);
    lv_obj_align(_value, LV_ALIGN_CENTER, 0, 0);

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

    // --- The value, dominant ----------------------------------------------
    char buf[40];
    cardFormatValue(*e, buf, sizeof(buf));
    lv_label_set_text          (_value, buf);
    lv_obj_set_style_text_font (_value, t.VALUE, 0);

    // A stale number is still the number - it is the chrome that shouts, not
    // the value. cards.md section 3 rejects dimming precisely so the reading
    // stays legible while the card makes clear it cannot be trusted.
    lv_obj_set_style_text_color(_value, UI::c(p.TEXT), 0);

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
