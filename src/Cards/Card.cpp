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

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void Card::build(lv_obj_t *parent) {
    const UIMetrics &m = UI::met();

    // The wrapper. Transparent, fills the grid cell, no padding of its own -
    // the header treatments differ in where they put things relative to the
    // surface, and this is the frame they are positioned against.
    _root = lv_obj_create(parent);
    lv_obj_set_style_bg_opa       (_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, 0, 0);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_CLICKABLE);

    // An external tag sits above the surface, so the surface has to start
    // below it. An internal bar and no bar both start at the top.
    const int32_t tagH = (_hdrStyle == CardHeaderStyle::HDR_EXTERNAL)
                       ? UI::sc(m.HEADER_H) : 0;

    // Padding the wrapper, rather than offsetting the surface, is what makes
    // the surface fill whatever is left without arithmetic - so the same
    // lv_pct(100) is correct for all three header treatments.
    if (tagH) lv_obj_set_style_pad_top(_root, tagH, 0);

    _surface = lv_obj_create(_root);
    lv_obj_set_size               (_surface, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all      (_surface, 0, 0);   // the body owns padding
    lv_obj_clear_flag             (_surface, LV_OBJ_FLAG_SCROLLABLE);

    // Touch. LVGL fires LV_EVENT_SHORT_CLICKED for a tap and
    // LV_EVENT_LONG_PRESSED once the press passes its threshold, which is the
    // pair cards.md asks for. CLICKED is deliberately not used: it also fires
    // at the end of a long press, so a long press would run both handlers.
    lv_obj_add_flag       (_surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_LONG_PRESSED,  this);

    buildHeader();

    // The body. Padded, transparent, and below the header bar when there is
    // one - which is what lets a subclass centre a value without having to
    // know whether a header exists.
    _body = lv_obj_create(_surface);
    lv_obj_set_width              (_body, lv_pct(100));
    lv_obj_set_style_bg_opa       (_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_body, 0, 0);
    lv_obj_set_style_pad_all      (_body, UI::sc(m.PAD), 0);
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_CLICKABLE);
    if (_header && _hdrStyle == CardHeaderStyle::HDR_INTERNAL) {
        lv_obj_set_height (_body, lv_pct(100));
        lv_obj_set_style_pad_top(_body, UI::sc(m.HEADER_H) + UI::sc(m.PAD), 0);
    } else {
        lv_obj_set_height (_body, lv_pct(100));
    }

    buildBody(_body);
    restyle();
}

void Card::buildHeader() {
    if (_hdrStyle == CardHeaderStyle::HDR_NONE) return;

    const UIMetrics &m = UI::met();
    const bool external = (_hdrStyle == CardHeaderStyle::HDR_EXTERNAL);

    // External tags hang off _root (they must escape the surface's clip);
    // internal bars are children of the surface and run edge to edge over the
    // border, which is what UIMetrics::HEADER_H's comment already anticipated.
    _header = lv_obj_create(external ? _root : _surface);
    lv_obj_set_height             (_header, UI::sc(m.HEADER_H));
    lv_obj_set_style_pad_all      (_header, 0, 0);
    lv_obj_set_style_pad_left     (_header, UI::sc(4), 0);
    lv_obj_set_style_pad_right    (_header, UI::sc(4), 0);
    lv_obj_set_style_border_width (_header, 0, 0);
    lv_obj_clear_flag             (_header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_header, LV_OBJ_FLAG_CLICKABLE);

    if (external) {
        lv_obj_set_width  (_header, LV_SIZE_CONTENT);
        lv_obj_align      (_header, LV_ALIGN_TOP_LEFT, UI::sc(m.RADIUS), 0);
    } else {
        lv_obj_set_width  (_header, lv_pct(100));
        lv_obj_align      (_header, LV_ALIGN_TOP_MID, 0, 0);
    }

    // Area left, STALE right. Fixed, per cards.md section 2 - and the
    // degenerate case of 2.8's configurable slot list, which is why this
    // deliberately reads as two named slots rather than as two labels.
    _lblArea = lv_label_create(_header);
    lv_obj_align (_lblArea, LV_ALIGN_LEFT_MID, 0, 0);

    _lblStale = lv_label_create(_header);
    lv_obj_align (_lblStale, LV_ALIGN_RIGHT_MID, 0, 0);
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
        lv_obj_set_style_pad_top  (_body, UI::sc(m.HEADER_H) + UI::sc(m.PAD), 0);
    }

    if (_header) {
        lv_obj_set_height             (_header, UI::sc(m.HEADER_H));
        lv_obj_set_style_bg_opa       (_header, LV_OPA_COVER, 0);
        lv_obj_set_style_radius       (_header, _hdrStyle == CardHeaderStyle::HDR_EXTERNAL
                                                ? UI::sc(m.RADIUS / 2) : 0, 0);
        // Text in the card's BACKGROUND colour, per cards.md section 2 - dark
        // text on a light accent, light text on a dark one, without anyone
        // having to pick per scheme.
        lv_obj_set_style_text_font (_lblArea,  UI::type().TAG, 0);
        lv_obj_set_style_text_color(_lblArea,  UI::c(p.SURFACE), 0);
        lv_obj_set_style_text_font (_lblStale, UI::type().TAG, 0);
        lv_obj_set_style_text_color(_lblStale, UI::c(p.SURFACE), 0);
    }

    applyState();
    render();
}

