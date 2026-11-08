#include "Cards/Card.h"
#include "Cards/CardDefaults.h"
#include "Cards/CardIcons.h"
#include "Cards/CardPopup.h"
#include "UI/UITokens.h"
#include "UI/Kit.h"         // the card's roles
#include "UI/UIToolkit.h"   // the scene toast
#include "Cards/CardGroups.h"   // the card label's colour, following the HA area (K46)
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
        case CardState::ST_UNAVAILABLE: return "unavailable";
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
Card &Card::setSheetArea(const char *a) { copyBounded(_sheetArea, sizeof(_sheetArea), a ? a : ""); return *this; }
const char *Card::haArea() const { const Entity *p = primary(); return p ? p->sourceArea : ""; }

Card &Card::setPaused(bool p) {
    // THE FLAG LIVES ON THE ENTITY, NOT HERE.
    //
    // A card is a view; pausing is a statement about the thing. Writing it to
    // the registry means every card bound to the same entity agrees by
    // construction, a local sensor actually stops publishing, and the choice
    // survives a reboot - none of which a per-card bool could do.
    //
    // Applied to every primary. A card showing two things pauses both, because
    // a half-paused card is not a state anyone can read at a glance.
    // A group defined at the source pauses with its members (K17): paused
    // as a whole means every member is, as in a group defined here, and its
    // own report stops too.
    if (s_reg) {
        const uint32_t now = millis();
        for (uint8_t i = 0; i < _nPrimary; i++) {
            const Entity *e = _primary[i];
            if (!e) continue;
            s_reg->setPaused(e->desc.id, p, now);
            for (uint8_t k = 0; k < e->nMembers; k++)
                if (const Entity *m = s_reg->memberOf(*e, k)) s_reg->setPaused(m->desc.id, p, now);
        }
    }
    if (_root) { applyState(); render(); }
    return *this;
}

uint8_t Card::liveEntities(const Entity **out) const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < _nPrimary && n < CARD_LIVE_MAX; i++) {
        const Entity *e = _primary[i];
        if (!e) continue;
        bool anyPaused = false;
        if (s_reg && !e->paused)
            for (uint8_t k = 0; k < e->nMembers; k++) {
                const Entity *m = s_reg->memberOf(*e, k);
                if (m && m->paused) { anyPaused = true; break; }
            }
        if (!anyPaused) { out[n++] = e; continue; }
        for (uint8_t k = 0; k < e->nMembers && n < CARD_LIVE_MAX; k++)
            if (const Entity *m = s_reg->memberOf(*e, k)) out[n++] = m;
    }
    return n;
}

Card &Card::setGroupOn(GroupOn g) {
    if (g == _groupOn) return *this;
    _groupOn = g;
    if (_surface) render();
    return *this;
}

// A PAUSED MEMBER IS OUT OF THE GROUP (K14): not counted for
// on or off, not commanded, not failed, not stale - as if it were not there.
// Only when EVERY member is paused is the card itself paused, and then it shows
// the members as they were frozen.
bool Card::counts(const Entity *e) const {
    return e && (!e->paused || isPaused());
}

uint8_t Card::liveCount() const {
    const Entity *live[CARD_LIVE_MAX];
    const uint8_t nLive = liveEntities(live);
    uint8_t n = 0;
    for (uint8_t i = 0; i < nLive; i++) if (counts(live[i])) n++;
    return n;
}

