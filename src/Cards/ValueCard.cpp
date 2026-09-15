#include "Cards/ValueCard.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// A transparent, padding-free container. Every row below is one.
static lv_obj_t *plain(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_style_bg_opa       (o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (o, 0, 0);
    lv_obj_set_style_pad_all      (o, 0, 0);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void ValueCard::buildBody(lv_obj_t *body) {
    // A VERTICAL STACK, not a set of aligned pieces.
    //
    // Everything here used to be positioned by hand against something else,
    // and every one of those calls resolved against a position that layout had
    // not computed yet. The symptoms were all the same bug: a unit beside the
    // card's name instead of its number, a battery percentage hidden behind
    // the value, a percentage hanging off the bottom edge of a 3248 card.
    //
    //     [ icon      ]   its own line, left
    //     [ value+unit]   grows, centred as a group with the name
    //     [ name      ]
    //     [ batt  seen]   the status row, absent in COMPACT
    //
    // Flex measures after layout rather than during render, so none of those
    // can come back.
    lv_obj_set_flex_flow (body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(body, 0, 0);

    // THE ICON IS OUT OF FLOW, in the corner, costing the stack nothing.
    //
    // It used to have a reserved band of its own, and that band is what broke
    // CYD_S3_3248: moving the name under the value turned three rows into
    // four, and 22 px is the whole difference on a 121 px card. The owner
    // noticed the status line had fit perfectly well before.
    //
    // It does not need a row. It sits top-left and the value is centred, so
    // they never contend for the same space - reserving a row for it was
    // paying for a collision that cannot happen.
    _icon = lv_label_create(body);
    lv_obj_add_flag(_icon, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align   (_icon, LV_ALIGN_TOP_LEFT, 0, 0);

    // The middle takes whatever is left and centres the value and the name in
    // it as one group - which is what stops the value drifting when the status
    // row appears or disappears.
    _mid = plain(body);
    lv_obj_set_width     (_mid, lv_pct(100));
    lv_obj_set_flex_grow (_mid, 1);
    lv_obj_set_flex_flow (_mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Value and unit share a bottom edge, which is as close to a shared
    // baseline as LVGL gets without font metrics - and the reason the unit can
    // be a size smaller.
    _valueRow = plain(_mid);
    lv_obj_set_size      (_valueRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow (_valueRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_valueRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_gap(_valueRow, UI::sc(3), 0);
    lv_obj_set_style_pad_bottom(_valueRow, Card::midGap(), 0);

    _value = lv_label_create(_valueRow);
    _unit  = lv_label_create(_valueRow);

    _name = lv_label_create(_mid);
    lv_label_set_long_mode     (_name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(_name, LV_TEXT_ALIGN_CENTER, 0);

    // Battery on the left, last-seen on the right, on one line at the bottom.
    // RESERVED, always, in a full layout - see Card::statusBandHeight(). A
    // card whose sensor reports no battery must not sit its name lower than
    // the one beside it that does.
    _statusRow = plain(body);
    lv_obj_set_width     (_statusRow, lv_pct(100));
    lv_obj_set_height    (_statusRow, Card::statusBandHeight());
    lv_obj_set_flex_flow (_statusRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_statusRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // The glyph and the percentage are TWO labels in a row, because no single
    // face has both: the icon subset has no digits and the text face has no
    // icons. Drawing them from one string rendered the glyph as a hollow
    // rectangle.
    _battGroup = plain(_statusRow);
    lv_obj_set_size      (_battGroup, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow (_battGroup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_battGroup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(_battGroup, UI::sc(3), 0);

    _battIcon = lv_label_create(_battGroup);
    _battery  = lv_label_create(_battGroup);

    _seen = lv_label_create(_statusRow);
}

void ValueCard::render() {
    const Entity *e = primary();
    if (!e) return;

    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const bool   compact = (variant() == CardVariant::VAR_COMPACT);

    // --- Icon, tinted by what this measures -------------------------------
    lv_label_set_text          (_icon, cardIconFor(e->desc));
    lv_obj_set_style_text_font (_icon, t.ICON_SM, 0);
    lv_obj_set_style_text_color(_icon, UI::c(tone(cardTintFor(e->desc))), 0);

    // --- The value, dominant, with its unit smaller beside it -------------
    char buf[40];
    cardFormatValue(*e, buf, sizeof(buf), false);   // no unit in this string
    lv_label_set_text          (_value, buf);
    lv_obj_set_style_text_font (_value, t.VALUE, 0);

    // A stale number is still the number - it is the chrome that shouts, not
    // the value. cards.md section 3 rejects dimming precisely so the reading
    // stays legible while the card makes clear it cannot be trusted.
    lv_obj_set_style_text_color(_value, UI::c(tone(p.TEXT)), 0);

    if (e->desc.unit[0]) {
        lv_label_set_text          (_unit, e->desc.unit);
        lv_obj_set_style_text_font (_unit, t.UNIT, 0);
        lv_obj_set_style_text_color(_unit, UI::c(tone(p.TEXT_DIM)), 0);
        lv_obj_clear_flag          (_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_unit, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Name, which is the LOCATION, under the value ---------------------
    lv_label_set_text          (_name, label());
    lv_obj_set_style_text_font (_name, t.NAME, 0);
    lv_obj_set_style_text_color(_name, UI::c(tone(p.TEXT_DIM)), 0);
    lv_obj_set_width           (_name, lv_pct(100));

    // --- The status row, which COMPACT does not have room for -------------
    //
    // Dropped wholesale rather than shrunk. cards.md section 1 already treats
    // this row as absent when there is nothing to put in it; a cell too small
    // to seat it is the same situation arriving from the other direction.
    if (compact) {
        lv_obj_add_flag(_statusRow, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(_statusRow, LV_OBJ_FLAG_HIDDEN);

    const Entity *batt = nullptr;
    for (uint8_t i = 0; i < secondaryCount(); i++) {
        const Entity *s = secondary(i);
        if (s && s->everSet && strcmp(s->desc.deviceClass, "battery") == 0) batt = s;
    }
    if (batt) {
        const int pct = (batt->value.type == ValueType::FLOAT)
                      ? (int)batt->value.f : (int)batt->value.i;
        char b[12];
        snprintf(b, sizeof(b), "%d%%", pct);

        lv_label_set_text          (_battIcon, cardBatteryGlyph(pct));
        lv_obj_set_style_text_font (_battIcon, t.ICON_SM, 0);
        lv_obj_set_style_text_color(_battIcon, UI::c(tone(p.TEXT_DIM)), 0);
        lv_label_set_text          (_battery, b);
        lv_obj_set_style_text_font (_battery, t.TAG, 0);
        lv_obj_set_style_text_color(_battery, UI::c(tone(p.TEXT_DIM)), 0);
    } else {
        // Emptied rather than hidden. Hiding it removed it from the row, and
        // SPACE_BETWEEN with a single child puts that child on the LEFT - which
        // is why "Seen: now" jumped to the wrong corner on a card with no
        // battery to report.
        lv_label_set_text(_battIcon, "");
        lv_label_set_text(_battery,  "");
    }

    if (e->everSet) {
        char age[12];
        cardFormatAge(millis() - e->lastUpdateMs, age, sizeof(age));

        // "Seen:" rather than "Last seen:" - the owner's call, to buy back the
        // width. Even so it does not always fit: on a 141 px CYD_S3_3248 card
        // there is no room beside a battery reading, so the prefix drops
        // rather than letting the two collide. Measured, not guessed per board.
        char full[24];
        snprintf(full, sizeof(full), "Seen: %s", age);
        lv_label_set_text          (_seen, full);
        lv_obj_set_style_text_font (_seen, t.TAG, 0);
        lv_obj_set_style_text_color(_seen, UI::c(tone(p.TEXT_DIM)), 0);

        lv_obj_update_layout(_statusRow);
        const int32_t avail = lv_obj_get_content_width(_statusRow)
                            - lv_obj_get_width(_battGroup) - UI::sc(6);
        if (lv_obj_get_width(_seen) > avail) lv_label_set_text(_seen, age);

        lv_obj_clear_flag(_seen, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_seen, LV_OBJ_FLAG_HIDDEN);
    }
}
