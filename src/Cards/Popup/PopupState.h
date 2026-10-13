#pragma once
#ifndef POPUP_STATE_H
#define POPUP_STATE_H

// THE CARD WINDOW'S INSIDES, SHARED (2.12 P1 - docs/design/ui-foundation.md). Not public:
// only src/Cards/CardPopup.cpp and the files in src/Cards/Popup/ include it; everything else
// uses Cards/CardPopup.h, which is unchanged.
//
// Until 2.12 the window was one 6,325-line file whose parts shared one file-static state. The
// parts now have a file each and share that state through this header, in namespace popup:
//
//   CardPopup.cpp            the frame: open and close, the header row, the views, the clock
//   PopupCommon.cpp          sizes (mm, pm), bare objects and labels, what the entities say
//   PopupHold.cpp            the hold on a card, and the tap catcher
//   PopupControls.cpp        the controls view: the toggle, the sensor's disc, the column
//   PopupLight.cpp           a light's slider and its column
//   PopupDeck.cpp            the control deck and the hand-drawn faces
//   PopupScenes.cpp          the Scenes view
//   PopupMembers.cpp         Members, and a member's own controls
//   PopupProps.cpp           Properties
//   PopupHistory.cpp         History: what is asked, the top line, drawing, its taps
//   PopupHistoryChart.cpp    History: the chart
//   PopupHistoryActivity.cpp History: activity
//   PopupPanels.cpp          the folder panels: building, sliding, dragging, the row controls
//   PopupSettingsRows.cpp    SETTINGS' rows and what they do
//   PopupSideRows.cpp        CHART's, ACTIVITY's and SCENES' rows
//   PopupDialog.cpp          the Remove dialog
//   PopupSparks.cpp          the sparks along the edge
//   PopupDebug.cpp           GET /popup (DEBUG_POPUP)
//   PopupTree.cpp            GET /popup?tree= (DEBUG_POPUP)
//
// The code moved verbatim (P1 changed no logic); its comments still say "below" and "above"
// about what is now in another file - the declarations at the end of this header say where.

#include "Cards/CardPopup.h"
#include "Cards/Card.h"
#include "Cards/CardIcons.h"
#include "LightColor.h"
#include "UI/UITokens.h"
#include "UI/UIToolkit.h"
#include "Settings.h"   // 2.10d: SETTINGS' rows are kept by card id
#include "Cards/CardGroups.h"   // the Card label row (K44)
#include "Cards/CardAdded.h"    // K17: Add as a card, Remove card
#include "Cards/PageSpec.h"     // CARD_ID_MAX
#include "HaHistory.h"          // the History view's source (2.10e); no LVGL in it
#include "TimeFormat.h"         // every time and duration it says (2.10e round 4)
#include "src/core/lv_obj_event_private.h"   // lv_hit_test_info_t: the tap catcher's hit test
#include "src/core/lv_obj_draw_private.h"    // lv_obj_get_ext_draw_size: the window's shadow
#include <Arduino.h>
#include "esp_heap_caps.h"   // the hand-drawn faces live in PSRAM
#include "src/misc/cache/instance/lv_image_cache.h"   // lv_image_cache_drop(): not in lvgl.h
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

// Per the repo's debug-flag convention (CLAUDE.md). What opening and closing
// cost on this side of the frame: building the window and tearing it down,
// and what the window's frame redrew. The frames themselves are
// -D DEBUG_FRAMES (GUIManager.cpp). With this flag, a long press on the
// window's title also cycles the interference: open + idle, open only, off.
#ifdef DEBUG_POPUP
    #define DBG_POPUP(...) Serial.printf("[Popup:debug] " __VA_ARGS__)
    #include "src/display/lv_display_private.h"   // inv_areas: what a frame redraws
    #include "src/core/lv_refr_private.h"         // lv_refr_get_top_obj()
    #include "src/core/lv_obj_private.h"          // /popup?tree=: an object's flags and styles
    #include "src/core/lv_obj_style_private.h"
    #include "src/core/lv_obj_class_private.h"    // ... its class's name
    #include "src/misc/lv_event_private.h"        // ... the events it listens for
    #include "HttpServer.h"                       // GET /popup
    #include <atomic>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
