#include "GUIManager.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "UI/Kit.h"                      // the component kit's styles, filled at begin()
#include "UI/Sheet.h"                    // sheetShowing(): gestures stand down under any sheet
#include "SystemReport.h"
#include "LVGL_Startup.h"                // drawBufInfo(), for [UI STATE]
#include "Cards/CardDemo.h"
#include "Cards/CardIcons.h"   // cardSetLabelMode(), for the Label knob
#include "Cards/CardPopup.h"   // modal: gestures stand down while it is open
#include "UI/ReferencePage.h"
#include "UI/LogPage.h"
#include "UI/Screenshot.h"
#include "UI/Bench.h"
#include "Dashboards/Dashboard_Fleet.h"
#include "Dashboards/Dashboard_HA.h"   // both pages on every board
#include "bsp_loader.h"
#include "Settings.h"   // the claims on saved settings, and [SETTINGS]
#include "Cards/CardGroups.h"
#include "Cards/CardAdded.h"   // cards added on the device
#include "Cards/ClusterCard.h" // the clusters' header glyphs
#include "UI/SlotPanels.h"     // the header slots' panels
#include "UI/HaOffer.h"        // what HA offers
#include <string.h>
#ifdef DEBUG_PAGE_TIMING
#include <esp_timer.h>   // rebuildDashboard()'s two halves
#endif

// LVGL's event and callback APIs take plain function pointers with no user
// context of their own for some of the toolkit hooks, so the single dashboard
// instance is reachable from a file-static. There is exactly one GUIManager by
// construction; if that ever stops being true these become member callbacks
// with user_data, which LVGL does support for lv_event_cb.
static GUIManager *s_self = nullptr;

// THE DASHBOARD'S PAGES, in swipe order - initPages() builds from it, and the
// settings file's claims (K35) ask it which card ids exist.
static const PageSpec *const DASH_PAGES[] = { &HA_PAGE, &FLEET_PAGE };
static constexpr uint8_t DASH_PAGE_N = sizeof(DASH_PAGES) / sizeof(DASH_PAGES[0]);

// Saved settings nobody claims (K35). A card's entry is claimed when
// its id is on a page - on ANY page, so a card this board skips (its entity not
// here) or drops for room still claims its settings. An entity's is claimed
// when the registry has it.
static EntityRegistry *s_claimReg = nullptr;

static bool compiledCard(const char *id) {
    bool found = false;   // a group card's members are cards too
    for (uint8_t p = 0; p < DASH_PAGE_N && !found; p++)
        forEachCardSpec(*DASH_PAGES[p], [&](const CardSpec &cs) {
            if (cs.id && !strcmp(cs.id, id)) found = true;
        });
    return found;
}

// A card on a page, or one added on the device - removed ones too: they
// are the page's pool (K66), not unclaimed.
static bool claimCard(const char *id) { return compiledCard(id) || CardAdded::known(id); }

// The current page as built: its compiled cards with the ones added on the
// device among them (CardAdded::merge()). Static: the page's cards keep
// pointers into the specs' strings only through CardAdded's own records.
static constexpr uint8_t PAGE_CARDS_MAX = 64;
static CardSpec s_pageCards[PAGE_CARDS_MAX];
// Added cards on the page just built whose entity was not in the table yet (a
// group's member, learnt once HA answers): the page is built again when the
// table grows (tick()).
static uint8_t s_addedWaiting = 0;
static uint8_t s_regCountAtBuild = 0;

static bool claimEntity(const char *id) { return s_claimReg && s_claimReg->find(id); }

static void reportSettingsSection() {
    const Settings::Stats st = Settings::stats();
    SystemReport::line("  File: %s, %lu bytes; %lu saves this boot, %lu failed%s",
                       Settings::persistent() ? "/cfg/settings.json" : "NOT SAVING (RAM only or read-only)",
                       (unsigned long)st.bytes, (unsigned long)st.saves, (unsigned long)st.failures,
                       st.pending ? ", a change pending" : "");
    char boot[160];
    Settings::bootNote(boot, sizeof(boot));
    SystemReport::line("  Boot: %s", boot);
    uint8_t n = 0;
    Settings::forEachUnclaimed([](const char *sec, const char *id, void *c) {
        (*(uint8_t *)c)++;
        SystemReport::line("  Unclaimed %s: %s", sec, id);
    }, &n);
    // The groups for card labels (K46): which are HA areas, which are local.
    CardGroups::forEach([](const CardGroups::Group &g, void *) {
        if (g.haName[0])
            SystemReport::line("  Group %-14s #%06lX  HA area \"%s\"%s", g.name, (unsigned long)g.color, g.haName,
                               strcmp(g.name, g.haName) ? " (renamed here)" : "");
        else
            SystemReport::line("  Group %-14s #%06lX  local%s", g.name, (unsigned long)g.color,
                               g.solo ? ", one card's own" : "");
    }, nullptr);
    if (!n) SystemReport::line("  Nothing unclaimed");
    else    SystemReport::line("  Clear with /settings?prune=cards or prune=entities (a paused member may only "
                               "look missing until HA reports its group)");
}

// Per the repo's debug-flag convention (CLAUDE.md): a diagnostic worth keeping
// rather than deleting, off unless an environment asks for it. It answers "did
// the swipe even arrive".
#ifdef DEBUG_GESTURE
    #define DBG_GESTURE(...) Serial.printf("[Gesture:debug] " __VA_ARGS__)
#else
    #define DBG_GESTURE(...) do {} while (0)
#endif

