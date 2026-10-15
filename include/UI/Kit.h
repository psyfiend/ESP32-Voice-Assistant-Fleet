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

namespace Kit {

// --- The shared styles -------------------------------------------------------
// One lv_style_t each; the selector is given where it is added (Kit::add()).
// Grouped by the component that uses them; components.md says what each sets.
enum class KitStyle : uint8_t {
    // Label roles: a face and a colour.
    KS_LABEL_NAME, KS_LABEL_NAME_DIM, KS_LABEL_TAG, KS_LABEL_TAG_DIM, KS_LABEL_TAG_ACCENT,
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

// --- Buttons ------------------------------------------------------------------
// A button with words: CANCEL is the quiet way out, DESTRUCTIVE the one that
// removes something (the scheme's ST_BAD).
enum class ButtonRole : uint8_t { BTN_CANCEL, BTN_DESTRUCTIVE };

// `w` x `h` px, the words centred; `cb` on LV_EVENT_CLICKED, with no user data.
lv_obj_t *button(lv_obj_t *parent, ButtonRole role, const char *text, int32_t w, int32_t h,
                 lv_event_cb_t cb);

// --- Faces drawn by hand (KitFaces.cpp) -------------------------------------------
// A raised metal face (black or silver: the selector, a lit icon button) or a
// soft dent in the surface (an unlit icon button), painted once into PSRAM and
// kept; the object's own background is turned off while it shows.
enum class FaceKind : uint8_t { FACE_BLACK, FACE_SILVER, FACE_DENT };

// The face as the object's first child (an image); `flat` is its colour when
// no face can be had. Returns the image, or null.
lv_obj_t *setFace(lv_obj_t *o, int32_t w, int32_t h, int32_t r, FaceKind kind, uint32_t flat);
lv_obj_t *labelIn(lv_obj_t *o);   // the label inside a faced button
void      facesNextWindow();      // a new window: faces the last one used may be taken back
uint32_t  facesFlat();            // how many came out flat (a debug readout)

// THE SELECTOR'S LOOK, per scheme (K36, K45): -1 the scheme's own, else bit 0
// round, bit 1 silver. Kept in the settings file under the scheme.
int8_t   selectorLook();
void     keepSelectorLook(int8_t look);
uint8_t  selectorLookNow();       // the look in force
FaceKind metalKind();             // its metal
int32_t  selectorRadius();        // its corners: round, or R_CONTROL

struct SelectorColours { uint32_t ribbon, icon, chosen; };
SelectorColours selectorColours();

// --- Icon buttons ----------------------------------------------------------------
// A round button at the minimum touch size with an icon: a soft dent in the
// surface, tinted while pressed. `cb` on LV_EVENT_CLICKED.
lv_obj_t *iconButton(lv_obj_t *parent, const char *glyph, const lv_font_t *f, lv_event_cb_t cb,
                     lv_obj_t **outLabel = nullptr);
// Lit (a tab that is showing): the selector's metal, the icon a size up in the accent.
void iconButtonLit(lv_obj_t *b, bool lit);

// --- The selector (KitSelector.cpp) ------------------------------------------------
// Choices on a ribbon and a metal selector that slides between them - dragged
// or glided to a tapped choice, snapping to the nearest - with an ACTION button
// (the window's Power) on a ribbon of its own before them.
//
// COMPACT (#92): the same selector at a span chip's height (H_CHIP), its
// choices WORDS in the label face, no action - one option chosen from a short
// list on a line of text (History's spans). `room` (0: any) is the most width
// it may take: the words' sides shrink to fit it, as far as S1.
constexpr uint8_t SELECTOR_MAX = 4;   // the control deck's three, History's four spans

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
};

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
};

int32_t selectorW(uint8_t n);   // with n choices and the action
int32_t selectorH();            // a touch target and its padding (a compact one: H_CHIP)
void    selector(Selector &S, lv_obj_t *parent, const SelectorSpec &spec);

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

} // namespace Kit