#else
    #define DBG_POPUP(...) do {} while (0)
#endif

namespace popup {

// Compound names, never a bare ALL-CAPS word - Arduino's pin-mode macros eat
// those (CLAUDE.md, "Arduino's global macro namespace will eat your enum").
// VIEW_MEMBER: one member's own controls, reached from VIEW_MEMBERS (2.10b).
// VIEW_SCENES: the light's scenes beside its brightness slider (2.10c).
// History, Members and Scenes are reached by the corner icons, which work as
// tabs (owner, 2026-10-06): see "The corner icons".
// VIEW_PROPS: the entity's properties, read-only, from SETTINGS' button (2.10e).
enum class PopupView  : uint8_t { VIEW_MAIN, VIEW_HISTORY, VIEW_MEMBERS, VIEW_MEMBER, VIEW_SCENES, VIEW_PROPS };
enum class PopupPhase : uint8_t { PHASE_CLOSED, PHASE_OPEN };
// Where the held card is. OWNED: its window is open, and it stays pressed in,
// with the accent border, until the window closes.
enum class HoldPhase  : uint8_t { HOLD_NONE, HOLD_PRESSING, HOLD_LETTING_GO, HOLD_OWNED };
// How often a spark runs along the window's edge (see "Interference"): now
// and then while it is open; once per window; never.
enum class IntfMode   : uint8_t { INTF_NOW_AND_THEN, INTF_ONCE, INTF_OFF };

// D6: auto-close after 60 s untouched. The owner chose all four close routes.
constexpr uint32_t POPUP_AUTOCLOSE_MS = 60000;
constexpr uint32_t POPUP_TICK_MS      = 250;

// THE P4_5 IS THE REFERENCE (owner, 2026-10-05: its toggle "is the perfect
// size"). Its window is 787 x 545 px (68.1 x 47.1 mm) with the system header
// showing, and its hero 349 px of that height (30.2 mm, measured with calipers
// at 30). What is inside a window is sized as the same share of the biggest
// window of THIS shape that fits in it - see open(), propH and pm(). (The
// window itself has been up to 2:1 since 2026-10-06.)
constexpr float POPUP_ASPECT  = 787.0f / 545.0f;
constexpr float REF_WIN_H_MM  = 47.1f;   // the P4_5 window's height, header showing
constexpr float HERO_H_MM     = 30.2f;   // its hero's

// The hold, in millimetres so it looks the same on every board. THE LEAP IS
// GONE (owner, round 6): it was clipped by the card's wrapper, and the window
// waiting two frames for it made the popup feel slower than it was - "the
// simple pressing down further visual effect of a long-press is by itself a
// really catchy visual".
constexpr float    HOLD_SINK_MM    = 0.4f;   // how far the card is pressed in at the long press
constexpr uint32_t HOLD_LETGO_MS   = 120;    // a tap lets go this fast, from full
constexpr uint32_t KNOB_SLIDE_MS   = 160;

extern IntfMode s_intfMode;   // PopupSparks.cpp

// A window's entities: a card's primaries, or the members of a group defined
// at the source (2.10c), whichever list is longer.
constexpr uint8_t POPUP_ENT_MAX = (CARD_PRIMARY_MAX > ENTITY_MEMBERS_MAX) ? CARD_PRIMARY_MAX
                                                                          : ENTITY_MEMBERS_MAX;

// The hand-drawn faces (see "Faces drawn by hand", with the control deck).
enum class FaceKind : uint8_t { FACE_BLACK, FACE_SILVER, FACE_DENT };   // setFace(): PopupDeck.cpp
struct Popup {
    PopupPhase phase = PopupPhase::PHASE_CLOSED;
    PopupView  view  = PopupView::VIEW_MAIN;
    bool       closeQueued = false;

