#include "Popup/PopupState.h"

// See CardPopup.h for what this is and docs/design/card-sheet.md sections 11-13
// for why it looks the way it does. Step 1 of 2.10a: the window, its header,
// the four ways to close, the body and the inner views. The settings deck
// comes next.
//
// USABLE FIRST (owner, 2026-10-04 evening). The window used to grow out of the
// card and land with the dim; every version of that was measured, and the
// motion was never the slow part - the frame it landed in was (a full-screen
// redraw for the dim, ~120 ms on Midnight, ~200 ms on Linen, after ~240 ms of
// growing). The window now appears complete in the first frame after the long
// press, and closes in one. What was learned is in docs/LESSONS.md, "Effects
// that cover the screen". Nothing is drawn behind it: the dim was tried that
// evening, drawn around the window a frame after it, and the owner chose
// speed - "the delay seems like an eternity and adds very little".
//
// What the eye gets instead is cheap because it is small: the held card is
// pressed in while its border fades to the accent (see "The hold"), now and
// then a spark runs along the window's edge (see "Interference"), and the
// switch's knob slides - and can be dragged.
//
// 2.12 P1: this file is open and close, the corner icons, the views and the clock, and the
// public interface. The rest of the window is in src/Cards/Popup/, listed in PopupState.h.
// 2.12 P4: the frame itself - the window, its header row, the auto-close, the quiet build, the
// tap catcher, the panels, the sparks - is the kit's Sheet (UI/Sheet.h); this is its client.

