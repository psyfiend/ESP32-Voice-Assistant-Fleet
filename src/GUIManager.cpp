#include "GUIManager.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "SystemReport.h"
#include "Cards/CardDemo.h"
#include "Cards/CardIcons.h"   // cardSetLabelMode(), for the Label knob
#include "UI/ReferencePage.h"
#include "UI/LogPage.h"
#include "Dashboards/Dashboard_Fleet.h"
#ifdef USE_HA_DASHBOARD
#include "Dashboards/Dashboard_HA.h"
#endif
#include "bsp_loader.h"

// LVGL's event and callback APIs take plain function pointers with no user
// context of their own for some of the toolkit hooks, so the single dashboard
// instance is reachable from a file-static. There is exactly one GUIManager by
// construction; if that ever stops being true these become member callbacks
// with user_data, which LVGL does support for lv_event_cb.
static GUIManager *s_self = nullptr;

// Per the repo's debug-flag convention (CLAUDE.md): a diagnostic worth keeping
// rather than deleting, off unless an environment asks for it. Gesture work is
// exactly the kind that needs "did the swipe even arrive" answered before
// anything else, and the answer is invisible without this.
#ifdef DEBUG_GESTURE
    #define DBG_GESTURE(...) Serial.printf("[Gesture:debug] " __VA_ARGS__)
#else
    #define DBG_GESTURE(...) do {} while (0)
#endif

GUIManager::GUIManager(SystemCore &core)
    : _core(core)
    , _pnlDisplay(core.display(), core.touch())
#ifdef HAS_AUDIO_HW
    , _pnlAudio(core.audio())
#endif
{
    s_self = this;
}

// THE ONE WAY TO OPEN OR CLOSE THE DRAWER. Both the header tap and the swipe
// come through here, and that is the point.
//
// The drawer hangs off whatever bar is currently above it, and there are three
// possibilities: a permanent header, a peeked one, or nothing. Getting that
// offset right was originally done in the swipe handler alone - so tapping the
// status icon while the header was peeked opened the drawer at y=0, behind the
// bar, with its SYSTEM - DIAGNOSTICS title cut off. Two entry points, one of
// which knew a rule the other did not.
//
// The duplication was the bug, so the fix is to have one entry point rather
// than to teach the second one the same rule.
void GUIManager::syncPanelOffset() {
    int32_t top = 0;
    if (!_headerHidden)      top = UIToolkit::sc(UI_HEADER_H);        // permanent bar
    else if (_headerPeeking) top = UIToolkit::sc(UI_HEADER_PEEK_H);   // temporary one
    // else: no bar at all, so the drawer starts at the top of the screen.
    _pnlSystem.setTopOffset(top);
}

void GUIManager::toggleSystemPanel() {
    syncPanelOffset();
    _pnlSystem.toggle();
}

void GUIManager::headerIconClickCb(lv_event_t *e) {
    (void)e;
    if (s_self) s_self->toggleSystemPanel();
}

// ---------------------------------------------------------------------------
// Swipe navigation - a FIRST CUT for milestone 2.6, and deliberately small.
//
// The owner's brief, 2026-09-19: swipe down from the top to open the system
// panel, swipe up from the bottom to show the deck, and "maybe a swipe
// downward on the right half of the screen can open the system panel, and
// swipe downward on the left side can pull down something else?"
//
// What is built here is the part that is decided. The left half is WIRED AND
// EMPTY on purpose - it logs and does nothing, because what belongs there is
// an open question and guessing at it overnight would produce a gesture the
// owner has to undo rather than react to. Same for the auto-hiding header: it
// needs a reveal, a timeout and a second-swipe rule, none of which are settled.
//
// WHY THE SCREEN AND NOT AN OVERLAY. An invisible full-screen catcher on
// lv_layer_top() would see every swipe - and eat every tap underneath it,
// which is how the touch visualiser broke before it was moved (see
// dashboard.md section 5). LVGL sends LV_EVENT_GESTURE to the screen only when
// no child consumed the drag, so a scrollable child still scrolls and a card
// still takes its click. The dashboard page does not scroll by design, so the
// gesture reaches here.
// ---------------------------------------------------------------------------
// Bring a hidden header back for a few seconds.
//
// Deliberately NOT a rebuild. cycleHeaderBar() rebuilds the dashboard because
// it changes how much room the cards get; a peek must not, or the grid would
// re-plan and the cards would jump every time somebody glanced at the clock.
// The bar is drawn on the TOP layer over the page instead, which is also what
// makes it retract without disturbing anything.
void GUIManager::peekHeader() {
    lv_obj_t *hdr = _header.getContainer();
    if (!hdr) return;

    _headerPeeking = true;
    lv_obj_set_height(hdr, UIToolkit::sc(UI_HEADER_PEEK_H));
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(hdr);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hdr);
    lv_anim_set_values(&a, -UIToolkit::sc(UI_HEADER_PEEK_H), 0);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); });
    lv_anim_start(&a);

    // Single-shot: LVGL deletes a timer whose repeat count runs out, so there
    // is nothing to clean up if it fires, and unpeekHeader() kills it if the
    // user acts first.
    if (_peekTimer) lv_timer_delete(_peekTimer);
    _peekTimer = lv_timer_create([](lv_timer_t *t) {
        (void)t;
        if (!s_self) return;
        // THE HEADER STAYS WHILE THE DRAWER IS OPEN.
        //
        // Retracting it underneath an open panel leaves the panel hanging a
        // header's height down the screen with nothing above it - the owner
        // saw exactly that and called the gap "even worse". The peek timeout
        // is about an unattended glance, and a drawer standing open is not
        // one, so the timer simply re-arms instead of firing.
        if (s_self->_pnlSystem.isExpanded()) return;
        s_self->unpeekHeader();
    }, UI_HEADER_PEEK_MS, nullptr);

    DBG_GESTURE("header peek for %d ms\n", (int)UI_HEADER_PEEK_MS);
}