    // Copied from the card at open - see "the window holds the entities".
    const Entity   *ent[POPUP_ENT_MAX] = {nullptr};
    uint8_t         nEnt = 0;
    // A GROUP DEFINED AT THE SOURCE (2.10c, DECISIONS K17): the card's one
    // entity - light.office - whose members the registry learnt. With none of
    // them paused, ent[] is the group itself: HA's report is shown and a
    // command goes to the group, which keeps its bulbs in step. While any
    // member is paused, ent[] holds the members (`expanded`) and the window
    // works exactly as for All Lamps: the paused one is out (K14).
    const Entity   *native = nullptr;
    bool            expanded = false;
    EntityRegistry *reg  = nullptr;
    TempUnit        tempUnit = TempUnit::TEMP_INHERIT;
    char area[ENTITY_SHORT_MAX] = {0};
    char name[ENTITY_NAME_MAX]  = {0};

    lv_area_t winRect = {};
    int32_t   winRadius = 0;
    int32_t   pad = 0;
    int32_t   propH = 0;   // the height the contents are sized from - see open()

    lv_obj_t *win = nullptr;
    lv_obj_t *btnLeft = nullptr, *lblLeft = nullptr;
    lv_obj_t *btnHistory = nullptr, *btnMembers = nullptr;
    lv_obj_t *btnScenes = nullptr;   // under the chart, on the light's own views (2.10c)
    lv_obj_t *lblTitleArea = nullptr, *lblTitleName = nullptr;
    int32_t   titleW = 0;      // what the two title labels may use together, centred
    lv_obj_t *titleLeft = nullptr;   // the X's slot: as wide as the right one, so the title centres
    int32_t   titleSlotW = 0, titleSlack = 0;   // its width, and the room in it beside the X
    lv_obj_t *stage = nullptr, *autoBar = nullptr;

    // The main view's widgets; null while another view is showing.
    lv_obj_t *hero = nullptr, *knob = nullptr, *heroIcon = nullptr;
    lv_obj_t *lblWhat = nullptr, *lblValue = nullptr, *lblUnit = nullptr, *lblAgo = nullptr;
    int32_t   heroW = 0, heroH = 0, heroR = 0, knobH = 0, knobInset = 0;
    bool      toggleHero = false;
    bool      knobPlaced = false;   // the first placement jumps; every later one slides
    int32_t   knobTarget = 0;
    // The knob under a finger (knobDragCb): where the drag began, where the
    // knob was then, and whether the CLICKED that ends a drag must be ignored.
    bool      knobDragging = false, swallowClick = false;
    int32_t   dragStartY = 0, dragKnobY0 = 0;

    // A light's controls (2.10b): see "The light's controls".
    GroupOn   groupOn = GroupOn::GROUP_ON_ANY;   // copied from the card at open
    SceneShow sceneShow = SceneShow::SCENES_VISIBLE;   // likewise (2.10c)
    lv_obj_t *ddGroup = nullptr;      // SETTINGS' live dropdown
    lv_obj_t *ddTapScene = nullptr;   // SCENES' Default scene
    bool      builtPaused = false, rebuildQueued = false;   // "A paused window"
    uint8_t   builtCaps = 0;                     // what the selector was built for
    lv_obj_t *pill = nullptr;                    // PAUSED
    // A member's own controls, reached from Members (2.10b): the group's
    // entities are kept here while ent[] holds the one member.
    const Entity *groupEnt[POPUP_ENT_MAX] = {nullptr};
    uint8_t   groupN = 0;
    bool      lightHero = false;
    uint8_t   lightCtl = 0;                      // LightCtl: which control the hero is
    lv_obj_t *fill = nullptr, *grip = nullptr, *mark = nullptr;
    lv_obj_t *btnCtl[4] = {nullptr, nullptr, nullptr, nullptr};   // power, dim, temp, colour
    lv_obj_t *swatch[8] = {nullptr};
    // Scenes (2.10c): their buttons; the one loaded from this window (-1:
    // none), when, and the light's levels once that scene had settled - see
    // sceneRingCheck(); and the control the slider showed before Scenes.
    lv_obj_t *sceneBtn[ENTITY_SCENES_MAX] = {nullptr};
    int8_t    lastScene = -1;
    uint32_t  sceneMs = 0;
    uint32_t  sceneSettled = 0;   // a signature of the levels; 0 = not taken yet
    uint8_t   ctlBeforeScenes = 0;
    int32_t   mainRowW = 0;       // the main view's slider and words together (Members' width)
    bool      stacked = false;    // the control deck under the hero (a tall window)
    bool      sliding = false;                   // a finger is on the slider
    int32_t   slideVal = 0, sentVal = -1;        // what it shows; what was last sent
    uint32_t  sentMs = 0;

