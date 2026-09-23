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

    // TWO bands, matching ValueCard: a growing middle and a reserved status
    // line. Neither layout reserves a row for its corner icon any more - that
    // one is out of the flow on both, which is what gave CYD_S3_3248 back the
    // height its status line needs.
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

    // The not-uniform badge, IN THE STATUS ROW. It used to sit out of the flow
    // at the top-left, which was free space until 2.7 gave every card a corner
    // icon there. The top-right is the floating STALE badge's in HDR_NONE, and
    // a mixed count is status - so it moved down to where status lives.
    _mixed = lv_label_create(_statusRow);
    lv_obj_align   (_mixed, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(_mixed, LV_OBJ_FLAG_HIDDEN);

    // The DOMAIN icon, top-left, out of the flow - the same object and the
    // same placement ValueCard uses. cards.md section 11: the corner says what
    // kind of card this is, the disc says which thing and in what state.
    _corner = makeCornerIcon(body);
}

bool StateCard::onFill(int32_t fromBottom, int pct) const {
    if (pct >= 100) return true;
    if (pct <= 0)   return false;
    // The surface's height, from the page-supplied cell. A tag hangs outside
    // the card, so it is not part of the surface.
    int32_t h = cellPx();
    if (headerStyle() == CardHeaderStyle::HDR_TAG) h -= Card::headerHeight();
    return fromBottom * 100 < h * pct;
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
    //
    // THE FACE STEPS DOWN WITH THE CELL, since 2.7 (#62). LG, then MD, then
    // SM: the largest whose glyph still fits the band with a margin. Before
    // this a cramped card kept the LG glyph and simply lost its disc, which is
    // what the owner saw on the 4B pair as "the disc is barely larger than its
    // glyph" and, on WS_S3_4B, as no disc at all.
    //
    // Both now live on Card - heroIconFace() and heroDiscPx() - so a value
    // card can place its name against the same reference. The clamp to the
    // band is there too: on CYD_S3_3248 the band is 55 px and twice the line
    // height wanted 68, so the disc was clipped by its own parent.
    const lv_font_t *heroFace = heroIconFace();
    const int32_t iconPx = lv_font_get_line_height(heroFace);
    const int32_t discPx = heroDiscPx();

    // AND NEVER SMALLER THAN THE GLYPH IT CONTAINS.
    //
    // The clamp above is against the band the disc sits in, and that band is
    // SHORTER in the full variant than in compact - full pays for a status row
    // that compact hides. So switching a card from compact to full made its
    // disc shrink, and on a tight cell it shrank past the icon: the owner saw
    // "the circle around the icons gets so small the icon is actually larger
    // than the circle" the moment No-hdr flipped his cards to full.
    //
    // A disc smaller than its own glyph is never the right answer. If the band
    // cannot hold that, the card is too small for this layout and the variant
    // logic is what should be giving way, not the geometry.
    // THE DISC NEVER EXCEEDS ITS BAND. The first attempt at the problem above
    // FLOORED it at the glyph size, which made it bigger than the band
    // containing it - and a child overflowing its parent is what forces LVGL
    // to render that parent to an intermediate LAYER so it can be clipped. On
    // WS_P4_5 a two-cell-wide card then asked for a 615x20 layer buffer,
    // failed, and the board froze on FIRST BOOT with nothing yet on screen.
    // That was worse than the bug it fixed.
    //
    // So when the band is too tight to hold a circle bigger than the glyph,
    // the circle is not drawn at all and the icon stands on its own. Draw
    // LESS, never the same thing smaller - the rule the variants already obey.
    const bool discFits = (discPx >= iconPx + UI::sc(4));
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
    // Only a single-entity card: an aggregate of several lights has no one
    // brightness to show, and averaging them would draw a level no light is
    // actually at. -1 means the entity does not report one - an on/off light,
    // a door - and such a card fills completely when on, exactly as before.
    const EntityAttrs &a = e->attrs;
    const bool single = (total == 1);
    int pct = 100;
    if (single && isOn && a.brightness >= 0) {
        pct = (a.brightness * 100 + 127) / 255;
        if (pct < 1) pct = 1;   // on is never drawn as empty
    }

    // Where each element's centre sits, measured up from the surface's bottom
    // edge, so each can pick the text colour for what is actually behind it.
    // From the tokens rather than from layout: see applyDiagonal() on why a
    // card does not call lv_obj_update_layout() per repaint.
    const UIMetrics &m = UI::met();
    const int32_t pad    = UI::sc(m.PAD);
    const int32_t status = compact ? 0 : Card::statusBandHeight();
    const int32_t nameH  = showName ? lv_font_get_line_height(t.NAME) : 0;
    int32_t surfH = cellPx();
    if (headerStyle() == CardHeaderStyle::HDR_TAG) surfH -= Card::headerHeight();
    const int32_t bodyTop = pad + (headerStyle() == CardHeaderStyle::HDR_BAR
                                   ? Card::headerHeight() : 0);
    const int32_t midBottom = pad + status + nameH + Card::midGap();
    const int32_t yName   = pad + status + nameH / 2 - nameShift;
    const int32_t yDisc   = (midBottom + (surfH - bodyTop)) / 2;
    const int32_t yCorner = surfH - bodyTop - lv_font_get_line_height(cornerFont()) / 2;
    const int32_t yMixed  = pad + status / 2;

    // Ink ON THE FILL is whichever of the scheme's ground and text colours
    // stands further from the active colour. It used to be SURFACE, which is
    // dark on the dark schemes and fine - and WHITE on a light scheme, on
    // yellow. The owner saw white icons on Paper, and the Desk icon (whose ink
    // already came from contrast, over its coloured disc) black beside them.
    // One rule for both makes them agree. Off the fill, the element's normal.
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
    // GLYPH takes the colour instead - the owner's "or the icon itself?".
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
    // It used to drop both, on cards.md section 4's reading that state IS the
    // icon and its colour - true, but it left a state card compact with no
    // name while a value card compact still had one, so the two types
    // disagreed about what "less" means. The owner's call, 2026-09-17: compact
    // drops the secondary row, full stop, and showing the name became its own
    // setting - which, since 2.7, it is: CardLabel.
    if (compact) lv_obj_add_flag  (_statusRow, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(_statusRow, LV_OBJ_FLAG_HIDDEN);

    if (showName) {
        lv_label_set_text(_name, lbl == CardLabel::LBL_STATE
                                 ? cardStateWord(e->desc, isOn) : label());
        lv_obj_set_style_text_font(_name, t.NAME, 0);
        lv_obj_set_style_translate_y(_name, nameShift, 0);
        lv_obj_clear_flag(_name, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_name, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Mixed: its own indicator, not a guess ----------------------------
    if (isMixed && !compact) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u/%u", (unsigned)active, (unsigned)total);
        lv_label_set_text          (_mixed, buf);
        lv_obj_set_style_text_font (_mixed, t.TAG, 0);
        lv_obj_set_style_text_color(_mixed, UI::c(tone(ink(yMixed, p.TEXT_DIM))), 0);
        lv_obj_clear_flag          (_mixed, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag            (_mixed, LV_OBJ_FLAG_HIDDEN);
    }

    // Last word, after the state block has set the real opacity: a disc too
    // small to enclose its own glyph is not drawn.
    if (!discFits) lv_obj_set_style_bg_opa(_disc, LV_OPA_TRANSP, 0);
}

void StateCard::commandAll(bool on) {
    for (uint8_t i = 0; i < primaryCount(); i++) {
        command(i, EntityValue::makeBool(on));
    }
}
