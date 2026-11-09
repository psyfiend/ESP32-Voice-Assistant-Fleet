#include "Cards/StateCard.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include "UI/Kit.h"
#include <Arduino.h>
#include <stdio.h>

StateCardFill StateCard::s_fill = StateCardFill::FRAME_CORNER;   // GUIManager's default (D-33)

using KS = Kit::KitStyle;

static lv_obj_t *plainCol(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    Kit::add(o, KS::KS_CARD_PLAIN);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void StateCard::buildBody(lv_obj_t *body) {
    if (mini()) { buildMiniBody(body); return; }   // a cluster's tile (MiniTile.cpp)
    // Same vertical stack as ValueCard, and for the same reason: a column with
    // the disc in a growing middle puts the disc and the name together and
    // centres the pair, where a hand-tuned offset leaves a gap.
    lv_obj_set_flex_flow (body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(body, 0, 0);

    // TWO bands, matching ValueCard: a growing middle and a reserved status
    // line. Neither layout reserves a row for its corner icon - it is out of
    // the flow on both, which leaves CYD_S3_3248 the height its status line
    // needs.
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
    Kit::add(_disc, KS::KS_CARD_DISC);   // round, no border, no padding

    _icon = lv_label_create(_disc);
    lv_obj_center(_icon);

    _name = lv_label_create(body);
    Kit::add(_name, KS::KS_CARD_NAME);   // the name's face, centred
    lv_label_set_long_mode     (_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width           (_name, lv_pct(100));

    _statusRow = plainCol(body);
    lv_obj_set_width (_statusRow, lv_pct(100));
    lv_obj_set_height(_statusRow, Card::statusBandHeight());

    // The not-uniform badge, IN THE STATUS ROW: the top-left is the corner
    // icon's, the top-right is the floating STALE badge's in HDR_NONE, and a
    // mixed count is status.
    _mixed = lv_label_create(_statusRow);
    Kit::add(_mixed, KS::KS_CARD_SMALL);
    lv_obj_align   (_mixed, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(_mixed, LV_OBJ_FLAG_HIDDEN);

    // NO CORNER ICON (K172): the disc's hero icon already says
    // what this is - a light, a door, a switch, someone there. Value cards keep
    // theirs (a number alone does not say what it measures). renderCornerIcon()
    // takes the null.
    _corner = nullptr;
}

bool StateCard::onFill(int32_t fromBottom, int pct) const {
    if (pct >= 100) return true;
    if (pct <= 0)   return false;
    // The surface's height, from the page-supplied cell. A tag hangs outside
    // the card, so it is not part of the surface.
    const int32_t h = surfaceHeightPx();
    return fromBottom * 100 < h * pct;
}

void StateCard::render() {
    if (mini()) { renderMini(); return; }
    const Entity *e = primary();
    if (!e) return;

    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();

    const uint8_t total  = liveCount();    // paused members are out (K14)
    const uint8_t active = activeCount();
    const bool    isOn    = groupIsOn();   // any, or all - GroupOn
    const bool    isMixed = active > 0 && active < total;

    // --- Disc geometry, derived from the type scale -----------------------
    // ICON is a FACE rather than a logical pixel count, because the type
    // scale is generated per board and already carries real density - so the
    // disc measures the font instead of scaling a number. It also has to be a
    // full face: VALUE is a digits-only subset on the dense boards and cannot
    // draw an LV_SYMBOL glyph at all.
    //
    // THE FACE STEPS DOWN WITH THE CELL: LG, then MD, then SM, the largest
    // whose glyph still fits the band with a margin. A cramped card that kept
    // the LG glyph would lose its disc.
    //
    // Both live on Card - heroIconFace() and heroDiscPx() - so a value card
    // can place its name against the same reference. The clamp to the band is
    // there too: on CYD_S3_3248 the band is 55 px and twice the line height
    // wants 68, so the disc would be clipped by its own parent.
    const lv_font_t *heroFace = heroIconFace();
    const int32_t iconPx = lv_font_get_line_height(heroFace);
    const int32_t discPx = heroDiscPx();

    // THE DISC IS NEVER SMALLER THAN THE GLYPH IT CONTAINS, AND NEVER EXCEEDS
    // ITS BAND.
    //
    // The clamp above is against the band the disc sits in, and that band is
    // SHORTER in the full variant than in compact - full pays for a status row
    // that compact hides - so on a tight cell the disc could shrink past the
    // icon. Flooring it at the glyph size is no answer: a disc bigger than its
    // band overflows its parent, and a child overflowing its parent forces
    // LVGL to render that parent to an intermediate LAYER so it can be
    // clipped. On WS_P4_5 that asked for a 615x20 layer buffer, failed, and
    // froze the board at first boot (LESSONS.md).
    //
    // So when the band is too tight to hold a circle bigger than the glyph,
    // the circle is not drawn at all and the icon stands on its own. Draw
    // LESS, never the same thing smaller - the rule the variants obey.
    const bool discFits = (discPx >= iconPx + UI::sc(4));
    lv_obj_set_size        (_disc, discPx, discPx);   // round by its role

    // --- State: the whole surface, not a corner ---------------------------
    //
    // cards.md section 0: state colour is not a finishing touch, it is the
    // thing the card library exists to deliver.
    //
    // ST_IDLE is shared across every scheme on purpose: a light being off is
    // content, not decoration. ST_ACTIVE differs on Linen (olive -
    // UITokens.cpp); the MEANING is the same everywhere, a filled card with a
    // lit disc, so it never rests on colour alone.
    //
    // KNOWN GAP: with the fill switched off (the branch below that lights only
    // the icon and name in ST_ACTIVE), a mid-tone active colour on Linen's
    // cream surface is weak (olive on cream is ~2.4:1). The default card
    // state is unaffected. Address it with the user-selectable active colours
    // (4.1): each needs a darker ink for that mode.
    const uint32_t chrome = stateColor();   // stale/refused override, or 0
    // ITS GROUP'S CARD STATE when it has one (a group view's card), as a mini
    // tile takes it (MiniTile.cpp); else the device's.
    const StateCardFill eff = ownFill() >= 0 ? (StateCardFill)ownFill() : s_fill;
    const bool fill = (eff == StateCardFill::FILL_SURFACE);
    const bool compact = (variant() == CardVariant::VAR_COMPACT);

    // --- What the line under the hero says --------------------------------
    // Decided before any colour, because whether a name exists changes where
    // the disc sits, and the fill below needs to know that.
    const CardLabel lbl = cardResolveLabel(labelMode());
    const bool showName = (lbl != CardLabel::LBL_NONE);

    // Compact: the name moves up to sit centred between the disc and the
    // card's bottom edge. A translate, so the flex layout - and the disc's
    // position - is untouched. See Card::compactNameShiftPx().
    const int32_t nameShift = showName ? compactNameShiftPx() : 0;

    // --- Brightness, from the source's attributes. cards.md section 13 ----
    //
    // A GROUP SHOWS THE MEAN, AS HA'S LIGHT GROUP DOES (HA's group/light.py:
    // brightness = the mean over the members that are ON and report one). One
    // at 100% and one at 50% fills to 75%. No member reporting one - an on/off
    // light, a door - fills the card completely when on.
    const EntityAttrs &a = e->attrs;
    const bool single = (primaryCount() == 1);
    int pct = 100;
    if (isOn) {
        int32_t sum = 0, n = 0;
        const Entity *live[CARD_LIVE_MAX];
        const uint8_t nLive = liveEntities(live);   // an HA group's members, when one is paused (K14)
        for (uint8_t i = 0; i < nLive; i++) {
            const Entity *m = live[i];
            if (!counts(m) || !(m->value.type == ValueType::BOOL && m->value.b)) continue;
            if (m->attrs.brightness >= 0) { sum += m->attrs.brightness; n++; }
        }
        if (n) {
            pct = (int)((sum / n * 100 + 127) / 255);
            if (pct < 1) pct = 1;   // on is never drawn as empty
        }
    }

    // Where each element's centre sits, measured up from the surface's bottom
    // edge, so each can pick the text colour for what is actually behind it.
    // From the tokens rather than from layout: see applyDiagonal() on why a
    // card does not call lv_obj_update_layout() per repaint.
    const UIMetrics &m = UI::met();
    const int32_t pad    = UI::sc(m.PAD);
    const int32_t status = compact ? 0 : Card::statusBandHeight();
    const int32_t nameH  = showName ? lv_font_get_line_height(t.NAME) : 0;
    const int32_t surfH   = surfaceHeightPx();
    const int32_t bodyTop = bodyTopPx();
    const int32_t midBottom = pad + status + nameH + Card::midGap();
    const int32_t yName   = pad + status + nameH / 2 - nameShift;
    const int32_t yDisc   = (midBottom + (surfH - bodyTop)) / 2;
    const int32_t yCorner = surfH - bodyTop - lv_font_get_line_height(cornerFont()) / 2;
    const int32_t yMixed  = pad + status / 2;

    // Ink ON THE FILL is whichever of the scheme's ground and text colours
    // stands further from the active colour (SURFACE would be white on a light
    // scheme, on yellow, while a disc's ink that already comes from contrast
    // would be black beside it - one rule for both makes them agree). Off the
    // fill, the element's normal.
    const bool filled = isOn && !chrome && fill;
    const uint32_t fillInk = UI::contrastOf(p.ST_ACTIVE, p.GROUND, p.TEXT);
    auto ink = [&](int32_t fromBottom, uint32_t normal) -> uint32_t {
        return (filled && onFill(fromBottom, pct)) ? fillInk : normal;
    };

    // --- The surface ------------------------------------------------------
    if (filled && pct < 100) {
        // A HARD-EDGED GRADIENT ON THE SURFACE ITSELF, not a child object.
        //
        // A child "fill" rectangle would have to be clipped to the card's
        // rounded corners, which means clip_corner, which means an
        // intermediate LAYER per card - the allocation that froze WS_P4_5
        // (LESSONS.md, "LVGL allocates a LAYER"). The surface already draws
        // inside its own radius, so painting the fill as its background does
        // the rounding for free.
        //
        // Both stops at the same position gives a hard edge: LVGL's software
        // gradient returns the first colour at or above the lower stop and
        // the last at or below the upper, and with the two equal no row falls
        // between them (lv_draw_sw_grad.c, checked for 9.5 - no division by
        // the zero-width span either). Top is the empty part, bottom the fill.
        const uint8_t stop = (uint8_t)(255 - (pct * 255) / 100);
        lv_obj_set_style_bg_color    (surface(), UI::c(tone(p.SURFACE)), 0);
        lv_obj_set_style_bg_grad_color(surface(), UI::c(tone(p.ST_ACTIVE)), 0);
        lv_obj_set_style_bg_grad_dir (surface(), LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_main_stop(surface(), stop, 0);
        lv_obj_set_style_bg_grad_stop(surface(), stop, 0);
    } else {
        lv_obj_set_style_bg_grad_dir (surface(), LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_bg_color    (surface(),
            UI::c(tone(filled ? p.ST_ACTIVE : p.SURFACE)), 0);
    }

    if (filled) {
        // The whole surface, or as much of it as the brightness says.
        const uint32_t discInk = ink(yDisc, p.ST_ACTIVE);
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_20, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(tone(onFill(yDisc, pct) ? p.SURFACE
                                                                         : p.ST_ACTIVE)), 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(discInk)), 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(ink(yName, p.TEXT))), 0);
    } else if (isOn && !chrome) {
        // Only the icon lights, in the state colour, with the disc tinted
        // behind it. The card keeps its own surface, which reads as quieter
        // across a page where several things are on at once.
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_30, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(tone(p.ST_ACTIVE)), 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(p.ST_ACTIVE)), 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(p.ST_ACTIVE)), 0);
    } else {
        lv_obj_set_style_bg_opa    (_disc, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color  (_disc, UI::c(tone(p.SURFACE_ALT)), 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(chrome ? chrome : p.ST_IDLE)), 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(p.TEXT)), 0);
    }

    // --- The active frame (D-30 to D-33): "on" on the card's edge ---------
    // The frame modes light the icon and name as LIGHT_ICON does (above). The
    // corner cut draws its own shadow along the cuts (CardFrame.cpp), on Linen
    // too.
    CardFrame frame = CardFrame::FRAME_NONE;
    if (isOn && !chrome) {
        if (eff == StateCardFill::FRAME_BRACKET) frame = CardFrame::FRAME_BRACKET;
        if (eff == StateCardFill::FRAME_CORNER)  frame = CardFrame::FRAME_CORNER;
    }
    activeFrame(frame, p.ST_ACTIVE);

    // --- The light's own colour, in the disc. cards.md section 13 ---------
    //
    // The DISC, not the surface: a saturated colour across the whole card is
    // where legibility goes. HA sends rgb_color for every colour mode,
    // colour-temperature included, so this one field covers both. The glyph
    // takes whichever of the scheme's ground and text colours stands further
    // from it, so a warm white and a deep blue both stay readable without a
    // colour literal in UI code.
    //
    // When the card is too tight to draw a disc at all (discFits, above), the
    // GLYPH takes the colour instead.
    if (single && isOn && !chrome && a.hasRgb) {
        if (discFits) {
            const uint32_t ink2 = UI::contrastOf(a.rgb, p.GROUND, p.TEXT);
            lv_obj_set_style_bg_opa    (_disc, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color  (_disc, UI::c(tone(a.rgb)), 0);
            lv_obj_set_style_text_color(_icon, UI::c(tone(ink2)), 0);
        } else {
            lv_obj_set_style_text_color(_icon, UI::c(tone(a.rgb)), 0);
        }
    }

    // --- Icons ------------------------------------------------------------
    lv_label_set_text          (_icon, cardHeroIcon(*e, isOn));
    lv_obj_set_style_text_font (_icon, heroFace, 0);

    // The corner takes the fill's surface colour where the fill reaches it,
    // and otherwise the same tint a value card's corner wears, so a row of
    // mixed cards reads as one family.
    renderCornerIcon(_corner, cardCornerIcon(*e, true),
                     ink(yCorner, chrome ? chrome : cardTintFor(e->desc)));

    // COMPACT DROPS THE SECONDARY ROW AND KEEPS THE NAME.
    //
    // A compact state card that dropped its name too would disagree with a
    // compact value card, which keeps one, about what "less" means. Compact
    // drops the secondary row, full stop; showing the name is its own
    // setting: CardLabel.
    if (compact) lv_obj_add_flag  (_statusRow, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(_statusRow, LV_OBJ_FLAG_HIDDEN);

    if (showName) {
        const char *txt = nameText(lbl, *e, isOn);
        lv_label_set_text(_name, txt);
        // A LONG NAME NEVER TAKES THE HERO'S ROOM (a name on two lines would
        // push the disc up and clip it). The name always takes
        // one line's room in the column; a name that wraps gets its second
        // line only if both fit between the disc and the card's bottom (above
        // the status row in full), the pair centred there; otherwise one line,
        // cut with "...". The disc is never made smaller for a name - on a
        // dense board it is small enough already.
        const int32_t lh = lv_font_get_line_height(t.NAME);
        int32_t dy = nameShift;
        bool two = false;
        lv_point_t ts;
        lv_text_get_size(&ts, txt, t.NAME, 0, 0, surfaceWidthPx() - 2 * pad, LV_TEXT_FLAG_NONE);
        if (ts.y > lh) {
            // In the body's content coordinates, as compactNameShiftPx().
            // The status row only ever holds the "1/3" badge: when that is not
            // showing, its room is the second line's.
            const int32_t contH   = surfH - bodyTop - pad;
            const int32_t statusH = compact ? 0 : Card::statusBandHeight();
            const int32_t badgeH  = (isMixed && !compact) ? statusH : 0;
            const int32_t midH    = contH - lh - Card::midGap() - statusH;
            const int32_t top     = midH / 2 + discPx / 2 + UI::sc(2);        // under the disc
            const int32_t floor   = contH + pad - badgeH - UI::sc(2);         // over the edge or the badge
            if (floor - top >= 2 * lh) {
                two = true;
                dy = ((top + floor) / 2 - lh) - (contH - statusH - lh);       // centred, from where flex put it
            }
        }
        lv_label_set_long_mode(_name, two ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_DOT);
        lv_obj_set_height(_name, two ? 2 * lh : lh);
        lv_obj_set_style_margin_bottom(_name, two ? -lh : 0, 0);   // still one line in the column
        lv_obj_set_style_translate_y(_name, dy, 0);
        lv_obj_clear_flag(_name, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_name, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Mixed: its own indicator, not a guess ----------------------------
    if (isMixed && !compact) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u/%u", (unsigned)active, (unsigned)total);
        lv_label_set_text          (_mixed, buf);
        lv_obj_set_style_text_color(_mixed, UI::c(tone(ink(yMixed, p.TEXT_DIM))), 0);
        lv_obj_clear_flag          (_mixed, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_mixed, LV_OBJ_FLAG_HIDDEN);
    }

    // Last word, after the state block has set the real opacity: a disc too
    // small to enclose its own glyph is not drawn.
    if (!discFits) lv_obj_set_style_bg_opa(_disc, LV_OPA_TRANSP, 0);
}

const char *StateCard::nameText(CardLabel lbl, const Entity &e, bool isOn) {
    return (lbl == CardLabel::LBL_STATE) ? cardStateWord(e.desc, isOn) : shownName(lbl);
}

void StateCard::commandAll(bool on) {
    // liveEntities(): a group defined in HA is commanded as itself, unless a
    // member is paused - then its other members, one by one (K14).
    const Entity *live[CARD_LIVE_MAX];
    const uint8_t nLive = liveEntities(live);
    for (uint8_t i = 0; i < nLive; i++) {
        if (!counts(live[i])) continue;   // a paused member is out of the group
        commandEntity(live[i], EntityValue::makeBool(on));
    }
}