void GUIManager::unpeekHeader() {
    if (!_headerPeeking) return;
    _headerPeeking = false;

    if (_peekTimer) { lv_timer_delete(_peekTimer); _peekTimer = nullptr; }

    // The drawer hung off the peeked bar while it was there; with it gone the
    // bar's space goes back to the screen. Through syncPanelOffset() rather
    // than a second copy of the rule - see toggleSystemPanel() for what the
    // duplication cost the first time.
    syncPanelOffset();

    lv_obj_t *hdr = _header.getContainer();
    if (!hdr) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hdr);
    lv_anim_set_values(&a, 0, -UIToolkit::sc(UI_HEADER_PEEK_H));
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); });
    // Hide only once it has finished travelling, or it vanishes mid-slide.
    lv_anim_set_completed_cb(&a, [](lv_anim_t *an) {
        lv_obj_add_flag((lv_obj_t *)an->var, LV_OBJ_FLAG_HIDDEN);
    });
    lv_anim_start(&a);
}

// Capture where a press began, for the edge gating above.
//
// ON THE INPUT DEVICE, not on the screen, and the difference is the whole bug.
//
// This was registered on the screen, which only sees a press that reaches it.
// Cards were made to bubble so their presses would - but the deck panels, the
// system drawer, the header and every button do NOT bubble, so a press landing
// on any of those left the PREVIOUS swipe's origin in place.
//
// That is exactly the "stickiness" the owner reported, and his description is
// worth keeping because it names the mechanism better than any summary:
//
//   "If I swipe from the very bottom to open the DECK, it opens. Then I can
//    swipe UP from anywhere on the screen to toggle the deck repeatedly. But
//    if I ever swipe DOWN, at this point swiping UP no longer toggles it."
//
// Once the deck is showing, the bottom of the screen IS the deck panel, so the
// next press never updated the origin and fromBottom stayed true forever. A
// downward swipe landed on a card, which DOES bubble, so the origin finally
// moved and the up-swipe stopped working. Same cause behind the log page
// sticking after a visit, and behind the header toggle changing which gesture
// fired.
//
// lv_indev_add_event_cb() fires for every press regardless of what was hit, so
// there is no target to have the wrong flags. Cards keep EVENT_BUBBLE - it is
// wanted for tap-to-dismiss - but nothing here depends on it any more.
void GUIManager::screenPressCb(lv_event_t *e) {
    (void)e;
    if (!s_self) return;
    lv_indev_t *indev = lv_indev_active();
    if (indev) lv_indev_get_point(indev, &s_self->_pressStart);
}

void GUIManager::screenGestureCb(lv_event_t *e) {
    (void)e;
    if (!s_self) return;

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    const lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    // WHERE THE FINGER STARTED, not where it is now.
    //
    // Two of the owner's complaints are this one fact. lv_indev_get_point()
    // returns the CURRENT position, so a downward swipe reports a y near the
    // BOTTOM of its travel - which meant "did this start at the top edge" could
    // not be asked at all, and a swipe beginning in the middle of the page
    // opened the drawer. The press origin is captured in screenPressCb().
    const lv_point_t p = s_self->_pressStart;
    const int32_t scrW = lv_obj_get_width(lv_screen_active());
    const int32_t scrH = lv_obj_get_height(lv_screen_active());
    const bool rightHalf = (p.x >= scrW / 2);

    // EDGE GATING. A vertical swipe only counts when it begins within a band
    // of the edge it is pulling from, which is how every phone does it and is
    // what the owner asked for: "I wanted these swipes to only engage when
    // swiping from the top edge of the screen downward."
    //
    // The band is in logical pixels so it is the same physical distance on
    // every panel, for the same reason the swipe DISTANCE is.
    const int32_t band = UIToolkit::sc(UI_EDGE_BAND);
    const bool fromTop    = (p.y <= band);
    const bool fromBottom = (p.y >= scrH - band);

    // SWALLOW THE REST OF THIS TOUCH BEFORE ACTING ON IT.
    //
    // LVGL delivers a gesture AND still delivers the press/click of the same
    // finger. Three separate faults were that one behaviour:
    //
    //   * "the location where I START swiping acts as a tap on whichever
    //     button is underneath" - the button got a click it should not see.
    //   * "the panel appears then retracts" - the gesture opened the drawer
    //     and the click closed it again via tap-to-dismiss.
    //   * swiping up with the deck hidden brought the deck back AND expanded
    //     the DISPLAY panel AND squashed the grid - because toggleDeck()
    //     rebuilds, and the still-live press then landed on a panel header
    //     that had just appeared under the finger.
    //
    // BEFORE the action, not after, and that ordering is the whole fix for the
    // third one: an action that rebuilds the screen puts new objects under a
    // finger LVGL still considers pressed, so the input has to be released
    // first or the rebuild hands it a fresh target.
    //
    // BUT ONLY FOR A GESTURE WE ACT ON. This used to run unconditionally, at
    // the top, for every gesture that reached the screen - including a quick
    // flick along a volume or brightness slider, which LVGL also classifies as
    // a gesture. The release froze the slider mid-drag: "the slider will get
    // stuck and stop moving, but only visually", the owner, 2026-09-23. Now
    // each branch releases just before it acts, and an ignored gesture leaves
    // the touch alone. The sliders also stop bubbling gestures at all - see
    // UIToolkit::create_slider_col().
    auto release = [&]() { lv_indev_wait_release(indev); };

    switch (dir) {
    case LV_DIR_BOTTOM:                      // swipe DOWN
        if (!fromTop) { DBG_GESTURE("down from y=%d, not the top band\n", (int)p.y); break; }
        release();
        if (rightHalf) {
            // RIGHT half: the system drawer - the side the status icon lives
            // on, so the gesture and the tap agree about where it comes from.
            //
            // With the bar HIDDEN there is an extra step first. The owner's
            // rule: one swipe brings the header back for three seconds so the
            // status glyphs can be read without committing to anything; a
            // second swipe while it is showing opens the drawer. That makes
            // "what is my signal" a cheaper question than "let me change
            // something", which is the right way round for a wall panel.
            if (!UIToolkit::systemHeaderH && !s_self->_headerPeeking) {
                s_self->peekHeader();
            } else {
                // Through the one entry point, which owns the top offset.
                s_self->toggleSystemPanel();
            }
        } else {
            // LEFT half: straight to the log. The owner's pick, and it is a
            // good one - the log was two taps deep behind a drawer whose only
            // other use is changing settings, so reading it always meant
            // opening something you did not want to touch.
            DBG_GESTURE("swipe down, left half -> log\n");
            s_self->openLog();
        }
        break;

    case LV_DIR_TOP:                         // swipe UP
        // An open drawer is dismissed by an up-swipe from anywhere; only the
        // DECK gesture is edge-gated, because that one is pulling something up
        // from the bottom of the screen and should read as such.
        // Edge-gated ONLY for the deck, which is the one being pulled up from
        // the bottom and should read as such. Dismissing an open drawer or a
        // peeked header is a "put that away" gesture and is allowed from
        // anywhere - a peek in particular lives at the TOP of the screen, so
        // requiring a swipe from the bottom to dismiss it would be perverse.
        if (!s_self->_pnlSystem.isExpanded() && !s_self->_headerPeeking && !fromBottom) {
            DBG_GESTURE("up from y=%d, not the bottom band\n", (int)p.y);
            break;
        }
        release();
        // A swipe up closes the drawer before it touches the deck. With the
        // panel open it is the obvious "put that away" gesture, and toggling
        // the deck underneath an open panel would change something the user
        // cannot see.
        if (s_self->_pnlSystem.isExpanded()) {
            s_self->_pnlSystem.close();
        } else if (s_self->_headerPeeking) {
            // Put a peeked header away immediately rather than waiting out the
            // three seconds. The owner asked for it and it is the right
            // symmetry: the gesture that brought it down should take it back.
            s_self->unpeekHeader();
        } else {
            s_self->toggleDeck();
        }
        break;

    default:
        // Horizontal swipes belong to page navigation, which is the rest of
        // 2.6 and does not exist yet. Left unclaimed rather than bound to
        // something plausible.
        DBG_GESTURE("gesture dir %d ignored\n", (int)dir);
        break;
    }
}