#ifdef DEBUG_FRAMES
// ---------------------------------------------------------------------------
// Every frame of every burst of motion, over serial.
//
// This times whatever is on screen as a finger drives it (the deck, the
// drawer, the header peek): each rendered frame's gap from the previous one
// and its render time, printed
// as one line once the screen has been still for 200 ms. A burst is any run
// of frames less than 200 ms apart - an animation plus its first and last
// frames, which is exactly where the hesitations hide.
// ---------------------------------------------------------------------------
namespace {
constexpr uint8_t FL_MAX = 40;
uint32_t s_flStart[FL_MAX], s_flRender[FL_MAX];
uint8_t  s_flN = 0;
uint32_t s_flT0 = 0;

void flRenderStart(lv_event_t *e) {
    (void)e;
    s_flT0 = millis();
}
void flRenderReady(lv_event_t *e) {
    (void)e;
    if (s_flN < FL_MAX) { s_flStart[s_flN] = s_flT0; s_flRender[s_flN] = millis() - s_flT0; s_flN++; }
}
void flFlush(lv_timer_t *t) {
    (void)t;
    if (!s_flN || millis() - s_flStart[s_flN - 1] < 200) return;
    if (s_flN >= 3) {
        char line[400];
        int n = 0;
        for (uint8_t i = 0; i < s_flN && n < (int)sizeof(line) - 16; i++) {
            const uint32_t gap = i ? s_flStart[i] - s_flStart[i - 1] : 0;
            n += snprintf(line + n, sizeof(line) - n, " %lu/%lu",
                          (unsigned long)gap, (unsigned long)s_flRender[i]);
        }
        Serial.printf("[Frames] %u frames in %lu ms (gap/render ms):%s\n", (unsigned)s_flN,
                      (unsigned long)(s_flStart[s_flN - 1] + s_flRender[s_flN - 1] - s_flStart[0]), line);
    }
    s_flN = 0;
}
void startFrameLog() {
    lv_display_t *d = lv_display_get_default();
    if (!d) return;
    lv_display_add_event_cb(d, flRenderStart, LV_EVENT_RENDER_START, nullptr);
    lv_display_add_event_cb(d, flRenderReady, LV_EVENT_RENDER_READY, nullptr);
    lv_timer_create(flFlush, 50, nullptr);
}
} // namespace
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
// come through here.
//
// The drawer hangs off whatever bar is currently above it, and there are three
// possibilities: a permanent header, a peeked one, or nothing. The offset is
// computed in one place so both entry points agree.
void GUIManager::syncPanelOffset() {
    int32_t top = 0;
    if (!_headerHidden)      top = UIToolkit::sc(UI_HEADER_H);        // permanent bar
    else if (_headerPeeking) top = UIToolkit::sc(UI_HEADER_PEEK_H);   // temporary one
    // else: no bar at all, so the drawer starts at the top of the screen.
    _pnlSystem.setTopOffset(top);
    // ...and a slot's drawer, which hangs from the same bar.
    Kit::drawerSetTop(_slotDrawer, top);
}

// THE SYSTEM PANEL IS THE STATUS SLOT'S (D-13): it hangs under that slot,
// however it was asked for - the slot, a swipe down, the harness.
void GUIManager::toggleSystemPanel() {
    syncPanelOffset();
    if (!_pnlSystem.isExpanded()) {
        closeSlotPanel(false);
        const int s = _header.slotOf(SlotKind::SLOT_STATUS);
        if (s >= 0) _pnlSystem.setAnchorX(_header.slotCentreX((uint8_t)s));
        else        _pnlSystem.setAnchorX(lv_obj_get_width(lv_screen_active()));   // no slot: the right edge
        _panelTouchMs = millis();
    }
    _pnlSystem.toggle();
}

// The header's slots and their panels: GUIManagerHeader.cpp.

// ---------------------------------------------------------------------------
// Swipe navigation. A swipe down from the top edge opens the system panel
// (right half) or UI Tweaks (left half); a swipe up from the bottom edge shows
// the deck (left) or the FPS / CPU overlay (right); a swipe sideways changes
// page. Each is handled in screenGestureCb().
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
// Deliberately NOT a rebuild. Showing or hiding the header for good rebuilds
// the dashboard because it changes how much room the cards get; a peek must
// not, or the grid would re-plan and the cards would jump every time somebody
// glanced at the clock.
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
        // header's height down the screen with nothing above it. The peek
        // timeout is about an unattended glance, and a drawer standing open is
        // not one, so the timer simply re-arms instead of firing.
        if (s_self->_pnlSystem.isExpanded() || s_self->_slotPanel >= 0 || s_self->_closingSlot >= 0) return;
        s_self->unpeekHeader();
    }, UI_HEADER_PEEK_MS, nullptr);

    DBG_GESTURE("header peek for %d ms\n", (int)UI_HEADER_PEEK_MS);
}

