#pragma once
#ifndef CARD_H
#define CARD_H

#include <lvgl.h>
#include "Entity.h"
#include "EntityRegistry.h"
#include "Cards/CardTypes.h"

// ---------------------------------------------------------------------------
// Card - the base every tile on a dashboard derives from.
//
// The split, which is the whole design:
//
//   Card (here)   everything every card does the same way. Binding, spans,
//                 staleness, the refused-command state, the header bar,
//                 tap and long-press, and the root object styled from tokens.
//   subclasses    the body, and what a change looks like. ValueCard and
//                 StateCard are genuinely different widget trees; that is the
//                 two layout families of cards.md section 4.
//
// This shape came out of the owner's answer when asked to pick a class shape:
// he described what varies instead, and it turned out to be two independent
// axes - how many entities a card binds, and whether a card contains other
// cards. The first is handled here (CARD_PRIMARY_MAX). The second is a future
// ContainerCard deriving from Card, which is why nothing below assumes a card
// is a leaf. Milestone 2.4 builds the seam, not the container.
//
// ---------------------------------------------------------------------------
// THREADING, and the one thing that would break it
// ---------------------------------------------------------------------------
// Every method here runs on the LVGL thread and nowhere else. Values arrive
// through CardBinder, which is the only caller of EntityRegistry::drainDirty()
// and therefore satisfies ROADMAP 4.2's rule that bound widgets are updated on
// exactly one task.
//
// render() reads bound values THROUGH THE POINTER rather than out of the
// snapshot. That is the same deliberately-unlocked read EntityRegistry::at()
// already documents, and it is correct for the same reason: every provider in
// the tree today runs on loop(), which is also the LVGL thread. It is not
// cosmetic that onSnapshot() carries the snapshot anyway - the day a provider
// moves onto its own task, caching from that snapshot is the ONE change
// required, and it is local to this file.
//
// Two things a card must never do, both from CLAUDE.md and tokens.md:
//   - cache a colour. The active palette is a mutable copy so setScheme() and
//     setAccent() work live; a cached colour will not repaint. Read UI::pal()
//     inside render() and restyle(), every time.
//   - store a scaled pixel value. Run tokens through UI::sc() at point of use.
// ---------------------------------------------------------------------------

class Card {
public:
    virtual ~Card();

    // --- Definition. Call before build(); chainable. ----------------------

    // A primary is what the card is ABOUT. More than one means the card
    // renders a single state derived from all of them - the aggregate light.
    // Ignores nulls and silently stops at CARD_PRIMARY_MAX.
    Card &bindPrimary(const Entity *e);

    // A secondary is another entity OF THE SAME PHYSICAL DEVICE - battery,
    // last-seen. That rule, not a list, is cards.md section 1's definition,
    // and it is the test to apply before adding one. Nothing here enforces it;
    // it cannot be checked from the registry, because the registry has no
    // concept of a device. The build sheet is where it will be enforced.
    Card &bindSecondary(const Entity *e);

    Card &setPlacement(const CardPlacement &p) { _place = p; return *this; }

    // Pin the variant instead of letting the card measure its own cell.
    Card &setVariant(CardVariant v) { _variant = v; return *this; }

    // The resolved variant - never VAR_AUTO. Valid after build().
    CardVariant variant() const { return _resolved; }

    // The card's own label. Left unset, a card uses its primary's name - but
    // cards.md section 4 is emphatic that on a measure card the label is the
    // LOCATION, not the measurement ("Deck", not "Temperature"), because the
    // icon already says what kind of quantity it is. That removes a whole line
    // from every sensor card in the fleet, and it is why this override exists.
    Card &setLabel(const char *label);

    // Area, shown on the left of the header bar. Comes free from Home
    // Assistant, which already carries it on devices - MqttProvider can
    // populate it from discovery rather than anyone tagging entities by hand.
    Card &setArea(const char *area);

    Card &setHeaderStyle(CardHeaderStyle s) { _hdrStyle = s; return *this; }

    // Whether this card displays its area (or, later, a custom grouping) at
    // all. Separate from the header style on purpose - they are two
    // independent build-sheet settings, and the owner asked for exactly this
    // split: pick a presentation, and separately decide whether there is
    // anything to present.
    //
    // With area off, HDR_TAG has nothing to put in its left pill. The card
    // still reserves the same clearance, because a page of cards where some
    // have tags and some do not must not have some cards sitting higher than
    // others.
    Card &setShowArea(bool v) { _showArea = v; return *this; }

    // The page-wide default, so a build sheet can set this once rather than on
    // every card. Same pattern as StateCard::setFill().
    static void setShowAreaDefault(bool v) { s_showArea = v; }

    // Overrides cardLongStaleMs(). This is the build sheet's hook.
    Card &setLongStaleMs(uint32_t ms) { _longStaleMs = ms; return *this; }