void GUIManager::closeSystemPanelCb() {
    if (s_self) s_self->_pnlSystem.close();
}

// The one part of the System Doctor that genuinely needs LVGL. Registered with
// SystemReport rather than living inside it, which is what keeps SystemReport
// free of any LVGL include.
// What the UI is actually set to.
//
// This used to print one line - "Active Panel: EXPANDED / NONE" - which could
// never say anything useful, because opening any panel closes the others by
// rule, so the answer was structurally always the same. The owner: "which
// never showed anything".
//
// The twiddly knobs are the useful thing. Every one of them is a live
// experiment the owner is running on glass, and until now the only way to know
// where they stood was to remember what you last pressed. A dump pasted into a
// conversation now carries the state it was taken in.
void GUIManager::reportUiSection() {
    if (!s_self) return;
    GUIManager &g = *s_self;

    SystemReport::line("  Screen:      %ldx%ld  %ld DPI",
                       (long)lv_obj_get_width(lv_screen_active()),
                       (long)lv_obj_get_height(lv_screen_active()),
                       (long)lv_display_get_dpi(lv_display_get_default()));

    // Cards placed vs declared is the single most useful UI fact on a small
    // board: it is how you know the page degraded rather than that something
    // failed to bind. The 3248 shows 8 of 18 and that is correct behaviour.
    if (g._page) {
        SystemReport::line("  Cards:       %u of %u placed, %u dropped for space",
                           (unsigned)g._page->placed(), (unsigned)g._page->count(),
                           (unsigned)g._page->dropped());
    }

    if (g._page) {
        SystemReport::line("  Grid:        %u cell rows   target card width %u px",
                           (unsigned)g._page->cellRows(),
                           (unsigned)UI::grid().TARGET_CARD_W);
    }

    SystemReport::line("  Card header: %s   variant: %s   area: %s",
                       g._hdr == CardHeaderStyle::HDR_TAG  ? "tag"
                     : g._hdr == CardHeaderStyle::HDR_BAR  ? "bar" : "none",
                       cardVariantName(g._variant),
                       g._showArea ? "shown" : "hidden");

    SystemReport::line("  Chrome:      header bar %s   deck %s   drawer %s",
                       g._headerHidden ? (g._headerPeeking ? "peeking" : "hidden")
                                       : "shown",
                       g._showDeck ? "shown" : "hidden",
                       g._pnlSystem.isExpanded() ? "open" : "closed");
}