void GUIManager::unpeekHeader() {
    if (!_headerPeeking) return;
    _headerPeeking = false;

    if (_peekTimer) { lv_timer_delete(_peekTimer); _peekTimer = nullptr; }

    // The drawer hung off the peeked bar while it was there; with it gone the
    // bar's space goes back to the screen. Through syncPanelOffset(), the one
    // place that rule lives.
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
// It is registered on the INPUT DEVICE, not on the screen. The screen only sees
// a press that reaches it; the deck panels, the system drawer, the header and
// every button do not bubble, so a press landing on one of those would leave
// the PREVIOUS swipe's origin in place and make the edge gating act on a stale
// start (with the deck showing, the bottom of the screen is the deck panel, so
// fromBottom would stay true).
//
// lv_indev_add_event_cb() fires for every press regardless of what was hit.
// Cards keep EVENT_BUBBLE for tap-to-dismiss, but nothing here depends on it.
void GUIManager::screenPressCb(lv_event_t *e) {
    (void)e;
    if (!s_self) return;
    lv_indev_t *indev = lv_indev_active();
    if (indev) lv_indev_get_point(indev, &s_self->_pressStart);
    s_self->_panelTouchMs = millis();   // a header panel waits from here (D-22)
}

void GUIManager::screenGestureCb(lv_event_t *e) {
    (void)e;
    if (!s_self) return;

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    // A SHEET IS MODAL. Its tap catcher on the top layer takes
    // every touch outside it, so a gesture should never get here while one is
    // open; this makes sure no page swipe or edge pull can happen underneath -
    // under the card window, or "About this board", reachable in every build
    // (#97).
    if (CardPopup::isOpen() || Kit::sheetShowing()) return;

    const lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    // WHERE THE FINGER STARTED. lv_indev_get_point() returns the CURRENT
    // position, so a downward swipe reports a y near the BOTTOM of its travel
    // and "did this start at the top edge" cannot be asked of it. The press
    // origin is captured in screenPressCb().
    const lv_point_t p = s_self->_pressStart;
    const int32_t scrW = lv_obj_get_width(lv_screen_active());
    const int32_t scrH = lv_obj_get_height(lv_screen_active());
    const bool rightHalf = (p.x >= scrW / 2);

    // EDGE GATING. A vertical swipe only counts when it begins within a band
    // of the edge it is pulling from, as on a phone.
    //
    // The band is in logical pixels so it is the same physical distance on
    // every panel, for the same reason the swipe DISTANCE is.
    const int32_t band = UIToolkit::sc(UI_EDGE_BAND);
    const bool fromTop    = (p.y <= band);
    const bool fromBottom = (p.y >= scrH - band);

    // SWALLOW THE REST OF THIS TOUCH BEFORE ACTING ON IT.
    //
    // LVGL delivers a gesture AND still delivers the press/click of the same
    // finger. Without a release the button under the swipe's start gets a
    // click, a drawer the gesture opened is closed again by tap-to-dismiss, and
    // an action that rebuilds the screen (toggleDeck()) puts new objects under
    // a finger LVGL still considers pressed.
    //
    // So each branch releases just BEFORE it acts, and only for a gesture it
    // acts on: releasing for every gesture would freeze a slider mid-drag on a
    // quick flick. The sliders also stop bubbling gestures at all - see
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
            // With the bar HIDDEN there is an extra step first: one swipe
            // brings the header back for three seconds so the status glyphs
            // can be read without committing to anything; a second swipe
            // while it is showing opens the drawer.
            if (!UIToolkit::systemHeaderH && !s_self->_headerPeeking) {
                s_self->peekHeader();
            } else {
                // Through the one entry point, which owns the top offset.
                s_self->toggleSystemPanel();
            }
        } else {
            // LEFT half: UI Tweaks (D-22); the log is a tap away on the
            // system panel.
            DBG_GESTURE("swipe down, left half -> UI Tweaks\n");
            s_self->openUiTweaks();
        }
        break;

    case LV_DIR_TOP:                         // swipe UP
        // Edge-gated ONLY for the deck, which is the one being pulled up from
        // the bottom and should read as such. Dismissing an open drawer or a
        // peeked header is a "put that away" gesture and is allowed from
        // anywhere - a peek lives at the TOP of the screen, so requiring a
        // swipe from the bottom to dismiss it would make no sense.
        if (!s_self->_pnlSystem.isExpanded() && s_self->_slotPanel < 0 && !s_self->_headerPeeking &&
            !fromBottom) {
            DBG_GESTURE("up from y=%d, not the bottom band\n", (int)p.y);
            break;
        }
        release();
        // A swipe up closes the drawer before it touches the deck. With the
        // panel open it is the obvious "put that away" gesture, and toggling
        // the deck underneath an open panel would change something the user
        // cannot see. A slot's panel the same.
        if (s_self->_pnlSystem.isExpanded()) {
            s_self->_pnlSystem.close();
        } else if (s_self->_slotPanel >= 0) {
            s_self->closeSlotPanel(true);
        } else if (s_self->_headerPeeking) {
            // Put a peeked header away immediately rather than waiting out the
            // three seconds: the gesture that brought it down takes it back.
            s_self->unpeekHeader();
        } else if (rightHalf) {
            // RIGHT half: the FPS/CPU overlay, which sits in that corner - the
            // mirror of the top edge, where the two halves open different
            // things.
            s_self->togglePerf();
        } else {
            s_self->toggleDeck();
        }
        break;

    case LV_DIR_LEFT:                        // swipe LEFT  -> next page
    case LV_DIR_RIGHT:                       // swipe RIGHT -> previous page
        // FROM ANYWHERE on the page - no edge band. Nothing on a card claims a
        // horizontal drag: cards are not scrollable and the deck's sliders do
        // not bubble gestures (UIToolkit).
        //
        // NOT while anything is open over the page - the drawer, a deck panel
        // or a peeking header. A sideways swipe then PUTS IT AWAY instead of
        // turning the page: panels are closed before a page swipe can happen.
        if (s_self->_pnlSystem.isExpanded() || s_self->_slotPanel >= 0 || s_self->_headerPeeking ||
            UIToolkit::getActiveAccordionPanel()) {
            release();
            if (s_self->_pnlSystem.isExpanded()) s_self->_pnlSystem.close();
            if (s_self->_slotPanel >= 0)         s_self->closeSlotPanel(true);
            if (s_self->_headerPeeking)          s_self->unpeekHeader();
            UIToolkit::closeActiveAccordion();
            DBG_GESTURE("horizontal swipe closed what was open; page unchanged\n");
            break;
        }
        release();
        if (dir == LV_DIR_LEFT) s_self->nextPage();
        else                    s_self->prevPage();
        break;

    default:
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
// What the UI is actually set to: the grid and look knobs, so a dump pasted
// into a conversation carries the state it was taken in.
void GUIManager::reportUiSection() {
    if (!s_self) return;
    GUIManager &g = *s_self;

    // LVGL's own version and the engine settings that shape every frame
    // (display-stack.md s8). Here, not in SystemReport, which includes no LVGL.
    {
        const LVGL_Startup::DrawBufInfo b = LVGL_Startup::drawBufInfo();
        char bb[40];
        SystemReport::line("  LVGL:        %d.%d.%d, refresh %d ms, draw buffers %u x %s (%lu lines, %s)",
                           LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                           (int)LV_DEF_REFR_PERIOD, (unsigned)b.count,
                           SystemReport::fmtBytes(b.bytes, bb, sizeof(bb)),
                           (unsigned long)b.lines, b.psram ? "PSRAM" : "internal");
    }
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
                       cardHeaderName(g._hdr),
                       cardVariantName(g._variant),
                       g._showArea ? "shown" : "hidden");

    SystemReport::line("  Chrome:      header bar %s   deck %s   drawer %s",
                       g._headerHidden ? (g._headerPeeking ? "peeking" : "hidden")
                                       : "shown",
                       g._showDeck ? "shown" : "hidden",
                       g._pnlSystem.isExpanded() ? "open" : "closed");
    // The header's slots: in list order, each one's redraws and its form
    // ("-" off the bar, "*N" shedding to form N), the plans, the glyph style, a panel.
    char kinds[112] = "";
    int  k = 0;
    for (uint8_t i = 0; i < g._header.slotCount() && k < (int)sizeof(kinds) - 20; i++) {
        char tail[8] = "";
        if (!g._header.slotShown(i))  snprintf(tail, sizeof(tail), " -");
        else if (g._header.slotForm(i)) snprintf(tail, sizeof(tail), " *%u", (unsigned)g._header.slotForm(i));
        k += snprintf(kinds + k, sizeof(kinds) - k, "%s%s %lu%s", i ? " | " : "", slotKindName(g._header.slotKind(i)),
                      (unsigned long)g._header.slotRedraws(i), tail);
    }
    SystemReport::line("  Header:      %s", kinds);
    SystemReport::line("               planned %lu times%s   glyphs %s   slot panel %d",
                       (unsigned long)g._header.plans(), g._header.planFits() ? "" : " (DOES NOT FIT)",
                       glyphStyleName(g._header.glyphStyle()), (int)g._slotPanel);
}

