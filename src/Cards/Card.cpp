#include "Cards/Card.h"
#include "Cards/CardDefaults.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <string.h>

EntityRegistry *Card::s_reg = nullptr;
bool            Card::s_showArea = true;

const char *cardStateName(CardState s) {
    switch (s) {
        case CardState::ST_LIVE:       return "live";
        case CardState::ST_STALE:      return "stale";
        case CardState::ST_LONG_STALE: return "long-stale";
        case CardState::ST_REFUSED:    return "refused";
        case CardState::ST_PARTIAL:    return "partial";
        case CardState::ST_PAUSED:     return "paused";
    }
    return "?";
}

const char *cardVariantName(CardVariant v) {
    switch (v) {
        case CardVariant::VAR_AUTO:    return "auto";
        case CardVariant::VAR_FULL:    return "full";
        case CardVariant::VAR_COMPACT: return "compact";
    }
    return "?";
}

// A card owns its widget tree. Deleting the root takes the whole subtree with
// it, which is also what returns the ~715 bytes to lv_mem rather than to the
// system heap - see docs/design/tokens.md on why those are different pools.
Card::~Card() {
    if (_root) lv_obj_delete(_root);
}

// ---------------------------------------------------------------------------
// Definition
// ---------------------------------------------------------------------------

Card &Card::bindPrimary(const Entity *e) {
    if (e && _nPrimary < CARD_PRIMARY_MAX) _primary[_nPrimary++] = e;
    return *this;
}

Card &Card::bindSecondary(const Entity *e) {
    if (e && _nSecondary < CARD_SECONDARY_MAX) _secondary[_nSecondary++] = e;
    return *this;
}

// Manual bounded copy rather than strncpy, matching EntityValue::makeText():
// guarantees termination and avoids the truncation warning strncpy produces at
// -Wall, which the entity library already had to solve the same way.
static void copyBounded(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    if (src) while (src[n] && n < cap - 1) { dst[n] = src[n]; n++; }
    dst[n] = '\0';
}

Card &Card::setLabel(const char *l) { copyBounded(_label, sizeof(_label), l); return *this; }
Card &Card::setArea (const char *a) { copyBounded(_area,  sizeof(_area),  a); return *this; }

Card &Card::setPaused(bool p) {
    _paused = p;
    if (_root) { applyState(); render(); }
    return *this;
}

const char *Card::label() const {
    if (_label[0]) return _label;
    const Entity *p = primary();
    return p ? p->desc.name : "";
}

// The header band's real height.
//
// UIMetrics::HEADER_H is a token and stays one, but a fixed 14 logical px is a
// height that can be SMALLER THAN ITS OWN TEXT - and when the type scale grows
// (which it must; see below) that is exactly what happens, and the band clips
// the word it exists to show. So the token is a floor, not a value: the band is
// whichever is larger, the token or the font's line height plus breathing room.
//
// Deriving rather than declaring is the same move tokens.md already made for UI
// scale, and for the same reason - a declared number is right on one board and
// wrong on seven.
int32_t Card::headerHeight() {
    const int32_t tok  = UI::sc(UI::met().HEADER_H);
    const int32_t text = lv_font_get_line_height(UI::type().TAG) + UI::sc(4);
    return (text > tok) ? text : tok;
}