    // The settings deck (pathway 1, card-sheet 11.1): see "The settings deck".
    lv_obj_t *deck = nullptr, *deckTab = nullptr, *deckTabLbl = nullptr, *deckPane = nullptr;
    lv_obj_t *deckBody = nullptr;                  // the pane's rows (buildFolder())
    lv_obj_t *deckTrack = nullptr;                 // what the panel is dragged in (buildFolder())
    lv_obj_t *deckUp = nullptr, *deckDown = nullptr;   // its "more above / below" carets
    lv_obj_t *swPause = nullptr;                   // the Paused checkbox
    int32_t   deckH = 0, deckHead = 0;
    int32_t   deckHide = 0;   // how much of the pane stays below the screen when open
    uint8_t   deckState = 0;                       // DeckState
    bool      deckFilled = false;                  // its rows exist (built at first open)

    lv_timer_t *timer = nullptr;
    uint32_t    lastTouchMs = 0;
    uint32_t    lastSig = 0;
    uint32_t    lastAgeMs = 0;
    lv_point_t  pressStart = {0, 0};
};
extern Popup s;   // CardPopup.cpp, as are h, s_indev, s_catcher, s_openFromTap
enum DeckState : uint8_t { DECK_HIDDEN, DECK_PEEK, DECK_OPEN };

// The card under the finger. `surface` is a card's own LVGL object, which a
// page rebuild can delete at any moment: a delete hook clears it the moment
// that happens (surfaceDeletedCb), and an animation whose var is the surface
// is deleted with it, by LVGL.
struct Hold {
    HoldPhase phase = HoldPhase::HOLD_NONE;
    lv_obj_t *surface = nullptr;
    int32_t   v = 0;          // 0..255: how far toward the accent
};
extern Hold h;
extern lv_indev_t *s_indev;

// THE TAP CATCHER: "tap outside" and the modal rule, with nothing drawn. A
// transparent full-screen object on lv_layer_top(), made once at begin() and
// never deleted, hidden or moved - each of those would invalidate the whole
// screen (LESSONS, "Never toggle HIDDEN on a screen-sized object"). It is
// switched by CLICKABLE alone, which costs no redraw. Its hit test says "not
// here" inside the window, so LVGL goes on to search the screen, where the
// window is (pointer_search_obj() in lv_indev.c: system layer, top layer,
// then the screen).
extern lv_obj_t *s_catcher;
extern bool s_openFromTap;   // open() called by openOn(): the finger is already up

struct Agg {
    bool on = false, anyBool = false, available = true, paused = false, everSet = false;
    uint8_t nPaused = 0;   // members left out because they are paused
    uint32_t lastChangeMs = 0;
};

struct LightAgg {
    uint8_t   caps = 0;                       // LightCapBits, the union of what is offered
    uint8_t   allCaps = 0;                    // ... of every member, paused or not
    int32_t   bri = -1, kelvin = -1, hue = -1, sat = -1;
    LightMode mode = LightMode::LMODE_UNKNOWN;
    int32_t   minK = 0, maxK = 0;
};

enum LightCtl : uint8_t { LCTL_DIM, LCTL_TEMP, LCTL_COLOUR };

struct DeckColours { uint32_t ribbon, icon, chosen; };

struct Deck {
    lv_obj_t *obj = nullptr, *sel = nullptr, *selIcon = nullptr;
    uint8_t   n = 0;                    // mode switches
    int32_t   x[3] = {0, 0, 0};         // their centres
    uint8_t   ctl[3] = {0, 0, 0};       // the LightCtl each is
    const char *glyph[3] = {nullptr, nullptr, nullptr};
    int32_t   selW = 0;
    uint8_t   shown = 0;                // the switch whose icon the selector carries
    bool      dragging = false;
    int32_t   pressX = 0, selX0 = 0;
};

struct SceneStack { int32_t ch, bw, gap, pad, rowsFit, cols, w, h; bool scroll; };

constexpr uint8_t  HIST_SPANS = 4;
constexpr uint32_t HIST_SPAN_SEC[HIST_SPANS] = { 86400, 43200, 21600, 3600 };
const char *const  HIST_SPAN_CHIP[HIST_SPANS] = { "24 h", "12 h", "6 h", "1 h" };
constexpr uint8_t  HIST_PERIODS = 2;
constexpr uint32_t HIST_PERIOD_SEC[HIST_PERIODS] = { 86400, 7 * 86400 };
const char *const  HIST_PERIOD_CHIP[HIST_PERIODS] = { "24 h", "7 d" };
constexpr uint8_t  HIST_ROWS_FIRST = 10;   // owner, 2026-10-02: 10 rows, then "Show more"
constexpr uint8_t  HIST_ROWS_MAX   = 60;   // what "Show more" draws at most (LVGL's pool)
// The chart's Legend (owner, round 2), in the panel's order.
enum : uint8_t { LEGEND_NONE = 0, LEGEND_X, LEGEND_Y, LEGEND_BOTH };   // None: round 4

// KEPT PER CARD (owner, round 2 - H3): every History choice is saved under the
// card's id, by name, like Tap action; a member's own History is not a card's,
// so it keeps nothing.
const char *const HIST_SPAN_NAME[HIST_SPANS]     = { "24h", "12h", "6h", "1h" };
const char *const HIST_PERIOD_NAME[HIST_PERIODS] = { "24h", "7d" };
const char *const HIST_LEGEND_NAME[4]            = { "none", "x", "y", "both" };

struct HistView {
    const Entity *e = nullptr;   // whose history: see histSubject()
    bool     chart = false;      // a number: the chart; anything else: activity
    uint8_t  span = 0;           // HIST_SPAN_SEC index (the chart)
    uint8_t  period = 0;         // HIST_PERIOD_SEC index (activity)
    bool     minMax = true;      // the chart's min and max
    bool     shade = true;       // the fill under the line (Shading: under line)
    uint8_t  legend = LEGEND_BOTH;
    bool     chips = true;       // the span buttons on the chart
    bool     actChips = true;    // and on activity
    bool     causes = true;      // activity's "by ..." lines (Show triggers)
    bool     duration = false;   // activity's "for: 2 hrs 38 min" lines (Show duration)
    bool     more = false;       // "Show more" was tapped
    uint32_t gen = 0;            // HaHistory's request this view made
    uint32_t drawnSeq = 0;       // HaHistory::doneSeq() when last drawn
    int16_t  openRow = -1;       // the activity row whose details show
    lv_obj_t *body = nullptr;    // the view's contents, redrawn whole
    // The chart as drawn, for a finger reading it (histScrub()).
    lv_obj_t *area = nullptr, *scrubLine = nullptr, *scrubLbl = nullptr;
    int32_t  *ys = nullptr;      // the chart's own values, tenths
    uint16_t  m = 0;             // how many
    uint32_t  t0 = 0, spanSec = 1;   // the drawn span's start and length
    int32_t   plotTop = 0, plotBot = 0;
    int32_t   plotL = 0, plotW = 1;   // the plot's left (past the value labels) and width
};

enum : uint8_t { SIDE_NONE, SIDE_CHART, SIDE_SCENES, SIDE_ACTIVITY };
struct ChartPanel { lv_obj_t *root = nullptr, *tab = nullptr, *pane = nullptr, *body = nullptr;
                    lv_obj_t *track = nullptr, *up = nullptr, *down = nullptr;
                    uint8_t state = DECK_HIDDEN; int32_t h = 0; uint8_t kind = SIDE_NONE; };

// ROW_BUTTON (2.10e): a button that does something - its words in `opts`.
enum class DeckRowKind : uint8_t { ROW_CHECK, ROW_DROP, ROW_BUTTON };

struct DeckRowSpec {
    const char   *label;
    DeckRowKind   kind;
    const char   *opts;     // a dropdown's choices, one per line
    uint16_t      sel;      // its chosen one, or the checkbox's state
    bool          live;     // false: shown greyed, does nothing yet
    lv_event_cb_t cb;
    lv_obj_t    **out;      // where the control is kept, or null
};
constexpr uint8_t DECK_ROWS_MAX = 12;   // 7 at most today; room for /popup?rows=

// ---------------------------------------------------------------------------
// The shared state - each defined once, in the file named
// ---------------------------------------------------------------------------
extern Deck d;                         // PopupDeck.cpp: the control deck being shown
extern uint32_t s_faceEpoch;           // PopupDeck.cpp
extern uint32_t s_faceFlat;            // PopupDeck.cpp
extern HaHistory *s_hist;              // PopupHistory.cpp
extern HistView s_hv;                  // PopupHistory.cpp
extern lv_obj_t *s_histList;           // PopupHistory.cpp
extern uint8_t s_memberPick;           // PopupMembers.cpp
extern ChartPanel s_chart;             // PopupPanels.cpp: the left panel
extern CardGroups::Choice s_clMap[24]; // PopupSettingsRows.cpp: the Card label row
extern uint8_t s_clN;
extern char s_clOpts[24 * 44];
extern lv_obj_t *s_ddSpan;             // PopupSideRows.cpp
extern lv_obj_t *s_cbHidden;           // PopupSideRows.cpp
extern lv_obj_t *s_confirm;            // PopupDialog.cpp
#ifdef DEBUG_POPUP
extern uint8_t s_dbgRows;              // PopupSettingsRows.cpp
extern uint32_t s_dbgPressMs;          // PopupDebug.cpp
extern char *s_tree;                   // PopupTree.cpp
extern size_t s_treeLen;
#endif


// ---------------------------------------------------------------------------
// What the files call in each other, by the file that has it
// ---------------------------------------------------------------------------

// CardPopup.cpp
void showView(PopupView v);
PopupView controlsView();

// PopupCommon.cpp
int32_t mm(float v);
int32_t pm(float v);
lv_obj_t *plain(lv_obj_t *parent);
lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *f, uint32_t hex);
void setText(lv_obj_t *l, const char *t);
int32_t textW(const char *txt, const lv_font_t *f);
lv_obj_t *iconButton(lv_obj_t *parent, const char *glyph, const lv_font_t *f,
                     lv_event_cb_t cb, lv_obj_t **outLabel = nullptr);