void GUIManager::begin() {
    // Styles, semantic fonts and the toast layer. This is the design system,
    // which is why it lives here and not in LVGL_Startup.
    SystemCore::heapMark("before UI");
    UIToolkit::init();

    // LVGL shows its FPS/CPU overlay by itself when the display is created
    // (lv_display.c, LV_USE_PERF_MONITOR). It is an instrument, not chrome:
    // off until asked for. See togglePerf().
    lv_sysmon_performance_pause(nullptr);
    lv_sysmon_hide_performance(nullptr);

    // The starting scheme. UITokens defaults to Fleet; the shipped scheme is
    // Midnight, set here rather than in the token file so "which scheme ships"
    // is a GUIManager decision beside the others.
    UI::setScheme(UI_PAL_MIDNIGHT, UI_MET_DARK);
    // ...unless the owner chose another, from the system panel.
    applySavedScheme();
    // The component kit's shared styles, in that scheme: filled once here,
    // refilled with every scheme change after.
    Kit::begin();

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
    // Rows need no equivalent line - buildDashboard() passes _rowsOverride to
    // the page every time it builds - but columns live in the token layer, so
    // the default has to be applied there.
    if (_colsOverride) UI::setColumnsOverride(_colsOverride);

    applyGround();
    lv_obj_clear_flag        (screen, LV_OBJ_FLAG_SCROLLABLE);               // Disable global scrolling

    // --= LAYER 3: HEADER BAR =--
    // Three places of slots (D-16); a tap on a slot opens its
    // panel - onSlotTap(). The device's name has left the bar: About this
    // board shows it.
    _header.init(screen, &_core.conn(), &_core.mqtt(), &_core.entities());
    _header.setGlyphStyle(savedGlyphStyle());
    ClusterCard::setGlyphStyle(savedGlyphStyle());   // and the clusters' header values
    _header.setOnSlotTap([](uint8_t slot) { if (s_self) s_self->onSlotTap(slot); });

    // Swipe navigation, on the screen rather than an overlay - see the notes
    // above screenGestureCb().
    lv_obj_add_event_cb(screen, screenGestureCb, LV_EVENT_GESTURE, NULL);

    // TAP ANYWHERE ELSE TO DISMISS, which is the rule the deck uses: an open
    // drawer, or a peeking header, closes on a tap that is not on the drawer
    // itself.
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
        else if (s_self->_slotPanel >= 0)    s_self->closeSlotPanel(true);
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

        // LONG PRESS AT 250 ms rather than LVGL's default 400 (lv_indev.c),
        // since a long press is the way into every card's window. A slow tap
        // held past a quarter second opens the window instead of toggling. The
        // value lives in CardPopup, whose hold fades over exactly this long.
        lv_indev_set_long_press_time(indev, CardPopup::LONG_PRESS_MS);
    }

#ifdef DEBUG_FRAMES
    startFrameLog();