uint32_t Card::stateColor() const {
    const UIPalette &p = UI::pal();
    switch (_state) {
        case CardState::ST_STALE:      return p.ST_WARN;
        case CardState::ST_LONG_STALE: return p.ST_WARN;
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

    if (_header) {
        lv_label_set_text(_lblArea, _area);

        const char *tag = "";
        switch (_state) {
            case CardState::ST_STALE:      tag = "STALE";  break;
            case CardState::ST_LONG_STALE: tag = "STALE";  break;
            case CardState::ST_REFUSED:    tag = "FAILED"; break;
            case CardState::ST_PAUSED:     tag = "PAUSED"; break;
            case CardState::ST_LIVE:       tag = "";       break;
        }
        lv_label_set_text(_lblStale, tag);

        const uint32_t sc = stateColor();
        lv_obj_set_style_bg_color(_header, UI::c(sc ? sc : p.ACCENT), 0);

        // A header carrying nothing at all is chrome for its own sake, so it
        // hides rather than sitting there empty. The card keeps its geometry
        // either way, which is what stops a card resizing as it goes stale.
        const bool empty = !_area[0] && !tag[0];
        if (empty) lv_obj_add_flag   (_header, LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_clear_flag (_header, LV_OBJ_FLAG_HIDDEN);
    }

    // ST_LONG_STALE: the fat corner-to-corner diagonal. cards.md section 3
    // offers two candidates for the loud treatment - a growing tag or this -
    // and says both should be prototyped. This is the one the header bar
    // cannot express, so it is the one worth building; the growing tag is a
    // font-size change away if the owner prefers it on glass.
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
    lv_obj_set_style_line_color   (_diagonal, UI::c(stateColor()), 0);
    lv_obj_set_style_line_opa     (_diagonal, LV_OPA_60, 0);
    lv_obj_set_style_line_rounded (_diagonal, true, 0);
    lv_obj_move_foreground        (_diagonal);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

CardState Card::deriveState(uint32_t nowMs) const {
    // Order is the priority order, and it is a decision rather than an
    // accident. Pause is the user's, so it outranks everything. A refused
    // command outranks staleness because the two mean different things -
    // cards.md section 3: stale is "I have not heard from this", refused is
    // "I told it to do something and it refused" - and the one the user just
    // caused is the one they need to see.
    if (_paused)  return CardState::ST_PAUSED;
    if (_refused) return CardState::ST_REFUSED;

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
    if (!_cmdActive || !_cmdEnt) return false;

    // Still in flight. EntityRegistry::tick() owns the deadline and will clear
    // this flag one way or the other - either an echo arrives (setValue clears
    // it) or the reconcile window expires (tick reverts and clears it). The
    // card does not need a timer of its own, which is the whole reason
    // cards.md could say this needs no new plumbing.
    if (_cmdEnt->pending) return false;

    _cmdActive = false;
    _refused   = !_cmdEnt->value.equals(_cmdValue);
    return true;
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
    // It is polled, from pollState(). See _cmdActive in the header.
    render();
}

bool Card::command(const Entity *e, const EntityValue &v) {
    if (!e || !e->desc.writable) return false;

    if (!s_reg) return false;

    // A new command clears a previous refusal. The loud state is about the
    // action the user just took; leaving it up while they are mid-retry would
    // report the old failure as if it were the new one.
    if (_refused) { _refused = false; _state = CardState::ST_LIVE; applyState(); }

    if (!s_reg->commandValue(e->desc.id, v, millis())) return false;

    _cmdEnt    = e;
    _cmdValue  = v;
    _cmdActive = true;
    render();
    return true;
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
