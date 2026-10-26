// A MINI TILE (2.11, DECISIONS K89, K100, K104): a cluster's member, drawn inside
// the cluster with no border, header or card label of its own - its hero, and its
// entity label under it. It is still a whole card: its id and its settings, its
// tap, its window on a long press, its state (a STALE badge, the long-stale
// diagonal). The cluster sizes it (Card::setCellWidthPx / setCellHeightPx) and
// places it (Cards/ClusterCard.cpp).
//
// "On" is the LIT DISC (K100, the owner's C1): a solid disc in the active colour
// behind a dark glyph, against the faint disc of "off" - fill and shape tell
// them apart, never colour alone, and it is a solo card's hero in small. The
// owner's warning: "the disc itself can sometimes shrink beyond its useful limit
// if too crowded" - a disc that cannot hold its glyph is not drawn, and the glyph
// takes the colour instead.
#include "Cards/StateCard.h"
#include "Cards/ValueCard.h"
#include "Cards/CardIcons.h"
#include "Cards/CardPopup.h"
#include "UI/UITokens.h"
#include "UI/Kit.h"
#include <stdio.h>

using KS = Kit::KitStyle;

namespace {
lv_obj_t *plainBox(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    Kit::add(o, KS::KS_CARD_PLAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// A column that centres what is in it, the hero growing and the label under it.
void column(lv_obj_t *o) {
    lv_obj_set_flex_flow (o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

// Between the hero and the label.
int32_t heroGap() { return UI::mm(UI::shape().C_TILE_GAP); }

// Off the tile's top and foot (round 12): the label sat on the tile's foot, in
// its active frame's glow.
int32_t foot() { return UI::mm(UI::shape().C_TILE_FOOT); }

// THE TILE'S COLUMN (round 12, the owner: "no reason why the hero shouldn't be
// centered within the tile frame like it is for solo cards"): the hero and its
// label as one, centred in the tile, clear of its top and foot.
void tileColumn(lv_obj_t *body, lv_obj_t *mid) {
    lv_obj_set_style_pad_top   (body, foot(), 0);
    lv_obj_set_style_pad_bottom(body, foot(), 0);
    lv_obj_set_size(mid, lv_pct(100), LV_SIZE_CONTENT);
    column(mid);
}

// The disc a state tile w x h (inside its foot) gets with its label in face `nf`
// and `gap` between them: most of the room left over, never wider than most of
// the tile, capped at twice ICON - or twice ICON_XL where the room holds most of
// that (`roomy`, round 9). StateCard::renderMini() says why.
int32_t discFor(int32_t w, int32_t h, const lv_font_t *nf, int32_t gap, bool &roomy) {
    const UIType &t = UI::type();
    const int32_t xl = lv_font_get_line_height(t.ICON_XL);
    const int32_t room = h - (nf ? lv_font_get_line_height(nf) + gap : 0);
    int32_t d = LV_MIN(room * 92 / 100, w * 72 / 100);
    roomy = t.ICON_XL != t.ICON && d * 10 >= xl * 16;
    d = LV_MIN(d, (roomy ? xl : lv_font_get_line_height(t.ICON)) * 2);
    return d < 0 ? 0 : d;
}

// A BIGGER LABEL (round 12, the owner: tiles on the P4_5 and 1060 "could handle a
// bump in the entity label font size"): the name's face where it costs a state
// tile's disc nothing, the tag's otherwise. By the tile's size alone, so every
// tile of a cluster - state or value - has the same.
const lv_font_t *labelFace(int32_t w, int32_t h, int32_t gap) {
    const UIType &t = UI::type();
    bool roomy = false;
    if (t.NAME == t.TAG || discFor(w, h, t.NAME, gap, roomy) < discFor(w, h, t.TAG, gap, roomy)) return t.TAG;
    return t.NAME;
}

// The gap the tile's column keeps between its hero and its label: the theme's
// row padding, which its plain holders keep (round 4: a tile limited by its
// height, now clusters fill their cells, got a disc too big for its middle).
int32_t rowGap(lv_obj_t *mid) {
    lv_obj_t *col = mid ? lv_obj_get_parent(mid) : nullptr;
    return col ? lv_obj_get_style_pad_row(col, LV_PART_MAIN) : 0;
}

int32_t          s_pageDisc = 0;
const lv_font_t *s_pageFace = nullptr;
}  // namespace

// The page's hero (Card.h): kept here, beside the tiles that use it - Card.cpp
// is over the structure check's 1,000 lines.
void Card::setPageHero(int32_t discPx, const lv_font_t *face) { s_pageDisc = discPx; s_pageFace = face; }
int32_t          Card::pageDisc() { return s_pageDisc; }
const lv_font_t *Card::pageFace() { return s_pageFace; }

// The tile's frame: Card::build()'s, less everything a tile does not have.
void Card::buildMini(lv_obj_t *parent) {
    _root = plainBox(parent);

    // The tile itself is the surface: what a press lands on, what the window's
    // hold presses in. No border and no fill; tinted while pressed.
    _surface = lv_obj_create(_root);
    lv_obj_set_size(_surface, lv_pct(100), lv_pct(100));
    Kit::add(_surface, KS::KS_TILE);
    Kit::add(_surface, KS::KS_TILE_PRESSED, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_clear_flag(_surface, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag  (_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag  (_surface, LV_OBJ_FLAG_EVENT_BUBBLE);   // swipes still reach the screen
    lv_obj_add_event_cb(_surface, eventCb, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb(_surface, eventCb, LV_EVENT_LONG_PRESSED,  this);
    lv_obj_add_flag    (_surface, CardPopup::CARD_SURFACE_FLAG);
    lv_obj_set_user_data(_surface, this);

    // A failure never depends on a cosmetic choice (cards.md section 3): the
    // badge of a card with no header, at the tile's corner.
    _badge = lv_label_create(_surface);
    Kit::add(_badge, KS::KS_CARD_BADGE);
    Kit::add(_badge, KS::KS_CARD_TAGTEXT);
    lv_obj_align   (_badge, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(_badge, LV_OBJ_FLAG_HIDDEN);

    _body = plainBox(_surface);
    lv_obj_set_size(_body, lv_pct(100), lv_pct(100));

    buildBody(_body);
    resolveVariant();
    restyle();
}

// --- A state tile: the disc and the label -----------------------------------

void StateCard::buildMiniBody(lv_obj_t *body) {
    column(body);
    _mid = plainBox(body);
    tileColumn(body, _mid);

    _disc = lv_obj_create(_mid);
    lv_obj_clear_flag(_disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_disc, LV_OBJ_FLAG_CLICKABLE);
    Kit::add(_disc, KS::KS_CARD_DISC);
    _icon = lv_label_create(_disc);
    lv_obj_center(_icon);

    _name = lv_label_create(body);
    Kit::add(_name, KS::KS_TILE_NAME);
    lv_label_set_long_mode(_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(_name, lv_pct(100));
}

void StateCard::renderMini() {
    const Entity *e = primary();
    if (!e || !_disc) return;
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();

    const CardLabel lbl = cardResolveLabel(labelMode());
    const bool showName = (lbl != CardLabel::LBL_NONE);
    const bool on = groupIsOn();
    const char *words = !showName ? "" : lbl == CardLabel::LBL_STATE ? cardStateWord(e->desc, on) : shownName(lbl);
    const int32_t w = cellWidthPx();
    const int32_t h = cellPx() - 2 * foot();
    const int32_t gap = heroGap() + rowGap(_mid);

    // The disc: most of the room left over, never wider than most of the tile,
    // never larger than a solo card's at its largest (Card::heroDiscPx()) - and
    // THE SOLO CARDS' OWN wherever that much room is there (round 4: "it is
    // really not a good look when dashboard icons are different sizes"), unless
    // they are squeezed (round 6: the page sets none then, CardPage::commit()).
    // THE ROOMY HERO (round 9, the owner on the 1060: "the icons inside each
    // cluster are relatively tiny"): the cap was twice ICON, which is ~4.5 mm on
    // every board - a third of a 27 mm tile. Where the room holds most of twice
    // ICON_XL (the 7" boards' 1.5 x ICON; ICON elsewhere), that is the cap and face.
    const lv_font_t *nameFace = labelFace(w, h, gap);
    bool roomy = false;
    int32_t d = discFor(w, h, showName ? nameFace : nullptr, gap, roomy);
    const lv_font_t *face = t.ICON_SM;
    if (pageDisc() > 0 && pageFace() && pageDisc() <= d) {
        d    = pageDisc();
        face = pageFace();
    } else if (roomy) {
        face = t.ICON_XL;
    } else {
        // Less room than a solo card's: the largest face that sits comfortably on it.
        if (lv_font_get_line_height(t.ICON) * 100 <= d * 62)         face = t.ICON;
        else if (lv_font_get_line_height(t.ICON_MD) * 100 <= d * 62) face = t.ICON_MD;
    }
    const bool discFits = d >= lv_font_get_line_height(face) + UI::sc(4);
    lv_obj_set_size(_disc, d, d);
    lv_obj_set_style_pad_bottom(_mid, showName ? heroGap() : 0, 0);

    const uint32_t chrome = stateColor();   // stale or failed take over, as on a card
    uint32_t discBg = p.SURFACE_ALT, ink = chrome ? chrome : p.ST_IDLE;
    if (on && !chrome) {
        discBg = p.ST_ACTIVE;
        ink    = UI::contrastOf(p.ST_ACTIVE, p.GROUND, p.TEXT);
        // A light's own colour lights its disc, as on its card (cards.md section 13).
        const EntityAttrs &a = e->attrs;
        if (primaryCount() == 1 && a.hasRgb) {
            discBg = a.rgb;
            ink    = UI::contrastOf(a.rgb, p.GROUND, p.TEXT);
        }
    }
    if (discFits) {
        lv_obj_set_style_bg_opa  (_disc, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(_disc, UI::c(tone(discBg)), 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(ink)), 0);
    } else {
        // Too crowded for a disc: the glyph alone, lit in the active colour.
        lv_obj_set_style_bg_opa(_disc, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(_icon, UI::c(tone(on && !chrome ? discBg : ink)), 0);
    }
    lv_label_set_text         (_icon, cardHeroIcon(*e, on));
    lv_obj_set_style_text_font(_icon, face, 0);

    // THE CARD STATE, ON A TILE TOO (round 11, the owner's E - K116): an active
    // tile framed as a solo card is (Corner cut, Bracket), its lit disc kept.
    CardFrame frame = CardFrame::FRAME_NONE;
    if (on && !chrome) {
        if (s_fill == StateCardFill::FRAME_BRACKET) frame = CardFrame::FRAME_BRACKET;
        if (s_fill == StateCardFill::FRAME_CORNER)  frame = CardFrame::FRAME_CORNER;
    }
    activeFrame(frame, p.ST_ACTIVE);

    if (showName) {
        lv_label_set_text(_name, words);
        lv_obj_set_style_text_font (_name, nameFace, 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(p.TEXT)), 0);
        lv_obj_clear_flag(_name, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_name, LV_OBJ_FLAG_HIDDEN);
    }
}

// --- A value tile: the number and its unit, the label ---------------------------

void ValueCard::buildMiniBody(lv_obj_t *body) {
    column(body);
    _mid = plainBox(body);
    tileColumn(body, _mid);

    _valueRow = plainBox(_mid);
    lv_obj_set_size      (_valueRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow (_valueRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_valueRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    _value = lv_label_create(_valueRow);
    _unit  = lv_label_create(_valueRow);
    Kit::add(_unit, KS::KS_CARD_UNIT);

    _name = lv_label_create(body);
    Kit::add(_name, KS::KS_TILE_NAME);
    lv_label_set_long_mode(_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(_name, lv_pct(100));
}

void ValueCard::renderMini() {
    const Entity *e = primary();
    if (!e || !_value) return;
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();

    const CardLabel lbl = cardResolveLabel(labelMode());
    const bool showName = (lbl != CardLabel::LBL_NONE);
    const int32_t w = cellWidthPx();
    const int32_t h = cellPx() - 2 * foot();
    const char *words = showName ? shownName(lbl) : "";

    char buf[40];
    cardFormatValue(*e, buf, sizeof(buf), false, tempUnit());
    const char *unit = cardDisplayUnit(*e, tempUnit());
    lv_label_set_text(_value, buf);

    // The small value face where it fits the tile, else the name's.
    const lv_font_t *nameFace = labelFace(w, h, heroGap() + rowGap(_mid));
    const int32_t room = h - (showName ? lv_font_get_line_height(nameFace) + heroGap() + rowGap(_mid) : 0);
    const lv_font_t *vf = t.VALUE_SM;
    const int32_t uw = unit[0] ? Kit::textW(unit, t.UNIT) : 0;
    if (Kit::textW(buf, vf) + uw > w * 9 / 10 || lv_font_get_line_height(vf) > room) vf = t.NAME;
    lv_obj_set_style_text_font (_value, vf, 0);
    lv_obj_set_style_text_color(_value, UI::c(tone(p.TEXT)), 0);
    if (unit[0]) {
        lv_label_set_text(_unit, unit);
        lv_obj_set_style_text_color(_unit, UI::c(tone(p.TEXT_DIM)), 0);
        lv_obj_clear_flag(_unit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_unit, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_pad_bottom(_mid, showName ? heroGap() : 0, 0);

    if (showName) {
        lv_label_set_text(_name, words);
        lv_obj_set_style_text_font (_name, nameFace, 0);
        lv_obj_set_style_text_color(_name, UI::c(tone(p.TEXT)), 0);
        lv_obj_clear_flag(_name, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_name, LV_OBJ_FLAG_HIDDEN);
    }
}
