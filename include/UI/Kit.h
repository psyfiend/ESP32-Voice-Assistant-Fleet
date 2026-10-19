#pragma once
// ---------------------------------------------------------------------------
// THE COMPONENT KIT (2.12 P3) - docs/design/components.md
//
// The pieces a window is built from: buttons, chips, the selector, checkboxes,
// dropdowns, rows, folder panels, dialogs and label roles. Each takes a ROLE,
// and a role's look is a few SHARED lv_style_t objects (one per part and
// state it styles), filled from the tokens and refilled whenever the scheme
// changes - the way UI::paint() keeps the chrome in step. A component costs
// its objects, not a copy of its style in every one of them.
//
// What a component does NOT own: where it goes (the caller sizes and places
// it), and anything that changes while it shows (a value, a greyed state that
// comes and goes) - those stay local properties, set by the caller, which
// beat a shared style in LVGL.
// ---------------------------------------------------------------------------
#include <lvgl.h>
#include <stdint.h>
#include "UI/UITokens.h"
#include "UI/HeaderSlots.h"   // GlyphStyle
#include "UI/GlyphShapes.h"   // Kit::strokeGlyph()

namespace Kit {

// --- The shared styles -------------------------------------------------------
// One lv_style_t each; the selector is given where it is added (Kit::add()).
// Grouped by the component that uses them; components.md says what each sets.
enum class KitStyle : uint8_t {
    // Label roles: a face and a colour (ACTIVE: a state that is on, ST_ACTIVE).
    KS_LABEL_NAME, KS_LABEL_NAME_DIM, KS_LABEL_TAG, KS_LABEL_TAG_DIM, KS_LABEL_TAG_ACCENT,
    KS_LABEL_TAG_ACTIVE,
    // Buttons: a face (MAIN), its pressed tint (MAIN | PRESSED), its ink (the label).
    KS_BTN_CANCEL, KS_BTN_CANCEL_PRESSED, KS_BTN_CANCEL_INK,
    KS_BTN_DESTRUCTIVE, KS_BTN_DESTRUCTIVE_PRESSED, KS_BTN_DESTRUCTIVE_INK,
    // The dialog: the dim behind it, a destructive question's box (its edge in
    // ST_BAD), its words, and the row its buttons stand in.
    KS_DIALOG_DIM, KS_DIALOG_BOX_DESTRUCTIVE, KS_DIALOG_BODY, KS_DIALOG_ROW,
    // Rows: the row; a dropdown (MAIN, live or greyed, its arrow) and its list
    // (MAIN, the chosen line, a pressed line); a checkbox (MAIN, its box, the
    // box ticked); a row's button (face, pressed, live or greyed edge).
    KS_ROW,
    KS_DROP, KS_DROP_LIVE, KS_DROP_GREYED, KS_DROP_ARROW,
    KS_LIST, KS_LIST_CHOSEN, KS_LIST_PRESSED,
    KS_CHECK, KS_CHECK_BOX, KS_CHECK_BOX_TICKED,
    KS_ROWBTN, KS_ROWBTN_PRESSED, KS_ROWBTN_LIVE, KS_ROWBTN_GREYED,
    // Chips: every chip, unlit, lit, pressed.
    KS_CHIP, KS_CHIP_UNLIT, KS_CHIP_LIT, KS_CHIP_PRESSED,
    // A list item (Kit::item()): a tappable surface in a list - a member, an
    // activity row, "Show more" - and its pressed tint.
    KS_ITEM, KS_ITEM_PRESSED,
    // A round button with words (a scene) and its pressed tint; a colour
    // swatch (its colour is its own, set by the caller); the PAUSED badge.
    KS_PILL, KS_PILL_PRESSED, KS_SWATCH, KS_BADGE_PAUSED,
    // Folder panels: the pane's and tab's face, the pieces that paint the join
    // out, the tab's title, the body the rows stand in, the carets.
    KS_PANEL_FACE, KS_PANEL_FILL, KS_PANEL_TITLE, KS_PANEL_BODY, KS_PANEL_CARET,
    // The sheet (UI/Sheet.h): the window's face, its auto-close bar.
    KS_SHEET_FACE, KS_SHEET_BAR,
    // A finished surface sinks 1 px while pressed (Kit::finish()).
    KS_SINK,
    // The page header (2.8, UI/HeaderSlots.h): the bar and its accent line; a
    // slot (the tappable share of the bar), pressed, and open - its panel
    // showing, in the drawer's face, so the slot opens into its panel like a
    // folder's tab; its words (the bar's face; a smaller, dim one) and a
    // sensor's icon; a glyph's frame (the rounded square, the circle); the
    // page's dots; the error mark beside a slot's dashes.
    KS_HDR_BAR, KS_HDR_SLOT, KS_HDR_SLOT_PRESSED, KS_HDR_SLOT_OPEN,
    KS_HDR_TEXT, KS_HDR_TEXT_DIM, KS_HDR_ICON,
    KS_HDR_CLOCK, KS_HDR_DOTS,   // the rows the time and am / pm, and the dots, stand in
    KS_GLYPH_SQUARE, KS_GLYPH_CIRCLE,
    KS_HDR_DOT_ON, KS_HDR_DOT_OFF, KS_HDR_MARK,
    // A slot's drawer (Kit::drawer()): the clipping wrapper that grows to
    // reveal it and casts the scheme's shadow, and its face; its lines - an
    // item to tap (a page to go to), a big value.
    KS_DRAWER, KS_DRAWER_FACE, KS_DRAWER_ITEM, KS_DRAWER_BIG,
    // ONE SHAPE (2.8 round 3, D-23): where an open slot meets the bar's accent
    // line, a rounded corner of the outline - a cove beside the tab: the
    // shape's fill, and a ring of the accent around the bar outside it.
    KS_JOIN_FILL, KS_JOIN_RING_BAR,
    // A short upright rule setting one choice apart from another (Kit::vrule()).
    KS_RULE,
    // CARDS (2.11 round 1): the parts every card has - the wrapper in its cell
    // (a flex column), a plain holder, the surface and its padded body, a tag's
    // strip and pill, the badge of a card with no header, the hero's disc, the
    // name under it, the small words (a tag, a count, the status row) and those
    // in the surface's colour (on a tag or a band), the unit, the status row's
    // icon, the long-stale diagonal. What follows the card's state - its
    // colours, a press, a size worked out from its cell - stays the card's own.
    KS_CARD_ROOT, KS_CARD_PLAIN, KS_CARD_SURFACE, KS_CARD_BODY, KS_CARD_STRIP, KS_CARD_PILL,
    KS_CARD_BADGE, KS_CARD_DISC, KS_CARD_NAME, KS_CARD_SMALL, KS_CARD_TAGTEXT, KS_CARD_UNIT,
    KS_CARD_ICON_SM, KS_CARD_DIAG,
    // A CLUSTER (2.11, K89, K101): a group card shown as a block of its members.
    // Its frame (a card's surface), its header row (one line, a rule under it),
    // the header's name, the group's swatch beside it, a value's words; a MINI
    // TILE - a member with no border of its own, tinted while pressed - and its
    // entity label.
    KS_CLUSTER_HDR, KS_CLUSTER_TITLE, KS_CLUSTER_SWATCH, KS_CLUSTER_VALUE,
    KS_TILE, KS_TILE_PRESSED, KS_TILE_NAME,
    KS_COUNT
};

// Fill the styles now (GUIManager::begin(), once the scheme is set), so the
// first window built does not pay for them. Otherwise the first style() does.
void begin();

// The style itself - all of them filled the first time any is asked for.
lv_style_t *style(KitStyle s);

// lv_obj_add_style() with a kit style.
void add(lv_obj_t *o, KitStyle s, lv_style_selector_t sel = 0);

// --- Basics -----------------------------------------------------------------
// A bare object: no theme styles, no scrolling, not clickable until asked.
lv_obj_t *plain(lv_obj_t *parent);

// A label in a font and colour, with no text yet. For colours worked out at
// the call (a mix, a contrast); a label of a fixed role uses that role's style.
lv_obj_t *label(lv_obj_t *parent, const lv_font_t *f, uint32_t hex);

// A label whose look is a kit style (a label role, a button's ink), with no text yet.
lv_obj_t *label(lv_obj_t *parent, KitStyle look);

// How wide a line of text draws, in pixels.
int32_t textW(const char *txt, const lv_font_t *f);

// --- The halo (KitHalo.cpp, D-35) ---------------------------------------------------
// What floats over the page - a header slot's panel, every window - stands on a
// black rim at its edge fading to clear, haloWidth() out (UIShape HALO): painted
// and dithered images (LVGL's own shadow cost 45 ms a full frame on the P4_5 and
// banded on these 16-bit panels). haloAttach() draws it under the object, round
// its rounded edge, and widens its redraw area; haloDraw() draws one round `a`,
// within `clip` (null: the layer's) - the bar's share of a panel's (drawerOverBar).
int32_t haloWidth();
void    haloAttach(lv_obj_t *o);
void    haloDraw(lv_layer_t *layer, const lv_area_t &a, int32_t radius, const lv_area_t *clip);

// --- Buttons ------------------------------------------------------------------
// A button with words: CANCEL is the quiet way out, DESTRUCTIVE the one that
// removes something (the scheme's ST_BAD).
enum class ButtonRole : uint8_t { BTN_CANCEL, BTN_DESTRUCTIVE };

// `w` x `h` px, the words centred; `cb` on LV_EVENT_CLICKED, with no user data.
lv_obj_t *button(lv_obj_t *parent, ButtonRole role, const char *text, int32_t w, int32_t h,
                 lv_event_cb_t cb);

// --- Faces drawn by hand (KitFaces.cpp) -------------------------------------------
// A raised metal face (black or silver: the selector, a lit icon button), a
// soft dent in the surface (an unlit icon button), or satin (below), painted
// once into PSRAM and kept; the object's own background is turned off while it
// shows. Painted, and dithered to the panels' 16 bits, because LVGL's own
// gradients band pink and green there (LESSONS).
enum class FaceKind : uint8_t { FACE_BLACK, FACE_SILVER, FACE_DENT, FACE_SATIN, FACE_LIT };

// The face as the object's first child (an image); `flat` is its colour when
// no face can be had. `tint`: a satin face's own colour. Returns the image, or null.
// `band`: on a metal face, the height of the rail seen through it (the selector).
lv_obj_t *setFace(lv_obj_t *o, int32_t w, int32_t h, int32_t r, FaceKind kind, uint32_t flat,
                  uint32_t tint = 0, int32_t band = 0);
lv_obj_t *labelIn(lv_obj_t *o);   // the label inside a faced button
void      facesNextWindow();      // a new window: faces the last one used may be taken back
uint32_t  facesFlat();            // how many came out flat (a debug readout)

// THE FINISHES (#99, DECISIONS K77): what a surface says about a thing you can
// tap. SATIN - it can be tapped and is not chosen: a faint light at the top
// fading by the middle, a faint shade below, a soft edge (Members' rows, a
// scene not loaded, the dialog's buttons, Power while off). GLASS - it is
// chosen or active: the selector's metal (a scene loaded, Power while on).
// FLAT - information - is no finish at all. Pressed, satin darkens and glass
// lightens, and the object sinks 1 px. `w` x `h` and the corner radius `r` are
// the object's own; `tint` is satin's colour, 0 for the scheme's (a role keeps
// its colour: the Remove button's red). Called again, it changes the finish.
// LIT (2.8 round 5) - a toggle that is ON: glass with a warm light behind it,
// "a light shining from within refracting through a semi-frosted glass surface"
// (Power while on; a toggle is glass while off).
enum class Finish : uint8_t { FINISH_SATIN, FINISH_GLASS, FINISH_LIT };
void     finish(lv_obj_t *o, int32_t w, int32_t h, int32_t r, Finish f, uint32_t tint = 0);
void     unfinish(lv_obj_t *o);   // the face put away, the object's own background back (a chip lit)
uint32_t satinBase();             // the scheme's satin, between its two surfaces
uint32_t litInk();                // words and icons on lit glass: the scheme's darkest, warmed
// A colour that reads on glass: pushed toward the scheme's far end until it
// stands apart from the metal (Linen's olive "on" vanished into its silver, T6).
uint32_t glassInk(uint32_t hex);

// THE SELECTOR'S LOOK, per scheme (K36, K45): -1 the scheme's own, else bit 0
// round, bit 1 silver. Kept in the settings file under the scheme.
int8_t   selectorLook();
void     keepSelectorLook(int8_t look);
void     forgetSelectorLooks();   // read every scheme's again (the file was changed under them)
void     selectorLookOverride(int8_t look);   // one window's own look (-1: none), until cleared
// Saved by name (schemes.<name>.selector, a card's "selector"): 0-3 as above.
const char *selectorLookName(int8_t look);
bool        selectorLookFromName(const char *name, int8_t &look);
uint8_t  selectorLookNow();       // the look in force
FaceKind metalKind();             // its metal
int32_t  selectorRadius();        // its corners: round, or R_CONTROL

struct SelectorColours { uint32_t ribbon, icon, chosen; };
SelectorColours selectorColours();

// --- Icon buttons ----------------------------------------------------------------
// A round button at the minimum touch size with an icon: satin (#99, the owner;
// a soft dent until then), darkening while pressed. `cb` on LV_EVENT_CLICKED.
lv_obj_t *iconButton(lv_obj_t *parent, const char *glyph, const lv_font_t *f, lv_event_cb_t cb,
                     lv_obj_t **outLabel = nullptr);
// Lit (a tab that is showing): glass, the icon a size up in the accent.
void iconButtonLit(lv_obj_t *b, bool lit);

// --- The selector (KitSelector.cpp) ------------------------------------------------
// Choices on a ribbon and a metal selector that slides between them - dragged
// or glided to a tapped choice, snapping to the nearest - with an ACTION button
// (the window's Power) before them: satin, glass while lit (#99); in a quiet
// (greyed) selector, plain on a ribbon of its own.
//
// COMPACT (#92): the same selector at a span chip's height (H_CHIP), its
// choices WORDS in the label face, no action - one option chosen from a short
// list on a line of text (History's spans). `room` (0: any) is the most width
// it may take: the words' sides shrink to fit it, as far as S1. `slotW` (0: the
// widest word's) makes every choice that wide, so selectors stacked in rows -
// UI Tweaks, Card UI (2.8 round 3, D-27) - line up choice for choice.
constexpr uint8_t SELECTOR_MAX = 5;   // the control deck's three, History's four spans, Card UI's five

struct SelectorSpec {
    const char   *glyph[SELECTOR_MAX];   // the choices: icons, or words when compact
    uint8_t       n;
    uint8_t       chosen;
    const char   *actionGlyph;           // null: no action (a compact one has none)
    lv_event_cb_t actionCb;              // CLICKED on the action, no user data
    bool          quiet;                 // greyed: every colour mixed toward the window
    bool        (*blocked)();            // asked before anything moves; true refuses
    void        (*onChosen)(uint8_t i);  // the selector has arrived on choice i
    bool          compact;               // words, H_CHIP tall, no action (#92)
    int32_t       room;                  // compact: the most width it may take (0: any)
    int32_t       slotW;                 // compact: every choice this wide (0: the widest word's)
};
// Compact choices for these words, each as wide as the widest of them and its
// sides: give it to every row's slotW so rows line up.
int32_t selectorSlotW(const char *const *words, uint8_t n);

// Its state, kept by the caller for as long as it shows (its objects point here).
struct Selector {
    lv_obj_t *obj = nullptr, *sel = nullptr, *selIcon = nullptr, *action = nullptr;
    lv_obj_t *item[SELECTOR_MAX] = {};
    const char *glyph[SELECTOR_MAX] = {};
    int32_t   x[SELECTOR_MAX] = {};      // the choices' centres
    uint8_t   n = 0;
    int32_t   selW = 0;
    uint8_t   shown = 0;                     // the choice whose icon the selector carries
    bool      dragging = false;
    int32_t   pressX = 0, selX0 = 0;
    bool    (*blocked)() = nullptr;
    void    (*onChosen)(uint8_t i) = nullptr;
    int32_t   actW = 0, actH = 0, actR = 0;   // the action's face
    int8_t    actLit = -1;                    // its finish: -1 none (quiet), 0 satin, 1 glass
};

int32_t selectorW(uint8_t n);   // with n choices and the action
int32_t selectorH();            // a touch target and its padding (a compact one: H_CHIP)
void    selector(Selector &S, lv_obj_t *parent, const SelectorSpec &spec);

// The action's face: glass, LIT glass while what it switches is on (the window's
// Power; round 5: the glass itself lit from within, not a border on it).
void    selectorActionLit(Selector &S, bool lit);

// --- Chip tables (KitChoices.cpp, 2.8 round 5: UI Tweaks, Card UI) --------------
// A list of option rows `w` wide filling `parent`'s height, scrolling when it
// must: each row its name and its chips, the lit one in the accent. THE LONGEST
// ROW CENTRED; every name right-justified against the chips' column, the chips
// left-justified from it; a row's chips one width (rows may differ). On a window
// too narrow for that (the 4B), every row's name above its chips, centred. `cb`
// on a chip's CLICKED, with ((row << 8) | chip) as its user data. (Round 3's
// selector rows took too much room for too little - the owner.)
constexpr uint8_t CHIP_ROWS_MAX = 12;
struct ChipRowSpec {
    const char        *what;
    const char *const *words;
    uint8_t            n;
    uint8_t            lit;
};
lv_obj_t *chipTable(lv_obj_t *parent, int32_t w, const ChipRowSpec *rows, uint8_t n, lv_event_cb_t cb);
void      chipTableLit(lv_obj_t *table, uint8_t row, uint8_t lit);   // after a choice
// A row of buttons after a chip table, in its list: centred, S5 apart.
lv_obj_t *buttonRow(lv_obj_t *list);

// --- Chips --------------------------------------------------------------------
// A CHIP ROW: small round-ended buttons side by side, the chosen one lit in
// the accent - a choice made in one tap (History's spans). `room` (0: any) is
// the most width they may take: their padding and gaps shrink to fit it, as
// far as half. Each chip's LV_EVENT_CLICKED carries its index as user data.
lv_obj_t *chipRow(lv_obj_t *parent, const char *const *words, uint8_t n, uint8_t lit, int32_t room,
                  lv_event_cb_t cb);

// --- Facts --------------------------------------------------------------------------
// A FACT LIST: a read-only list such as an entity's Properties or "About this
// board" - the whole of `parent`, scrolling if it must, a row per fact.
// A FACT ROW: what something is, dim, and its value beside it, wrapping if
// long ("-" when there is none). Both left aligned (#97). Returns the value's
// label. A fact list holds fact rows only.
// FACT COLUMNS, once the rows are in: the words' column as wide as the longest
// words in the list (at most half of it), the values' as wide as the longest
// value, the two CENTRED - the space left over shared equally between the
// walls and the gap (S5 at least; a long value wraps in its column).
lv_obj_t *factList(lv_obj_t *parent);
lv_obj_t *factRow(lv_obj_t *list, const char *what, const char *value);
void      factColumns(lv_obj_t *list);

// --- List items -----------------------------------------------------------------
// A tappable surface in a list (a member, an activity row, "Show more"): the
// card's radius and surface, tinted while pressed. The caller sizes it, lays
// out what it holds and says what a tap does.
lv_obj_t *item(lv_obj_t *parent);

// --- Rows ---------------------------------------------------------------------
// A ROW in a panel: its words on the left, its control on the right - a
// checkbox, a dropdown of choices, or a button that does something. One list
// of specs is a panel's rows, so its size and its contents cannot disagree.
enum class RowKind : uint8_t { ROW_CHECK, ROW_DROP, ROW_BUTTON };

constexpr uint8_t ROWS_MAX = 12;   // a panel's rows at most: 7 today, room for /popup?rows=

struct RowSpec {
    const char   *label;
    RowKind       kind;
    const char   *opts;     // a dropdown's choices, one per line; a button's words
    uint16_t      sel;      // the chosen one, or the checkbox's state
    bool          live;     // false: shown greyed, does nothing yet
    lv_event_cb_t cb;       // VALUE_CHANGED (checkbox, dropdown), CLICKED (button)
    lv_obj_t    **out;      // where the control is kept, or null
};

int32_t controlH();                               // every row control's height
int32_t controlW(const RowSpec &r);               // this one's width
int32_t rowsNeedW(const RowSpec *r, uint8_t n);   // the widest row, with a panel's sides

lv_obj_t *row(lv_obj_t *parent, const char *what, bool live);
lv_obj_t *dropdown(lv_obj_t *parent, const RowSpec &r);
lv_obj_t *checkbox(lv_obj_t *parent, const RowSpec &r);
lv_obj_t *rowButton(lv_obj_t *parent, const RowSpec &r);
lv_obj_t *rowControl(lv_obj_t *row, const RowSpec &r);   // the one its kind says
void      rows(lv_obj_t *body, const RowSpec *r, uint8_t n);   // each row and its control

// --- Folder panels --------------------------------------------------------------
// A FOLDER PANEL (card-sheet 11.1, the owner's rounds 7-11): a pane of rows
// with a tab rising from its top edge, the join curving in like a file
// folder's, built below the bottom of the screen in a TRACK it can be dragged
// in. The caller slides `root` up and down; the panel's rows go in `body`.
struct FolderSpec {
    const lv_area_t *win;      // the window it belongs to: its width, and its top edge
    bool          left;        // the tab at the window's left edge, the pane growing right
    const char   *title;
    int32_t       needW;       // the widest row (rowsNeedW())
    int32_t       rows;
    int32_t       tabW;
    lv_event_cb_t tabCb;       // CLICKED on the tab
    lv_event_cb_t trackCb;     // SCROLL and SCROLL_END on the track
    void         *trackUser;
};

struct Folder {
    lv_obj_t *root = nullptr, *tab = nullptr, *tabLbl = nullptr, *pane = nullptr, *body = nullptr;
    lv_obj_t *track = nullptr, *up = nullptr, *down = nullptr;
    int32_t   h = 0;           // the whole panel, tab to hidden bottom
    int32_t   head = 0;        // the tab's strip that shows, folded
    int32_t   hide = 0;        // how much of the pane stays below the screen, open
};

void folder(Folder &F, const FolderSpec &spec);

// --- Dialog -------------------------------------------------------------------
// A question over everything - on the top layer, the rest dimmed - with its
// words centred and its buttons side by side. A tap outside the box calls
// `outside` (with the dim's event).
enum class DialogRole : uint8_t { DLG_DESTRUCTIVE };

struct DialogButton {
    const char   *text;
    ButtonRole    role;
    lv_event_cb_t cb;
};

struct DialogSpec {
    DialogRole    role;
    const char   *title;
    const char   *body[2];      // up to two paragraphs; a null ends them early
    DialogButton  buttons[2];
    uint8_t       nButtons;
    int32_t       width;        // px; the height follows the words
    lv_event_cb_t outside;
    bool          countdown;    // it closes with the sheet under it: show that sheet's countdown (#23)
};

// Returns the dim, which holds everything: deleting it closes the dialog.
lv_obj_t *dialog(const DialogSpec &spec);

// --- The page header (KitHeader.cpp, 2.8) ----------------------------------------
// The BAR: full width, the header's height, the scheme's raised surface with its
// accent line along the bottom. Its slots go in it, each where the header plan
// puts it (Layout/HeaderPlan.h) - the bar lays nothing out itself.
lv_obj_t *headerBar(lv_obj_t *parent);
// A SLOT: the bar's height, tappable, tinted while pressed, in the drawer's
// face while LV_STATE_CHECKED (its panel open). `cb` on CLICKED, with `user` as
// its user data. What it shows goes inside, centred in a row; its x and width
// are the caller's, from the plan.
lv_obj_t *headerSlot(lv_obj_t *bar, lv_event_cb_t cb, void *user);
// A GLYPH'S FRAME (#19): BARE an empty holder the glyph's own size, SQUARE the
// rounded square, CIRCLE the circle - whose colour is the caller's to set: it is
// the glyph's state. The glyph goes inside, centred.
lv_obj_t *glyphFrame(lv_obj_t *parent, GlyphStyle style);

// --- A stroke glyph (KitGlyph.cpp, 2.8 round 3) ------------------------------------
// One of the status glyphs drawn from its strokes (UI/GlyphShapes.h) at `px`
// square: LVGL's own round-capped arcs and lines, each stroke its own object so
// each PART takes its own colour. A STRIKE (switched off on purpose) lies over
// it on a halo of the ground it stands on. Its state lives here, kept by the
// caller while it shows (a line keeps a pointer to its points).
struct StrokeGlyph {
    lv_obj_t *box = nullptr;
    lv_obj_t *obj[Glyphs::STROKES_MAX] = {};
    uint8_t   part[Glyphs::STROKES_MAX] = {};
    uint8_t   n = 0;
    lv_obj_t *halo = nullptr, *strike = nullptr;
    lv_point_precise_t pts[Glyphs::STROKES_MAX][2];
    lv_point_precise_t strikePts[2];
};
void strokeGlyph(StrokeGlyph &g, lv_obj_t *parent, Glyphs::Shape shape, int32_t px);
// Colour every stroke of `part` (ALL_PARTS: every stroke), at `opa`.
constexpr uint8_t ALL_PARTS = 0xFF;
void strokeGlyphPaint(StrokeGlyph &g, uint8_t part, lv_color_t c, lv_opa_t opa = LV_OPA_COVER);
void strokeGlyphShow(StrokeGlyph &g, uint8_t part, bool shown);
void strokeGlyphStrike(StrokeGlyph &g, bool on, lv_color_t c, lv_color_t halo);

// --- A drawer (KitDrawer.cpp, 2.8) -------------------------------------------------
// A PANEL THAT SLIDES DOWN from a header slot: square where it meets the bar,
// its bottom corners rounded, the scheme's shadow. The caller builds its rows in
// `face` (a column), then places and opens it; it grows from nothing to the
// height its rows need (at most the screen below `top`). The system panel is the
// status slot's own drawer, and older than this one (Panel_System).
struct Drawer {
    lv_obj_t *root = nullptr;   // the clip that grows; holds everything
    lv_obj_t *face = nullptr;   // the rows go here
    bool      open = false;
    int32_t   x = 0, w = 0, openH = 0, top = 0;
    // Called once it has finished closing - its slot's tab stays open until
    // then (round 4: the slot and its panel joined for as long as any of the
    // panel shows). Not called for a close cut short by opening again.
    void    (*onClosed)(void *user) = nullptr;
    void     *closedUser = nullptr;
};
int32_t drawerWidth();                                // a slot's drawer on this screen
void    drawer(Drawer &d, lv_obj_t *parent, int32_t w);
// Under its slot, kept on screen EDGE from its sides - where a slot's tab
// stands, so a panel at a screen edge shares that side with its tab (D-23).
void    drawerPlace(Drawer &d, int32_t centreX, int32_t top);
// Open: its side walls rise straight to the bar's line (round 4: no flares).
void    drawerOpen(Drawer &d);
void    drawerClose(Drawer &d, bool animate = true);
// The bar's DRAW_MAIN_END (Kit::headerBar() hooks it): the bar is drawn over the
// panels. For each open panel it fades its accent line out short of the panel's
// walls - the panel lies on the bar (D-35) - and draws the halo where it falls
// on the bar (round the panel's top and its slot's tab); and on a scheme with a
// shadow it paints the panel's face back over the bar's shadow (round 6, Linen).
void    drawerOverBar(lv_event_t *e);

// ONE SHAPE (D-23): a join is a corner of the outline where it meets the bar's
// accent line - a COVE beside an open slot's tab, in the bar (round 4: the
// panel's own corners, the flares, are gone - its walls meet the line square).
// Built hidden; placed at the outer edge of the side it rounds (exclusive for a
// right side).
lv_obj_t *joinCorner(lv_obj_t *parent);
void      coveAt (lv_obj_t *j, int32_t edgeX, bool right, int32_t barH);
// A drawer's lines. An ITEM to tap, full width and a finger tall: a mark (filled
// when `marked`, a ring otherwise - this page and the others) and its words;
// `cb` on CLICKED with `user`. A NOTE, dim, wrapping. A BIG value - a time, a
// reading.
lv_obj_t *drawerItem(lv_obj_t *face, const char *words, bool marked, lv_event_cb_t cb, void *user);
lv_obj_t *drawerNote(lv_obj_t *face, const char *words);
lv_obj_t *drawerBig(lv_obj_t *face, const char *words);
// A ROW of things side by side, the panel's width, DRAWER_GAP apart (buttons,
// a selector and a toggle). A STATUS ROW (2.8 round 3, the system panel's
// CONNECTIVITY): room for a glyph (`glyphRoom`), what it is, and its live value
// dim beside it - returns the value's label.
lv_obj_t *drawerRow(lv_obj_t *face);
lv_obj_t *statusRow(lv_obj_t *face, const char *what, lv_obj_t **glyphRoom);
// A drawer's section title, in the panel-title role.
lv_obj_t *drawerTitle(lv_obj_t *face, const char *words);
// A short upright rule between things in a row - one choice set apart from
// another (the scheme and the Status bar, D-25).
lv_obj_t *vrule(lv_obj_t *row, int32_t h);
// A TOGGLE button: lit (glass) while on, satin while off (#99's finishes).
lv_obj_t *toggleButton(lv_obj_t *parent, const char *words, int32_t w, int32_t h, bool on, lv_event_cb_t cb);
void      toggleButtonSet(lv_obj_t *b, bool on);
// A selector's chosen, set from outside (no onChosen): the scheme changed elsewhere.
void      selectorSet(Selector &S, uint8_t i);

} // namespace Kit