namespace popup {

// The window's state, one of each (PopupState.h says what each is).
Popup s;
Hold h;
lv_indev_t *s_indev = nullptr;
bool      s_openFromTap = false;

// Changes whenever anything the window shows could have changed. An FNV-1a
// over the fields, so a re-render costs nothing when nothing moved.
uint32_t signature() {
    uint32_t h = 2166136261u;
    auto mixIn = [&h](uint32_t v) { h = (h ^ v) * 16777619u; };
    // The entities shown, and - for a group defined at the source - its
    // members, whose rows the Members view shows and whose pauses change what
    // the window stands for (2.10c).
    const Entity *all[2 * POPUP_ENT_MAX];
    uint8_t nAll = 0;
    for (uint8_t i = 0; i < s.nEnt; i++) all[nAll++] = s.ent[i];
    if (s.native) for (uint8_t i = 0; i < s.native->nMembers && nAll < 2 * POPUP_ENT_MAX; i++)
        all[nAll++] = memberAt(i);
    for (uint8_t i = 0; i < nAll; i++) {
        const Entity *e = all[i];
        if (!e) continue;
        mixIn(e->lastChangeMs);
        mixIn(e->lastUpdateMs);
        mixIn((e->available ? 1u : 0u) | (e->paused ? 2u : 0u) | (e->pending ? 4u : 0u) |
              (e->cmdFailed ? 8u : 0u) | (e->everSet ? 16u : 0u) | (e->attrPending ? 32u : 0u));
        if (e->value.type == ValueType::BOOL) mixIn(e->value.b ? 1u : 0u);
        // A light's levels move without its value changing (2.10b).
        const EntityAttrs &at = e->attrs;
        mixIn((uint32_t)(uint16_t)at.brightness | ((uint32_t)at.lightCaps << 16) | ((uint32_t)at.lightMode << 24));
        mixIn((uint32_t)(uint16_t)at.colorTempK | ((uint32_t)(uint16_t)at.hue << 16));
        mixIn((uint32_t)(uint8_t)at.sat);
    }
    mixIn((uint32_t)s.groupOn);
    return h;
}

void forgetDeck();   // below, with the control deck

// The main view's widgets, before its stage is cleaned or the window goes.
void forgetMainWidgets() {
    forgetDeck();
    s.hero = s.knob = s.heroIcon = nullptr;
    s.lblWhat = s.lblValue = s.lblUnit = s.lblAgo = nullptr;
    s.fill = s.grip = s.mark = s.pill = nullptr;
    for (lv_obj_t *&b : s.btnCtl) b = nullptr;
    for (lv_obj_t *&w : s.swatch) w = nullptr;
    for (lv_obj_t *&b : s.sceneBtn) b = nullptr;
    s.sliding = false;
}

// The window's own widgets, when it goes (the sheet forgets its frame's).
void forgetWidgets() {
    s.btnHistory = s.btnMembers = s.btnScenes = nullptr;
    forgetMainWidgets();
}

// --- THE CORNER ICONS (owner, 2026-10-06) ------------------------------------
// History (the chart), Members and Scenes are views of their own, and the icons
// in the corner are the sheet's tabs (Kit::sheetTabLook()): shown on every view
// they belong to, the one showing lit, and a tap on a lit one goes back to the
// controls. Members on the window's own views, not inside one member; Scenes
// only on a light's controls and its scenes; the chart everywhere - the
// group's from the group's views, a member's from its own. The X closes from
// every view of the window; inside one member the arrow goes back to Members.
void cornerLook(lv_obj_t *b, bool shown, bool lit) { Kit::sheetTabLook(b, shown, lit); }

void showView(PopupView v) {
    const PopupView was = s.view;
    s.view = v;
    forgetMainWidgets();
    lv_obj_clean(s.stage);
    // HISTORY REACHES UP INTO THE HEADER ROW (owner, round 6): see histTopRow().
    lv_obj_set_style_pad_row(s.win, v == PopupView::VIEW_HISTORY ? -histLift() : mm(UI::shape().S5), 0);

    // SCENES SHOWS THE BRIGHTNESS SLIDER (owner): whatever the slider showed
    // before comes back with the controls.
    if (v == PopupView::VIEW_SCENES && was != PopupView::VIEW_SCENES) {
        s.ctlBeforeScenes = s.lightCtl;
        if (lightAggregate().caps & LIGHT_CAN_DIM) s.lightCtl = LCTL_DIM;
    } else if (v != PopupView::VIEW_SCENES && was == PopupView::VIEW_SCENES) {
        s.lightCtl = s.ctlBeforeScenes;
    }

    const bool member   = inMember();
    const bool controls = (v == PopupView::VIEW_MAIN || v == PopupView::VIEW_MEMBER);
    // X on the controls, the back arrow everywhere else (owner, round 4).
    setText(s.lblLeft, v == PopupView::VIEW_MAIN ? LV_SYMBOL_CLOSE : LV_SYMBOL_LEFT);
    cornerLook(s.btnHistory, true, v == PopupView::VIEW_HISTORY);
    cornerLook(s.btnMembers, !member && hasMembers(), v == PopupView::VIEW_MEMBERS);
    cornerLook(s.btnScenes, (controls || v == PopupView::VIEW_SCENES) && hasScenes(),
               v == PopupView::VIEW_SCENES);
    // The left panel belongs to its view: CHART to History, SCENES to Scenes.
    sideFor(v);

    // Whose view it is: the window's, or (inside one) the member's.
    const char *who = (member && s.ent[0]) ? s.ent[0]->desc.name : s.name;
    char title[ENTITY_NAME_MAX + 16];
    if (v == PopupView::VIEW_MAIN) {
        // A cluster's name is its group's: "Office", never "Office > Office".
        if (s.area[0] && !(s.cluster && !strcmp(s.area, s.name))) {
            snprintf(title, sizeof(title), "%s > ", s.area);
            setText(s.lblTitleArea, title);
        } else {
            setText(s.lblTitleArea, "");
        }
        setText(s.lblTitleName, s.name);
    } else if (v == PopupView::VIEW_MEMBER) {
        // "Group > member" (card-sheet 11.1).
        snprintf(title, sizeof(title), "%s > ", s.name);
        setText(s.lblTitleArea, title);
        setText(s.lblTitleName, who);
    } else {
        snprintf(title, sizeof(title), "%s > ", who);
        setText(s.lblTitleArea, title);
        setText(s.lblTitleName, v == PopupView::VIEW_HISTORY ? "History"
                              : v == PopupView::VIEW_SCENES  ? "Scenes"
                              : v == PopupView::VIEW_PROPS   ? "Properties"
                              : v == PopupView::VIEW_CARD_UI ? "Card UI"
                              : v == PopupView::VIEW_CLUSTER_SLOTS ? "Header"
                              : v == PopupView::VIEW_VISIBILITY ? "Visibility"
                              : v == PopupView::VIEW_ADD_TO ? "Add to" : "Members");
    }
    // SIZED BEFORE THE VIEW IS BUILT: placeWide() lays the window out, and a
    // DOT label laid out at the last view's width rewrites its own text - the
    // layout then measured "Sce..." and kept it (round 7, the 4B).
    Kit::sheetLayoutTitle(s);
    if (v == PopupView::VIEW_HISTORY)      buildHistory();
    else if (v == PopupView::VIEW_MEMBERS) buildMembers();
    else if (v == PopupView::VIEW_PROPS)   buildProps();
    else if (v == PopupView::VIEW_CARD_UI) buildCardUi();
    else if (v == PopupView::VIEW_CLUSTER_SLOTS) buildClusterSlots();
    else if (v == PopupView::VIEW_VISIBILITY) buildVisibility();      // K96
    else if (v == PopupView::VIEW_ADD_TO)     buildAddTo();           // K91, K100
    else if (v == PopupView::VIEW_MAIN && s.cluster) buildCluster();   // K104
    else                                   buildMain();
    deckSync();     // SETTINGS' rows are the view's (a member's own has more)
    deckRender();   // Paused follows whatever the window is showing now
    s.lastSig = signature();
}

// The controls view of whatever the window is showing: the window's, or one
// member's.
PopupView controlsView() { return inMember() ? PopupView::VIEW_MEMBER : PopupView::VIEW_MAIN; }

// A corner icon: its view, or - when lit - back to the controls.
void cornerTap(PopupView v) { showView(s.view == v ? controlsView() : v); }

// BACK IS HARD-LINKED, NOT A HISTORY (owner, round 4): every view has one
// fixed place it goes back to, so "back" never depends on how you got there.
//   History, Members, Scenes  -> the controls
//   a member                  -> Members
//   a member's History/Scenes -> that member
// On the controls it is the X, and closes the window.
void goBack() {
    switch (s.view) {
        case PopupView::VIEW_MAIN:   CardPopup::close();     break;
        case PopupView::VIEW_MEMBER: leaveMember();          break;
        case PopupView::VIEW_ADD_TO: showView(PopupView::VIEW_VISIBILITY); break;   // its view's own
        default:                     showView(controlsView()); break;
    }
}

void leftCb(lv_event_t *ev) { (void)ev; goBack(); }

// THE TITLE'S FIRST PART IS A LINK (owner, round 4): "Desk >" goes to Desk's
// controls, "Office lamp >" to that member's - from any view below them. On
// the controls themselves it is the area, which leads nowhere yet.
void crumbCb(lv_event_t *ev) {
    (void)ev;
    if (s.view == PopupView::VIEW_MAIN) return;
    if (s.view == PopupView::VIEW_MEMBER) { leaveMember(PopupView::VIEW_MAIN); return; }
    showView(controlsView());
}
void historyCb(lv_event_t *ev) { (void)ev; cornerTap(PopupView::VIEW_HISTORY); }
void membersCb(lv_event_t *ev) { (void)ev; cornerTap(PopupView::VIEW_MEMBERS); }
void scenesCb (lv_event_t *ev) { (void)ev; cornerTap(PopupView::VIEW_SCENES); }

#ifdef DEBUG_POPUP
// The interference trial: a long press on the title cycles how often a spark
// runs along the edge - now and then, once per window, never - from the next
// window. Debug builds only, until the owner chooses (a setting, later).
void intfCycleCb(lv_event_t *ev) {
    (void)ev;
    using M = Kit::SparksMode;
    const M m = Kit::sparksMode();
    Kit::setSparksMode(m == M::SPARKS_NOW_AND_THEN ? M::SPARKS_ONCE
                     : m == M::SPARKS_ONCE         ? M::SPARKS_OFF : M::SPARKS_NOW_AND_THEN);
    UIToolkit::show_toast(Kit::sparksMode() == M::SPARKS_NOW_AND_THEN ? "Sparks: now and then"
                        : Kit::sparksMode() == M::SPARKS_ONCE         ? "Sparks: once per window"
                                                                      : "Sparks: off");
}
#endif

// ---------------------------------------------------------------------------
// The window's contents - the sheet's frame (Kit::sheetOpen()) with the
// window's tabs: History - the mock's chart icon, axes and bars (mdi:chart-bar)
// - always in the corner; a card standing for several things gets the members
// icon INSIDE it (owner, 2026-10-06: nearly every window has a chart, few have
// members); Scenes under them, floating, shown when the light has scenes
// (mdi:movie-open, a clapperboard - HA's own scene icon, the palette, is
// Colour's here).
// ---------------------------------------------------------------------------
void buildFirst() {
    if (hasMembers()) { s.btnMembers = s.tab[0]; s.btnHistory = s.tab[1]; }
    else              { s.btnHistory = s.tab[0]; }
    s.btnScenes = s.floatTab;
    showView(PopupView::VIEW_MAIN);
}

// ---------------------------------------------------------------------------
// The clock: following the entity (the sheet runs the auto-close and its bar)
// ---------------------------------------------------------------------------
void tick(uint32_t now) {
    if (s.phase != PopupPhase::PHASE_OPEN) return;

    // A MEMBER OF A GROUP DEFINED IN HA PAUSED OR RESUMED (2.10c): the window
    // now stands for the members, or for the group again. Checked every tick,
    // not only on a change, because leaving a member's own view settles the
    // signature before this could see it. Not in a member's own view, whose
    // ent[] is that member.
    if (s.native && !inMember() && nativeWantsMembers() != s.expanded) {
        applyNative();
        // Scenes go while a member is paused: from their view, back to the controls.
        showView((s.view == PopupView::VIEW_SCENES && !hasScenes()) ? controlsView() : s.view);
        s.lastAgeMs = now;
        return;
    }

    // History's answer (2.10e), drawn when it arrives.
    histPoll();

    // A scene's ring goes when the light no longer looks as the scene left it.
    if (!sceneRingCheck(now)) renderMain();

    // Scenes learnt after the window opened: its icon appears.
    if (s.btnScenes && lv_obj_has_flag(s.btnScenes, LV_OBJ_FLAG_HIDDEN) && hasScenes() &&
        (s.view == PopupView::VIEW_MAIN || s.view == PopupView::VIEW_MEMBER))
        cornerLook(s.btnScenes, true, false);

    // The window is live (card-sheet 7): it follows the entity while open.
    const uint32_t sig = signature();
    if (sig != s.lastSig) {
        s.lastSig = sig;
        if (s.view == PopupView::VIEW_MEMBERS) showView(PopupView::VIEW_MEMBERS);
        else                                    renderMain();
        s.lastAgeMs = now;
    } else if (now - s.lastAgeMs >= 1000) {
        s.lastAgeMs = now;
        renderMain();   // the "N ago" line; no-op off the main view
    }
}

// ---------------------------------------------------------------------------
// The window as a sheet (UI/Sheet.h): its spec, filled each time it opens -
// the tabs follow the card - and what it does as it goes.
// ---------------------------------------------------------------------------
Kit::SheetSpec s_spec;
#ifdef DEBUG_POPUP
uint32_t s_closeT0 = 0;
#endif

void closing() {
    confirmClose();   // Remove card's dialog, if it was up
#ifdef DEBUG_POPUP
    s_closeT0 = micros();
#endif
}

// Gone: everything in one frame, the card's border with it.
void closed() {
    forgetWidgets();
    holdDetach();
    s.phase = PopupPhase::PHASE_CLOSED;
    s.nEnt = 0;
    s.groupN = 0;
    s.native = nullptr;
    s.expanded = false;
    s.lastScene = -1;
    s.rebuildQueued = false;
    Kit::selectorLookOverride(-1);   // the card's own look went with its window (Card UI)
    // K8: settings are written when the window closes - once, if anything
    // changed, on the settings task, 400 ms on so the write never lands in
    // the frames of the close or of SETTINGS' slide.
    Settings::save(400);
#ifdef DEBUG_POPUP
    char mem[96];
    dbgMem(mem, sizeof(mem));
    dbgLater("close: torn down in %lu us; %s", (unsigned long)(micros() - s_closeT0), mem);
#endif
}

// SETTINGS' rows went (made again for other rows, or with the window): the
// controls the window kept went with them. Likewise the left panel's.
void panelGone() { s.swPause = nullptr; s.ddGroup = nullptr; }
void sideGone()  { s.ddTapScene = nullptr; s_cbHidden = nullptr; s_ddSpan = nullptr; }

// The window itself: the sheet built, made modal, its clock started,
// SETTINGS' tab sliding up and the sparks waiting their turn.
void showWindow() {
#ifdef DEBUG_POPUP
    const uint32_t t0 = micros();
#endif
    s.phase = PopupPhase::PHASE_OPEN;
    s_hv = HistView{};   // History starts on 24 hours in every window (2.10e)
    Kit::SheetSpec &k = s_spec;
    k = Kit::SheetSpec{};
    k.leftCb  = leftCb;
    k.crumbCb = crumbCb;
    k.nTabs   = hasMembers() ? 2 : 1;
    if (k.nTabs == 2) { k.tabGlyph[0] = MDI_LIGHTBULB_GROUP; k.tabCb[0] = membersCb; }
    k.tabGlyph[k.nTabs - 1] = MDI_CHART_BAR;
    k.tabCb[k.nTabs - 1]    = historyCb;
    k.floatGlyph = MDI_MOVIE_OPEN;
    k.floatCb    = scenesCb;
#ifdef DEBUG_POPUP
    k.headerLongCb = intfCycleCb;
#endif
    k.buildFirst = buildFirst;
    k.tick       = tick;
    k.closing    = closing;
    k.closed     = closed;
    k.sparks     = true;
    k.panelTitle = "SETTINGS";
    k.panelRows  = deckSpecs;
    k.panelGone  = panelGone;
    k.sideGone   = sideGone;
    // The card's own window looks (Card UI, D-28): its timeout, bar, selector.
    if (Card *c = cardOf(h.surface)) cardWindowLook(*c, k);
    Kit::sheetOpen(s, k);
    s.lastAgeMs = s.lastTouchMs;
#ifdef DEBUG_POPUP
    char mem[96];
    dbgMem(mem, sizeof(mem));
    dbgLater("open: window and deck built in %lu us; %s", (unsigned long)(micros() - t0), mem);
#endif
}

} // namespace popup

