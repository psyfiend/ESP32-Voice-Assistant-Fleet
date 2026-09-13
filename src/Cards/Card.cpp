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

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void Card::build(lv_obj_t *parent) {
    const UIMetrics &m = UI::met();

    // THE CARD IS THE SAME SIZE AND SHAPE IN ALL THREE MODES.
    //
    // The owner's rule, stated more than once because it is the thing two
    // earlier attempts broke: "the cards must not differ in shape or size
    // because of the tag". The surface fills the whole cell, always. A tag
    // hangs outside it, into clearance the PAGE carves from the row gap.
    _root = lv_obj_create(parent);
    lv_obj_set_style_bg_opa       (_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, 0, 0);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_CLICKABLE);
    // Without this LVGL clips the tag away and it simply is not there.
    lv_obj_add_flag               (_root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    _surface = lv_obj_create(_root);
    lv_obj_set_size               (_surface, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all      (_surface, 0, 0);   // the body owns padding
    lv_obj_clear_flag             (_surface, LV_OBJ_FLAG_SCROLLABLE);
    // An edge-to-edge band with square corners would otherwise draw little
    // rectangular ears over the card's rounded top corners.
    lv_obj_set_style_clip_corner  (_surface, true, 0);

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

    // The strip is reserved for the two modes that draw INSIDE the card, and
    // not for the one that does not. That is the whole trade between them: a
    // tag costs the card no space, a bar and in-card text each cost a strip.
    //
    // It is reserved whether or not there is anything in it right now, so that
    // a card's contents do not jump when it goes stale or loses its area.
    if (_hdrStyle != CardHeaderStyle::HDR_TAG) {
        lv_obj_set_style_pad_top(_body, Card::headerHeight() + UI::sc(m.PAD), 0);
    }

    buildBody(_body);
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

    // A tag parents to the CELL; a bar and plain text parent to the CARD. That
    // single line is the entire structural difference between the modes.
    _header = makeStrip(tag ? _root : _surface);

    if (tag) {
        // Sized to its own text and lifted clear of the card by its own
        // height, less a couple of pixels. That overlap is what makes it read
        // as attached to the card rather than floating above it.
        lv_obj_set_width      (_header, LV_SIZE_CONTENT);
        lv_obj_align          (_header, LV_ALIGN_TOP_LEFT, UI::sc(8),
                               -Card::headerHeight() + UI::sc(2));
        // Behind the card, so the card's own edge draws over the pill's bottom
        // and the two read as one shape.
        lv_obj_move_background(_header);

        // The STALE marker gets its own pill at the other end. In the two
        // in-card modes it is just the right-hand end of the same strip; here
        // there is no shared strip to sit in, because the card does not own
        // this space at all.
        _stale = makeStrip(_root);
        lv_obj_set_width      (_stale, LV_SIZE_CONTENT);
        lv_obj_align          (_stale, LV_ALIGN_TOP_RIGHT, -UI::sc(8),
                               -Card::headerHeight() + UI::sc(2));
        lv_obj_move_background(_stale);
    } else {
        lv_obj_set_width  (_header, lv_pct(100));
        lv_obj_align      (_header, LV_ALIGN_TOP_MID, 0, 0);
    }

    // Area left, STALE right - fixed in every mode, per cards.md section 2,
    // and the degenerate two-slot case of 2.8's configurable slot list.
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

    lv_obj_set_style_bg_color     (_surface, UI::c(p.SURFACE), 0);
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

    lv_obj_set_height          (_header, Card::headerHeight());
    lv_obj_set_style_text_font (_lblArea, UI::type().TAG, 0);
    lv_obj_set_style_text_font (_badge,   UI::type().TAG, 0);
    if (_stale) lv_obj_set_height(_stale, Card::headerHeight());

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
        case CardState::ST_LIVE:
        case CardState::ST_PAUSED:     return 0;
    }
    return 0;
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
    lv_obj_set_style_opa(_root, cardStateMayDim(_state) ? LV_OPA_40 : LV_OPA_COVER, 0);

    const char *mark = "";
    switch (_state) {
        case CardState::ST_STALE:      mark = "STALE";   break;
        case CardState::ST_LONG_STALE: mark = "STALE";   break;
        case CardState::ST_REFUSED:    mark = "FAILED";  break;
        case CardState::ST_PARTIAL:    mark = "PARTIAL"; break;
        case CardState::ST_PAUSED:     mark = "PAUSED";  break;
        case CardState::ST_LIVE:       mark = "";        break;
    }

    const bool wantArea = _showArea && _area[0];
    lv_label_set_text(_lblArea, wantArea ? _area : "");
    lv_label_set_text(_badge,   mark);

    // --- the area holder ---------------------------------------------------
    if (tag) {
        // A pill only exists when it has something in it. Hiding it is safe
        // precisely because the clearance is the PAGE's, not the card's: a
        // card with no tag is exactly as tall as one with a tag.
        lv_obj_set_style_bg_color  (_header, UI::c(p.ACCENT), 0);
        lv_obj_set_style_bg_opa    (_header, LV_OPA_COVER, 0);
        lv_obj_set_style_radius    (_header, UI::sc(m.RADIUS / 2), 0);
        lv_obj_set_style_text_color(_lblArea, UI::c(p.SURFACE), 0);
        if (wantArea) lv_obj_clear_flag(_header, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag  (_header, LV_OBJ_FLAG_HIDDEN);
    } else if (_hdrStyle == CardHeaderStyle::HDR_BAR) {
        // The whole band takes the state colour, so it reads as the warning
        // rather than just the word inside it.
        lv_obj_set_style_bg_color  (_header, UI::c(tc ? tc : p.ACCENT), 0);
        lv_obj_set_style_bg_opa    (_header, LV_OPA_COVER, 0);
        lv_obj_set_style_radius    (_header, 0, 0);
        // Text in the card's BACKGROUND colour - dark on a light accent, light
        // on a dark one, without anyone picking per scheme. cards.md section 2.
        lv_obj_set_style_text_color(_lblArea, UI::c(p.SURFACE), 0);
        lv_obj_set_style_text_color(_badge,   UI::c(p.SURFACE), 0);
        // The band stays even when empty: it is part of the card's shape in
        // this mode, and appearing only sometimes would be worse than blank.
        lv_obj_clear_flag(_header, LV_OBJ_FLAG_HIDDEN);
    } else {
        // HDR_NONE: no fill at all, just text in the strip the body left free.
        lv_obj_set_style_bg_opa    (_header, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(_lblArea, UI::c(p.TEXT_DIM), 0);
        lv_obj_set_style_text_color(_badge,   UI::c(tc ? tc : p.TEXT_DIM), 0);
        lv_obj_clear_flag(_header, LV_OBJ_FLAG_HIDDEN);
    }

    // --- the stale holder, which only exists in tag mode --------------------
    if (_stale) {
        lv_obj_set_style_bg_color  (_stale, UI::c(tc ? tc : p.ACCENT), 0);
        lv_obj_set_style_bg_opa    (_stale, LV_OPA_COVER, 0);
        lv_obj_set_style_radius    (_stale, UI::sc(m.RADIUS / 2), 0);
        lv_obj_set_style_text_color(_badge, UI::c(p.SURFACE), 0);
        if (mark[0]) lv_obj_clear_flag(_stale, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_add_flag  (_stale, LV_OBJ_FLAG_HIDDEN);
    }

    // --- the loud treatment -------------------------------------------------
    //
    // cards.md section 3 offers two candidates - a growing tag or a
    // corner-to-corner diagonal - and asks for both to be prototyped. This is
    // the one a header bar cannot express.
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

    // The surface's size comes from a percentage of a grid cell, so it is not
    // known until layout has run - and applyState() is reached from build()
    // before that has happened. Without this the first diagonal on a page
    // would be a zero-length point.
    lv_obj_update_layout(_surface);

    _diagPts[1].x = lv_obj_get_width(_surface);
    _diagPts[1].y = lv_obj_get_height(_surface);
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
    uint8_t failed = 0, resolved = 0;
    for (uint8_t i = 0; i < _nPrimary; i++) {
        const Entity *e = _primary[i];
        if (!e || !e->desc.writable) continue;
        if (e->pending) continue;          // still in flight; no verdict yet
        resolved++;
        if (e->cmdFailed) failed++;
    }
    if (failed) {
        // Every child that has an answer failed -> the card itself failed.
        // Some but not all -> the body is still telling the truth about the
        // children, so only the tag changes. cards.md and the owner agree on
        // this one from opposite directions.
        return (failed == resolved) ? CardState::ST_REFUSED
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
void Card::pollState(uint32_t nowMs) {
    if (!_root) return;

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
