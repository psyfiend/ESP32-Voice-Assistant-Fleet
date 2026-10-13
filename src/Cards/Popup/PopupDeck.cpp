// The control deck (2.12 P1, split from CardPopup.cpp; the kit's selector since P3): a light's
// controls - dim, white, colour - as the kit's Selector, with Power as its action.
#include "PopupState.h"

namespace popup {

Deck d;

// ---------------------------------------------------------------------------
// THE CONTROL DECK (owner, 2026-10-07 - after HTC's TouchFLO 3D): Kit::selector()
// - a ribbon of the light's controls, a metal selector slid or glided between
// them, Power a plain button at the start. Only what is showing is built, so
// the control chosen rebuilds the view - after the glide, never during. The
// deck is the same size as before its ribbon got shorter, so nothing around it
// moves. Its four looks are the kit's (Kit::selectorLook()), chosen per scheme
// on the system panel (K45).
// ---------------------------------------------------------------------------

int8_t chosenLook()        { return Kit::selectorLook(); }
void   keepLook(int8_t look) { Kit::keepSelectorLook(look); }

void forgetDeck() { d = Deck{}; }

// The selector has arrived on a control: that one shows.
void deckChosen(uint8_t i) {
    if (i >= d.n || d.ctl[i] == s.lightCtl) return;
    s.lightCtl = d.ctl[i];
    lv_async_call(rebuildMainAsync, nullptr);
}

// Power: a plain button.
void deckPowerCb(lv_event_t *ev) {
    (void)ev;
    if (!pausedBlocks()) commandAll(!aggregate().on);
}

void buildControlDeck(lv_obj_t *parent, uint8_t caps) {
    forgetDeck();
    Kit::SelectorSpec spec = {};
    uint8_t n = 0, chosen = 0;
    auto offer = [&](uint8_t bit, uint8_t ctl, const char *glyph) {
        if (!(caps & bit)) return;
        if (ctl == s.lightCtl) chosen = n;
        d.ctl[n] = ctl;
        spec.glyph[n++] = glyph;
    };
    offer(LIGHT_CAN_DIM,    LCTL_DIM,    MDI_BRIGHTNESS_5);
    offer(LIGHT_CAN_TEMP,   LCTL_TEMP,   MDI_SUN_THERMOMETER);
    offer(LIGHT_CAN_COLOUR, LCTL_COLOUR, MDI_PALETTE);
    spec.n           = n;
    spec.chosen      = chosen;
    spec.actionGlyph = MDI_POWER;
    spec.actionCb    = deckPowerCb;
    spec.quiet       = s.builtPaused;   // a paused window's controls are greyed
    spec.blocked     = pausedBlocks;    // ... and a touch on one says why
    spec.onChosen    = deckChosen;
    Kit::selector(d, parent, spec);
    s.btnCtl[0] = d.action;
    for (uint8_t i = 0; i < d.n; i++) s.btnCtl[1 + d.ctl[i]] = d.item[i];
}

} // namespace popup