using namespace popup;

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void CardPopup::setHistory(HaHistory *history) { s_hist = history; }

void CardPopup::begin() {
    // Every press and release, whatever it lands on - the same reason
    // GUIManager hooks the input device rather than the screen
    // (screenPressCb()). It also spares every card four event registrations.
    if (lv_indev_t *in = lv_indev_get_next(nullptr)) {
        s_indev = in;
        lv_indev_add_event_cb(in, pressCb,   LV_EVENT_PRESSED,  nullptr);
        lv_indev_add_event_cb(in, releaseCb, LV_EVENT_RELEASED, nullptr);
    }
#ifdef DEBUG_POPUP
    // What the window's own frames cost, and what they redrew.
    if (lv_display_t *d = lv_display_get_default()) {
        lv_display_add_event_cb(d, dbgRenderStart, LV_EVENT_RENDER_START, nullptr);
        lv_display_add_event_cb(d, dbgRenderReady, LV_EVENT_RENDER_READY, nullptr);
    }
#endif
    // The sheets' tap catcher, once, for the life of the device (UI/Sheet.h).
    Kit::sheetBegin();
}

bool CardPopup::isOpen() { return s.phase != PopupPhase::PHASE_CLOSED; }

void CardPopup::close() {
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    Kit::sheetClose(s);
}

