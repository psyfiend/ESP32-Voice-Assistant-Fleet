#include "Cards/ValueCard.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

void ValueCard::buildBody(lv_obj_t *body) {
    // --- Title row: icon then name, on ONE line ---------------------------
    //
    // A flex row rather than two aligned labels. The previous version measured
    // the icon and positioned the name from it by hand, which needed a layout
    // pass that had not happened yet - so the name landed at the wrong x, and
    // with no height set LV_LABEL_LONG_DOT wrapped instead of ellipsising.
    // "Deck" came out as "Dec / k" on a card 230 px wide. Flex does the
    // measuring, and it does it after layout rather than during render.
    _titleRow = lv_obj_create(body);
    lv_obj_set_width              (_titleRow, lv_pct(100));
    lv_obj_set_height             (_titleRow, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow          (_titleRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align         (_titleRow, LV_FLEX_ALIGN_START,
                                   LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa       (_titleRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_titleRow, 0, 0);
    lv_obj_set_style_pad_all      (_titleRow, 0, 0);
    lv_obj_clear_flag             (_titleRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_titleRow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align                  (_titleRow, LV_ALIGN_TOP_LEFT, 0, 0);

    _icon = lv_label_create(_titleRow);

    _name = lv_label_create(_titleRow);
    lv_label_set_long_mode(_name, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow  (_name, 1);   // takes the rest of the row, so DOT
                                        // has a real width to ellipsise against

    // --- Value and unit, as one centred group -----------------------------
    //
    // Also a flex row, and for a sharper reason: lv_obj_align_to() resolves
    // against the reference object's position AT THE MOMENT IT IS CALLED. The
    // value had just been re-aligned and layout had not run, so the unit was
    // placed against the value's stale top-left origin and came out beside the
    // card's NAME instead of beside the number. Visible in both flash photos.
    //
    // CROSS-AXIS END aligns the two on their bottoms, which is as close to a
    // shared baseline as LVGL gets without font metrics - and the reason the
    // unit can be a different size at all.
    _valueRow = lv_obj_create(body);
    lv_obj_set_size               (_valueRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow          (_valueRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align         (_valueRow, LV_FLEX_ALIGN_CENTER,
                                   LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_bg_opa       (_valueRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_valueRow, 0, 0);
    lv_obj_set_style_pad_all      (_valueRow, 0, 0);
    lv_obj_set_style_pad_gap      (_valueRow, UI::sc(3), 0);
    lv_obj_clear_flag             (_valueRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_valueRow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center                 (_valueRow);

    _value = lv_label_create(_valueRow);
    _unit  = lv_label_create(_valueRow);

    // --- Status: the two bottom corners -----------------------------------
    // Battery bottom-LEFT with its own glyph, last-seen bottom-RIGHT. The
    // owner's layout, replacing one concatenated string in the right corner.
    _battery = lv_label_create(body);
    lv_obj_align(_battery, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    _seen = lv_label_create(body);
    lv_obj_align(_seen, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void ValueCard::render() {
    const Entity *e = primary();
    if (!e) return;

    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();

    // --- Icon, tinted by what this measures -------------------------------
    lv_label_set_text          (_icon, cardIconFor(e->desc));
    lv_obj_set_style_text_font (_icon, t.ICON_SM, 0);
    lv_obj_set_style_text_color(_icon, UI::c(cardTintFor(e->desc)), 0);
    lv_obj_set_style_pad_right (_icon, UI::sc(5), 0);

    // --- Name, which is the LOCATION --------------------------------------
    lv_label_set_text          (_name, label());
    lv_obj_set_style_text_font (_name, t.NAME, 0);
    lv_obj_set_style_text_color(_name, UI::c(p.TEXT_DIM), 0);

    // --- The value, dominant, with its unit smaller beside it -------------
    char buf[40];
    cardFormatValue(*e, buf, sizeof(buf), false);   // no unit in this string
    lv_label_set_text          (_value, buf);
    lv_obj_set_style_text_font (_value, t.VALUE, 0);

    // A stale number is still the number - it is the chrome that shouts, not
    // the value. cards.md section 3 rejects dimming precisely so the reading
    // stays legible while the card makes clear it cannot be trusted.
    lv_obj_set_style_text_color(_value, UI::c(p.TEXT), 0);

    if (e->desc.unit[0]) {
        lv_label_set_text          (_unit, e->desc.unit);
        lv_obj_set_style_text_font (_unit, t.UNIT, 0);
        lv_obj_set_style_text_color(_unit, UI::c(p.TEXT_DIM), 0);
        lv_obj_clear_flag          (_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_unit, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Bottom-left: battery ---------------------------------------------
    //
    // cards.md section 1: if the entity has neither secondary, the row is
    // simply absent - not an empty reserved strip. Two labels rather than one
    // means each corner disappears independently, which is the same rule
    // applied one level finer.
    const Entity *batt = nullptr;
    for (uint8_t i = 0; i < secondaryCount(); i++) {
        const Entity *s = secondary(i);
        if (s && s->everSet && strcmp(s->desc.deviceClass, "battery") == 0) batt = s;
    }
    if (batt) {
        const int pct = (batt->value.type == ValueType::FLOAT)
                      ? (int)batt->value.f : (int)batt->value.i;
        char b[24];
        snprintf(b, sizeof(b), "%s %d%%", cardBatteryGlyph(pct), pct);
        lv_label_set_text          (_battery, b);
        lv_obj_set_style_text_font (_battery, t.TAG, 0);
        lv_obj_set_style_text_color(_battery, UI::c(p.TEXT_DIM), 0);
        lv_obj_clear_flag          (_battery, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_battery, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Bottom-right: last seen ------------------------------------------
    if (e->everSet) {
        char age[12];
        cardFormatAge(millis() - e->lastUpdateMs, age, sizeof(age));

        // The owner asked for a "Last seen:" label rather than a bare age. It
        // does not always fit - at TAG size on CYD_S3_3248 a 141 px card has
        // no room for eleven extra characters beside a battery reading - so
        // the prefix is dropped when the two corners would collide rather than
        // letting them overlap. Measured, not guessed at per board.
        char full[28];
        snprintf(full, sizeof(full), "Last seen: %s", age);
        lv_label_set_text          (_seen, full);
        lv_obj_set_style_text_font (_seen, t.TAG, 0);
        lv_obj_set_style_text_color(_seen, UI::c(p.TEXT_DIM), 0);

        lv_obj_t *par = lv_obj_get_parent(_seen);
        lv_obj_update_layout(par);
        const int32_t avail = lv_obj_get_content_width(par)
                            - (lv_obj_has_flag(_battery, LV_OBJ_FLAG_HIDDEN)
                               ? 0 : lv_obj_get_width(_battery) + UI::sc(6));
        if (lv_obj_get_width(_seen) > avail) lv_label_set_text(_seen, age);

        lv_obj_clear_flag          (_seen, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_seen, LV_OBJ_FLAG_HIDDEN);
    }
}