    // A paused card is the user's own choice rather than a failure, so it is
    // the ONE state allowed to go quiet. See cards.md section 3.
    Card &setPaused(bool p);

    // --- Lifecycle --------------------------------------------------------

    // Build the widget tree under parent and draw the current values.
    void build(lv_obj_t *parent);

    // Re-read every token and repaint. Registered scheme-change hook.
    void restyle();

    lv_obj_t *root() const { return _root; }
    const CardPlacement &placement() const { return _place; }
    CardState state() const { return _state; }

    // --- CardBinder hooks. LVGL thread only. ------------------------------

    // Does this card render entityId? Used to route a dirty snapshot.
    bool owns(const char *entityId) const;

    // One of our bound entities changed. See the threading note above for why
    // the snapshot is passed even though render() re-reads.
    void onSnapshot(const Entity &snap);

    // Staleness is a POLL, not an event, and that is deliberate: an entity
    // going stale sets no dirty flag (EntityRegistry::tick() only reverts
    // optimistic writes), and making it do so would wake every card on the
    // page every tick just to discover that nothing had changed. Re-deriving
    // one subtraction per binding at 10 Hz is cheaper than that, and it only
    // touches LVGL when the state actually transitions.
    void pollState(uint32_t nowMs);

    virtual const char *typeName() const = 0;

    // --- Testing only -----------------------------------------------------
    //
    // Pin this card to a state and stop deriving one. It exists because the
    // states are otherwise almost impossible to SEE: nothing on a running
    // panel goes stale inside a test session - the Zigbee entities are on a
    // 30 minute window and the system ones are refreshed every 5 seconds - and
    // the long-stale threshold is an hour. Judging five visual treatments
    // would mean five waits or five builds.
    //
    // It overrides the derivation rather than faking the inputs, deliberately:
    // it is the RENDERING being judged here. Whether a card goes stale at the
    // right moment is a different question, tested by pulling the sensor's
    // battery, and no amount of forcing helps with that.
    //
    // Pass ST_LIVE with force=false to hand control back.
    void debugForceState(CardState s, bool force = true);

    // Where a commanded value goes. Set once by CardBinder::begin().
    //
    // Static rather than a per-card pointer: there is exactly one registry per
    // device by construction - the same reasoning behind GUIManager's s_self -
    // and storing the same four bytes in every card on every page is waste
    // that buys nothing. If a second registry ever exists, this becomes a
    // member and CardBinder passes its own.
    static void useRegistry(EntityRegistry *r) { s_reg = r; }

    // The height of the header strip, derived rather than declared:
    // UIMetrics::HEADER_H is a floor, and the real value is whichever is
    // larger, that token or the TAG font's line height plus breathing room. A
    // fixed 14 logical px was a band smaller than its own text once the type
    // scale grew.
    //
    // Public because a PAGE needs it: an external tag hangs ABOVE its card, in
    // the grid's row gap, so the page has to reserve that much clearance or
    // the tag lands on the card above. See CardPage::begin().
    static int32_t headerHeight();

    // How far an HDR_TAG pill rises above its card, in real pixels. The PAGE
    // needs this: the clearance comes out of the grid's row gap, not out of
    // the card. Returns the same value as headerHeight() today and is a
    // separate function because they answer different questions and only one
    // of them is the page's business.
    static int32_t tagOverhang() { return headerHeight(); }

protected:
    // --- Subclass contract ------------------------------------------------

    // Build the body. body is a padded, transparent container inside the card
    // surface, already sized and below any header bar.
    virtual void buildBody(lv_obj_t *body) = 0;

    // Draw current values and the current state into the body. Called after
    // build(), on every snapshot, on every state transition and on restyle,
    // so it must be idempotent and must not create widgets.
    virtual void render() = 0;

    // What a card does when touched. Default is nothing, which is correct for
    // a read-only sensor.
    virtual void onTap() {}

    // Long press. The BASE class implements this rather than leaving it empty,
    // because pausing is a property of every card regardless of type - cards.md
    // section 3's "per-card pause / ignore this entity", the one state allowed
    // to go quiet because it is the user's own choice rather than a failure.
    //
    // cards.md section 4 eventually wants a long press on a group to open a
    // sheet of per-light cards. That needs an overlay this milestone does not
    // have; when it arrives, a group card overrides this and everything else
    // keeps pausing.
    virtual void onLongPress();

    // --- Helpers for subclasses -------------------------------------------

    uint8_t primaryCount() const { return _nPrimary; }
    const Entity *primary(uint8_t i = 0) const {
        return (i < _nPrimary) ? _primary[i] : nullptr;
    }
    uint8_t secondaryCount() const { return _nSecondary; }
    const Entity *secondary(uint8_t i) const {
        return (i < _nSecondary) ? _secondary[i] : nullptr;
    }

    const char *label() const;