void CardPopup::open(Card &card) {
    if (s.phase != PopupPhase::PHASE_CLOSED) return;
    if (!card._surface || !card._nPrimary) return;

    // A FINGER THAT MOVED IS A DRAG, NOT A LONG PRESS (card-sheet 7). LVGL
    // fires LONG_PRESSED for a slow drag that never crossed the gesture
    // distance; it must not open a window.
    if (movedTooFar()) { letGo(); return; }
#ifdef DEBUG_POPUP
    s_dbgPressMs = millis();
#endif

    // --- Copy what the window needs; never keep the card -------------------
    s.nEnt = 0;
    for (uint8_t i = 0; i < card._nPrimary && i < CARD_PRIMARY_MAX; i++) {
        if (card._primary[i]) s.ent[s.nEnt++] = card._primary[i];
    }
    if (!s.nEnt) { letGo(); return; }
    // Release the touch BEFORE building anything (LESSONS): the finger is still
    // down, and its release must not land on the window. (It also means LVGL
    // sends no RELEASED for this touch - PRESS_LOST to the card instead.)
    // Not from a tap (openOn()): that finger is already up, and waiting for a
    // release would swallow the next touch - the first tap in the window.
    if (!s_openFromTap)
        if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);
    s.reg      = Card::s_reg;
    // One entity that HA defines as a group, with its members learnt (2.10c).
    s.native   = (s.nEnt == 1 && s.ent[0]->nMembers) ? s.ent[0] : nullptr;
    applyNative();
    s.tempUnit = card.tempUnit();
    s.cluster  = !strcmp(card.typeName(), "cluster");
    s.groupOn  = card.groupOn();
    s.sceneShow = card.sceneShow();
    s.lightCtl = firstCtl(lightAggregate().caps);   // D3: open on the control
    snprintf(s.area, sizeof(s.area), "%s", card._area);
    snprintf(s.name, sizeof(s.name), "%s", card.label());
    // A cluster is called what its header says: its own words, else its group's.
    if (s.cluster && s.area[0]) snprintf(s.name, sizeof(s.name), "%s", s.area);

    // --- Where the window goes: the sheet's place (Kit::sheetPlace()) -------
    // Under the system header, over the panels' tabs, up to twice as wide as
    // tall and never near the screen's sides; the hero is tied to its height
    // (propH).
    Kit::sheetPlace(s);

    s.view        = PopupView::VIEW_MAIN;
    s.closeQueued = false;

    // --- The card stays pressed in, with the accent, while its window is open.
    // The held card stops where its hold got to and finishes it; a card that
    // somehow was not the held one gets the same look.
    if (h.surface == card._surface) {
        lv_anim_delete(h.surface, holdExec);
        lv_anim_delete(h.surface, letGoExec);
    } else {
        holdDetach();
        holdAttach(card._surface);
    }
    holdLook(h.surface, 255);
    h.phase = HoldPhase::HOLD_OWNED;
    showWindow();
}

bool CardPopup::takeRebuildWanted() {
    if (isOpen()) return false;
    // Every one, so none waits for the next window.
    const bool ui = cardUiTakeRebuild(), cl = clusterTakeRebuild(), vis = visibilityTakeRebuild();
    return ui || cl || vis;
}

void CardPopup::openOn(Card &card, uint8_t view) {
    s_openFromTap = true;
    open(card);
    s_openFromTap = false;
    if (!isOpen()) return;
    if (view == OPEN_HISTORY)                      showView(PopupView::VIEW_HISTORY);
    else if (view == OPEN_MEMBERS && hasMembers()) showView(PopupView::VIEW_MEMBERS);
}