#endif

    // The card popup's own press hook: where a long press began, and when
    // anything was last touched (its auto-close). Cards/CardPopup.h.
    CardPopup::begin();
    CardPopup::setHistory(&_core.history());   // History's source
    // A group card's cluster, made again from the page for the window (K154, K155).
    CardPopup::setGroupMaker([](const char *id, bool allTiles) -> ClusterCard * {
        return (s_self && s_self->_page) ? s_self->_page->makeGroup(id, allTiles) : nullptr;
    });
    HaOffer::begin(&_core.catalog(), &_core.haProvider(), &_core.entities());   // what HA offers

    // Bottom deck height = screen height - header height.
    int32_t header_h = UIToolkit::systemHeaderPx();
    // The deck ends a FIXED distance below the screen, so that exactly one
    // panel header shows however tall the system header is. (Ending at the
    // screen edge would leave the panels as floating buttons with four
    // rounded corners.)
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
    // PADDING ZERO. lv_obj carries theme padding by default, and
    // lv_obj_set_pos() positions against the parent's CONTENT area - inside
    // that padding. The system drawer, which sets its own x and y precisely,
    // would be shifted by an invisible margin (down, leaving a gap under the
    // header bar, and right, pushing its border off the screen). A transparent
    // full-screen positioning layer must not have padding.
    lv_obj_set_style_pad_all      (upper_deck, 0, 0);
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_SCROLLABLE);

    // THE DISMISS SCRIM: an invisible sheet covering everything below the
    // drawer, shown only while the drawer is open.
    //
    // A card is clickable and consumes the tap, so screen-level handling never
    // sees it - the event stops at the card. A scrim is better than bubbling
    // the card's click, because it also makes the first tap MEAN dismiss:
    // putting a drawer away should not also toggle the light that was tapped.
    //
    // Created here, immediately after upper_deck, because it is a child of it.
    _dismissScrim = lv_obj_create(upper_deck);
    lv_obj_remove_style_all       (_dismissScrim);
    lv_obj_set_size               (_dismissScrim, lv_pct(100), lv_pct(100));
    lv_obj_set_pos                (_dismissScrim, 0, 0);
    lv_obj_set_style_bg_opa       (_dismissScrim, LV_OPA_TRANSP, 0);
    // Switched by CLICKABLE, not HIDDEN - unhiding a full-screen object
    // redraws the full screen in the drawer's first animation frame. See the
    // deck scrim below for the measurement that found it.
    lv_obj_clear_flag             (_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_dismissScrim, [](lv_event_t *ev) {
        (void)ev;
        if (!s_self) return;
        s_self->_pnlSystem.close();
        s_self->closeSlotPanel(true);   // a slot's panel too
    }, LV_EVENT_CLICKED, NULL);

    // Bottom panel open -> close system panel
    UIToolkit::registerSystemCloseCb(closeSystemPanelCb);

    // THE DECK'S OWN DISMISS SCRIM - the drawer's rule, applied to the deck.
    //
    // Any tap outside an exposed panel closes it, as for the system panel.
    // Same mechanism as _dismissScrim above, for the same reason: a card
    // consumes its own tap, so only a sheet over the cards can catch it - and
    // the first tap then MEANS "put the panel away" rather than also toggling
    // the light underneath.
    //
    // On the SCREEN, and slotted in directly ABOVE the cards each time it is
    // shown: over the cards, under the deck, so the open panel itself (and
    // its sliders) still take their touches.
    _deckScrim = lv_obj_create(screen);
    lv_obj_remove_style_all       (_deckScrim);
    lv_obj_set_size               (_deckScrim, lv_pct(100), lv_pct(100));
    lv_obj_set_pos                (_deckScrim, 0, 0);
    lv_obj_set_style_bg_opa       (_deckScrim, LV_OPA_TRANSP, 0);
    // lv_obj_create() makes an object clickable by default, and
    // remove_style_all() does not touch flags - left alone, this sheet would
    // swallow every tap on every card from boot.
    lv_obj_clear_flag             (_deckScrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_deckScrim, [](lv_event_t *ev) {
        (void)ev;
        UIToolkit::closeActiveAccordion();
    }, LV_EVENT_CLICKED, NULL);
    // ALWAYS PRESENT, switched by CLICKABLE alone - never hidden, never moved.
    //
    // Unhiding the scrim or re-ordering it on every open would invalidate the
    // object's whole area, and this object IS the whole screen: every card and
    // every shadow would redraw in the first frame of the panel's animation (a
    // stalled first frame on a time-based animation).
    //
    // A non-clickable object is passed over by the touch search
    // (lv_obj_hit_test() returns false for it, lv_obj_pos.c), so a transparent
    // sheet that is merely not clickable is invisible to touch AND costs no
    // redraw to switch. Its z-order is set once per build, in buildDashboard().
    UIToolkit::registerAccordionChangeCb([](bool open) {
        if (!s_self || !s_self->_deckScrim) return;
        if (open) lv_obj_add_flag  (s_self->_deckScrim, LV_OBJ_FLAG_CLICKABLE);
        else      lv_obj_clear_flag(s_self->_deckScrim, LV_OBJ_FLAG_CLICKABLE);
    });

    // System open -> hide touch window
    _pnlSystem.setOnToggleCallback([](bool isOpen) {
        if (!s_self) return;
        // The status slot is drawn open while its panel shows. The
        // scrim is synced after toggle() has set the drawer's state, so read
        // isOpen rather than isExpanded() here.
        // Its tab closes once the panel has finished closing (setOnClosed below).
        if (isOpen) {
            int32_t pl, pr;
            s_self->_pnlSystem.panelSpan(pl, pr);
            s_self->_header.setOpenSlot(s_self->_header.slotOf(SlotKind::SLOT_STATUS), pl, pr);
        }
        if (s_self->_dismissScrim) {
            if (isOpen || s_self->_slotPanel >= 0) lv_obj_add_flag  (s_self->_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
            else                                    lv_obj_clear_flag(s_self->_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
        }
        // If System Panel is OPEN (true), Hide Touch Window (false)
        // pnlDisplay.setTouchWindowVisibility(!isOpen);
    });
    // THE SLOT STAYS OPEN UNTIL ITS PANEL HAS GONE (D-30): joined for as long
    // as any of the panel shows. Then what waited for no panel to be open
    // (headerPanelsClosed()).
    _pnlSystem.setOnClosed([]() {
        if (!s_self) return;
        const int s = s_self->_header.slotOf(SlotKind::SLOT_STATUS);
        if (s >= 0 && s_self->_header.openSlot() == s) s_self->_header.setOpenSlot(-1);
        s_self->headerPanelsClosed();
    });

#ifdef HAS_AUDIO_HW
    _pnlAudio.init(deck);
#endif
    _pnlDisplay.init(deck);
    // The system panel's CONNECTIVITY reads the links it shows.
    _pnlSystem.setLinks(&_core.conn(), &_core.mqtt());
    _pnlSystem.setConnectivityText([this](char *w, char *h, char *m, size_t n) { connectivityWords(w, h, m, n); });
    _pnlSystem.init(upper_deck, &_header);
    // The other slots' drawer, beside the system panel and over the scrim.
    Kit::drawer(_slotDrawer, upper_deck, Kit::drawerWidth());
    _slotDrawer.onClosed = [](void *) { if (s_self) s_self->slotDrawerClosed(); };
    // The scheme it shows chosen is the one the board booted into.
    _pnlSystem.setScheme(UI::schemeIndex());

    // Every Dump button runs this: the log is emptied first, so the page shows
    // exactly ONE report, whole, top to bottom (there is no Clear button).
    // Manually triggered, so mirrored to Serial too.
    auto dumpFresh = [this]() {
        _pnlSystem.clearLog();
        SystemReport::run(_core, true);
    };

    // The System panel's "Dump Config" button. Registered with the panel
    // rather than reached for through an extern (UI code registers with lower
    // layers).
    _pnlSystem.setOnDumpRequested(dumpFresh);

    // The card binder. Started AFTER UI::begin() because a card reads tokens
    // the moment it is built, and before any page exists because a page
    // registers its cards with it. Nothing has a card yet - this only starts
    // the pump.
    _binder.begin(&_core.entities());

    // Cards read UI::pal() on every render and cache nothing, which is what
    // makes a live scheme change possible at all. This is the other half of
    // that bargain: somebody has to tell them the scheme moved.
    UI::onSchemeChanged([]() { if (s_self) s_self->_binder.restyleAll(); });

    // The system panel's buttons (D-22): Log, UI Tweaks (the window of the
    // looks; Tokens opens from there), About. CardDemo is reachable from a PC.
    _pnlSystem.setOnLogRequested     ([this]() { openLog();      });
    _pnlSystem.setOnUiTweaksRequested([this]() { openUiTweaks(); });

    // The log page's own Dump button re-runs the report and redraws it in
    // place. The panel keeps buffering while the page is open. Dump just runs
    // the report; the REDRAW comes from the drain, not from here (see the note
    // on the button in LogPage.cpp), because every line is still queued when
    // the dump returns.
    LogPage::setDumpHandler(dumpFresh);

    // The single wire that makes the log live: Panel_System drains five lines
    // a tick and tells us, so an open page scrolls as output arrives.
    _pnlSystem.setLogOnChange([](const char *text) { LogPage::refresh(text); });
    LogPage::setCloseHandler([this]() { rebuildDashboard(); });
    _pnlSystem.setOnSchemeChosen([this](uint8_t i) { chooseScheme(i); });

    // The grid knobs. The panel knows a button was pressed; what a column is
    // remains entirely this class's business.
    _pnlSystem.setOnGridAction([this](Panel_System::GridAction a) {
        switch (a) {
            case Panel_System::GridAction::CARD_W_DOWN: nudgeColumns(-1);  break;
            case Panel_System::GridAction::CARD_W_UP:   nudgeColumns(+1);  break;
            case Panel_System::GridAction::ASPECT_DOWN: nudgeRows(-1);     break;
            case Panel_System::GridAction::ASPECT_UP:   nudgeRows(+1);     break;
            case Panel_System::GridAction::GRID_AUTO:   gridAuto();        break;
        }
    });
    // Its Grid line: the counts held ("fixed" - K164 keeps "pin" for a member
    // shown on the page), or "auto" and the units the arranger chose.
    _pnlSystem.setGridText([this](char *out, size_t n) {
        if (_pinCols && _pinRows)  snprintf(out, n, "%u x %u, fixed", (unsigned)_pinCols, (unsigned)_pinRows);
        else if (_pinCols)         snprintf(out, n, "%u columns, fixed", (unsigned)_pinCols);
        else if (_pinRows)         snprintf(out, n, "%u rows, fixed", (unsigned)_pinRows);
        else if (_page && _page->arranged())
            snprintf(out, n, "auto, %u x %u units", (unsigned)_page->unitCols(), (unsigned)_page->unitRows());
        else                       snprintf(out, n, "auto");
    });
    // Its Dashboard summary and Reset (GUIManagerTweaks.cpp). The panel shows
    // no Reset button (K187, K192); only /popup?dash=2 reaches it.
    _pnlSystem.setDashboard([this](char *b, size_t nb, char *d, size_t nd) { dashboardText(b, nb, d, nd); },
                            [this]() { resetDashboard(); });

    // The card page's own Dump button runs the same report the System panel's
    // does, Serial echo and all.
    CardDemo::setDumpHandler(dumpFresh);

    // The bench derives the grid from its own host - a screen minus a button
    // bar - and UI::grid() is global. Rebuilding on the way back is the only
    // thing that reliably restores the dashboard's own geometry.
    CardDemo::setCloseHandler([this]() { rebuildDashboard(); });
    ReferencePage::setCloseHandler([]() { if (s_self) s_self->rebuildDashboard(); });

    // Contribute the one LVGL-dependent section of the report.
    SystemReport::addSection("UI STATE", reportUiSection);

    // Saved settings nobody claims (K35): Settings knows the file, this
    // knows the cards and the entities, so the claims come from here.
    s_claimReg = &_core.entities();
    Settings::setClaims(claimCard, claimEntity);
    SystemReport::addSection("SETTINGS", reportSettingsSection);

    // GET /screenshot (#58), when built with ENABLE_SCREENSHOT. Registered from
    // here because the capture needs LVGL and SystemCore must not; the server
    // itself belongs to SystemCore.
    Screenshot::begin(_core.http());
    // GET /bench (#67), when built with ENABLE_BENCH. Same reasoning.
    Bench::begin(_core.http(), *this);
#ifdef DEBUG_POPUP
    // GET /popup: drive the card popup from a PC (CardPopup.h).
    CardPopup::beginDebug(_core.http());
    // ...and the system panel, so the harness can see it (#97). True when it
    // was already as asked.
    CardPopup::setDrawerHook([](bool open) {
        if (!s_self || s_self->_pnlSystem.isExpanded() == open) return true;
        s_self->toggleSystemPanel();
        return false;
    });
    // ...and the header's slots: slot=<kind> opens that slot's panel as
    // a tap does, slot=-1 closes any; glyphs=0|1|2 shows a glyph style without
    // saving it, glyphs=3 the saved one again; alarm=HHMM a pretend alarm.
    CardPopup::setHeaderHook([](const char *slot, int glyphs, int pageHdr, int alarm, int alarmIn) -> const char * {
        return s_self ? s_self->headerDebug(slot, glyphs, pageHdr, alarm, alarmIn) : "no GUI";
    });
#endif

    // --= Z-INDEX SANDWICH =--
    // 0. Dashboard (the cards; moved to the back at each build)
    // 1. Deck (bottom panels, which may expand OVER the cards)
    // 2. System panel (middle - slides out)
    // 3. Header (top - covers the system panel's top edge)
    // The touch overlay is on LVGL's top layer (Panel_Display::init).
    lv_obj_move_to_index(deck, 1);
    lv_obj_move_to_index(upper_deck, 2);
    lv_obj_move_to_index(_header.getContainer(), 3);

    // THE DASHBOARD IS THE BOOT SCREEN.
    //
    // Built LAST so it can read the real geometry of everything above it, and
    // moved to the back so the deck's panels expand over the cards rather than
    // pushing them.
    //
    // The pages first: which page, and that page's own settings, decide what
    // buildDashboard() builds. Boot always lands on the first page. UI Tweaks'
    // saved choices go on the working set first, so every page starts from
    // them.
    applyUiSettings();
    initPages();
    loadPageState();
    buildDashboard();
    SystemCore::heapMark("after dashboard");
}

// ---------------------------------------------------------------------------
// The dashboard
// ---------------------------------------------------------------------------

void GUIManager::buildDashboard() {
    lv_obj_t *screen = lv_screen_active();

    // THE PAGE'S HEADER, before anything measures the room it leaves (D-14).
    applyHeaderHidden(!pageHeaderShown());
    const int32_t headerH = UIToolkit::systemHeaderPx();

    // WHAT THE DECK ACTUALLY COSTS, MEASURED RATHER THAN ASSUMED.
    //
    // A collapsed panel is sc(85) tall, but the deck is as tall as the whole
    // screen and starts BELOW the header, so its bottom edge hangs 50 px off
    // the bottom of the panel and the panel's lower ~40 px are off-screen.
    // What is visible is the panel's sc(45) header alone. Rather than encode
    // that as a number, ask the objects where they are, which survives a
    // change to a panel's height.
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

        // AN OPEN PANEL IS NOT THE DECK'S HEIGHT. This measures where the
        // deck's children actually are - and an expanded panel reaches far up
        // the screen, so a page built while one was open was squeezed into
        // the space above it. So the measurement is only trusted, and
        // remembered, with every panel collapsed; with one open, the
        // remembered collapsed value is used.
        if (UIToolkit::getActiveAccordionPanel() && _deckReserveCollapsed > 0) {
            deckReserve = _deckReserveCollapsed;
        } else if (!UIToolkit::getActiveAccordionPanel()) {
            _deckReserveCollapsed = deckReserve;
        }
        // NO EXTRA GAP. The page's own INSET already holds the bottom row off
        // the edge of its host, and with the deck hidden that inset is the
        // margin; a gap on top of it would make the deck case looser than the
        // hidden case.
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
    // from bsp_display.WIDTH/HEIGHT: the panel's size is wrong on every rotated
    // board, and the cards do not get the whole screen (see UI::begin() in
    // begin()).
#ifdef DEBUG_PAGE_TIMING
    const int64_t tL0 = esp_timer_get_time();
#endif
    lv_obj_update_layout(screen);
#ifdef DEBUG_PAGE_TIMING
    const int64_t tL1 = esp_timer_get_time();
#endif
    UI::setViewport(lv_obj_get_content_width(_dashHost),
                    lv_obj_get_content_height(_dashHost));

    _page = new CardPage();
    _page->begin(_dashHost, &_binder);
    _page->setRowsOverride(_rowsOverride);
    _page->setPin(_pinCols, _pinRows);
#ifdef DEBUG_PAGE_TIMING
    const int64_t tL2 = esp_timer_get_time();
#endif

    // THE CURRENT PAGE, and its own settings laid over a copy of its spec (the
    // spec is const; PageSpec is a plain aggregate).
    PageSpec page = *_pages[_curPage].spec;
    page.headerDefault  = _hdr;
    page.variantDefault = _variant;
    page.showArea       = _showArea;
    // The cards added on the device among the compiled ones.
    page.count = CardAdded::merge(*_pages[_curPage].spec, s_pageCards, PAGE_CARDS_MAX);
    page.cards = s_pageCards;
    CardAdded::setPage(page.slug, _pages[_curPage].spec);
    s_regCountAtBuild = _core.entities().count();
    s_addedWaiting = 0;
    for (uint8_t i = 0; i < page.count; i++)
        if (CardAdded::isAdded(s_pageCards[i].id) && !_core.entities().find(s_pageCards[i].primaries[0]))
            s_addedWaiting++;
    _page->applySpec(page, _core.entities());
#ifdef DEBUG_PAGE_TIMING
    const int64_t tL3 = esp_timer_get_time();
#endif

    // The page indicator: this page's title, centred in the bar, and a dot
    // per page. Set on every build so a knob that rebuilds cannot lose it.
#ifdef DEBUG_PAGE_TIMING
    // Lay the new page out HERE, alone, so its cost is not charged to the
    // header's own measurement inside setPage() (lv_obj_update_layout lays out
    // the whole screen an object is on, not only the object).
    const int64_t tLx = esp_timer_get_time();
    lv_obj_update_layout(screen);
    const int64_t tL4 = esp_timer_get_time();
    Serial.printf("[PageTiming] layout of the new page %lld us\n", (long long)(tL4 - tLx));
#endif
    _header.setPage(page.title, _curPage, _nPages, page.headerSlots, page.headerSlotCount);
#ifdef DEBUG_PAGE_TIMING
    const int64_t tL5 = esp_timer_get_time();
#endif

    // THE DASHBOARD GOES TO THE BACK, by role rather than by index, so a
    // change to the screen's child list cannot put it above the deck.
    lv_obj_move_background(_dashHost);

    // The deck's tap-away scrim sits directly above the cards and below the
    // deck, set HERE because a build is already a full redraw - see the scrim's
    // creation for why it must never be re-ordered while a panel animates.
    if (_deckScrim) {
        lv_obj_move_background(_deckScrim);
        lv_obj_move_background(_dashHost);
    }
#ifdef DEBUG_PAGE_TIMING
    const int64_t tL6 = esp_timer_get_time();
#endif

    if (_deck) {
        // Up under UI Tweaks, it stays up whatever the page says (D-25).
        if (_showDeck || _deckRaised) lv_obj_clear_flag(_deck, LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_add_flag  (_deck, LV_OBJ_FLAG_HIDDEN);
    }
#ifdef DEBUG_PAGE_TIMING
    const int64_t tL7 = esp_timer_get_time();
    Serial.printf("[PageTiming] build: layout(screen) %lld | page begin %lld | applySpec %lld | hdrLabel %lld"
                  " | setPage %lld | moveBg %lld | deck %lld (us)\n",
                  (long long)(tL1 - tL0), (long long)(tL2 - tL1), (long long)(tL3 - tL2),
                  (long long)(tL4 - tL3), (long long)(tL5 - tL4), (long long)(tL6 - tL5),
                  (long long)(tL7 - tL6));
#endif
}

void GUIManager::destroyDashboard() {
    // The page first: it deletes its cards, and a card deregisters from the
    // binder in its destructor's path. Deleting the host out from under them
    // would take the widgets without the bookkeeping.
    if (_page)     { delete _page;              _page = nullptr; }
    if (_dashHost) { lv_obj_delete(_dashHost);  _dashHost = nullptr; }
}

void GUIManager::rebuildDashboard() {
#ifdef DEBUG_PAGE_TIMING
    // -D DEBUG_PAGE_TIMING: the rebuild's two halves (CardPage::commit()
    // splits the build further). Printed after both, so the print is not timed.
    const int64_t t0 = esp_timer_get_time();
    destroyDashboard();
    const int64_t t1 = esp_timer_get_time();
    buildDashboard();
    const int64_t t2 = esp_timer_get_time();
    Serial.printf("[PageTiming] destroy %lld us | build %lld us\n",
                  (long long)(t1 - t0), (long long)(t2 - t1));
#else
    destroyDashboard();
    buildDashboard();
#endif
    // A rebuild is the one thing that happens over and over on a running
    // board. If internal heap trends down across these, the leak is here.
    SystemCore::heapMark("after rebuild");
    _pnlSystem.refreshGrid();   // its Grid line: held, or what the arranger chose
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
// layout. From auto the first press moves one step from what is on screen.
static uint8_t cycleCount(uint8_t current, uint8_t showing, int8_t steps, uint8_t maxN) {
    int n = (current ? current : showing) + steps;
    if (n > (int)maxN) return 0;      // past the top -> auto
    if (n < 1)         return 0;      // past the bottom -> auto
    return (uint8_t)n;
}

// The screen's own background. Called at start-up and again on every scheme
// change, so the strip behind the deck follows the scheme.
void GUIManager::applyGround() {
    lv_obj_t *screen = lv_screen_active();
    if (screen) lv_obj_set_style_bg_color(screen, UI::c(UI::pal().GROUND), LV_PART_MAIN);
}

void GUIManager::openTokens() {
    // THE DASHBOARD STANDS DOWN FIRST, exactly as it does for the card bench.
    //
    // ReferencePage builds a whole second screen. With the dashboard's cards
    // still alive that is two full widget trees, and LVGL then fails to
    // allocate the layer buffers it needs to composite ("No memory: 482x17").
    destroyDashboard();
    ReferencePage::show();
}

// The bar shown or hidden, and everything that hangs from it - without a
// rebuild, so buildDashboard() can call it for the page it is building.
void GUIManager::applyHeaderHidden(bool hidden) {
    if (hidden == _headerHidden && UIToolkit::systemHeaderH == (hidden ? 0 : UI_HEADER_H)) return;
    _headerHidden = hidden;
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
    Serial.printf("[UI] system header %s\n", _headerHidden ? "hidden" : "shown");
}

void GUIManager::openLog() {
    // Dashboard down first, same as the bench and the token page - two full
    // widget trees is what exhausts LVGL's layer buffers.
    destroyDashboard();
    LogPage::show(_pnlSystem.logText());
}

// --- The knobs (#50) --------------------------------------------------------
// The card looks - label type, entity label, active state, density - are UI
// Tweaks' (GUIManagerTweaks.cpp), chosen outright and saved, for every page.
// The grid's counts are on the system panel, saved with the page.

// A PRESS PINS THE COUNT: from what the page shows, one more or
// less, kept by the arranger; past either end the pin goes (the arranger chooses).
void GUIManager::nudgeColumns(int8_t steps) {
    _pinCols = cycleCount(_pinCols, _page ? _page->cellCols() : (uint8_t)UI::grid().cols, steps, UI_MAX_COLS);
    saveGrid();
}

void GUIManager::nudgeRows(int8_t steps) {
    // The page is rebuilt from scratch on every change, so the pin is held HERE
    // and handed to each new page - a CardPage cannot remember it.
    _pinRows = cycleCount(_pinRows, _page ? _page->cellRows() : 1, steps, UI_MAX_ROWS);
    saveGrid();
}

void GUIManager::gridAuto() {
    _pinCols = 0;
    _pinRows = 0;
    saveGrid();
}

// ---------------------------------------------------------------------------
// Pages. docs/design/pages.md.
// ---------------------------------------------------------------------------

// THE SWIPE ORDER. House first, then Fleet. The order is a list and is
// independent of the page ids, which are frozen identities (Fleet is id 1 and
// stays id 1 while being page 2).
//
// NO OVERFLOW between pages: each page shows what fits and drops the rest by
// priority. A card never moves pages.
void GUIManager::initPages() {
    const PageSpec *const *order = DASH_PAGES;
    // Before any page is built: CardPage gives a card its id only if this
    // found it usable (PageSpec.h).
    checkCardIds(order, DASH_PAGE_N);
    CardAdded::begin(compiledCard);   // the cards added on the device
    CardGroups::setSources(order, DASH_PAGE_N, &_core.entities());   // groups for card labels (K46)
    _nPages = 0;
    for (uint8_t pi = 0; pi < DASH_PAGE_N; pi++) {
        const PageSpec *s = order[pi];
        if (_nPages >= GUI_MAX_PAGES) break;
        // Every page starts from the SAME defaults - the working set at this
        // moment holds them - so a swipe changes what is on the page and
        // nothing about how it looks.
        _pages[_nPages] = PageLive{
            .spec = s, .hdr = _hdr, .variant = _variant, .fill = _fill,
            .label = cardLabelMode(), .showArea = _showArea,
            .cols = _colsOverride, .rows = _rowsOverride,
            .scheme = UI::schemeIndex(),
            .pinCols = 0, .pinRows = 0,
        };
        // The page's own grid, if the owner set one (the system panel's Col /
        // Row, saved with the page): a count is a PIN, which the arranger
        // keeps; "auto" (older files) or none, not pinned.
        char v[8];
        PageLive &pl = _pages[_nPages];
        if (Settings::page(s->slug, "cols", v, sizeof(v)) && atoi(v) > 0) pl.cols = pl.pinCols = (uint8_t)atoi(v);
        if (Settings::page(s->slug, "rows", v, sizeof(v)) && atoi(v) > 0) pl.rows = pl.pinRows = (uint8_t)atoi(v);
        _nPages++;
    }
    _curPage = 0;
}

void GUIManager::savePageState() {
    if (_curPage >= _nPages) return;
    PageLive &p = _pages[_curPage];
    p.hdr      = _hdr;
    p.variant  = _variant;
    p.fill     = _fill;
    p.label    = cardLabelMode();
    p.showArea = _showArea;
    p.cols     = _colsOverride;
    p.rows     = _rowsOverride;
    p.scheme   = UI::schemeIndex();
    p.pinCols  = _pinCols;
    p.pinRows  = _pinRows;
}

void GUIManager::loadPageState() {
    if (_curPage >= _nPages) return;
    const PageLive &p = _pages[_curPage];
    _hdr          = p.hdr;
    _variant      = p.variant;
    _fill         = p.fill;
    _showArea     = p.showArea;
    _colsOverride = p.cols;
    _rowsOverride = p.rows;
    _pinCols      = p.pinCols;
    _pinRows      = p.pinRows;

    // The three that live outside this class, applied where they live.
    StateCard::setFill(_fill);
    cardSetLabelMode(p.label);
    UI::setColumnsOverride(_colsOverride);

    // The scheme is device-wide (D-29): every page's set holds the same one,
    // and setSchemeIndex() is a no-op when the scheme is already right.
    if (UI::schemeIndex() != p.scheme) {
        UI::setSchemeIndex(p.scheme);
        applyGround();
        _header.restyle();
        _pnlSystem.restyle();
    }
    refreshKnobLabels();
}

// The system panel shows the scheme in force.
void GUIManager::refreshKnobLabels() {
    _pnlSystem.setScheme(UI::schemeIndex());
}

void GUIManager::goToPage(int16_t index) {
    if (_nPages < 2) return;

    // WRAP-AROUND, both ways: with many pages, no-wrap means swiping all the
    // way back to reach page 1.
    const int16_t n = (int16_t)_nPages;
    const uint8_t next = (uint8_t)(((index % n) + n) % n);
    if (next == _curPage) return;

    closeSlotPanel(false);   // it described the page being left
    _headerPending = false;  // the next page's own header is applied as it is built
    savePageState();
    _curPage = next;
    loadPageState();
    rebuildDashboard();

    // The toast carries the position whether or not the header bar - and so
    // the dots - is visible. ASCII only: a
    // middle dot or an en dash renders as tofu on the panels (CLAUDE.md).
    char t[40];
    snprintf(t, sizeof(t), "%s  -  %u of %u",
             _pages[_curPage].spec->title ? _pages[_curPage].spec->title : "Page",
             (unsigned)_curPage + 1, (unsigned)_nPages);
    UIToolkit::show_toast(t, 1500);
    Serial.printf("[Pages] -> %s (%u of %u)\n", _pages[_curPage].spec->slug,
                  (unsigned)_curPage + 1, (unsigned)_nPages);
}

void GUIManager::togglePerf() {
    _showPerf = !_showPerf;
    // Paused as well as hidden when off: a hidden label still has a timer
    // refreshing it, and an instrument should not cost anything while nobody
    // is looking at it.
    if (_showPerf) { lv_sysmon_show_performance(nullptr); lv_sysmon_performance_resume(nullptr); }
    else           { lv_sysmon_performance_pause(nullptr); lv_sysmon_hide_performance(nullptr); }
    Serial.printf("[UI] perf overlay %s\n", _showPerf ? "shown" : "hidden");
}

lv_obj_t *GUIManager::firstCard() const {
    if (!_page || !_page->root()) return nullptr;
    const uint32_t n = lv_obj_get_child_count(_page->root());
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(_page->root(), i);
        if (!lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) return c;
    }
    return nullptr;
}

void GUIManager::toggleDeck() {
    // Put an open panel away first, so the deck never goes into hiding with a
    // panel half-expanded and the scrim left up behind it.
    UIToolkit::closeActiveAccordion();
    _showDeck = !_showDeck;
    rebuildDashboard();
    Serial.printf("[Cards] deck %s\n", _showDeck ? "shown" : "hidden");
}

void GUIManager::tick() {
    _header.tick();
    // A panel goes with its slot: a claim that has left the bar (an alarm
    // that has gone off) takes its panel with it.
    if (_slotPanel >= 0 && !_header.slotShown((uint8_t)_slotPanel)) closeSlotPanel(true);
    // EVERY HEADER PANEL FOLLOWS THE WINDOW TIMEOUT (D-22): untouched that
    // long, it goes, as a window does. Asked twice a second, not every pass.
    static uint32_t lastAsk = 0;
    const uint32_t now = millis();
    if ((_pnlSystem.isExpanded() || _slotPanel >= 0) && now - lastAsk >= 500) {
        lastAsk = now;
        const uint32_t wait = Kit::windowCloseMs();
        if (wait && now - _panelTouchMs >= wait) {
            if (_pnlSystem.isExpanded()) _pnlSystem.close();
            closeSlotPanel(true);
        }
    }
    _pnlDisplay.tick();
#ifdef HAS_AUDIO_HW
    _pnlAudio.tick();
#endif
    // WHILE THE PAGE IS ARRANGED: a toast held until the arranger
    // has its answer - after a short wait, so a quick one does not flash it.
    {
        constexpr uint32_t ARRANGE_TOAST_AFTER_MS = 250;
        static uint32_t since = 0;
        static bool shown = false;
        if (PageArranger::running()) {
            if (!since) since = now | 1;
            if (!shown && now - since >= ARRANGE_TOAST_AFTER_MS) {
                UIToolkit::show_held("Optimizing page view...");
                shown = true;
            }
        } else {
            since = 0;
            if (shown) { UIToolkit::end_held(); shown = false; }
        }
    }
    // A card added or removed shows once no card window is open (the window holds the
    // page's cards); one whose entity was learnt late, once the table has grown; the arranger's answer once kept.
    if (!CardPopup::isOpen()) {
        const bool learnt = s_addedWaiting && _core.entities().count() != s_regCountAtBuild;
        if (CardAdded::takeChanged() || learnt || CardPopup::takeRebuildWanted() || PageArranger::takeDone()) rebuildDashboard();
    }
    // What HA offers: a new catalogue, and its notices.
    HaOffer::tick();
    // Renders a waiting screenshot request, on this - the LVGL - thread.
    Screenshot::service();
    // Same for GET /bench, which may also change page first.
    Bench::service();
}
