#include "Cards/Card.h"
#include "Cards/CardDefaults.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <string.h>

EntityRegistry *Card::s_reg = nullptr;

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
static int32_t headerHeight() {
    const int32_t tok  = UI::sc(UI::met().HEADER_H);
    const int32_t text = lv_font_get_line_height(UI::type().TAG) + UI::sc(4);
    return (text > tok) ? text : tok;
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void Card::build(lv_obj_t *parent) {
    const UIMetrics &m = UI::met();

    // The wrapper: a FLEX COLUMN, transparent, filling the grid cell.
    //
    // It was padding plus absolute alignment, and that was wrong in a way the
    // owner caught immediately - lv_obj_align() positions against the parent's
    // CONTENT area, so padding the root to make room for an external tag moved
    // the tag down onto the card it was supposed to sit above, covering the
    // name in the top-left corner. A flex column has no such trap: the tag is
    // the first child, the surface is the second and takes what is left.
    _root = lv_obj_create(parent);
    lv_obj_set_style_bg_opa       (_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, 0, 0);
    lv_obj_set_style_pad_gap      (_root, 0, 0);
    lv_obj_set_flex_flow          (_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_CLICKABLE);

    // An external tag is built BEFORE the surface, because in a flex column
    // "above" means "first".
    if (_hdrStyle == CardHeaderStyle::HDR_EXTERNAL) buildHeader();

    _surface = lv_obj_create(_root);
    lv_obj_set_width              (_surface, lv_pct(100));
    lv_obj_set_flex_grow          (_surface, 1);
    lv_obj_set_style_pad_all      (_surface, 0, 0);   // the body owns padding
    lv_obj_clear_flag             (_surface, LV_OBJ_FLAG_SCROLLABLE);

    // Clip children to the rounded corners. Without this an internal header
    // bar - a full-width child with square corners - pokes out past the card's
    // radius and draws little rectangular ears over both top corners.
    lv_obj_set_style_clip_corner  (_surface, true, 0);

    // Touch. LVGL fires LV_EVENT_SHORT_CLICKED for a tap and
    // LV_EVENT_LONG_PRESSED once the press passes its threshold, which is the
    // pair cards.md asks for. CLICKED is deliberately not used: it also fires
    // at the end of a long press, so a long press would run both handlers.
    lv_obj_add_flag       (_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_LONG_PRESSED,  this);

    if (_hdrStyle == CardHeaderStyle::HDR_INTERNAL) buildHeader();

    // The state badge. Created ALWAYS, even with no header, which is the
    // owner's correction: FAILED must not be something a cosmetic choice can
    // switch off. With a header it lives in the header's right slot; without
    // one it floats over the card's top-right corner.
    _badge = lv_label_create(_header ? _header : _surface);
    if (_header) lv_obj_align(_badge, LV_ALIGN_RIGHT_MID, 0, 0);
    else         lv_obj_align(_badge, LV_ALIGN_TOP_RIGHT, UI::sc(-4), UI::sc(4));
    lv_obj_add_flag(_badge, LV_OBJ_FLAG_HIDDEN);

    // The body. Padded, transparent, and below the header bar when there is
    // one - which is what lets a subclass centre a value without having to
    // know whether a header exists.
    _body = lv_obj_create(_surface);
    lv_obj_set_size               (_body, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa       (_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_body, 0, 0);
    lv_obj_set_style_pad_all      (_body, UI::sc(m.PAD), 0);
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_CLICKABLE);
    if (_hdrStyle == CardHeaderStyle::HDR_INTERNAL) {
        lv_obj_set_style_pad_top(_body, headerHeight() + UI::sc(m.PAD), 0);
    }

    buildBody(_body);
    restyle();
}

void Card::buildHeader() {
    const UIMetrics &m = UI::met();
    const bool external = (_hdrStyle == CardHeaderStyle::HDR_EXTERNAL);

    _header = lv_obj_create(external ? _root : _surface);
    lv_obj_set_height             (_header, headerHeight());
    lv_obj_set_style_pad_all      (_header, 0, 0);
    lv_obj_set_style_pad_left     (_header, UI::sc(6), 0);
    lv_obj_set_style_pad_right    (_header, UI::sc(6), 0);
    lv_obj_set_style_border_width (_header, 0, 0);
    lv_obj_clear_flag             (_header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_header, LV_OBJ_FLAG_CLICKABLE);

    if (external) {
        // A tag, so it is only as wide as its content and sits at the left.
        // It is a flex child of _root, so no alignment is involved at all.
        lv_obj_set_width  (_header, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_width  (_header, lv_pct(100));
        lv_obj_align      (_header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_flag   (_header, LV_OBJ_FLAG_IGNORE_LAYOUT);
    }

    // Area left, STALE right. Fixed, per cards.md section 2 - and the
    // degenerate case of 2.8's configurable slot list, which is why this
    // deliberately reads as named slots rather than as two labels.
    _lblArea = lv_label_create(_header);
    lv_obj_align (_lblArea, LV_ALIGN_LEFT_MID, 0, 0);
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
    if (_header && _hdrStyle == CardHeaderStyle::HDR_INTERNAL) {
        lv_obj_set_style_pad_top  (_body, headerHeight() + UI::sc(m.PAD), 0);
    }

    if (_header) {
        lv_obj_set_height             (_header, headerHeight());
        lv_obj_set_style_bg_opa       (_header, LV_OPA_COVER, 0);
        lv_obj_set_style_radius       (_header, _hdrStyle == CardHeaderStyle::HDR_EXTERNAL
                                                ? UI::sc(m.RADIUS / 2) : 0, 0);
        // Text in the card's BACKGROUND colour, per cards.md section 2 - dark
        // text on a light accent, light text on a dark one, without anyone
        // having to pick per scheme.
        lv_obj_set_style_text_font (_lblArea, UI::type().TAG, 0);
        lv_obj_set_style_text_color(_lblArea, UI::c(p.SURFACE), 0);
    }

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

// The badge's colour, which is deliberately NOT stateColor(): a partial
// failure has a tag but no body takeover, so the two questions have to be
// asked separately.
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
    const UIPalette &p = UI::pal();

    // Staleness NEVER dims - cards.md section 3 rejects it outright, because a
    // dimmed card is easy to miss and stale data has to be conspicuous. Pause
    // is the one state that may go quiet, because it is the user's own choice.
    lv_obj_set_style_opa(_root, cardStateMayDim(_state) ? LV_OPA_40 : LV_OPA_COVER, 0);

    const char *tag = "";
    switch (_state) {
        case CardState::ST_STALE:      tag = "STALE";   break;
        case CardState::ST_LONG_STALE: tag = "STALE";   break;
        case CardState::ST_REFUSED:    tag = "FAILED";  break;
        case CardState::ST_PARTIAL:    tag = "PARTIAL"; break;
        case CardState::ST_PAUSED:     tag = "PAUSED";  break;
        case CardState::ST_LIVE:       tag = "";        break;
    }

    const uint32_t tc = tagColor();

    if (_header) {
        lv_label_set_text(_lblArea, _area);
        // The header takes the state's colour when there is one, so the whole
        // band reads as the warning rather than just the word in it.
        lv_obj_set_style_bg_color(_header, UI::c(tc ? tc : p.ACCENT), 0);

        // A header carrying nothing at all is chrome for its own sake, so it
        // hides rather than sitting there empty. The card keeps its geometry
        // either way, which is what stops a card resizing as it goes stale.
        const bool empty = !_area[0] && !tag[0];
        if (empty) lv_obj_add_flag   (_header, LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_clear_flag (_header, LV_OBJ_FLAG_HIDDEN);
    }

    if (_badge) {
        lv_obj_set_style_text_font (_badge, UI::type().TAG, 0);
        if (!tag[0]) {
            lv_obj_add_flag(_badge, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(_badge, tag);
            lv_obj_clear_flag(_badge, LV_OBJ_FLAG_HIDDEN);
            if (_header) {
                // In the header's right slot: the band is already coloured, so
                // the text is the card's background, per cards.md section 2.
                lv_obj_set_style_bg_opa    (_badge, LV_OPA_TRANSP, 0);
                lv_obj_set_style_text_color(_badge, UI::c(p.SURFACE), 0);
            } else {
                // Free-floating, so it has to carry its own colour. This is the
                // path that makes FAILED visible with HDR_NONE - the owner's
                // point that a failure signal must not depend on a cosmetic
                // choice.
                lv_obj_set_style_bg_color  (_badge, UI::c(tc ? tc : p.ACCENT), 0);
                lv_obj_set_style_bg_opa    (_badge, LV_OPA_COVER, 0);
                lv_obj_set_style_radius    (_badge, UI::sc(3), 0);
                lv_obj_set_style_pad_hor   (_badge, UI::sc(4), 0);
                lv_obj_set_style_pad_ver   (_badge, UI::sc(2), 0);
                lv_obj_set_style_text_color(_badge, UI::c(p.SURFACE), 0);
            }
            lv_obj_move_foreground(_badge);
        }
    }

    // The loud treatment. cards.md section 3 offers two candidates for it - a
    // growing tag or a corner-to-corner diagonal - and asks for both to be
    // prototyped. This is the one the header bar cannot express.
    //
    // A line rather than an image: one lv_line is a handful of bytes against
    // an over-drawn bitmap, and it scales with the cell without a redraw hook.
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
    // before that has happened. Without this the first diagonal drawn on a
    // page would be a zero-length point.
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

    if (_failMask) {
        // THE OWNER'S RULE for a card commanding several entities: a parent
        // says FAILED only when every child it commanded failed. Short of
        // that it keeps reporting what its children are actually doing and
        // raises a warning tag instead - because the body is still correct,
        // and replacing correct information with a failure banner loses more
        // than it tells.
        return (_failMask == _sentMask) ? CardState::ST_REFUSED
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
bool Card::resolveCommand() {
    if (!_cmdMask) return false;

    bool anyResolved = false;
    for (uint8_t i = 0; i < _nPrimary; i++) {
        const uint8_t bit = (uint8_t)(1u << i);
        if (!(_cmdMask & bit)) continue;

        const Entity *e = _primary[i];
        if (!e) { _cmdMask &= (uint8_t)~bit; anyResolved = true; continue; }

        // Still in flight. EntityRegistry::tick() owns the deadline and will
        // clear this one way or the other - either an echo arrives (setValue
        // clears it) or the reconcile window expires (tick reverts and clears
        // it). The card needs no timer of its own, which is the whole reason
        // cards.md could say this needs no new plumbing.
        if (e->pending) continue;

        _cmdMask &= (uint8_t)~bit;
        if (e->value.equals(_cmdValue)) _failMask &= (uint8_t)~bit;
        else                            _failMask |= bit;
        anyResolved = true;
    }
    return anyResolved;
}

void Card::pollState(uint32_t nowMs) {
    if (!_root) return;

    const bool resolved = resolveCommand();
    const CardState next = deriveState(nowMs);

    // The common case is neither: touch no LVGL at all. That is what makes a
    // 10 Hz poll over every card on a page cost nothing worth measuring.
    if (next == _state && !resolved) return;

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

    const uint8_t bit = (uint8_t)(1u << slot);

    // A new command clears this entity's previous failure. The loud state is
    // about the action the user just took; leaving it up while they are
    // mid-retry would report the old failure as if it were the new one.
    //
    // _sentMask is REBUILT per tap rather than accumulated, which is what the
    // owner's bug report exposed: the old code tracked a single _cmdEnt and
    // overwrote it on every entity in the loop, so a card commanding two
    // switches only ever watched the second one. That is why "Both" failed
    // unpredictably depending on which card had been tapped before it.
    if (!(_cmdMask & bit)) _failMask &= (uint8_t)~bit;

    if (!s_reg->commandValue(e->desc.id, v, millis())) return false;

    _cmdValue  = v;
    _cmdMask  |= bit;
    _sentMask |= bit;
    return true;
}

// Called by a subclass before it issues the commands for one tap, so that
// _sentMask describes THIS tap and not the union of every tap so far.
void Card::beginCommandBatch() {
    _sentMask = 0;
    _failMask = 0;
    if (_state == CardState::ST_REFUSED || _state == CardState::ST_PARTIAL) {
        _state = CardState::ST_LIVE;
        applyState();
    }
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