void GUIManager::begin() {
    // Styles, semantic fonts and the toast layer. Design system, not engine -
    // which is why it lives here and not in LVGL_Startup. This is the seam
    // milestone 2.2 lands on.
    SystemCore::heapMark("before UI");
    UIToolkit::init();

    // The starting scheme. UITokens defaults to Fleet; the owner's pick is
    // Midnight (Slate, which this used to be, was deleted 2026-09-23 as a
    // Midnight with a violet accent). Applied here rather than in the token
    // file so "which scheme ships" is a GUIManager decision beside the others.
    UI::setScheme(UI_PAL_MIDNIGHT, UI_MET_DARK);

    // --= ROOT SCREEN =--
    lv_obj_t *screen = lv_screen_active();

    // Design tokens. Started here rather than in LVGL_Startup for the same
    // reason UIToolkit is: this is the design system, not the engine. The
    // viewport comes from the live screen so it is already rotated - deriving
    // the grid from bsp_display.WIDTH/HEIGHT would be wrong on every board
    // running at rotation 1 or 3.
    UI::begin(lv_obj_get_width(screen), lv_obj_get_height(screen));

    // Apply the board's default column count, if it has one.
    //
    // Rows need no equivalent line - buildDashboard() already passes
    // _rowsOverride to the page every time it builds - but columns live in the
    // token layer and were only ever set from the Col button, so a default
    // that nobody applied would be a number that did nothing.
    if (_colsOverride) UI::setColumnsOverride(_colsOverride);

    applyGround();
    lv_obj_clear_flag        (screen, LV_OBJ_FLAG_SCROLLABLE);               // Disable global scrolling

    // --= LAYER 3: HEADER BAR =--
    // Header click -> toggle system panel
    _header.init(screen, bsp_hw.device_name, &_core.conn(), &_core.mqtt());
    lv_obj_add_event_cb(_header.getStatusIcon(), headerIconClickCb, LV_EVENT_CLICKED, NULL);

    // Swipe navigation, milestone 2.6 first cut. On the screen, not an
    // overlay - see screenGestureCb() for why that distinction matters.
    lv_obj_add_event_cb(screen, screenGestureCb, LV_EVENT_GESTURE, NULL);

    // TAP ANYWHERE ELSE TO DISMISS, which is the rule the deck already uses.
    //
    // The owner: "The deck panels were wired such that tapping to open any one
    // of them would close any other open panels. This rule should be applied
    // here." So an open drawer, or a peeking header, closes on a tap that is
    // not on the drawer itself.
    //
    // On the SCREEN and on LV_EVENT_CLICKED, which means a tap that landed on
    // a card never reaches here - LVGL delivers the click to the card and
    // stops. That is the behaviour we want and it is why this does not need a
    // hit-test against the panel's rectangle: a tap inside the drawer hits one
    // of the drawer's own children instead.
    // Explicit rather than relying on lv_obj's default flags: everything above
    // depends on the screen being the thing that finally catches a stray tap,
    // and a default is a poor place to rest that.
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, [](lv_event_t *ev) {
        (void)ev;
        if (!s_self) return;
        if (s_self->_headerPeeking)          s_self->unpeekHeader();
        else if (s_self->_pnlSystem.isExpanded()) s_self->_pnlSystem.close();
    }, LV_EVENT_CLICKED, NULL);

    // HOW FAR A SWIPE HAS TO TRAVEL, in real millimetres rather than pixels.
    //
    // LVGL's default is 50 PHYSICAL pixels, which means a swipe is a different
    // physical gesture on every board: 7.7 mm of finger travel on CYD_S3_3248
    // at 165 PPI, but only 4.3 mm on WS_P4_5 at 294 PPI. That is backwards -
    // the denser the panel, the twitchier the gesture - and it is the same
    // mistake the type scale already fixed for fonts.
    //
    // sc() is exactly the right tool: 50 LOGICAL px is a constant ~7.5 mm on
    // every panel in the fleet, which is a deliberate swipe and not a slipped
    // finger. Set here rather than in LVGL_Startup because it is a gesture
    // POLICY decision and belongs beside the handler that reads it.
    // The setter is lv_indev_set_gesture_min_distance(), and it takes a
    // uint8_t - so the value is CLAMPED. sc(50) is 87 on WS_P4_5, still inside
    // the range, but a denser panel than anything in the fleet would wrap
    // silently and make every stray finger a swipe.
    if (lv_indev_t *indev = lv_indev_get_next(NULL)) {
        const int32_t want = UIToolkit::sc(50);
        lv_indev_set_gesture_min_distance(indev, (uint8_t)(want > 255 ? 255 : want));

        // Every press, whatever it lands on. See screenPressCb().
        lv_indev_add_event_cb(indev, screenPressCb, LV_EVENT_PRESSED, NULL);
    }

    // Bottom deck height = screen height - header height.
    int32_t header_h = UIToolkit::systemHeaderPx();
    // The deck ends a FIXED distance below the screen, so that exactly one
    // panel header shows however tall the system header is.
    //
    // It used to be as tall as the whole display while starting below the
    // header, which meant its overhang WAS the header height - so the visible
    // strip changed whenever the header did. That is what put a gap under the
    // word AUDIO when the header went to 35. Making the deck stop at the
    // screen edge fixed the gap and broke the effect instead: the panels
    // became floating buttons with four rounded corners. Both wrong; this is
    // the relationship they were each half of.
    int32_t deck_h   = lv_obj_get_height(screen) - header_h + UIToolkit::deckOverhangPx();

    // --= LAYER 1: BOTTOM DECK =--
    // Contains the Audio/Display panels.
    lv_obj_t *deck = lv_obj_create(screen);
    _deck = deck;
    lv_obj_set_size               (deck, lv_pct(100), deck_h);
    lv_obj_set_y                  (deck, header_h); // Bottom of header
    lv_obj_set_flex_flow          (deck, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align         (deck, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_bg_opa       (deck, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (deck, 0, 0);
    lv_obj_set_style_pad_all      (deck, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_gap      (deck, UIToolkit::sc(10), 0);
    lv_obj_clear_flag             (deck, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag             (deck, LV_OBJ_FLAG_SCROLLABLE);

    // --= LAYER 2: UPPER DECK (System Panel) =--
    // Full screen transparent layer to hold the system drawer.
    lv_obj_t *upper_deck = lv_obj_create(screen);
    lv_obj_set_size               (upper_deck, lv_pct(100), lv_pct(100));
    lv_obj_set_y                  (upper_deck, 0); // Hidden behind header
    lv_obj_set_style_bg_opa       (upper_deck, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (upper_deck, 0, 0);
    // PADDING ZERO, and this one line was two visible bugs.
    //
    // lv_obj carries theme padding by default, and lv_obj_set_pos() positions
    // against the parent's CONTENT area - inside that padding. So the system
    // drawer, which sets its own x and y precisely, was being shifted by an
    // invisible margin it could not see: down, leaving a gap between it and
    // the header bar (the "floating in the air" look), and right, pushing its
    // right border off the edge of the screen.
    //
    // Both were reported as separate faults and both are this. A transparent
    // full-screen positioning layer must not have padding.
    lv_obj_set_style_pad_all      (upper_deck, 0, 0);
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_SCROLLABLE);

    // THE DISMISS SCRIM: an invisible sheet covering everything below the
    // drawer, shown only while the drawer is open.
    //
    // The owner: "with the panel open tapping on cards does not close the
    // panel." A card is clickable and consumes the tap, so no amount of
    // screen-level handling can ever see it - the event stops at the card.
    //
    // A scrim is the standard answer and it is better than bubbling the card's
    // click, because it also makes the first tap MEAN dismiss: putting a
    // drawer away should not also toggle the light you happened to tap.
    //
    // Created here, immediately after upper_deck, because it is a child of it.
    _dismissScrim = lv_obj_create(upper_deck);
    lv_obj_remove_style_all       (_dismissScrim);
    lv_obj_set_size               (_dismissScrim, lv_pct(100), lv_pct(100));
    lv_obj_set_pos                (_dismissScrim, 0, 0);
    lv_obj_set_style_bg_opa       (_dismissScrim, LV_OPA_TRANSP, 0);
    lv_obj_add_flag               (_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag               (_dismissScrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(_dismissScrim, [](lv_event_t *ev) {
        (void)ev;
        if (s_self) s_self->_pnlSystem.close();
    }, LV_EVENT_CLICKED, NULL);

    // Bottom panel open -> close system panel
    UIToolkit::registerSystemCloseCb(closeSystemPanelCb);

    // System open -> hide touch window
    _pnlSystem.setOnToggleCallback([](bool isOpen) {
        if (s_self && s_self->_dismissScrim) {
            if (isOpen) lv_obj_clear_flag(s_self->_dismissScrim, LV_OBJ_FLAG_HIDDEN);
            else        lv_obj_add_flag  (s_self->_dismissScrim, LV_OBJ_FLAG_HIDDEN);
        }
        // If System Panel is OPEN (true), Hide Touch Window (false)
        // pnlDisplay.setTouchWindowVisibility(!isOpen);
    });

#ifdef HAS_AUDIO_HW
    _pnlAudio.init(deck);
#endif
    _pnlDisplay.init(deck);
    _pnlSystem.init(upper_deck, &_header);
    // The scheme button was built reading "Fleet" whatever the board booted
    // into - visible in the owner's photos with Slate on screen. Say the truth.
    _pnlSystem.setSchemeLabel(UI::pal().name);

    // The System panel's "Dump Config" button re-runs the report with Serial
    // echo on. Registered rather than reached for: Panel_System used to call
    // an `extern void debug_dump_config(bool)` straight into main.
    _pnlSystem.setOnDumpRequested([this]() {
        SystemReport::run(_core, true); // manually triggered - mirror to Serial too
    });

    // The card binder. Started AFTER UI::begin() because a card reads tokens
    // the moment it is built, and before any page exists because a page
    // registers its cards with it. Nothing has a card yet - this only starts
    // the pump.
    _binder.begin(&_core.entities());

    // Cards read UI::pal() on every render and cache nothing, which is what
    // makes a live scheme change possible at all. This is the other half of
    // that bargain: somebody has to tell them the scheme moved.
    UI::onSchemeChanged([]() { if (s_self) s_self->_binder.restyleAll(); });

    // The System panel's "Cards" button. Registered rather than reached for,
    // for the same reason Dump Config is: Panel_System has no business knowing
    // the entity registry exists.
    _pnlSystem.setOnCardsRequested([this]() {
        // THE DASHBOARD COMES DOWN FIRST.
        //
        // The bench builds its own page of thirteen more cards. With the
        // dashboard still alive that is twenty-six cards and two full widget
        // trees, which is precisely the condition 2.4 documented as fatal:
        // "Allocating layer buffer failed", then a reboot. The owner hit it
        // immediately - tapping Cards reset the board.
        //
        // The close handler below rebuilds it on the way back.
        destroyDashboard();
        CardDemo::show(_core.entities(), _binder);
    });

    _pnlSystem.setOnTokensRequested([this]() { openTokens();  });
    _pnlSystem.setOnLogRequested   ([this]() { openLog();     });

    // The log page's own Dump button re-runs the report and redraws it in
    // place. The panel keeps buffering while the page is open, so this is just
    // "run it again and show me".
    // Dump just runs the report. The REDRAW comes from the drain, not from
    // here - see the note on the button in LogPage.cpp. Calling refresh() on
    // this line redrew the text from before the dump, because every line was
    // still queued.
    LogPage::setDumpHandler([this]() { SystemReport::run(_core, true); });

    LogPage::setClearHandler([this]() { _pnlSystem.clearLog(); });

    // The single wire that makes the log live: Panel_System drains five lines
    // a tick and tells us, so an open page scrolls as output arrives. It is
    // also what restores the effect the owner liked in the panel's first
    // incarnation, before the log moved to its own screen.
    _pnlSystem.setLogOnChange([](const char *text) { LogPage::refresh(text); });
    LogPage::setCloseHandler([this]() { rebuildDashboard(); });
    _pnlSystem.setOnSchemeRequested([this]() { cycleScheme(); });

    // The grid knobs. The panel knows a button was pressed; what a column is
    // remains entirely this class's business.
    _pnlSystem.setOnGridAction([this](Panel_System::GridAction a) {
        switch (a) {
            case Panel_System::GridAction::CARD_W_DOWN: nudgeColumns(-1);  break;
            case Panel_System::GridAction::CARD_W_UP:   nudgeColumns(+1);  break;
            case Panel_System::GridAction::ASPECT_DOWN: nudgeRows(-1);     break;
            case Panel_System::GridAction::ASPECT_UP:   nudgeRows(+1);     break;
            case Panel_System::GridAction::DECK_TOGGLE: toggleDeck();       break;
            case Panel_System::GridAction::HDR_CYCLE:   cycleHeader();     break;
            case Panel_System::GridAction::BAR_CYCLE:   toggleHeaderBar(); break;
            case Panel_System::GridAction::VARIANT_CYCLE: cycleVariant();  break;
            case Panel_System::GridAction::FILL_CYCLE:    cycleFill();     break;
            case Panel_System::GridAction::AREA_TOGGLE:   toggleArea();    break;
            case Panel_System::GridAction::LABEL_CYCLE:   cycleLabel();    break;
        }
    });

    // The card page's own Dump button runs the same report the System panel's
    // does, Serial echo and all.
    CardDemo::setDumpHandler([this]() { SystemReport::run(_core, true); });

    // The bench derives the grid from its own host - a screen minus a button
    // bar - and UI::grid() is global. Rebuilding on the way back is the only
    // thing that reliably restores the dashboard's own geometry.
    CardDemo::setCloseHandler([this]() { rebuildDashboard(); });
    ReferencePage::setCloseHandler([]() { if (s_self) s_self->rebuildDashboard(); });

    // Contribute the one LVGL-dependent section of the report.
    SystemReport::addSection("UI STATE", reportUiSection);

    // --= Z-INDEX SANDWICH =--
    // 0. Touch overlay (bottom - hidden by default, set in Panel_Display::init)
    // 1. Dashboard (the cards)
    // 2. Deck (bottom panels, which may expand OVER the cards)
    // 3. System panel (middle - slides out)
    // 4. Header (top - covers the system panel's top edge)
    lv_obj_move_to_index(deck, 1);
    lv_obj_move_to_index(upper_deck, 2);
    lv_obj_move_to_index(_header.getContainer(), 3);

    // THE DASHBOARD IS THE BOOT SCREEN NOW.
    //
    // Until 2.5 the device booted into the Phase 1 UI - a header and two
    // accordion panels - and the cards were a demo behind a button in the
    // System drawer. That was the right shape while the card layer was being
    // built and the wrong one the moment it worked.
    //
    // Built LAST so it can read the real geometry of everything above it, and
    // moved to index 1 so the deck's panels expand over the cards rather than
    // pushing them - which is what the owner asked to see.
    buildDashboard();
    SystemCore::heapMark("after dashboard");
}

// ---------------------------------------------------------------------------
// The dashboard
// ---------------------------------------------------------------------------

void GUIManager::buildDashboard() {
    lv_obj_t *screen = lv_screen_active();

    const int32_t headerH = UIToolkit::systemHeaderPx();

    // WHAT THE DECK ACTUALLY COSTS, MEASURED RATHER THAN ASSUMED.
    //
    // The first version of this reserved sc(85) + sc(20), on the reasoning that
    // UIToolkit builds a collapsed panel at sc(85). That was wrong by about
    // 60 px and the owner spotted it on the glass: "with the deck present there
    // is always a massive gap between the bottom row and the deck".
    //
    // The panel really is 85 px tall. It is just that the deck is as tall as
    // the whole screen and starts BELOW the header, so its bottom edge hangs
    // 50 px off the bottom of the panel - and a bottom-aligned panel therefore
    // has its lower ~40 px off-screen. What you can actually see is the panel's
    // sc(45) header and nothing else, which is exactly what he described.
    //
    // Rather than encode that coincidence as a number, ask the objects where
    // they are. This survives someone changing a panel's height, and it is the
    // same "verify from outside" rule the connectivity work runs on.
    int32_t deckReserve = 0;
    if (_showDeck && _deck) {
        lv_obj_update_layout(screen);

        // lv_obj_get_coords() gives ABSOLUTE screen coordinates. The x/y
        // accessors are relative to the parent, and the deck's children have a
        // different parent from the screen - mixing the two would measure
        // nothing meaningful.
        lv_area_t sc_area;
        lv_obj_get_coords(screen, &sc_area);
        const int32_t screenBottom = sc_area.y2;

        int32_t topMost = screenBottom;
        const uint32_t kids = lv_obj_get_child_count(_deck);
        for (uint32_t i = 0; i < kids; i++) {
            lv_obj_t *k = lv_obj_get_child(_deck, i);
            if (!k || lv_obj_has_flag(k, LV_OBJ_FLAG_HIDDEN)) continue;
            lv_area_t k_area;
            lv_obj_get_coords(k, &k_area);
            if (k_area.y1 < topMost) topMost = k_area.y1;
        }
        deckReserve = screenBottom - topMost;
        if (deckReserve < 0) deckReserve = 0;
        // NO EXTRA GAP. The page's own INSET already holds the bottom row off
        // the edge of its host, and with the deck hidden that inset is exactly
        // the margin the owner liked ("6x3 sits neatly against the bottom
        // margin"). Adding a gap on top of it made the deck case visibly
        // looser than the hidden case for no reason - his "modest gap".
    }

    int32_t h = lv_obj_get_height(screen) - headerH - deckReserve;
    if (h < UIToolkit::sc(80)) h = lv_obj_get_height(screen) - headerH;

    _dashHost = lv_obj_create(screen);
    lv_obj_set_size               (_dashHost, lv_pct(100), h);
    lv_obj_set_y                  (_dashHost, headerH);
    lv_obj_set_style_bg_opa       (_dashHost, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_dashHost, 0, 0);
    lv_obj_set_style_pad_all      (_dashHost, 0, 0);
    lv_obj_clear_flag             (_dashHost, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_dashHost, LV_OBJ_FLAG_CLICKABLE);

    // The grid is derived from THIS CONTAINER, not from the screen and not
    // from bsp_display.WIDTH/HEIGHT. Deriving it from the panel would be wrong
    // on every rotated board and wrong here as well, because the cards do not
    // get the whole screen - the same mistake begin() already documents for
    // UI::begin().
    lv_obj_update_layout(screen);
    UI::setViewport(lv_obj_get_content_width(_dashHost),
                    lv_obj_get_content_height(_dashHost));

    _page = new CardPage();
    _page->begin(_dashHost, &_binder);
    _page->setRowsOverride(_rowsOverride);

    // The fleet spec is const and carries a header default; the live choice is
    // laid over a copy of it. PageSpec is a plain aggregate, so this is a copy
    // and an assignment rather than any kind of mechanism - which is the point
    // of the spec being data.
    // WHICH PAGE THIS BOARD BOOTS INTO.
    //
    // An either/or rather than a choice, and only until 2.6. A board can show
    // exactly one page today, and 18 HA cards plus 12 fleet cards fit nowhere,
    // so -D USE_HA_DASHBOARD swaps the whole page. When horizontal swipes land
    // this becomes two pages and the flag goes away - HA_PAGE already carries
    // id 2 for that day.
#ifdef USE_HA_DASHBOARD
    PageSpec page = HA_PAGE;
#else
    PageSpec page = FLEET_PAGE;
#endif
    page.headerDefault  = _hdr;
    page.variantDefault = _variant;
    page.showArea       = _showArea;
    _page->applySpec(page, _core.entities());

    _pnlSystem.setHeaderLabel(_hdr == CardHeaderStyle::HDR_TAG  ? "Tag"
                            : _hdr == CardHeaderStyle::HDR_BAR  ? "Bar"
                                                                : "No hdr");

    // THE DASHBOARD GOES TO THE BACK, by role rather than by index.
    //
    // This was `move_to_index(_dashHost, 1)`, which assumed the touch overlay
    // was still the screen's child 0. Moving that overlay to lv_layer_top()
    // shifted every remaining index down by one, so index 1 quietly became
    // ABOVE the deck instead of below it - and the panels that used to animate
    // over the cards started expanding behind them.
    //
    // move_background() says what is actually meant and cannot rot when the
    // screen's child list changes again.
    lv_obj_move_background(_dashHost);

    if (_deck) {
        if (_showDeck) lv_obj_clear_flag(_deck, LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag  (_deck, LV_OBJ_FLAG_HIDDEN);
    }
}

void GUIManager::destroyDashboard() {
    // The page first: it deletes its cards, and a card deregisters from the
    // binder in its destructor's path. Deleting the host out from under them
    // would take the widgets without the bookkeeping.
    if (_page)     { delete _page;              _page = nullptr; }
    if (_dashHost) { lv_obj_delete(_dashHost);  _dashHost = nullptr; }
}

void GUIManager::rebuildDashboard() {
    destroyDashboard();
    buildDashboard();
    // A rebuild is the one thing that happens over and over on a running
    // board. If internal heap trends down across these, the leak is here.
    SystemCore::heapMark("after rebuild");
}

// ---------------------------------------------------------------------------
// The grid knobs
//
// A rebuild rather than CardPage::relayout(), and the difference is the whole
// reason these exist: a card decides compact-vs-full from its cell height when
// it is BUILT, so moving the grid without rebuilding would re-place the same
// cards at the same level of detail and show nothing.
// ---------------------------------------------------------------------------

// Wraps through 0 = AUTO, so the button can always get back to the derived
// layout. From auto the first press moves one step from what is on screen,
// which is what makes it feel like a nudge rather than a jump.
static uint8_t cycleCount(uint8_t current, uint8_t showing, int8_t steps, uint8_t maxN) {
    int n = (current ? current : showing) + steps;
    if (n > (int)maxN) return 0;      // past the top -> auto
    if (n < 1)         return 0;      // past the bottom -> auto
    return (uint8_t)n;
}

// The screen's own background. Called at start-up and again on every scheme
// change, which is the part that was missing: GROUND was applied once in
// begin() and never again, so switching scheme left the strip behind the deck
// painted in the old scheme's colour. Reported on both P4 boards.
void GUIManager::applyGround() {
    lv_obj_t *screen = lv_screen_active();
    if (screen) lv_obj_set_style_bg_color(screen, UI::c(UI::pal().GROUND), LV_PART_MAIN);
}

void GUIManager::cycleScheme() {
    UI::cycleScheme();   // Fleet -> Midnight -> Linen; the order lives in UITokens.cpp
    applyGround();
    _header.restyle();
    _pnlSystem.setSchemeLabel(UI::pal().name);

    // A rebuild rather than a restyle. Cards would survive restyleAll() - that
    // is what "never cache a colour" buys - but a METRICS change moves radii
    // and padding, which a card reads when it is built.
    rebuildDashboard();
}

void GUIManager::openTokens() {
    // THE DASHBOARD STANDS DOWN FIRST, exactly as it does for the card bench.
    //
    // ReferencePage builds a whole second screen. With the dashboard's cards
    // still alive that is two full widget trees, and LVGL then fails to
    // allocate the layer buffers it needs to composite - "No memory: 482x17",
    // repeated, and on WS_P4_5 an unrecoverable board. The owner hit it by
    // going into Tokens, changing scheme, and coming back.
    destroyDashboard();
    ReferencePage::show();
}

// Hide or show the system header. NOT a size cycle any more.
//
// The owner, 2026-09-19: "35 is now the permanent header bar size. No more
// adjusting the header size. However we do still need to be able to hide/show
// the header as without header and deck we have the most screen real estate
// available."
//
// The size cycle (50/45/40/35/30/none) had also produced a trap: hiding the
// bar moved it off-screen and nothing could bring it back, because every path
// that restored it went through the same cycle that had six positions and one
// of them was "gone". Two states cannot get lost in the same way.
void GUIManager::toggleHeaderBar() {
    _headerHidden = !_headerHidden;
    UIToolkit::systemHeaderH = _headerHidden ? 0 : UI_HEADER_H;

    // Any peek in flight belongs to the state we are leaving.
    if (_headerPeeking) { _headerPeeking = false;
                          if (_peekTimer) { lv_timer_delete(_peekTimer); _peekTimer = nullptr; } }

    lv_obj_t *hdr = _header.getContainer();
    if (hdr) {
        lv_anim_delete(hdr, nullptr);        // kill a half-finished peek slide
        lv_obj_set_height(hdr, UIToolkit::sc(UI_HEADER_H));
        lv_obj_set_y     (hdr, 0);
        if (_headerHidden) lv_obj_add_flag  (hdr, LV_OBJ_FLAG_HIDDEN);
        else               lv_obj_clear_flag(hdr, LV_OBJ_FLAG_HIDDEN);
    }

    // The drawer hangs off the header, so it moves with it - otherwise hiding
    // the bar leaves the panel floating a header's height down an empty
    // screen. Needed here as well as on open, because the drawer may be open
    // RIGHT NOW while this runs.
    syncPanelOffset();

    _pnlSystem.setBarLabel(_headerHidden ? "Show Bar" : "Hide Bar");
    rebuildDashboard();
    Serial.printf("[UI] system header %s\n", _headerHidden ? "hidden" : "shown");
}

void GUIManager::openLog() {
    // Dashboard down first, same as the bench and the token page - two full
    // widget trees is what exhausts LVGL's layer buffers.
    destroyDashboard();
    LogPage::show(_pnlSystem.logText());
}

// --- The three controls #50 added --------------------------------------
//
// All three drive statics that the card layer has had since 2.4 and that
// nothing on the device could reach. They are new CONTROLS, not new behaviour,
// which is why each is a handful of lines rather than a feature.

void GUIManager::cycleVariant() {
    // A rebuild, not a restyle, for the same reason cycleHeader() is: a card
    // decides compact-vs-full in build() by measuring its cell, so overriding
    // the decision means making it again.
    _variant = (_variant == CardVariant::VAR_AUTO)   ? CardVariant::VAR_FULL
             : (_variant == CardVariant::VAR_FULL)   ? CardVariant::VAR_COMPACT
                                                     : CardVariant::VAR_AUTO;
    rebuildDashboard();
    const char *n = (_variant == CardVariant::VAR_AUTO) ? "Auto"
                  : (_variant == CardVariant::VAR_FULL) ? "Full" : "Cmpct";
    _pnlSystem.setVariantLabel(n);
    Serial.printf("[Cards] variant override -> %s\n", n);
}

void GUIManager::cycleFill() {
    _fill = (_fill == StateCardFill::FILL_SURFACE) ? StateCardFill::LIGHT_ICON
                                                   : StateCardFill::FILL_SURFACE;
    StateCard::setFill(_fill);
    // Fill IS a live style - StateCard reads it in render() - so the binder's
    // restyle is enough and a rebuild would be a waste of a page.
    _binder.restyleAll();
    const char *n = (_fill == StateCardFill::FILL_SURFACE) ? "Fill" : "Icon";
    _pnlSystem.setFillLabel(n);
    Serial.printf("[Cards] active state -> %s\n", n);
}

void GUIManager::cycleLabel() {
    // The FLEET default, which every page and card inherits unless it says
    // otherwise - the same place TempUnit's fleet default lives. Live, like
    // Fill: StateCard resolves it in render(), so a restyle is enough.
    const CardLabel cur  = cardLabelMode();
    const CardLabel next = (cur == CardLabel::LBL_NAME)  ? CardLabel::LBL_STATE
                         : (cur == CardLabel::LBL_STATE) ? CardLabel::LBL_NONE
                                                         : CardLabel::LBL_NAME;
    cardSetLabelMode(next);
    _binder.restyleAll();
    const char *n = (next == CardLabel::LBL_NAME)  ? "Name"
                  : (next == CardLabel::LBL_STATE) ? "State" : "No lbl";
    _pnlSystem.setLabelModeLabel(n);
    Serial.printf("[Cards] state-card label -> %s\n", n);
}

void GUIManager::toggleArea() {
    _showArea = !_showArea;
    // Through the PAGE SPEC rather than the card-layer static.
    //
    // PageSpec::showArea already existed and buildDashboard() now sets it from
    // this flag, so the page stays the single description of itself. Calling
    // Card::setShowAreaDefault() as well would give the same answer twice from
    // two places, and they would disagree the first time a second page exists.
    rebuildDashboard();
    _pnlSystem.setAreaLabel(_showArea ? "Area" : "No Area");
    Serial.printf("[Cards] show area -> %s\n", _showArea ? "on" : "off");
}

void GUIManager::cycleHeader() {
    // A rebuild, not a restyle. A header bar is CREATED in Card::build() rather
    // than styled in restyle(), and that is correct - it is a structural choice
    // a card makes once, not a live style. CardDemo's own header button has
    // always worked this way for the same reason.
    _hdr = (_hdr == CardHeaderStyle::HDR_TAG) ? CardHeaderStyle::HDR_BAR
         : (_hdr == CardHeaderStyle::HDR_BAR) ? CardHeaderStyle::HDR_NONE
                                              : CardHeaderStyle::HDR_TAG;
    rebuildDashboard();
    Serial.printf("[Cards] header mode -> %s\n",
                  _hdr == CardHeaderStyle::HDR_TAG ? "tag"
                : _hdr == CardHeaderStyle::HDR_BAR ? "bar" : "none");
}

void GUIManager::nudgeColumns(int8_t steps) {
    _colsOverride = cycleCount(_colsOverride, (uint8_t)UI::grid().cols, steps, UI_MAX_COLS);
    UI::setColumnsOverride(_colsOverride);
    rebuildDashboard();

    char lbl[12];
    if (_colsOverride) snprintf(lbl, sizeof(lbl), "Col %u", (unsigned)_colsOverride);
    else               snprintf(lbl, sizeof(lbl), "Col A");
    _pnlSystem.setColsLabel(lbl);
}

void GUIManager::nudgeRows(int8_t steps) {
    // The page is rebuilt from scratch on every change, so the override is
    // held HERE and handed to each new page - a CardPage cannot remember it.
    _rowsOverride = cycleCount(_rowsOverride, _page ? _page->cellRows() : 1, steps, UI_MAX_ROWS);
    rebuildDashboard();

    char lbl[12];
    if (_rowsOverride) snprintf(lbl, sizeof(lbl), "Row %u", (unsigned)_rowsOverride);
    else               snprintf(lbl, sizeof(lbl), "Row A");
    _pnlSystem.setRowsLabel(lbl);
}

void GUIManager::toggleDeck() {
    _showDeck = !_showDeck;
    rebuildDashboard();
    Serial.printf("[Cards] deck %s\n", _showDeck ? "shown" : "hidden");
}

void GUIManager::tick() {
    _header.tick();
    _pnlDisplay.tick();
#ifdef HAS_AUDIO_HW
    _pnlAudio.tick();
#endif
}