// Which variant this card's cell can actually carry.
//
// Measured, not declared. The question is whether a FULL layout's rows fit at
// the sizes the type scale already settled on - and if they do not, the answer
// is to draw less, never to draw the same thing smaller. Shrinking text below
// the scale would undo the work gen_type_scale.py exists to do.
//
// The budget is the tallest thing each layout stacks: a title row, the hero,
// and an optional row, plus the padding between them. A card that cannot seat
// all three goes compact and drops the optional one.
void Card::resolveVariant() {
    if (_variant != CardVariant::VAR_AUTO) { _resolved = _variant; return; }

    const UIType    &t = UI::type();
    const UIMetrics &m = UI::met();
    const UIGrid    &g = UI::grid();

    // DERIVED FROM THE TOKENS, not measured off the widget.
    //
    // It used to call lv_obj_update_layout() and read the surface's height -
    // and got LVGL's default object size, because resolveVariant() runs from
    // build() and a card has no grid cell until placeCard() a moment later.
    // On WS_P4_5 that default is ~92 px against a real cell of 264, so every
    // card on the fleet's largest panel went compact and lost its name and
    // status row. The owner's report of "no text under the icons" was that.
    //
    // The cell height is knowable without asking LVGL anything: the grid
    // derived it, and the card knows its own row span.
    int32_t h = (int32_t)g.cellH * (_place.prefSpanY ? _place.prefSpanY : 1);
    if (_place.prefSpanY > 1) h += UI::sc(g.GAP) * (_place.prefSpanY - 1);
    if (_hdrStyle == CardHeaderStyle::HDR_TAG) h -= Card::headerHeight();

    // What a full layout needs: a title row, the hero, and an optional row,
    // plus the padding between them.
    // Counts every band a FULL layout reserves, which now includes the icon
    // line at the top. Leaving it out made CYD_S3_3248 cards claim they could
    // seat a full layout in 121 px when they could not, and the name was
    // clipped underneath the status row as a result.
    // No top band: the corner icon is out of the flow on both layouts, so it
    // costs the stack nothing. Charging for it here is what pushed the 3248
    // into compact and took away a status line that used to fit.
    int32_t need = lv_font_get_line_height(t.VALUE)
                 + midGap()
                 + lv_font_get_line_height(t.NAME)
                 + statusBandHeight()
                 + UI::sc(m.PAD) * 2;
    if (_hdrStyle == CardHeaderStyle::HDR_BAR) need += Card::headerHeight();

    // A MARGIN, because "it exactly fits" is not a safe answer.
    //
    // A font's line box is taller than the ink in it, labels round up, and the
    // body's own padding is approximate here. Landing within a pixel or two of
    // the cell meant CYD_S3_3248 claimed a full layout at 121 px and then
    // overflowed - the name clipped in half and drawn over the status row, and
    // in the two modes that also spend height on a header the status row was
    // pushed off the card entirely. Being slightly too eager to go compact
    // costs a status line; being slightly too reluctant breaks the card.
    need += UI::sc(8);

    _resolved = (h >= need) ? CardVariant::VAR_FULL : CardVariant::VAR_COMPACT;

    // Printed once per card, because this decision was guessed at twice and
    // both times the guess was wrong. The numbers are cheap and they end the
    // argument - and they say WHY a card went compact rather than leaving it
    // to be inferred from what is missing on screen.
    DBG_CARDS("%s: cell %ld need %ld -> %s\n",
              typeName(), (long)h, (long)need, cardVariantName(_resolved));
}

