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
//   subclasses    the body, and what a change looks like. MeasureCard and
//                 ActorCard are genuinely different widget trees; that is the
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

    // Where a commanded value goes. Set once by CardBinder::begin().
    //
    // Static rather than a per-card pointer: there is exactly one registry per
    // device by construction - the same reasoning behind GUIManager's s_self -
    // and storing the same four bytes in every card on every page is waste
    // that buys nothing. If a second registry ever exists, this becomes a
    // member and CardBinder passes its own.
    static void useRegistry(EntityRegistry *r) { s_reg = r; }

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
    virtual void onLongPress() {}

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
    // gives it a face. How the refusal is DETECTED is documented at
    // _cmdActive below, because it is not obvious.
    bool command(const Entity *e, const EntityValue &v);

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
    CardState deriveState(uint32_t nowMs) const;
    bool resolveCommand();             // true if the outcome just became known
    static void eventCb(lv_event_t *e);

    // --- Widget tree ------------------------------------------------------
    //
    // _root is a TRANSPARENT wrapper filling the grid cell, and _surface is
    // the styled card inside it. That extra object exists for exactly one
    // reason: HDR_EXTERNAL bolts a tag to the top edge OUTSIDE the border, and
    // LVGL clips children to their parent, so the tag cannot be a child of the
    // thing it sits above. Making the wrapper unconditional keeps all three
    // header treatments one code path instead of two, which matters while both
    // are still being compared on glass.
    lv_obj_t *_root     = nullptr;
    lv_obj_t *_surface  = nullptr;
    lv_obj_t *_body     = nullptr;
    lv_obj_t *_header   = nullptr;   // null when HDR_NONE
    lv_obj_t *_lblArea  = nullptr;
    lv_obj_t *_lblStale = nullptr;
    lv_obj_t *_diagonal = nullptr;   // ST_LONG_STALE: corner to corner

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
    CardHeaderStyle _hdrStyle    = CardHeaderStyle::HDR_NONE;
    CardState       _state       = CardState::ST_LIVE;
    uint32_t        _longStaleMs = 0;     // 0 = ask cardLongStaleMs()
    bool            _paused      = false;

    char _label[ENTITY_NAME_MAX] = {0};
    char _area[ENTITY_SHORT_MAX] = {0};

    // --- Refusal detection ------------------------------------------------
    //
    // The registry's revert is honest but ANONYMOUS: after it fires, pending
    // is false and value is back to prevValue, which is indistinguishable from
    // an ordinary value change arriving from the source. Rather than add a
    // flag to Entity - Fleet_Entities is dependency-free and stable, and this
    // is a UI concern - the card remembers what it asked for and compares.
    //
    // Once pending clears on _cmdEnt:
    //   value == _cmdValue  -> the command took. Confirmed.
    //   anything else       -> it did not. ST_REFUSED.
    //
    // That comparison is right in BOTH failure modes, which is what makes it
    // worth preferring over a registry flag: a silent timeout reverts to the
    // old value, and a light that actively reports back that it stayed off
    // also arrives as the old value. Both mean the same thing to the user.
    //
    // IT IS RESOLVED BY POLLING, NOT BY THE SNAPSHOT, and that is not a
    // shortcut - a snapshot never arrives in the success case. A confirming
    // echo carries the value the optimistic write ALREADY applied, so
    // setValue() finds nothing changed, declines to dirty the entity, and no
    // card is told anything. That suppression is correct and deliberate on the
    // registry's side (it is most of ROADMAP 4.2's rate limiting), so the card
    // is what has to adapt: resolveCommand() watches the pending flag from
    // pollState() and needs no notification at all.
    const Entity *_cmdEnt    = nullptr;
    EntityValue   _cmdValue;
    bool          _cmdActive = false;   // a command is out, outcome unknown
    bool          _refused   = false;   // the last one did not take

    static EntityRegistry *s_reg;
};

#endif // CARD_H