uint8_t memberCount();
const Entity *memberAt(uint8_t i);
bool hasMembers();
bool nativeWantsMembers();
void applyNative();
Agg aggregate();
LightAgg lightAggregate();
const char *whatWord(const Entity &e, char *buf, size_t cap);
uint32_t quiet(uint32_t hex);
bool inMember();
int32_t lumOf(uint32_t hex);

// PopupHold.cpp
Card *cardOf(lv_obj_t *sf);
void holdLook(lv_obj_t *sf, int32_t v);
bool movedTooFar();
void holdExec(void *var, int32_t v);
void letGoExec(void *var, int32_t v);
void holdAttach(lv_obj_t *sf);
void holdDetach();
void letGo();
void pressCb(lv_event_t *ev);
void releaseCb(lv_event_t *ev);
void catcherCb(lv_event_t *ev);

// PopupControls.cpp
void renderAgo(const Agg &a);
void makeLabelRow(lv_obj_t *col);
int32_t deckBreak();
int32_t selectorW(uint8_t caps);
int32_t controlDeckH();
void checkPausedChange(const Agg &a);
void renderMain();
void commandAll(bool on);
bool pausedBlocks();
void buildMain();

// PopupLight.cpp
bool canSlide(uint8_t caps);
uint8_t firstCtl(uint8_t caps);
void lightSend(int32_t v, int8_t sat = -1);
void renderLight();
void rebuildMainAsync(void *unused);
int32_t stripInset();
void buildLightHero(lv_obj_t *row);
void buildLightColumn(lv_obj_t *col, lv_obj_t *deckParent);