uint8_t Card::activeCount() const {
    const Entity *live[CARD_LIVE_MAX];
    const uint8_t nLive = liveEntities(live);
    uint8_t n = 0;
    for (uint8_t i = 0; i < nLive; i++) {
        const Entity *e = live[i];
        if (!counts(e) || !e->everSet) continue;
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

bool Card::groupIsOn() const {
    const uint8_t n = activeCount(), live = liveCount();
    return _groupOn == GroupOn::GROUP_ON_ALL ? (live && n == live) : n > 0;
}

// Is this card paused? Asked of the ENTITIES, so a card built after the pause -
// or a second card on the same entity - reports it correctly without being told.
// EVERY primary: a group with one paused member is not paused (K14) - that
// member is simply out of it (counts()).
bool Card::isPaused() const {
    const Entity *live[CARD_LIVE_MAX];
    const uint8_t nLive = liveEntities(live);
    if (!nLive) return false;
    for (uint8_t i = 0; i < nLive; i++) if (!live[i]->paused) return false;
    return true;
}

const char *Card::label() const {
    if (_label[0]) return _label;
    const Entity *p = primary();
    return p ? p->desc.name : "";
}

Card &Card::setTapScene(const char *ref) {
    copyBounded(_tapScene, sizeof(_tapScene), ref ? ref : "");
    return *this;
}

// THE SCENES THIS CARD OFFERS: its one light's, filtered as its window filters
// them (CardPopup.cpp, shownScenes()) - so a tap and the window agree (K39).
uint8_t Card::offeredScenes(const Entity **out, uint8_t cap) const {
    if (!s_reg || _nPrimary != 1 || _sceneShow == SceneShow::SCENES_OFF) return 0;
    const Entity *h = _primary[0];
    if (!h || h->desc.kind != EntityKind::LIGHT || !h->desc.writable) return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < h->nScenes && n < cap; i++) {
        const Entity *sc = s_reg->sceneOf(*h, i);
        if (sc && (_sceneShow == SceneShow::SCENES_ALL || !sc->sourceHidden)) out[n++] = sc;
    }
    return n;
}

namespace {
// Where each card's Cycle scenes has got to, by card id: the card object is
// rebuilt with its page, the place in the cycle is not. RAM only, so every
// boot starts from the first scene (K37).
struct CyclePos { const char *id; uint8_t next; };
CyclePos s_cycle[16] = {};

uint8_t &cycleNext(const char *id) {
    static uint8_t spare = 0;
    if (!id || !*id) return spare;
    for (CyclePos &c : s_cycle) if (c.id && !strcmp(c.id, id)) return c.next;
    for (CyclePos &c : s_cycle) if (!c.id) { c.id = id; c.next = 0; return c.next; }
    return spare;
}

void sceneToast(const char *card, const char *scene) {
    char msg[96];
    snprintf(msg, sizeof(msg), "Scene: %s - %s", card, scene);
    UIToolkit::show_toast(msg);
}
}  // namespace

void Card::addGroupSwitch(const Entity *e) {
    if (!e || _nGroupSwitch >= CARD_LIVE_MAX) return;
    for (uint8_t i = 0; i < _nGroupSwitch; i++) if (_groupSwitch[i] == e) return;
    _groupSwitch[_nGroupSwitch++] = e;
}

void Card::toggleTap() {
    if (!_nGroupSwitch) { onTap(); return; }
    // The whole group, as the face shows it: a face that is on turns them all
    // off. A paused member is out of it (K14).
    const bool on = !groupIsOn();
    onTap();   // the face
    for (uint8_t i = 0; i < _nGroupSwitch; i++)
        if (!_groupSwitch[i]->paused) commandEntity(_groupSwitch[i], EntityValue::makeBool(on));
}

void Card::tap() {
    switch (_tap) {
        case TapAction::TAP_DEFAULT:
            // A card whose tap toggles nothing - a sensor, a binary sensor -
            // opens on its History.
            // A mixed group's solo card toggles nothing: Members (K188).
            if (_groupMixed) { CardPopup::openOn(*this, CardPopup::OPEN_MEMBERS); return; }
            if (!tapToggles()) { CardPopup::openOn(*this, CardPopup::OPEN_HISTORY); return; }
            toggleTap();
            return;
        case TapAction::TAP_TOGGLE:
            if (_groupMixed) { CardPopup::openOn(*this, CardPopup::OPEN_MEMBERS); return; }
            toggleTap();
            return;
        case TapAction::TAP_DETAILS: CardPopup::openOn(*this, CardPopup::OPEN_CONTROLS); return;
        case TapAction::TAP_MEMBERS: CardPopup::openOn(*this, CardPopup::OPEN_MEMBERS);  return;
        case TapAction::TAP_GROUP:   CardPopup::openOn(*this, CardPopup::OPEN_GROUP);    return;
        case TapAction::TAP_HISTORY: CardPopup::openOn(*this, CardPopup::OPEN_HISTORY);  return;
        case TapAction::TAP_NOTHING: return;
        case TapAction::TAP_CYCLE_SCENES:
        case TapAction::TAP_LOAD_SCENE: break;
    }
    // A scene. Not on a paused card: a paused light changes no state (K14).
    if (isPaused() || !s_reg) return;
    const Entity *sc[ENTITY_SCENES_MAX];
    const uint8_t n = offeredScenes(sc, ENTITY_SCENES_MAX);
    if (!n) {
        const Entity *p = primary();
        sceneToast(label(), (p && p->nScenes) ? "none offered" : "not loaded yet");
        return;
    }
    const Entity *pick = nullptr;
    if (_tap == TapAction::TAP_CYCLE_SCENES) {
        uint8_t &next = cycleNext(_id);
        pick = sc[next % n];
        next = (uint8_t)((next % n) + 1);
    } else {
        for (uint8_t i = 0; i < n && !pick; i++)
            if (!strcmp(sc[i]->desc.externalRef, _tapScene)) pick = sc[i];
        if (!pick) { sceneToast(label(), "not available"); return; }
    }
    const bool ok = s_reg->press(pick->desc.id, millis());
    sceneToast(label(), ok ? pick->desc.name : "refused");
}

// The name the card shows for a resolved Label (K40). "From HA" is the
// source's own name once it has said one; until then, and for anything not
// from HA, the card's own.
const char *Card::shownName(CardLabel lbl) const {
    if (lbl == CardLabel::LBL_HA) {
        const Entity *p = primary();
        if (p && p->sourceName[0]) return p->sourceName;
    }
    return label();
}

// The header band's real height.
//
// UIMetrics::HEADER_H is a token, but a fixed logical px height can be SMALLER
// THAN ITS OWN TEXT when the type scale grows, and the band would clip the word
// it exists to show. So the token is a floor: the band is whichever is larger,
// the token or the font's line height plus breathing room.
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
// What a FULL layout needs, in real pixels. Separate from resolveVariant() so
// that CardPage asks the same question before it decides how many rows to carve
// the page into - a guess that disagrees with this function is how a page plans
// a row no card can live in.
int32_t Card::fullCellNeedPx(CardHeaderStyle style, const lv_font_t *hero) {
    const UIType    &t = UI::type();
    const UIMetrics &m = UI::met();

    // Counts every band a FULL layout reserves. No top band: the corner icon is
    // out of the flow on both layouts, so it costs the stack nothing.
    int32_t need = lv_font_get_line_height(hero ? hero : t.VALUE)
                 + midGap()
                 + lv_font_get_line_height(t.NAME)
                 + statusBandHeight()
                 + UI::sc(m.PAD) * 2;

    // HDR_BAND reserves a strip inside the card. HDR_TAG takes its room from
    // OUTSIDE, so the caller subtracts it from the cell instead - see
    // resolveVariant(). HDR_NONE costs nothing.
    if (style == CardHeaderStyle::HDR_BAND) need += Card::headerHeight();

    // A MARGIN, because "it exactly fits" is not a safe answer. A font's line
    // box is taller than the ink in it, labels round up, and the body's own
    // padding is approximate here. Landing within a pixel or two of the cell
    // would let CYD_S3_3248 claim a full layout at 121 px and then overflow -
    // the name clipped in half and drawn over the status row.
    return need + UI::sc(8);
}

// The least a cell can be and still hold a card at all: the hero, the NAME and
// the padding, plus the same safety margin. A page will not plan a row shorter
// than this.
//
// The name is counted: compact drops the secondary row and nothing else, so
// the floor has to include a name or the page could plan a row that a compact
// card overflows.
int32_t Card::compactCellNeedPx() {
    const UIMetrics &m = UI::met();
    return lv_font_get_line_height(UI::type().VALUE)
         + midGap()
         + lv_font_get_line_height(UI::type().NAME)
         + UI::sc(m.PAD) * 2
         + UI::sc(8);
}

void Card::resolveVariant() {
    int32_t h = cellPx();

    // HDR_TAG hangs OUTSIDE the card, so the cell it leaves the body is
    // shorter by exactly the tag. HDR_TAG_FLOAT pays the same total, half of
    // it as a top inset inside the body - see tagRowPx().
    h -= tagRowPx() + tagInsetPx();

    const UIType &t = UI::type();
    const int32_t need   = fullCellNeedPx(_hdrStyle, t.VALUE);
    const int32_t needSm = fullCellNeedPx(_hdrStyle, t.VALUE_SM);

    // THE HERO STEPS DOWN A SIZE BEFORE THE CARD DROPS A ROW.
    //
    // With many cards on a small panel, a value or name would often fit if it
    // were smaller. So a cell too short for a full layout at VALUE tries
    // VALUE_SM first, and only goes compact if even that will not seat. Name,
    // unit and tag faces never change - only the one role that dominates the
    // vertical stack does.
    //
    //   full    at VALUE      the cell seats everything at full size
    //   full    at VALUE_SM   it does with a smaller number
    //   compact at VALUE_SM   it does not; drop the status row too
    if (h >= need) {
        _resolved = CardVariant::VAR_FULL;    _valueSmall = false;
    } else if (h >= needSm) {
        _resolved = CardVariant::VAR_FULL;    _valueSmall = true;
    } else {
        _resolved = CardVariant::VAR_COMPACT; _valueSmall = true;
    }

    // A pinned variant still gets the hero size ITS layout can carry. Compact
    // has no status row, so it keeps the full-size value in cells where full
    // would have had to shrink it - which is most of the point of compact, and
    // it is the fleet default.
    if (_variant != CardVariant::VAR_AUTO) {
        _resolved = _variant;
        if (_variant == CardVariant::VAR_FULL) {
            _valueSmall = (h < need);
        } else {
            int32_t needC = compactCellNeedPx();
            if (_hdrStyle == CardHeaderStyle::HDR_BAND) needC += Card::headerHeight();
            _valueSmall = (h < needC);
        }
    }
    // Beside tiles, the tiles' face (K126): a larger number would dwarf the
    // tile next to it and clip its corner icon.
    if (_besideTiles) _valueSmall = true;

    // Printed once per card: the numbers say WHY a card went compact rather
    // than leaving it to be inferred from what is missing on screen.
    DBG_CARDS("%s: cell %ld need %ld / %ld (sm) -> %s%s\n",
              typeName(), (long)h, (long)need, (long)needSm,
              cardVariantName(_resolved), _valueSmall ? ", small value" : "");
}

const lv_font_t *Card::valueFont() const {
    return _valueSmall ? UI::type().VALUE_SM : UI::type().VALUE;
}

int32_t Card::shortSidePx() const {
    int32_t h = cellPx();
    h -= tagRowPx() + tagInsetPx();
    const int32_t w = surfaceWidthPx();
    return (w < h) ? w : h;
}

// Long press opens the popup, on every card type (K3). Pause is reachable on
// every card in the popup's SETTINGS deck (card-sheet.md section 13).
void Card::onLongPress() {
    if (_longOpen != 0xFF) CardPopup::openHeldOn(*this, _longOpen);   // K156
    else                   CardPopup::open(*this);
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

void Card::build(lv_obj_t *parent) {
    if (_mini) { buildMini(parent); return; }   // a cluster's tile: Cards/MiniTile.cpp
    const UIMetrics &m = UI::met();

    // THE TAG LIVES INSIDE THE CELL, above the card.
    //
    // LV_OBJ_FLAG_OVERFLOW_VISIBLE stops a card's own root clipping its
    // children - but the GRID CONTAINER above it clips too, and that one is
    // scrollable, so anything drawn above a cell would be cut away. A tag
    // hanging outside the cell cannot be made to show reliably.
    //
    // A flex column inside the cell gets the result with none of that:
    //
    //     [ tag row   ]  headerHeight(), only in HDR_TAG
    //     [ surface   ]  everything else, flex_grow 1
    //
    // The card is shorter by exactly the tag's height, every card in the mode
    // takes that same height, and the space between a tag and the card above
    // it is the page's ordinary row gap, by construction. CardPage widens
    // nothing.
    _root = lv_obj_create(parent);
    Kit::add(_root, Kit::KitStyle::KS_CARD_ROOT);   // transparent, unpadded, a flex column
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_root, LV_OBJ_FLAG_CLICKABLE);
    // THE SHADOW LIVES OUTSIDE THIS WRAPPER, so the wrapper must not clip it.
    // The surface fills _root exactly, which puts every pixel of its shadow
    // outside its parent - and a parent clips its children. Overflow-visible
    // costs no layer: it REMOVES a clip rather than adding one. The page's gaps
    // and inset are where the shadows land.
    //
    // AND THE FLAG ALONE IS NOT ENOUGH: LVGL 9.5's lv_obj_redraw() (lv_refr.c)
    // clips an overflow-visible object's children to its coords PLUS ITS OWN
    // ext_draw_size - and a transparent wrapper has none, so the shadow would
    // show only in the rounded corners, cut off sharply at the card's edge. The
    // wrapper declares one, big enough for its surface's shadow -
    // UI::unclipShadows(), shared with every other container that lets a
    // child's shadow out (the drawer's button rows, the tag row below).
    UI::unclipShadows(_root);
    // A floating tag's side overhang comes out of the card's width, not out of
    // the gap: see buildHeader() and tagSidePx().
    lv_obj_set_style_pad_left     (_root, tagSidePx(), 0);

    if (cardHeaderIsTag(_hdrStyle)) {
        _tagRow = lv_obj_create(_root);
        lv_obj_set_width              (_tagRow, lv_pct(100));
        lv_obj_set_height             (_tagRow, tagRowPx());
        Kit::add(_tagRow, Kit::KitStyle::KS_CARD_PLAIN);
        lv_obj_clear_flag             (_tagRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag             (_tagRow, LV_OBJ_FLAG_CLICKABLE);
        // The area and STALE pills cast the scheme's shadow (Linen), and the
        // row they sit in is exactly their height - so it must not clip them.
        UI::unclipShadows(_tagRow);
    }

    _surface = lv_obj_create(_root);
    lv_obj_set_width              (_surface, lv_pct(100));
    lv_obj_set_flex_grow          (_surface, 1);
    Kit::add(_surface, Kit::KitStyle::KS_CARD_SURFACE);   // its look; the body owns padding
    lv_obj_clear_flag             (_surface, LV_OBJ_FLAG_SCROLLABLE);

    // NO CLIP_CORNER, EVEN FOR THE BAND.
    //
    // clip_corner makes LVGL render the object to an intermediate LAYER so it
    // can mask the rounded corners, and that layer is sized by the object's
    // WIDTH. A layer is a buffer allocation per card per frame; a two-cell
    // card is 482 px, so it asks for 32,776 bytes out of the lv_mem pool,
    // which already holds the page's other cards. On WS_P4_5 that failed and
    // retried forever ("lv_draw_layer_alloc_buf: Allocating layer buffer
    // failed"), with the system heap still healthy - draw buffers come from
    // lv_mem, not the heap - and the board froze (LESSONS.md).
    //
    // The band gets the card's own radius instead (see buildHeader() for how
    // its bottom stays straight).

    // LVGL fires LV_EVENT_SHORT_CLICKED for a tap and LV_EVENT_LONG_PRESSED
    // once the press passes its threshold. CLICKED is deliberately not used:
    // it also fires at the end of a long press, so a long press would run both.
    lv_obj_add_flag       (_surface, LV_OBJ_FLAG_CLICKABLE);
    // EVENTS BUBBLE UP TO THE SCREEN, which the swipe handling needs.
    //
    // A clickable object consumes its events, so a press that lands on a card
    // never reaches the screen - and the screen is where the swipe origin is
    // recorded. Without it the origin stays at its initial {0,0}, which reads
    // as "left half, top edge" for EVERY gesture.
    //
    // Bubbling ADDS the ancestors as listeners; it does not take the event
    // away from this card, so taps and long-presses still work.
    lv_obj_add_flag       (_surface, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb   (_surface, eventCb, LV_EVENT_LONG_PRESSED,  this);
    // A press here starts the popup's hold - the border fading to the accent
    // (CardPopup.cpp, "The hold, and the leap"). A flag rather than four more
    // event registrations per card: CardPopup hears every press on the input
    // device already. The surface carries its card, for the press look.
    lv_obj_add_flag       (_surface, CardPopup::CARD_SURFACE_FLAG);
    lv_obj_set_user_data  (_surface, this);

    buildHeader();

    _body = lv_obj_create(_surface);
    lv_obj_set_size               (_body, lv_pct(100), lv_pct(100));
    Kit::add(_body, Kit::KitStyle::KS_CARD_BODY);   // transparent, the card's padding
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_body, LV_OBJ_FLAG_CLICKABLE);
    if (_hdrStyle == CardHeaderStyle::HDR_BAND) {
        lv_obj_set_style_pad_top(_body, Card::headerHeight() + UI::sc(m.PAD), 0);
    } else if (_hdrStyle == CardHeaderStyle::HDR_TAG_FLOAT) {
        lv_obj_set_style_pad_top(_body, tagInsetPx() + UI::sc(m.PAD), 0);
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
    Kit::add(o, Kit::KitStyle::KS_CARD_STRIP);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void Card::buildHeader() {
    const bool tag   = cardHeaderIsTag(_hdrStyle);
    const bool float_ = (_hdrStyle == CardHeaderStyle::HDR_TAG_FLOAT);

    // HDR_NONE builds no area holder at all: in that mode the area is not
    // displayed, which makes it the mode to pick when you do not want one.
    if (_hdrStyle == CardHeaderStyle::HDR_NONE) {
        _badge = lv_label_create(_surface);
        Kit::add(_badge, Kit::KitStyle::KS_CARD_BADGE);
        Kit::add(_badge, Kit::KitStyle::KS_CARD_TAGTEXT);
        lv_obj_align   (_badge, LV_ALIGN_TOP_RIGHT, -UI::sc(6), UI::sc(6));
        lv_obj_add_flag(_badge, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // A tag's pills sit in the row above the card; a bar sits inside it.
    if (float_) {
        // A FLOATING tag's pills are children of the card's root, created here
        // - after the surface - so they draw ON TOP of it, and kept out of the
        // root's flex column. In the row above they would be drawn first and
        // the surface would paint over the half of each pill that lies on the
        // card.
        //
        // The AREA pill hangs over the top-left corner: up by the row, out by
        // tagSidePx() (half that). The out part is paid for INSIDE the cell:
        // the root's left padding (build()) moves the card right by it, and the
        // pill sits at the cell's own left edge, so the gap to the card on the
        // left is the page's ordinary gap.
        _header = makeStrip(_root);
        lv_obj_add_flag  (_header, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width (_header, LV_SIZE_CONTENT);
        lv_obj_align     (_header, LV_ALIGN_TOP_LEFT, -tagSidePx(), 0);

        // The STATUS pill (STALE, FAILED, PAUSED...) FOLLOWS the header style -
        // it floats on the top edge like the area pill - but never sticks out
        // past the card's right edge (it would hit the next card's tag). Inset
        // by half the card's radius so it clears the rounded corner. A long
        // area name gives way to it - see applyState().
        _stale = makeStrip(_root);
        lv_obj_add_flag  (_stale, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width (_stale, LV_SIZE_CONTENT);
        lv_obj_align     (_stale, LV_ALIGN_TOP_RIGHT, -UI::sc(UI::met().RADIUS) / 2, 0);

        // Lying on the card, they cast their shadow ONTO it on Linen. Same
        // paint as the attached pills.
        lv_obj_add_style (_header, UI::paint(UIPaint::PAINT_LIFT), 0);
        lv_obj_add_style (_stale,  UI::paint(UIPaint::PAINT_LIFT), 0);
        Kit::add(_header, Kit::KitStyle::KS_CARD_PILL);
        Kit::add(_stale,  Kit::KitStyle::KS_CARD_PILL);
    } else if (tag) {
        _header = makeStrip(_tagRow);

        // FLUSH with the card's left edge, the pills at the side edges of the
        // card, pushed inwards by their width.
        lv_obj_set_width (_header, LV_SIZE_CONTENT);
        lv_obj_align     (_header, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        // The STALE marker gets its own pill at the other end of the same row.
        _stale = makeStrip(_tagRow);
        lv_obj_set_width (_stale, LV_SIZE_CONTENT);
        lv_obj_align     (_stale, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

        // Both pills float above the page like the card does, so they cast
        // the same shadow - nothing on a scheme without one.
        lv_obj_add_style (_header, UI::paint(UIPaint::PAINT_LIFT), 0);
        lv_obj_add_style (_stale,  UI::paint(UIPaint::PAINT_LIFT), 0);
        Kit::add(_header, Kit::KitStyle::KS_CARD_PILL);
        Kit::add(_stale,  Kit::KitStyle::KS_CARD_PILL);
    } else {
        _header = makeStrip(_surface);
        lv_obj_set_width (_header, lv_pct(100));
        lv_obj_align     (_header, LV_ALIGN_TOP_MID, 0, 0);

        // ROUND TOP, STRAIGHT BOTTOM, AND NO LAYER. cards.md section 12: a
        // square band pokes square corners past the card's rounded top, and
        // clip_corner - the obvious fix - renders the card to a layer and
        // freezes WS_P4_5 (LESSONS.md). Giving the band the card's radius
        // rounds all four of its corners, but the bottom of the band must be
        // completely straight across the card.
        //
        // So two plain rectangles: the band, rounded like the card (applyState
        // sets it), and a SKIRT over its lower half with radius 0, in the same
        // colour, squaring the bottom corners. One small object per card; no
        // clipping, no layer. Created first so the labels draw over it. The
        // band's side padding moves onto the labels so the skirt can span the
        // band edge to edge.
        lv_obj_set_style_pad_left (_header, 0, 0);
        lv_obj_set_style_pad_right(_header, 0, 0);
        _bandSkirt = lv_obj_create(_header);
        lv_obj_remove_style_all(_bandSkirt);
        lv_obj_add_flag  (_bandSkirt, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_clear_flag(_bandSkirt, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(_bandSkirt, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size  (_bandSkirt, lv_pct(100), lv_pct(50));
        lv_obj_align     (_bandSkirt, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(_bandSkirt, LV_OPA_COVER, 0);
    }

    // Area left, STALE right - fixed in every mode, per cards.md section 2.
    // In bar mode the band has no side padding (see the skirt above), so the
    // labels carry the inset makeStrip() would have given them.
    const int32_t inset = _bandSkirt ? UI::sc(7) : 0;
    _lblArea = lv_label_create(_header);
    Kit::add(_lblArea, Kit::KitStyle::KS_CARD_TAGTEXT);   // in the surface's colour, on its tag or band
    lv_label_set_long_mode(_lblArea, LV_LABEL_LONG_DOT);
    lv_obj_align (_lblArea, LV_ALIGN_LEFT_MID, inset, 0);

    _badge = lv_label_create(_stale ? _stale : _header);
    Kit::add(_badge, Kit::KitStyle::KS_CARD_TAGTEXT);
    lv_obj_align (_badge, _stale ? LV_ALIGN_CENTER : LV_ALIGN_RIGHT_MID, -inset, 0);
}

// ---------------------------------------------------------------------------
// Styling
//
// Everything here re-reads UI::pal() and UI::met(). Nothing is cached, because
// the active palette is a mutable copy that setScheme() and setAccent() edit
// live - a card that cached a colour would simply not repaint.
// ---------------------------------------------------------------------------

// The surface's colour: the card's own, mixed toward the ground while paused. A
// mini tile keeps its role's faint panel (KS_TILE, K116) unless paused.
void Card::paintSurface() {
    const UIPalette &p = UI::pal();
    if (_mini && !_dimmed) lv_obj_remove_local_style_prop(_surface, LV_STYLE_BG_COLOR, 0);
    else lv_obj_set_style_bg_color(_surface, UI::c(_dimmed ? UI::mix(p.SURFACE, p.GROUND, 55) : p.SURFACE), 0);
}

void Card::restyle() {
    if (!_root) return;

    const UIPalette &p = UI::pal();
    const UIMetrics &m = UI::met();

    // The surface's radius, border and shadow, and the body's padding, are the
    // card's roles (KS_CARD_SURFACE, KS_CARD_BODY), refilled with the scheme.
    // The colour is the card's own: a paused card mixes it toward the ground.
    paintSurface();

    // BAND ONLY (and the floating tag's inset): HDR_NONE reserves nothing, as
    // build() and fullCellNeedPx() also treat it. restyle() runs at the end of
    // build(), so reserving a header's height here would win and leave every
    // No-hdr card an invisible empty band.
    if (_hdrStyle == CardHeaderStyle::HDR_BAND) {
        lv_obj_set_style_pad_top  (_body, Card::headerHeight() + UI::sc(m.PAD), 0);
    } else if (_hdrStyle == CardHeaderStyle::HDR_TAG_FLOAT) {
        lv_obj_set_style_pad_top  (_body, tagInsetPx() + UI::sc(m.PAD), 0);
    }
    if (_tagRow) lv_obj_set_height(_tagRow, tagRowPx());

    if (_header) {
        lv_obj_set_height          (_header, Card::headerHeight());
        // ONE LINE, fixed. LV_LABEL_LONG_DOT only shortens text that overflows
        // the label's HEIGHT; a content-height label wraps instead, and a
        // narrowed area name ("Outdoor" beside STALE on WS_P4_4B) would show as
        // its middle line, "doo". See the stale holder in applyState().
        lv_obj_set_height(_lblArea, lv_font_get_line_height(UI::type().TAG));
    }
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
        case CardState::ST_UNAVAILABLE: return p.ST_BAD;
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
        case CardState::ST_UNAVAILABLE: return p.ST_BAD;
        // Paused gets a colour of its own - muted, because the state means
        // "the user asked for quiet", but a colour nonetheless. With 0 its badge
        // would get no chip in bar mode and be painted the same hue as the band
        // it sits in, so it would not read as a badge at all.
        case CardState::ST_PAUSED:     return p.ST_IDLE;
        case CardState::ST_LIVE:       return 0;
    }
    return 0;
}

int32_t Card::topBandHeight() {
    // Plus a couple of pixels. An icon GLYPH can be taller than its font's
    // nominal line height - the MDI subset is drawn to its own metrics, not
    // Montserrat's - and a band sized to the line height alone clips the top
    // of the thermometer on CYD_S3_3248.
    return lv_font_get_line_height(UI::type().ICON_SM) + UI::sc(2);
}

int32_t Card::statusBandHeight() {
    // Whichever of the two things that can sit here is taller. The battery
    // glyph comes from the icon face and the percentage from the text face,
    // and sizing to only one of them would put a card with a battery's status
    // line lower than a card with just an age on it.
    const int32_t a = lv_font_get_line_height(UI::type().ICON_SM);
    const int32_t b = lv_font_get_line_height(UI::type().TAG);
    return ((a > b) ? a : b) + UI::sc(2);
}

int32_t Card::midGap() {
    return UI::sc(6);
}

// How tall this card's cell really is.
//
// The page measures it and hands it over, because spans are in grid UNITS and
// a unit's height is the page's business. The fallback is ONE WHOLE
// CELL rather than cellH * prefSpanY: multiplying by a unit span would double
// an ordinary card, and a card built outside a page is by definition one that
// nobody sized.
int32_t Card::cellPx() const {
    if (_cellPx > 0) return _cellPx;
    return (int32_t)UI::grid().cellH;
}

int32_t Card::midHeight() const {
    const UIMetrics &m = UI::met();

    int32_t h = cellPx();

    // ALWAYS charges for a header, whatever this card's mode is.
    //
    // A hero sized from the real band would grow in HDR_NONE, where there is
    // no header to pay for, and clip. Sizing to the most constrained mode makes
    // a card's hero identical in all modes, which is what a reader expects of
    // the same card wearing different chrome.
    h -= Card::headerHeight();

    h -= UI::sc(m.PAD) * 2;                       // the body's own padding
    h -= lv_font_get_line_height(UI::type().NAME);
    h -= midGap();
    if (_resolved != CardVariant::VAR_COMPACT) h -= statusBandHeight();

    // And never let a hero touch the edges of its band.
    h -= UI::sc(4);
    return h > 0 ? h : 0;
}

int32_t Card::surfaceHeightPx() const {
    return cellPx() - tagRowPx();
}

int32_t Card::bodyTopPx() const {
    const int32_t pad = UI::sc(UI::met().PAD);
    return pad + (_hdrStyle == CardHeaderStyle::HDR_BAND ? Card::headerHeight() : 0)
               + tagInsetPx();
}

// Half the pill above the card's edge, half on it. The ROW takes the upper
// half so the page's ordinary row gap still separates a tag from the card
// above it, exactly as in HDR_TAG.
int32_t Card::tagRowPx() const {
    if (_hdrStyle == CardHeaderStyle::HDR_TAG)       return Card::headerHeight();
    if (_hdrStyle == CardHeaderStyle::HDR_TAG_FLOAT) return Card::headerHeight() / 2;
    return 0;
}

int32_t Card::tagInsetPx() const {
    if (_hdrStyle == CardHeaderStyle::HDR_TAG_FLOAT)
        return Card::headerHeight() - Card::headerHeight() / 2;
    return 0;
}

// HALF the rise: the same height above, half the distance to the side.
// Sticking out as far to the left as above would make a 4x3 on the square
// boards narrow.
int32_t Card::tagSidePx() const {
    return (_hdrStyle == CardHeaderStyle::HDR_TAG_FLOAT) ? Card::headerHeight() / 4 : 0;
}

int32_t Card::surfaceWidthPx() const {
    const int32_t w = _cellWPx > 0 ? _cellWPx : (int32_t)UI::grid().cellW;
    return w - tagSidePx();
}

// LG, then MD, then SM: the largest glyph that fits the hero band with a
// margin. See StateCard::render() for why - a cramped card that kept LG would
// lose its disc.
const lv_font_t *Card::heroIconFace() const {
    const UIType &t = UI::type();
    const int32_t room = midHeight();
    if (room <= 0) return t.ICON;
    // The roomy hero: where most of its disc fits (the tiles' rule, MiniTile.cpp).
    if (t.ICON_XL != t.ICON && room * 10 >= lv_font_get_line_height(t.ICON_XL) * 16) return t.ICON_XL;
    if (room >= lv_font_get_line_height(t.ICON)    + UI::sc(4)) return t.ICON;
    if (room >= lv_font_get_line_height(t.ICON_MD) + UI::sc(4)) return t.ICON_MD;
    return t.ICON_SM;
}

// Twice the glyph's line height is the look on a large card, clamped to the
// band - a disc sized from the font alone is clipped by its own parent on
// CYD_S3_3248, and a disc larger than its band forces a layer (LESSONS.md).
int32_t Card::heroDiscPx() const {
    const int32_t room = midHeight();
    int32_t d = lv_font_get_line_height(heroIconFace()) * 2;
    if (room > 0 && d > room) d = room;
    return d;
}

int32_t Card::compactNameShiftPx() const {
    if (_resolved != CardVariant::VAR_COMPACT) return 0;

    // Everything in BODY CONTENT coordinates: 0 is the top of the padded body.
    // Compact has no status row, so the stack is [middle band][name], and the
    // hero is centred in the middle band - which ends a midGap above the name.
    const int32_t pad   = UI::sc(UI::met().PAD);
    const int32_t contH = surfaceHeightPx() - bodyTopPx() - pad;
    const int32_t nameH = lv_font_get_line_height(UI::type().NAME);
    const int32_t midH  = contH - nameH - midGap();
    if (contH <= 0 || midH <= 0) return 0;

    // The reference hero is the STATE card's disc, for both layouts, so that
    // a row mixing lights and temperatures keeps its names on one line.
    const int32_t heroBottom = midH / 2 + heroDiscPx() / 2;
    const int32_t cardBottom = contH + pad;            // the surface's edge
    const int32_t wantTop    = (heroBottom + cardBottom) / 2 - nameH / 2;
    const int32_t flowTop    = contH - nameH;          // where flex put it

    int32_t dy = wantTop - flowTop;
    // Never over the hero, never off the card.
    if (wantTop < heroBottom + UI::sc(2)) dy = heroBottom + UI::sc(2) - flowTop;
    if (flowTop + dy + nameH > cardBottom - UI::sc(2)) dy = cardBottom - UI::sc(2) - nameH - flowTop;
    return dy;
}

lv_obj_t *Card::makeCornerIcon(lv_obj_t *body) {
    lv_obj_t *o = lv_label_create(body);
    lv_obj_add_flag(o, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(o, LV_ALIGN_TOP_LEFT, 0, 0);
    _corner = o;
    return o;
}

// See Card.h. Every part is set every time, so the look at any (toAccent,
// sink) does not depend on the steps before it.
void Card::pressLook(uint8_t toAccent, int32_t sink) {
    if (!_surface) return;
    // The held look owns the edge; an active frame is not drawn until pressClear().
    _pressed = toAccent > 0 || sink > 0;
    const int32_t base = _mini ? 0 : UI::met().BORDER_W;   // a mini tile has no border of its own
    const int32_t wide = LV_MAX(base, UI::sc(2));
    const int32_t bw   = base + (wide - base) * toAccent / 255;
    lv_obj_set_style_border_color(_surface,
        lv_color_mix(UI::c(UI::pal().ACCENT), UI::border(), toAccent), 0);
    lv_obj_set_style_border_width(_surface, bw, 0);
    lv_obj_set_style_transform_width (_surface, -sink, 0);
    lv_obj_set_style_transform_height(_surface, -sink, 0);

    // What hangs on the edges goes in with them.
    if (_hdrStyle == CardHeaderStyle::HDR_BAND && _header) {
        // Down with the top edge, narrower with the sides. The skirt is the
        // band's own child, so it moves with it but must narrow by itself.
        // The wider border pushes the band in by itself (children sit inside
        // the border), so its corner follows the border's inner curve.
        lv_obj_set_style_translate_y     (_header, sink, 0);
        lv_obj_set_style_transform_width (_header, -sink, 0);
        lv_obj_set_style_radius          (_header, UI::sc(UI::met().RADIUS) - bw, 0);
        if (_bandSkirt) lv_obj_set_style_transform_width(_bandSkirt, -sink, 0);
    } else if (_hdrStyle == CardHeaderStyle::HDR_TAG) {
        // Pills standing on the top edge, flush with the sides.
        if (_header) { lv_obj_set_style_translate_x(_header,  sink, 0);
                       lv_obj_set_style_translate_y(_header,  sink, 0); }
        if (_stale)  { lv_obj_set_style_translate_x(_stale,  -sink, 0);
                       lv_obj_set_style_translate_y(_stale,   sink, 0); }
    } else if (_hdrStyle == CardHeaderStyle::HDR_NONE && _badge) {
        lv_obj_set_style_translate_x(_badge, -sink, 0);
        lv_obj_set_style_translate_y(_badge,  sink, 0);
    }
    if (_corner) {
        lv_obj_set_style_translate_x(_corner, sink, 0);
        lv_obj_set_style_translate_y(_corner, sink, 0);
    }
}

// Back to exactly what restyle() gives every card - and its state drawn again,
// an active frame with it.
void Card::pressClear() {
    pressLook(0, 0);
    if (_surface) {
        // Back to the surface's role (KS_CARD_SURFACE), which follows the scheme.
        lv_obj_remove_local_style_prop(_surface, LV_STYLE_BORDER_COLOR, 0);
        lv_obj_remove_local_style_prop(_surface, LV_STYLE_BORDER_WIDTH, 0);
        if (_root) render();
    }
}

// The active frames (Card::activeFrame()): CardFrame.cpp.

// THE CORNER ICON SCALES WITH THE CARD'S PHYSICAL HEIGHT.
//
// The larger icon stays where the card is tall enough, whatever the column
// count; once the hero moves up towards the icon (a fourth row on the 7-inch
// boards) the smaller icon is needed. A row count is not a size - three rows
// on a 5-inch panel are much shorter cards than three rows on a 7-inch one -
// and neither is a share of the card's short side, which varies with columns.
//
// So: MD when the card is at least CORNER_MD_MIN_MM tall, in millimetres on
// glass, whatever the columns. At the default grids that puts the 7B's
// three-row cards (~20-23 mm, deck and header or not) on MD and its four-row
// cards (~15 mm) on SM, and keeps WS_P4_5 (~16 mm) on SM. The 4-inch pair
// land near 19 mm and get MD.
static constexpr float CORNER_MD_MIN_MM = 17.5f;

const lv_font_t *Card::cornerFont() const {
    const float ppi = (float)bspPixelDensity();
    if (ppi <= 0.f) return UI::type().ICON_SM;
    const float hMm = (float)surfaceHeightPx() * 25.4f / ppi;
    return (hMm >= CORNER_MD_MIN_MM) ? UI::type().ICON_MD : UI::type().ICON_SM;
}

void Card::renderCornerIcon(lv_obj_t *icon, const char *glyph, uint32_t hex) const {
    if (!icon) return;
    const lv_font_t *f = cornerFont();
    lv_label_set_text          (icon, glyph);
    lv_obj_set_style_text_font (icon, f, 0);

    // GLUED TO THE CORNER: the same distance from the card's left edge as from
    // whatever bounds its top. The body's padding supplies that distance in
    // every mode; the label is flush to the body's top-left.
    //
    // In No-header mode one more thing is needed. A label's box is a LINE box
    // with leading above the ink, so a flush box puts the visible glyph lower
    // than it is far from the side. Measured from the font - see
    // cardGlyphTopBearing() - and applied in HDR_NONE only: in band and tag
    // modes the icon already sits where it should.
    const int32_t lift = (_hdrStyle == CardHeaderStyle::HDR_NONE)
                       ? cardGlyphTopBearing(f, glyph) : 0;
    lv_obj_align               (icon, LV_ALIGN_TOP_LEFT, 0, -lift);
    lv_obj_set_style_text_color(icon, UI::c(tone(hex)), 0);
}

// The area's colour, or the accent when it has none.
//
// This is what the header is FOR: areas differentiated by colour so cards can
// be ID'd visually at a glance. Band and tag colours are the same for cards
// within a group, so the name on them confirms what the colour already says.
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
    const bool tag      = cardHeaderIsTag(_hdrStyle);
    const uint32_t tc   = tagColor();

    // Staleness NEVER dims - cards.md section 3 rejects it outright, because a
    // dimmed card is easy to miss and stale data has to be conspicuous. Pause
    // is the one state that may go quiet, because it is the user's own choice.
    //
    // Dimmed by MIXING toward the ground rather than by setting an opa. An opa
    // below LV_OPA_COVER on a container makes LVGL render that whole subtree to
    // an intermediate layer buffer, and on a page of cards those allocations
    // are what exhausted the P4's draw buffers.
    // APPLIED HERE, not in restyle(): applyState() runs on every transition,
    // restyle() only on a scheme change, so a paused card would never dim.
    _dimmed = cardStateMayDim(_state);
    paintSurface();

    const char *mark = "";
    switch (_state) {
        case CardState::ST_STALE:      mark = "STALE";   break;
        case CardState::ST_LONG_STALE: mark = "STALE";   break;
        case CardState::ST_REFUSED:    mark = "FAILED";  break;
        case CardState::ST_PARTIAL:    mark = "PARTIAL"; break;
        case CardState::ST_PAUSED:     mark = "PAUSED";  break;
        // The SOURCE said so, which is a different claim from STALE and gets a
        // different word.
        case CardState::ST_UNAVAILABLE: mark = "N/A";    break;
        case CardState::ST_LIVE:       mark = "";        break;
    }

    // HDR_NONE has no area holder, so it is only ever the floating badge.
    if (!_header) {
        if (mark[0]) {
            // Its shape and words are its roles (KS_CARD_BADGE, KS_CARD_TAGTEXT).
            lv_label_set_text          (_badge, mark);
            lv_obj_set_style_bg_color  (_badge, UI::c(tc ? tc : p.ACCENT), 0);
            lv_obj_clear_flag          (_badge, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground     (_badge);
        } else {
            lv_obj_add_flag(_badge, LV_OBJ_FLAG_HIDDEN);
        }
        applyDiagonal();
        return;
    }

    // A card label in its own colour shows even with no words (K46).
    const bool wantArea = _showArea && (_area[0] || _labelAlways);
    // A GROUP SHOWN AS ITS SOLO CARD SAYS SO: a small group glyph
    // after its card label's words - the stock face's stacked squares, in the words'
    // colour, so no second face or object.
    char words[sizeof(_area) + 8];
    snprintf(words, sizeof(words), "%s%s%s", wantArea ? _area : "", wantArea && _groupCard && _area[0] ? " " : "",
             wantArea && _groupCard ? LV_SYMBOL_COPY : "");
    lv_label_set_text(_lblArea, words);
    lv_label_set_text(_badge,   mark);

    // --- the area holder ---------------------------------------------------
    if (tag) {
        // A pill only exists when it has something in it. Hiding it is safe
        // precisely because the clearance is the PAGE's, not the card's: a
        // card with no tag is exactly as tall as one with a tag.
        // A pill (KS_CARD_PILL) in its group's colour, the words in the surface's.
        lv_obj_set_style_bg_color  (_header, UI::c(headerColor()), 0);
        if (wantArea) lv_obj_clear_flag(_header, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag  (_header, LV_OBJ_FLAG_HIDDEN);
    } else {
        // HDR_BAND. The band keeps the group's colour and only the BADGE takes
        // the state colour: the whole header does not turn yellow, only the
        // badge section around STALE.
        lv_obj_set_style_bg_color  (_header, UI::c(headerColor()), 0);
        lv_obj_set_style_bg_opa    (_header, LV_OPA_COVER, 0);
        // THE CARD'S OWN CURVE on top, the skirt squaring the bottom - see
        // buildHeader(); a square band would stick out past the card's rounded
        // corners.
        //
        // The card's radius LESS ITS BORDER: LVGL places children inside the
        // border (lv_obj_get_style_space_left), so the band's corner meets the
        // border's INNER curve, whose radius is the card's minus the border
        // width. With the full radius a sliver of card shows in each corner -
        // invisible at 1 px, plain when the press widens the border.
        // pressLook() keeps it in step.
        lv_obj_set_style_radius    (_header, UI::sc(m.RADIUS) - m.BORDER_W, 0);
        if (_bandSkirt) lv_obj_set_style_bg_color(_bandSkirt, UI::c(headerColor()), 0);
        // Text in the card's BACKGROUND colour - dark on a light accent, light
        // on a dark one, without anyone picking per scheme (cards.md section 2):
        // the words' role, KS_CARD_TAGTEXT.
        // The badge carries the state, as its own chip inside the band, for
        // every state that has a colour, paused included.
        if (tc) {
            lv_obj_set_style_bg_color  (_badge, UI::c(tc), 0);
            lv_obj_set_style_bg_opa    (_badge, LV_OPA_COVER, 0);
            lv_obj_set_style_radius    (_badge, UI::sc(m.RADIUS / 2), 0);
            lv_obj_set_style_pad_hor   (_badge, UI::sc(5), 0);
        } else {
            lv_obj_set_style_bg_opa    (_badge, LV_OPA_TRANSP, 0);
        }
        // The band stays even when empty: it is part of the card's shape in
        // this mode, and appearing only sometimes would be worse than blank.
        lv_obj_clear_flag(_header, LV_OBJ_FLAG_HIDDEN);
    }

    // --- the stale holder, which only exists in tag mode --------------------
    if (_stale) {
        lv_obj_set_style_bg_color  (_stale, UI::c(tc ? tc : headerColor()), 0);   // a pill, KS_CARD_PILL
        if (mark[0]) lv_obj_clear_flag(_stale, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_add_flag  (_stale, LV_OBJ_FLAG_HIDDEN);

        // THE AREA GIVES WAY TO THE STATUS. Both pills share one edge, and on a
        // narrow card (WS_P4_4B at 4x3) "Outdoor" and "STALE" would overlap. The
        // status is the news, so the area name is shortened with an ellipsis
        // while a status is showing, and gets its full width back after.
        // Measured from the text, not from layout - see applyDiagonal() on why
        // a card does not lay itself out per repaint.
        int32_t maxW = LV_COORD_MAX;
        if (mark[0]) {
            const int32_t padH = UI::sc(7) * 2;   // makeStrip()'s side padding
            lv_point_t ts;
            lv_text_get_size(&ts, mark, UI::type().TAG, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            const int32_t staleW = ts.x + padH;
            const int32_t right  = (_hdrStyle == CardHeaderStyle::HDR_TAG_FLOAT)
                                 ? UI::sc(m.RADIUS) / 2 : 0;
            maxW = tagSidePx() + surfaceWidthPx() - right - staleW - UI::sc(4);
            if (maxW < padH) maxW = padH;
        }
        lv_obj_set_style_max_width(_header,  maxW, 0);
        lv_obj_set_style_max_width(_lblArea, maxW == LV_COORD_MAX ? LV_COORD_MAX
                                                                  : maxW - UI::sc(7) * 2, 0);
    }

    applyDiagonal();
}

// The loud treatment, shared by every mode: a corner-to-corner diagonal
// (cards.md section 3), which a header bar cannot express - which is why it
// lives outside the per-mode branching above.
void Card::applyDiagonal() {
    const bool wantDiag = (_state == CardState::ST_LONG_STALE ||
                           _state == CardState::ST_REFUSED   ||
                           _state == CardState::ST_UNAVAILABLE);
    if (!wantDiag) {
        if (_diagonal) { lv_obj_delete(_diagonal); _diagonal = nullptr; }
        return;
    }

    if (!_diagonal) {
        _diagonal = lv_line_create(_surface);
        Kit::add(_diagonal, Kit::KitStyle::KS_CARD_DIAG);   // its width, strength and round ends
        lv_obj_clear_flag(_diagonal, LV_OBJ_FLAG_CLICKABLE);
    }

    // Sized from what the PAGE handed over, not by measuring: a card has no
    // laid-out size when this first runs, and lv_obj_update_layout() here
    // would walk the whole screen once per card per repaint.
    //
    // The fallback is not UI::grid().cellW x prefSpanX: spans are UNITS, so a
    // one-cell card is prefSpan 2 and the line would be drawn for a card twice
    // the size until a later restyle, with the card laid out, read the real
    // size.
    int32_t w = surfaceWidthPx();
    int32_t h = surfaceHeightPx();
    const int32_t mw = lv_obj_get_width(_surface);
    const int32_t mh = lv_obj_get_height(_surface);
    if (mw > 8) w = mw;
    if (mh > 8) h = mh;

    // INSET BY THE CORNER, not just by the stroke.
    //
    // Half the line width covers the rounded cap overhanging its endpoint. It
    // does NOT cover the card's own RADIUS: a diagonal to the true corner of a
    // rounded rectangle leaves the shape well before it gets there. The
    // distance from the corner to the arc along a 45 degree line is
    // r*(1 - 1/sqrt(2)), about 0.293r.
    const int32_t half = UI::sc(3) + (UI::sc(UI::met().RADIUS) * 293) / 1000;
    _diagPts[0].x = half;
    _diagPts[0].y = half;
    _diagPts[1].x = w - half;
    _diagPts[1].y = h - half;
    lv_line_set_points            (_diagonal, _diagPts, 2);
    lv_obj_align                  (_diagonal, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_line_color   (_diagonal, UI::c(tagColor()), 0);
    lv_obj_move_foreground        (_diagonal);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

CardState Card::deriveState(uint32_t nowMs) const {
    // Order is the priority order, and it is deliberate. Pause is the user's,
    // so it outranks everything. A command
    // failure outranks staleness because the two mean different things -
    // cards.md section 3: stale is "I have not heard from this", refused is
    // "I told it to do something and it refused" - and the one the user just
    // caused is the one they need to see.
    // Read off the ENTITY - see setPaused().
    if (isPaused()) return CardState::ST_PAUSED;

    // UNAVAILABLE OUTRANKS EVERYTHING EXCEPT THE USER'S OWN CHOICE.
    //
    // Above staleness because it is a better class of evidence: stale is our
    // inference from silence, unavailable is the source's own statement. Above
    // the command states because a command against something that is not there
    // did not "fail" in any way worth reporting - saying FAILED would send the
    // user looking for a fault in the wrong place.
    //
    // ANY primary being unavailable is enough. A card showing two things, one
    // of which is gone, is not a card that can be trusted at a glance.
    // A paused member is out of the group (counts()), here and below - a
    // member of a group defined at the source too (liveEntities(), K17).
    const Entity *live[CARD_LIVE_MAX];
    const uint8_t nLive = liveEntities(live);
    for (uint8_t i = 0; i < nLive; i++) {
        const Entity *e = live[i];
        if (counts(e) && !e->available) return CardState::ST_UNAVAILABLE;
    }

    // THE FAILURE STATE IS READ OFF THE ENTITIES, NOT OFF THIS CARD.
    //
    // A card that tracked which of its own commands had failed would disagree
    // with a card bound to the same switch, because each only knows about
    // commands IT issued. A parent reflects where its children ARE, not the
    // path they took to get there, and reading Entity::cmdFailed makes that
    // true by construction: every card bound to an entity reads the identical
    // fact.
    uint8_t failed = 0, commandable = 0;
    for (uint8_t i = 0; i < nLive; i++) {
        const Entity *e = live[i];
        if (!counts(e) || !e->desc.writable) continue;
        commandable++;                     // counted whether or not it resolved
        if (!e->pending && e->cmdFailed) failed++;
    }
    if (failed) {
        // Every child that has an answer failed -> the card itself failed.
        // Some but not all -> the body is still telling the truth about the
        // children, so only the tag changes (cards.md section 3).
        // Against EVERY commandable child, not just the resolved ones. While
        // one child is still in flight the only child with a verdict may be a
        // failed one, and comparing against the resolved subset would make a
        // two-switch card flash FAILED on its way to PARTIAL.
        return (failed == commandable) ? CardState::ST_REFUSED
                                       : CardState::ST_PARTIAL;
    }

    CardState worst = CardState::ST_LIVE;
    for (uint8_t i = 0; i < nLive; i++) {
        const Entity *e = live[i];
        if (!counts(e)) continue;
        if (!e->desc.staleAfterMs) continue;   // 0 = never goes stale
        if (!e->everSet) { worst = CardState::ST_STALE; continue; }

        const uint32_t age = nowMs - e->lastUpdateMs;
        const uint32_t lng = _longStaleMs ? _longStaleMs : cardLongStaleMs(e->desc);

        if (age > lng)                   return CardState::ST_LONG_STALE;
        if (age > e->desc.staleAfterMs)  worst = CardState::ST_STALE;
    }
    return worst;
}

// Testing only: pins the card to a state (see Card.h).
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
    // measuring - and it is the ONLY thing this method does, because the
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
    for (uint8_t i = 0; i < _nPrimary; i++) {
        const Entity *e = _primary[i];
        if (!e) continue;
        if (strcmp(e->desc.id, entityId) == 0) return true;
        // A member of a group defined at the source repaints the card too
        // (K17): pausing one changes what the card stands for.
        for (uint8_t k = 0; s_reg && k < e->nMembers; k++) {
            const Entity *m = s_reg->memberOf(*e, k);
            if (m && strcmp(m->desc.id, entityId) == 0) return true;
        }
    }
    for (uint8_t i = 0; i < _nSecondary; i++)
        if (_secondary[i] && strcmp(_secondary[i]->desc.id, entityId) == 0) return true;
    return false;
}

// AND RENDERS. applyState() paints the surface its plain colour; render() is
// what puts an on card's fill back over it. Without it, a card that was on
// when HA's areas settled would keep its dark on-fill icon and name on a plain
// surface until its next value.
void Card::reapplyGroup() {
    if (!_root) return;
    CardGroups::apply(*this, _pageAreaColor);
    applyState();
    render();
}

void Card::onSnapshot(const Entity &snap) {
    (void)snap;   // see the threading note in Card.h: render() re-reads, and
                  // the snapshot is carried so that caching it later is a
                  // change to this file alone
    if (!_root) return;

    // A revert arrives here as a dirty snapshot, but a confirmation does not
    // arrive at all - so resolution deliberately does NOT live in this method.
    // It is polled, from pollState(). See the command masks in the header.

    // ITS GROUP CAN HANG ON HA'S AREAS (K46), learnt after the page is built,
    // once per session: whether its dashboard area is an HA area, and which.
    // Worked out again when its own entity's area arrives or changes - once,
    // not on every value.
    if (const Entity *p = primary()) {
        if (strncmp(p->sourceAreaId, _seenAreaId, sizeof(_seenAreaId) - 1) != 0) {
            copyBounded(_seenAreaId, sizeof(_seenAreaId), p->sourceAreaId);
            CardGroups::apply(*this, _pageAreaColor);
            applyState();
        }
    }
    render();
}

bool Card::command(uint8_t slot, const EntityValue &v) {
    if (slot >= _nPrimary) return false;
    return commandEntity(_primary[slot], v);
}

bool Card::commandEntity(const Entity *e, const EntityValue &v) {
    if (!e || !e->desc.writable) return false;
    if (!s_reg) return false;

    // No bookkeeping here. commandValue() clears the entity's own cmdFailed and
    // arms the reconcile window; EntityRegistry decides the outcome;
    // deriveState() reads it back.
    return s_reg->commandValue(e->desc.id, v, millis());
}

void Card::eventCb(lv_event_t *e) {
    Card *self = (Card *)lv_event_get_user_data(e);
    if (!self) return;
    // Modal. CardPopup's catcher takes every touch outside an open window
    // before a card sees it; this is the second lock on the same door - not
    // for a card inside the window (the group view's cluster, K154).
    if (CardPopup::isOpen() && !CardPopup::inWindow(*self)) return;
    switch (lv_event_get_code(e)) {
        case LV_EVENT_SHORT_CLICKED: self->tap();         break;   // the setting, then onTap()
        case LV_EVENT_LONG_PRESSED:  self->onLongPress(); break;
        default: break;
    }
}