    // Optimistically command an entity and arm refusal detection.
    //
    // EntityRegistry::commandValue() applies the value immediately and starts
    // the reconcile window; if the echo never arrives, tick() reverts it and
    // dirties the entity. cards.md section 3 says that revert IS the "command
    // didn't take" event and needs no new plumbing - this is the code that
    // gives it a face. How the refusal is DETECTED is documented at the
    // command masks below, because it is not obvious.
    // `slot` is the index into the primary list, so a card commanding several
    // entities can tell afterwards WHICH of them refused. That distinction is
    // the whole of the owner's partial-failure rule.
    bool command(uint8_t slot, const EntityValue &v);

    // The card surface, for a subclass that tints its whole body - which
    // cards.md section 4 requires of a light ("reflects state across its whole
    // surface, not in a corner").
    lv_obj_t *surface() const { return _surface; }

    // Colour for the current state, or 0 when the state has no colour of its
    // own and the subclass should use its normal palette.
    uint32_t stateColor() const;

private:
    void buildHeader();
    void applyState();                 // repaint chrome for _state
    void applyDiagonal();              // the loud treatment, every mode
    void resolveVariant();             // measure the cell, pick FULL or COMPACT
    CardState deriveState(uint32_t nowMs) const;
    uint32_t tagColor() const;         // the badge's colour, which is NOT
                                       // stateColor() - see ST_PARTIAL
    static void eventCb(lv_event_t *e);

    // --- Widget tree ------------------------------------------------------
    //
    // _root is a TRANSPARENT wrapper filling the grid cell, and _surface is
    // the styled card inside it. That extra object exists for exactly one
    // reason: HDR_TAG bolts a tag to the top edge OUTSIDE the border, and
    // LVGL clips children to their parent, so the tag cannot be a child of the
    // thing it sits above. Making the wrapper unconditional keeps all three
    // header treatments one code path instead of two, which matters while both
    // are still being compared on glass.
    lv_obj_t *_root     = nullptr;
    lv_obj_t *_surface  = nullptr;
    lv_obj_t *_body     = nullptr;
    // The top section. Every card has one in every mode; what differs is where
    // it is parented and how it is painted.
    //
    //   HDR_BAR / HDR_NONE   _header spans the card's top strip and holds BOTH
    //                        labels - area left, badge right. _stale is unused.
    //   HDR_TAG              _header is the area pill and _stale is a second
    //                        pill on the right, both OUTSIDE the card.
    //
    // Two objects rather than one in tag mode because the two pills are sized
    // to their own content and sit at opposite ends of a strip the card does
    // not own - there is nothing for them to share.
    lv_obj_t *_header   = nullptr;   // area holder
    lv_obj_t *_stale    = nullptr;   // HDR_TAG only: the badge's own pill
    lv_obj_t *_lblArea  = nullptr;
    lv_obj_t *_badge    = nullptr;   // STALE / FAILED / PARTIAL
    lv_obj_t *_diagonal = nullptr;   // ST_LONG_STALE: corner to corner

    // The state tag has TWO HOMES, and that is the owner's correction: it used
    // to live only in the header, so choosing HDR_NONE silently disabled the
    // FAILED indicator. A failure signal must never depend on a cosmetic
    // choice. With a header the tag sits in its right-hand slot, exactly as
    // cards.md section 2 lays out; without one it floats at the card's
    // top-right. Same object, reparented at build time.

    // lv_line stores its points BY POINTER and does not copy them, so they
    // must outlive the widget - which rules out a local, and rules out one
    // shared static as well: cards do not all have the same geometry once a
    // card spans two cells, and a shared array would give every diagonal on
    // the page whichever card restyled last.
    lv_point_precise_t _diagPts[2] = {{0, 0}, {0, 0}};

    // --- Binding ----------------------------------------------------------
    const Entity *_primary[CARD_PRIMARY_MAX]     = {nullptr};
    const Entity *_secondary[CARD_SECONDARY_MAX] = {nullptr};
    uint8_t _nPrimary   = 0;
    uint8_t _nSecondary = 0;

    CardPlacement   _place;
    CardVariant     _variant  = CardVariant::VAR_AUTO;
    CardVariant     _resolved = CardVariant::VAR_FULL;
    CardHeaderStyle _hdrStyle    = CardHeaderStyle::HDR_NONE;
    CardState       _state       = CardState::ST_LIVE;
    uint32_t        _longStaleMs = 0;     // 0 = ask cardLongStaleMs()
    bool            _paused      = false;
    bool            _showArea    = s_showArea;
    bool            _forced      = false;   // debugForceState()

    char _label[ENTITY_NAME_MAX] = {0};
    char _area[ENTITY_SHORT_MAX] = {0};

    // Refusal detection lives on the ENTITY now, not here - see
    // Entity::cmdFailed and deriveState(). A card asks the entities it is
    // bound to whether their last command took; it no longer remembers what it
    // asked for, which is what makes a parent and a child agree about a switch
    // they both render.

    static EntityRegistry *s_reg;
    static bool            s_showArea;
};

#endif // CARD_H