// Long press pauses, on every card type.
//
// cards.md section 3: a paused card is "the user's own choice rather than a
// failure, so quiet is correct" - it is the one state allowed to dim, and the
// one the user causes deliberately. Binding it to a long press makes PAUSED
// reachable on a real card rather than only through a test button, and it is
// the only whole-card action that makes sense on a read-only sensor as well as
// on a switch.
void Card::onLongPress() {
    setPaused(!_paused);
    pollState(millis());
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void Card::build(lv_obj_t *parent) {
    const UIMetrics &m = UI::met();

    // THE TAG LIVES INSIDE THE CELL, above the card. It used to hang outside
    // it, and it never once appeared on hardware.
    //
    // LV_OBJ_FLAG_OVERFLOW_VISIBLE stops a card's own root clipping its
    // children - but the GRID CONTAINER above it clips too, and that one is
    // scrollable, so anything drawn above a cell was cut away. All that showed
    // was the two-pixel overlap, and only once a paused card went translucent
    // enough to see through. Escaping two levels of clipping to sit in a gap
    // the page had to be widened to create was a lot of machinery for a thing
    // that cannot be made to work reliably.
    //
    // A flex column inside the cell gets the same result with none of it:
    //
    //     [ tag row   ]  headerHeight(), only in HDR_TAG
    //     [ surface   ]  everything else, flex_grow 1
    //
    // The card is shorter by exactly the tag's height, every card in the mode
    // takes that same height, and the space between a tag and the card above
    // it is the page's ordinary row gap - which is precisely the owner's rule,
    // now satisfied by construction rather than by arithmetic. CardPage no
    // longer widens anything.
    _root = lv_obj_create(parent);
    lv_obj_set_style_bg_opa       (_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, 0, 0);
    lv_obj_set_style_pad_gap      (_root, 0, 0);
    lv_obj_set_flex_flow          (_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_CLICKABLE);

    if (_hdrStyle == CardHeaderStyle::HDR_TAG) {
        _tagRow = lv_obj_create(_root);
        lv_obj_set_width              (_tagRow, lv_pct(100));
        lv_obj_set_height             (_tagRow, Card::headerHeight());
        lv_obj_set_style_bg_opa       (_tagRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width (_tagRow, 0, 0);
        lv_obj_set_style_pad_all      (_tagRow, 0, 0);
        lv_obj_clear_flag             (_tagRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag             (_tagRow, LV_OBJ_FLAG_CLICKABLE);
    }

    _surface = lv_obj_create(_root);
    lv_obj_set_width              (_surface, lv_pct(100));
    lv_obj_set_flex_grow          (_surface, 1);
    lv_obj_set_style_pad_all      (_surface, 0, 0);   // the body owns padding
    lv_obj_clear_flag             (_surface, LV_OBJ_FLAG_SCROLLABLE);

    // CLIP_CORNER ONLY WHERE SOMETHING ACTUALLY NEEDS CLIPPING.
    //
    // It forces LVGL to render the whole card to an intermediate LAYER so it
    // can mask the rounded corners - and a layer is a buffer allocation, per
    // card, per frame. With a page rebuilt while the old one is still alive
    // that is thirty-six allocations, and the P4 filled its draw buffers and
    // spewed "lv_draw_layer_alloc_buf: Allocating layer buffer failed" until
    // it was reset. Only an edge-to-edge header band needs it.
    if (_hdrStyle == CardHeaderStyle::HDR_BAR) {
        lv_obj_set_style_clip_corner(_surface, true, 0);
    }

    // LVGL fires LV_EVENT_SHORT_CLICKED for a tap and LV_EVENT_LONG_PRESSED
    // once the press passes its threshold. CLICKED is deliberately not used:
    // it also fires at the end of a long press, so a long press would run both.
    lv_obj_add_flag       (_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_LONG_PRESSED,  this);

    buildHeader();

    _body = lv_obj_create(_surface);
    lv_obj_set_size               (_body, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa       (_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_body, 0, 0);
    lv_obj_set_style_pad_all      (_body, UI::sc(m.PAD), 0);
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_CLICKABLE);
    if (_hdrStyle == CardHeaderStyle::HDR_BAR) {
        lv_obj_set_style_pad_top(_body, Card::headerHeight() + UI::sc(m.PAD), 0);
    }

    buildBody(_body);
    resolveVariant();
    restyle();
}

// One helper for the three near-identical containers, so the differences
// between the modes stay visible instead of being buried in repetition.
static lv_obj_t *makeStrip(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_height             (o, Card::headerHeight());
    lv_obj_set_style_pad_all      (o, 0, 0);
    lv_obj_set_style_pad_left     (o, UI::sc(7), 0);
    lv_obj_set_style_pad_right    (o, UI::sc(7), 0);
    lv_obj_set_style_border_width (o, 0, 0);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void Card::buildHeader() {
    const bool tag = (_hdrStyle == CardHeaderStyle::HDR_TAG);

    // HDR_NONE builds no area holder at all. The owner was explicit: "no header
    // mode: Area is not displayed." There is nowhere in that mode for an area
    // to go, which is what makes it the mode you pick when you do not want one.
    if (_hdrStyle == CardHeaderStyle::HDR_NONE) {
        _badge = lv_label_create(_surface);
        lv_obj_align   (_badge, LV_ALIGN_TOP_RIGHT, -UI::sc(6), UI::sc(6));
        lv_obj_add_flag(_badge, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // A tag's pills sit in the row above the card; a bar sits inside it.
    _header = makeStrip(tag ? _tagRow : _surface);

    if (tag) {
        // FLUSH with the card's left edge. They were inset and crowded each
        // other in the middle; the owner wants them "at the side edges of the
        // cards... and push inwards depending on width".
        lv_obj_set_width (_header, LV_SIZE_CONTENT);
        lv_obj_align     (_header, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        // The STALE marker gets its own pill at the other end of the same row.
        _stale = makeStrip(_tagRow);
        lv_obj_set_width (_stale, LV_SIZE_CONTENT);
        lv_obj_align     (_stale, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    } else {
        lv_obj_set_width (_header, lv_pct(100));
        lv_obj_align     (_header, LV_ALIGN_TOP_MID, 0, 0);
    }

    // Area left, STALE right - fixed in every mode, per cards.md section 2.
    _lblArea = lv_label_create(_header);
    lv_label_set_long_mode(_lblArea, LV_LABEL_LONG_DOT);
    lv_obj_align (_lblArea, LV_ALIGN_LEFT_MID, 0, 0);

    _badge = lv_label_create(_stale ? _stale : _header);
    lv_obj_align (_badge, _stale ? LV_ALIGN_CENTER : LV_ALIGN_RIGHT_MID, 0, 0);
}

// ---------------------------------------------------------------------------
// Styling
//
// Everything here re-reads UI::pal() and UI::met(). Nothing is cached, because
// the active palette is a mutable copy that setScheme() and setAccent() edit
// live - a card that cached a colour would simply not repaint.
// ---------------------------------------------------------------------------

void Card::restyle() {
    if (!_root) return;

    const UIPalette &p = UI::pal();
    const UIMetrics &m = UI::met();

    const uint32_t surf = _dimmed ? UI::mix(p.SURFACE, p.GROUND, 55) : p.SURFACE;
    lv_obj_set_style_bg_color     (_surface, UI::c(surf), 0);
    lv_obj_set_style_bg_opa       (_surface, LV_OPA_COVER, 0);
    lv_obj_set_style_radius       (_surface, UI::sc(m.RADIUS), 0);
    lv_obj_set_style_border_width (_surface, m.BORDER_W, 0);
    lv_obj_set_style_border_color (_surface, UI::border(), 0);
    lv_obj_set_style_shadow_width (_surface, UI::sc(m.SHADOW), 0);
    lv_obj_set_style_shadow_opa   (_surface, m.SHADOW ? LV_OPA_40 : LV_OPA_TRANSP, 0);

    lv_obj_set_style_pad_all      (_body, UI::sc(m.PAD), 0);
    if (_hdrStyle != CardHeaderStyle::HDR_TAG) {
        lv_obj_set_style_pad_top  (_body, Card::headerHeight() + UI::sc(m.PAD), 0);
    }

    if (_header) {
        lv_obj_set_height          (_header, Card::headerHeight());
        lv_obj_set_style_text_font (_lblArea, UI::type().TAG, 0);
    }
    lv_obj_set_style_text_font (_badge, UI::type().TAG, 0);
    if (_stale) lv_obj_set_height(_stale, Card::headerHeight());

    resolveVariant();
    applyState();
    render();
}

// Does the STATE take over the card's body? Only when the body would
// otherwise be lying. A partial failure returns 0 because the body is still
// telling the truth - see cardStateOwnsBody() in CardTypes.h.
uint32_t Card::stateColor() const {
    const UIPalette &p = UI::pal();
    if (!cardStateOwnsBody(_state)) return 0;
    switch (_state) {
        case CardState::ST_STALE:
        case CardState::ST_LONG_STALE: return p.ST_WARN;
        case CardState::ST_REFUSED:    return p.ST_BAD;
        default:                       return 0;
    }
}

// The badge's colour, deliberately NOT stateColor(): a partial failure has a
// tag but no body takeover, so the two questions are asked separately.
uint32_t Card::tagColor() const {
    const UIPalette &p = UI::pal();
    switch (_state) {
        case CardState::ST_STALE:
        case CardState::ST_LONG_STALE: return p.ST_WARN;
        case CardState::ST_PARTIAL:    return p.ST_WARN;
        case CardState::ST_REFUSED:    return p.ST_BAD;
        // Paused gets a colour of its own - muted, because the state means
        // "the user asked for quiet", but a colour nonetheless. Returning 0
        // meant its badge got no chip in bar mode and was painted the same
        // hue as the band it sat in, so it did not read as a badge at all.
        case CardState::ST_PAUSED:     return p.ST_IDLE;
        case CardState::ST_LIVE:       return 0;
    }
    return 0;
}

int32_t Card::topBandHeight() {
    // Plus a couple of pixels. An icon GLYPH can be taller than its font's
    // nominal line height - the MDI subset is drawn to its own metrics, not
    // Montserrat's - and a band sized to the line height alone clipped the top
    // of the thermometer on CYD_S3_3248.
    return lv_font_get_line_height(UI::type().ICON_SM) + UI::sc(2);
}

int32_t Card::statusBandHeight() {
    // Whichever of the two things that can sit here is taller. The battery
    // glyph comes from the icon face and the percentage from the text face,
    // and sizing to only one of them made a card with a battery sit its status
    // line lower than a card with just an age on it - which is exactly what
    // the owner spotted comparing the temperature and lux cards.
    const int32_t a = lv_font_get_line_height(UI::type().ICON_SM);
    const int32_t b = lv_font_get_line_height(UI::type().TAG);
    return ((a > b) ? a : b) + UI::sc(2);
}

int32_t Card::midGap() {
    return UI::sc(6);
}

// The area's colour, or the accent when it has none.
//
// The owner's idea, and it is what the header is FOR: "have the areas be
// differentiated by color so cards could be ID'd visually at a glance. Header
// bars and tag colors would be the same for cards within a group." Two cards in
// the same area therefore carry the same band, and the name on it becomes
// confirmation rather than the only cue.
uint32_t Card::headerColor() const {
    return _areaColor ? _areaColor : UI::pal().ACCENT;
}

uint32_t Card::tone(uint32_t hex) const {
    return _dimmed ? UI::mix(hex, UI::pal().GROUND, 55) : hex;
}

void Card::applyState() {
    if (!_root) return;
    const UIPalette &p  = UI::pal();
    const UIMetrics &m  = UI::met();
    const bool tag      = (_hdrStyle == CardHeaderStyle::HDR_TAG);
    const uint32_t tc   = tagColor();

    // Staleness NEVER dims - cards.md section 3 rejects it outright, because a
    // dimmed card is easy to miss and stale data has to be conspicuous. Pause
    // is the one state that may go quiet, because it is the user's own choice.
    //
    // Dimmed by MIXING toward the ground rather than by setting an opa. An opa
    // below LV_OPA_COVER on a container makes LVGL render that whole subtree to
    // an intermediate layer buffer, and on a page of cards those allocations
    // are what exhausted the P4's draw buffers.
    // APPLIED HERE, not in restyle(). Setting the flag and leaving the colour
    // to restyle() meant a paused card never actually dimmed: applyState()
    // runs on every transition, restyle() only on a scheme change. That is the
    // regression the owner spotted - "cards no longer dim when paused".
    _dimmed = cardStateMayDim(_state);
    lv_obj_set_style_bg_color(_surface,
        UI::c(_dimmed ? UI::mix(p.SURFACE, p.GROUND, 55) : p.SURFACE), 0);

    const char *mark = "";
    switch (_state) {
        case CardState::ST_STALE:      mark = "STALE";   break;
        case CardState::ST_LONG_STALE: mark = "STALE";   break;
        case CardState::ST_REFUSED:    mark = "FAILED";  break;
        case CardState::ST_PARTIAL:    mark = "PARTIAL"; break;
        case CardState::ST_PAUSED:     mark = "PAUSED";  break;
        case CardState::ST_LIVE:       mark = "";        break;
    }

    // HDR_NONE has no area holder, so it is only ever the floating badge.
    if (!_header) {
        if (mark[0]) {
            lv_label_set_text          (_badge, mark);
            lv_obj_set_style_bg_color  (_badge, UI::c(tc ? tc : p.ACCENT), 0);
            lv_obj_set_style_bg_opa    (_badge, LV_OPA_COVER, 0);
            lv_obj_set_style_radius    (_badge, UI::sc(m.RADIUS / 2), 0);
            lv_obj_set_style_pad_hor   (_badge, UI::sc(6), 0);
            lv_obj_set_style_pad_ver   (_badge, UI::sc(3), 0);
            lv_obj_set_style_text_color(_badge, UI::c(p.SURFACE), 0);
            lv_obj_clear_flag          (_badge, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground     (_badge);
        } else {
            lv_obj_add_flag(_badge, LV_OBJ_FLAG_HIDDEN);
        }
        applyDiagonal();
        return;
    }

    const bool wantArea = _showArea && _area[0];
    lv_label_set_text(_lblArea, wantArea ? _area : "");
    lv_label_set_text(_badge,   mark);

    // --- the area holder ---------------------------------------------------
    if (tag) {
        // A pill only exists when it has something in it. Hiding it is safe
        // precisely because the clearance is the PAGE's, not the card's: a
        // card with no tag is exactly as tall as one with a tag.
        lv_obj_set_style_bg_color  (_header, UI::c(headerColor()), 0);
        lv_obj_set_style_bg_opa    (_header, LV_OPA_COVER, 0);
        lv_obj_set_style_radius    (_header, UI::sc(m.RADIUS / 2), 0);
        lv_obj_set_style_text_color(_lblArea, UI::c(p.SURFACE), 0);
        if (wantArea) lv_obj_clear_flag(_header, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag  (_header, LV_OBJ_FLAG_HIDDEN);
    } else {
        // HDR_BAR. The band keeps the ACCENT and only the BADGE takes the state
        // colour - the owner's call after seeing a whole header go yellow:
        // "instead of the entire bar changing color only the badge section
        // should turn yellow around STALE".
        lv_obj_set_style_bg_color  (_header, UI::c(headerColor()), 0);
        lv_obj_set_style_bg_opa    (_header, LV_OPA_COVER, 0);
        lv_obj_set_style_radius    (_header, 0, 0);
        // Text in the card's BACKGROUND colour - dark on a light accent, light
        // on a dark one, without anyone picking per scheme. cards.md section 2.
        lv_obj_set_style_text_color(_lblArea, UI::c(p.SURFACE), 0);
        // The badge carries the state, as its own chip inside the band - and
        // now for every state that has a colour, paused included.
        if (tc) {
            lv_obj_set_style_bg_color  (_badge, UI::c(tc), 0);
            lv_obj_set_style_bg_opa    (_badge, LV_OPA_COVER, 0);
            lv_obj_set_style_radius    (_badge, UI::sc(m.RADIUS / 2), 0);
            lv_obj_set_style_pad_hor   (_badge, UI::sc(5), 0);
            lv_obj_set_style_text_color(_badge, UI::c(p.SURFACE), 0);
        } else {
            lv_obj_set_style_bg_opa    (_badge, LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(_badge, UI::c(p.SURFACE), 0);
        }
        // The band stays even when empty: it is part of the card's shape in
        // this mode, and appearing only sometimes would be worse than blank.
        lv_obj_clear_flag(_header, LV_OBJ_FLAG_HIDDEN);
    }

    // --- the stale holder, which only exists in tag mode --------------------
    if (_stale) {
        lv_obj_set_style_bg_color  (_stale, UI::c(tc ? tc : headerColor()), 0);
        lv_obj_set_style_bg_opa    (_stale, LV_OPA_COVER, 0);
        lv_obj_set_style_radius    (_stale, UI::sc(m.RADIUS / 2), 0);
        lv_obj_set_style_text_color(_badge, UI::c(p.SURFACE), 0);
        if (mark[0]) lv_obj_clear_flag(_stale, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_add_flag  (_stale, LV_OBJ_FLAG_HIDDEN);
    }

    applyDiagonal();
}

// The loud treatment, shared by every mode.
//
// cards.md section 3 offers two candidates - a growing tag or a
// corner-to-corner diagonal - and asks for both to be prototyped. This is the
// one a header bar cannot express, which is also why it has to live outside
// the per-mode branching above.
void Card::applyDiagonal() {
    const bool wantDiag = (_state == CardState::ST_LONG_STALE ||
                           _state == CardState::ST_REFUSED);
    if (!wantDiag) {
        if (_diagonal) { lv_obj_delete(_diagonal); _diagonal = nullptr; }
        return;
    }

    if (!_diagonal) {
        _diagonal = lv_line_create(_surface);
        lv_obj_clear_flag(_diagonal, LV_OBJ_FLAG_CLICKABLE);
    }

    // Sized from the TOKENS rather than by measuring, for the same reason
    // resolveVariant() is: a card has no cell when this first runs, and
    // lv_obj_update_layout() here walked the whole screen once per card per
    // repaint. The line is redrawn on the next restyle anyway.
    const UIGrid &g = UI::grid();
    int32_t w = (int32_t)g.cellW * (_place.prefSpanX ? _place.prefSpanX : 1);
    int32_t h = (int32_t)g.cellH * (_place.prefSpanY ? _place.prefSpanY : 1);
    if (_hdrStyle == CardHeaderStyle::HDR_TAG) h -= Card::headerHeight();
    const int32_t mw = lv_obj_get_width(_surface);
    const int32_t mh = lv_obj_get_height(_surface);
    if (mw > 8) w = mw;
    if (mh > 8) h = mh;

    // INSET BY THE CORNER, not just by the stroke.
    //
    // Half the line width covers the rounded cap overhanging its endpoint. It
    // does NOT cover the card's own RADIUS: a diagonal to the true corner of a
    // rounded rectangle leaves the shape well before it gets there, which is
    // why the line still appeared to escape at both ends. The distance from
    // the corner to the arc along a 45 degree line is r*(1 - 1/sqrt(2)),
    // about 0.293r.
    //
    // Bar mode looked right throughout only because clip_corner was masking
    // the overhang for it.
    const int32_t half = UI::sc(3) + (UI::sc(UI::met().RADIUS) * 293) / 1000;
    _diagPts[0].x = half;
    _diagPts[0].y = half;
    _diagPts[1].x = w - half;
    _diagPts[1].y = h - half;
    lv_line_set_points            (_diagonal, _diagPts, 2);
    lv_obj_align                  (_diagonal, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_line_width   (_diagonal, UI::sc(6), 0);
    lv_obj_set_style_line_color   (_diagonal, UI::c(tagColor()), 0);
    lv_obj_set_style_line_opa     (_diagonal, LV_OPA_60, 0);
    lv_obj_set_style_line_rounded (_diagonal, true, 0);
    lv_obj_move_foreground        (_diagonal);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

CardState Card::deriveState(uint32_t nowMs) const {
    // Order is the priority order, and it is a decision rather than an
    // accident. Pause is the user's, so it outranks everything. A command
    // failure outranks staleness because the two mean different things -
    // cards.md section 3: stale is "I have not heard from this", refused is
    // "I told it to do something and it refused" - and the one the user just
    // caused is the one they need to see.
    if (_paused) return CardState::ST_PAUSED;

    // THE FAILURE STATE IS READ OFF THE ENTITIES, NOT OFF THIS CARD.
    //
    // The card used to track which of its own commands had failed, in a
    // bitmask. That made a parent card and a child card bound to the same
    // switch disagree, because each only knew about commands IT had issued -
    // so "Both" reported something different depending on whether you had
    // tapped it or tapped one of its children. The owner's rule is that a
    // parent reflects where its children ARE, not the path they took to get
    // there, and reading Entity::cmdFailed makes that true by construction:
    // every card bound to an entity reads the identical fact.
    uint8_t failed = 0, commandable = 0;
    for (uint8_t i = 0; i < _nPrimary; i++) {
        const Entity *e = _primary[i];
        if (!e || !e->desc.writable) continue;
        commandable++;                     // counted whether or not it resolved
        if (!e->pending && e->cmdFailed) failed++;
    }
    if (failed) {
        // Every child that has an answer failed -> the card itself failed.
        // Some but not all -> the body is still telling the truth about the
        // children, so only the tag changes. cards.md and the owner agree on
        // this one from opposite directions.
        // Against EVERY commandable child, not just the resolved ones. While
        // one child is still in flight the only child with a verdict may be a
        // failed one, and comparing against the resolved subset made a
        // two-switch card flash FAILED for a second on its way to PARTIAL -
        // exactly what the owner saw tapping Obeys while Ignores was failed.
        return (failed == commandable) ? CardState::ST_REFUSED
                                       : CardState::ST_PARTIAL;
    }

    CardState worst = CardState::ST_LIVE;
    for (uint8_t i = 0; i < _nPrimary; i++) {
        const Entity *e = _primary[i];
        if (!e) continue;
        if (!e->desc.staleAfterMs) continue;   // 0 = never goes stale
        if (!e->everSet) { worst = CardState::ST_STALE; continue; }

        const uint32_t age = nowMs - e->lastUpdateMs;
        const uint32_t lng = _longStaleMs ? _longStaleMs : cardLongStaleMs(e->desc);

        if (age > lng)                   return CardState::ST_LONG_STALE;
        if (age > e->desc.staleAfterMs)  worst = CardState::ST_STALE;
    }
    return worst;
}

// True when an outstanding command's outcome just became known, so the caller
// knows it has to repaint the body as well as the chrome.
void Card::debugForceState(CardState s, bool force) {
    _forced = force;
    if (!_root) return;
    _state = force ? s : deriveState(millis());
    applyState();
    render();
}

void Card::pollState(uint32_t nowMs) {
    if (!_root) return;
    if (_forced) return;   // pinned by debugForceState()

    const CardState next = deriveState(nowMs);

    // The common case is no transition: touch no LVGL at all. That is what
    // makes a 10 Hz poll over every card on a page cost nothing worth
    // measuring - and it is now the ONLY thing this method does, because the
    // registry resolves commands itself.
    if (next == _state) return;

    _state = next;
    applyState();
    render();
}

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------

bool Card::owns(const char *entityId) const {
    if (!entityId || !entityId[0]) return false;
    for (uint8_t i = 0; i < _nPrimary; i++)
        if (_primary[i] && strcmp(_primary[i]->desc.id, entityId) == 0) return true;
    for (uint8_t i = 0; i < _nSecondary; i++)
        if (_secondary[i] && strcmp(_secondary[i]->desc.id, entityId) == 0) return true;
    return false;
}

void Card::onSnapshot(const Entity &snap) {
    (void)snap;   // see the threading note in Card.h: render() re-reads, and
                  // the snapshot is carried so that caching it later is a
                  // change to this file alone
    if (!_root) return;

    // A revert arrives here as a dirty snapshot, but a confirmation does not
    // arrive at all - so resolution deliberately does NOT live in this method.
    // It is polled, from pollState(). See the command masks in the header.
    render();
}

bool Card::command(uint8_t slot, const EntityValue &v) {
    if (slot >= _nPrimary) return false;
    const Entity *e = _primary[slot];
    if (!e || !e->desc.writable) return false;
    if (!s_reg) return false;

    // No bookkeeping left here at all. commandValue() clears the entity's own
    // cmdFailed and arms the reconcile window; EntityRegistry decides the
    // outcome; deriveState() reads it back. Three bitmasks and a resolver went
    // away when the fact moved to where it belonged.
    return s_reg->commandValue(e->desc.id, v, millis());
}

void Card::eventCb(lv_event_t *e) {
    Card *self = (Card *)lv_event_get_user_data(e);
    if (!self) return;
    switch (lv_event_get_code(e)) {
        case LV_EVENT_SHORT_CLICKED: self->onTap();       break;
        case LV_EVENT_LONG_PRESSED:  self->onLongPress(); break;
        default: break;
    }
}