// PopupScenes.cpp
const Entity *sceneHost();
uint8_t shownScenes();
const Entity *shownScene(uint8_t i);
bool hasScenes();
void sceneLoaded(uint8_t i);
bool sceneRingCheck(uint32_t now);
SceneStack sceneStack(int32_t maxW);
int32_t sceneGridW(int32_t maxW);
void buildSceneGrid(lv_obj_t *col, int32_t colW);

// PopupDeck.cpp
int8_t &chosenLook();
void keepLook(int8_t look);
DeckColours deckColours();
FaceKind metalKind();
lv_obj_t *setFace(lv_obj_t *o, int32_t w, int32_t h, int32_t r, FaceKind kind, uint32_t flat);
lv_obj_t *labelIn(lv_obj_t *o);
void forgetDeck();
void buildControlDeck(lv_obj_t *parent, uint8_t caps);

// PopupMembers.cpp
void buildMembers();
void enterMemberAsync(void *unused);
void memberRowCb(lv_event_t *ev);
void leaveMember(PopupView to = PopupView::VIEW_MEMBERS);

// PopupProps.cpp
void buildProps();

// PopupHistory.cpp
void histKeep(const char *key, const char *value);
bool histFromHa(const Entity *e);
bool histSync();
void histNote(lv_obj_t *parent, const char *text);
void buildHistory();
void histAsk();
int32_t histLift();
lv_obj_t *histTopRow(lv_obj_t *body);
void histCorner(lv_obj_t *o, lv_align_t where);
void histTopMiddle(lv_obj_t *row, int32_t room = 0);
void histDraw();
void histPoll();
void spanChipCb(lv_event_t *ev);
void histShowingCb(lv_event_t *ev);
void histMoreCb(lv_event_t *ev);
void histRowCb(lv_event_t *ev);

