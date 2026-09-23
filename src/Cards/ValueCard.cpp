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
    //
    // Placement is the base class's, shared with StateCard - Card::
    // renderCornerIcon() explains the corner rule.
    _icon = makeCornerIcon(body);

    // The middle takes whatever is left and centres the HERO in it - the value
    // only, with the name as a sibling BELOW it.
    //
    // The name used to live inside here, centred as a group with the value,
    // while StateCard kept its name outside its own middle. Two different
    // structures, so the two card types put their names at different heights -
    // which is exactly what the owner saw: "for cards with value the hero text
    // is tucked up hard against the value and NOT aligned with the other
    // cards". Both are the same shape now.
    _mid = plain(body);
    lv_obj_set_width     (_mid, lv_pct(100));
    lv_obj_set_flex_grow (_mid, 1);
    lv_obj_set_flex_flow (_mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    // Keeps the hero clear of the corner icon, which is out of the flow above
    // it. Half a band rather than a whole one: they sit side by side, and the
    // owner's only constraint is that they must not touch.
    lv_obj_set_style_pad_top(_mid, Card::topBandHeight() / 2, 0);

    // Value and unit share a bottom edge, which is as close to a shared
    // baseline as LVGL gets without font metrics - and the reason the unit can
    // be a size smaller.
    _valueRow = plain(_mid);
    lv_obj_set_size      (_valueRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow (_valueRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_valueRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_gap(_valueRow, UI::sc(3), 0);
    lv_obj_set_style_pad_bottom(_mid, Card::midGap(), 0);

    _value = lv_label_create(_valueRow);
    _unit  = lv_label_create(_valueRow);

    _name = lv_label_create(body);
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
    // false: the corner is this card's ONLY icon - see cardCornerIcon().
    renderCornerIcon(_icon, cardCornerIcon(*e, false), cardTintFor(e->desc));

    // --- The value, dominant, with its unit smaller beside it -------------
    char buf[40];
    cardFormatValue(*e, buf, sizeof(buf), false, tempUnit());  // unit drawn separately
    lv_label_set_text          (_value, buf);

    // The HEIGHT decided the face in resolveVariant(); the WIDTH gets a say
    // here, because only now is the text known. A long reading ("1024" lux,
    // "3d 04:15") can overflow a narrow card at VALUE while fitting at
    // VALUE_SM - the owner's "the values would actually fit if they were
    // smaller", #62. Measured with the font, not estimated per character.
    const lv_font_t *vf = valueFont();
    const int32_t cw = cellWidthPx() > 0
                     ? cellWidthPx() - UI::sc(UI::met().PAD) * 2 : 0;
    if (cw > 0 && vf != t.VALUE_SM) {
        const char *du = cardDisplayUnit(*e, tempUnit());
        lv_point_t vs, us = {0, 0};
        lv_text_get_size(&vs, buf, vf, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (du[0]) lv_text_get_size(&us, du, t.UNIT, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (vs.x + us.x + UI::sc(3) > cw) vf = t.VALUE_SM;
    }
    lv_obj_set_style_text_font (_value, vf, 0);

    // A stale number is still the number - it is the chrome that shouts, not
    // the value. cards.md section 3 rejects dimming precisely so the reading
    // stays legible while the card makes clear it cannot be trusted.
    lv_obj_set_style_text_color(_value, UI::c(tone(p.TEXT)), 0);

    // NOT desc.unit. A card may render a temperature in a unit the source does
    // not use, in which case the number beside this label has already been
    // converted and printing the source's unit would caption it with a lie.
    const char *dispUnit = cardDisplayUnit(*e, tempUnit());
    if (dispUnit[0]) {
        lv_label_set_text          (_unit, dispUnit);
        lv_obj_set_style_text_font (_unit, t.UNIT, 0);
        lv_obj_set_style_text_color(_unit, UI::c(tone(p.TEXT_DIM)), 0);
        lv_obj_clear_flag          (_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_unit, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Name, which is the LOCATION, under the value ---------------------
    //
    // The label setting applies here too - the owner found temperatures
    // keeping their names under "No lbl" and looking odd as the only cards
    // that did. For a value card the number already IS the state, so State
    // shows the name, and No icon changes nothing: there is no hero glyph to
    // drop, and the corner stays on every card.
    const bool showName = (cardResolveLabel(labelMode()) != CardLabel::LBL_NONE);
    if (showName) lv_obj_clear_flag(_name, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag  (_name, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text          (_name, label());
    lv_obj_set_style_text_font (_name, t.NAME, 0);
    lv_obj_set_style_text_color(_name, UI::c(tone(p.TEXT_DIM)), 0);
    lv_obj_set_width           (_name, lv_pct(100));

    // Compact: the same shift a state card's name takes, so a row of mixed
    // cards keeps its names on one line - Card::compactNameShiftPx() measures
    // against the state card's disc for exactly that reason. Clamped here so
    // it can never ride up into this card's own number, which may be taller
    // than the disc it is measured against.
    int32_t shift = showName ? compactNameShiftPx() : 0;
    if (shift < 0) {
        const int32_t pad   = UI::sc(UI::met().PAD);
        const int32_t contH = surfaceHeightPx() - bodyTopPx() - pad;
        const int32_t nameH = lv_font_get_line_height(t.NAME);
        const int32_t band  = Card::topBandHeight() / 2;      // _mid's pad_top
        const int32_t midH  = contH - nameH - band - Card::midGap();
        const int32_t valBottom = band + midH / 2 + lv_font_get_line_height(vf) / 2;
        const int32_t minShift  = valBottom + UI::sc(2) - (contH - nameH);
        if (shift < minShift) shift = minShift < 0 ? minShift : 0;
    }
    lv_obj_set_style_translate_y(_name, shift, 0);

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

    // LAST-SEEN IS FOR VALUES THAT ARRIVE FROM SOMEWHERE ELSE.
    //
    // The owner, seeing "Seen: now" on the panel's own RSSI and uptime cards:
    // "Do local entities (like Panel) need secondary information such as
    // battery or last seen?" They do not. An entity this board OWNS is read
    // straight off the hardware every couple of seconds, so its age is always
    // "now" and the line is pure clutter.
    //
    // advertise == true is exactly the "we own it" flag - Entity.h calls it
    // "the real difference between the two groups of entity". The staleness
    // machinery still runs: if our own telemetry ever DID stop updating, the
    // card would still raise a STALE tag, which is the part worth keeping.
    //
    // It also answers the question underneath his: "Seen" is the last time
    // THIS ENTITY'S VALUE changed, not the last time the device was heard
    // from. For a Zigbee sensor publishing four values on one topic those are
    // the same moment; for anything else they are not, and the device-level
    // question is one only #43's device registry can answer.
    if (e->everSet && !e->desc.advertise) {
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

        // MEASURED FROM THE TEXT, not from layout. This used to call
        // lv_obj_update_layout() and read widths back - and right after a page
        // rebuild the layout is not settled, so the same card measured
        // differently depending on HOW it was last drawn. The owner saw "Seen:"
        // appear for every card after the Label knob (a repaint, layout
        // settled) and vanish at random after Auto -> Full (a rebuild, not
        // settled). The text's own size and the page-supplied width answer the
        // same question the same way every time.
        const int32_t cw = cellWidthPx() > 0
                         ? cellWidthPx() - UI::sc(UI::met().PAD) * 2 : 0;
        if (cw > 0) {
            lv_point_t fs, bs = {0, 0}, bi = {0, 0};
            lv_text_get_size(&fs, full, t.TAG, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            if (batt) {
                lv_text_get_size(&bi, lv_label_get_text(_battIcon), t.ICON_SM, 0, 0,
                                 LV_COORD_MAX, LV_TEXT_FLAG_NONE);
                lv_text_get_size(&bs, lv_label_get_text(_battery), t.TAG, 0, 0,
                                 LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            }
            const int32_t battW = batt ? bi.x + UI::sc(3) + bs.x : 0;
            if (fs.x > cw - battW - UI::sc(6)) lv_label_set_text(_seen, age);
        }

        lv_obj_clear_flag(_seen, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_seen, LV_OBJ_FLAG_HIDDEN);
    }
}