// PopupHistoryChart.cpp
void histDrawChart(lv_obj_t *body);
void histFillCb(lv_event_t *ev);
int32_t histX(uint32_t t);
float histNice(float raw);
lv_obj_t *histVLine(lv_obj_t *parent, lv_point_precise_t *pts, int32_t x, uint32_t col, int32_t width);
void histScrub(int32_t x);
void histChartCb(lv_event_t *ev);

// PopupHistoryActivity.cpp
void histDrawActivity(lv_obj_t *body);

// PopupSparks.cpp
void intfCreate();
void intfStart();
void intfEnd();

// PopupPanels.cpp
void deckSet(uint8_t state);
bool panelOpen();
bool inDeck(const lv_point_t &pt);
void deckCreate();
void deckFill();
void deckSync();
void deckOpen();
void deckEnd();
void chartSet(uint8_t state);
void chartFold();
void panelTrackCb(lv_event_t *ev);
void chartEnd();
void sideFor(PopupView v);

// PopupSettingsRows.cpp
TapAction sceneActionOf(const Card &c);
uint8_t deckSpecs(DeckRowSpec *r);
void deckRender();
void keepCardSetting(const char *key, const char *value);
void groupDropCb(lv_event_t *ev);
void ensureDefaultScene(Card &c);
void tapDropCb(lv_event_t *ev);
void cardLabelDropCb(lv_event_t *ev);
void labelDropCb(lv_event_t *ev);
void unitsDropCb(lv_event_t *ev);
void propsCb(lv_event_t *ev);
void addCardCb(lv_event_t *ev);
void pauseCheckCb(lv_event_t *ev);

// PopupSideRows.cpp
void sceneActionDropCb(lv_event_t *ev);
void tapSceneDropCb(lv_event_t *ev);
void histSyncPanel();
uint8_t chartSpecs(DeckRowSpec *r);
uint8_t activitySpecs(DeckRowSpec *r);
uint8_t scenesSpecs(DeckRowSpec *r);
void hiddenScenesCb(lv_event_t *ev);

// PopupDialog.cpp
void confirmClose();
void confirmRemove();
void removeCardCb(lv_event_t *ev);

// PopupDebug.cpp
#ifdef DEBUG_POPUP
void dbgRenderStart(lv_event_t *e);
#endif
#ifdef DEBUG_POPUP
void dbgRenderReady(lv_event_t *e);
#endif
#ifdef DEBUG_POPUP
void dbgLater(const char *fmt, ...);
#endif
#ifdef DEBUG_POPUP
void dbgMem(char *buf, size_t cap);
#endif
#ifdef DEBUG_POPUP
void dbgProbe();
#endif
#ifdef DEBUG_POPUP
void dbgOut(const char *fmt, ...);
#endif

// PopupTree.cpp
#ifdef DEBUG_POPUP
void treeDump(bool page);
#endif
} // namespace popup

#endif // POPUP_STATE_H
