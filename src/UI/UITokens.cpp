#include "UI/UITokens.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Schemes
//
// Values come from the design bench sessions, 2026-09-10. The owner signed off
// four configurations; what they disagreed about was accent and per-board grid
// density, and what they agreed about is everything below.
// ---------------------------------------------------------------------------

// Shared across every scheme: state colours and sensor tints are content, not
// decoration, so they do not change when the scheme does. A warning is amber
// whether the dashboard is dark or light.
#define FLEET_STATE_AND_TINTS               \
    .ST_ACTIVE  = 0xF0A63C,                 \
    .ST_IDLE    = 0x64748B,                 \
    .ST_OK      = 0x4ADE80,                 \
    .ST_WARN    = 0xFBBF24,                 \
    .ST_BAD     = 0xF87171,                 \
    .TINT_TEMP  = 0xE0705F,                 \
    .TINT_HUMID = 0x4A9EDA,                 \
    .TINT_LIGHT = 0xE0A53C,                 \
    .TINT_AIR   = 0x9B7BD4,                 \
    .TINT_POWER = 0x93C04A,                 \
    .TINT_OPENING  = 0x5B8DEF,              \
    .TINT_PRESENCE = 0x2EC4B6

// Lighting's corner tint: yellow-white on the dark schemes, where it reads as
// a bulb; a deeper amber on the light ones, where yellow-white would vanish.
#define TINT_LIGHTING_DARK  0xFFE3A3
#define TINT_LIGHTING_LIGHT 0xC98A12

// THREE SCHEMES, pruned on glass 2026-09-23: Fleet, Midnight, Linen.
//
// Slate went because it differed from Midnight only in its accent ("slate is
// purplish, midnight is blue"). Paper went because Linen replaced it. Frost
// went because it lost to Linen - "I might have thought Frost would look good
// but when I saw it it's definitely not for me". All three are in git history.

// LINEN - the light scheme. A candidate from 2026-09-22 that won against Paper
// and Frost: a sand ground, cream cards, dark brown text, and a copper accent
// dark enough to read as text on a light surface - which Paper's amber was
// not (the owner's complaint about the device name and the panel titles).
const UIPalette UI_PAL_LINEN = {
    .name        = "Linen",
    .GROUND      = 0xDDD5C8,
    .SURFACE     = 0xF8F4EC,
    .SURFACE_ALT = 0xEDE6DA,
    .TEXT        = 0x2B2520,
    .TEXT_DIM    = 0x6E6459,
    .ACCENT      = 0xA8561C,   // copper
    .BORDER      = 0,
    FLEET_STATE_AND_TINTS,
    .TINT_LIGHTING = TINT_LIGHTING_LIGHT,
};

// Today's shipped UI, read out of UIToolkit.cpp and Panel_Header.cpp rather
// than eyeballed. Kept so the new system can be compared against the old one on
// the same glass instead of from memory.
const UIPalette UI_PAL_FLEET = {
    .name        = "Fleet",
    .GROUND      = 0x101010,
    .SURFACE     = 0x181818,
    .SURFACE_ALT = 0x202020,
    .TEXT        = 0xDDDDDD,
    .TEXT_DIM    = 0x808080,
    .ACCENT      = 0x00A8FF,
    .BORDER      = 0x404040,   // explicit — this one is not derived
    FLEET_STATE_AND_TINTS,
    .TINT_LIGHTING = TINT_LIGHTING_DARK,
};

// The owner's verdict after seeing the dark schemes on glass: the former
// Slate's ground, Fleet's blue. The DEFAULT since 2026-09-23.
const UIPalette UI_PAL_MIDNIGHT = {
    .name        = "Midnight",
    .GROUND      = 0x1A1F27,
    .SURFACE     = 0x212429,
    .SURFACE_ALT = 0x2B2F36,
    .TEXT        = 0xF0F2F4,
    .TEXT_DIM    = 0x98A0AA,
    .ACCENT      = 0x00A8FF,   // Fleet cyan
    .BORDER      = 0,
    FLEET_STATE_AND_TINTS,
    .TINT_LIGHTING = TINT_LIGHTING_DARK,
};

#undef FLEET_STATE_AND_TINTS
#undef TINT_LIGHTING_DARK
#undef TINT_LIGHTING_LIGHT

// Dark schemes take a hairline border because a dark card on a dark ground
// needs an edge; light schemes lean on the shadow instead. That difference was
// the only thing separating the owner's light and dark configs.
//
// NO CARD SHADOW HAS EVER BEEN VISIBLE, until 2026-09-23. Both metric sets
// asked for SHADOW = 8, and Card clipped it away: the styled surface sits in a
// transparent wrapper exactly its own size, and a parent clips its children.
// The owner put his finger on it from the other end - Linen "looks too flat" -
// and the fix is one flag on that wrapper (Card::build()). The dark schemes are
// set to SHADOW 0 so they look exactly as they always have; only the light
// scheme, where a shadow is what separates a card from the page, gets one.
const UIMetrics UI_MET_DARK  = {
    .RADIUS = 10, .PAD = 5, .BORDER_W = 1, .BORDER_OPA_PCT = 40, .SHADOW = 0, .HEADER_H = 14,
    .SHADOW_Y = 0, .SHADOW_OPA = 0
};
// Linen's. A 2 px border, darker than the old hairline (the owner: "make it a
// bit darker and increase by 1 or 2px", knobs included - the drawer's buttons
// take BORDER_W through UI::paint()), and a mild DROP shadow - "in HA for most
// of my custom dashboards I shamelessly use a mild drop shadow".
//
// Round four, from the owner's close-up of the shadow he means: TIGHTER and
// DARKER, not wider. A short blur, a small drop, a firm edge - the first try
// (12 px blur at 30%) read as fuzz rather than depth. Border darker again
// ("a bit darker than it is or expand it 1 more pixel" - darker was chosen).
const UIMetrics UI_MET_LIGHT = {
    .RADIUS = 12, .PAD = 5, .BORDER_W = 2, .BORDER_OPA_PCT = 34, .SHADOW = 6, .HEADER_H = 14,
    .SHADOW_Y = 3, .SHADOW_OPA = 140
};

// ---------------------------------------------------------------------------
// Active state
// ---------------------------------------------------------------------------
namespace {

// Fleet, not Slate: the port onto tokens should be invisible on glass, so the
// default has to be today's colours. Switching the default to Slate or Paper is
// a one-line change once the new look has been seen on a board.
UIPalette s_pal = UI_PAL_FLEET;
UIMetrics s_met = UI_MET_DARK;

// TARGET_CARD_W IS PER BOARD, and it has to be.
//
// It decides how many COLUMNS fit, and the answer is not transferable: the
// same logical width that gives WS_P4_7B the 6 columns the owner chose gives
// WS_P4_5 only 4, because the 7B is 1024 logical px wide and the P4_5 is
// 1280/1.73 = 740. One fleet-wide number cannot serve both, and the attempt
// cost a session - moving the default 130 -> 135 for the 7B's benefit quietly
// knocked the P4_5 from 5 columns to 4, and at 4 columns nothing on it was
// readable.
//
// Chosen on the glass, per board, by the owner turning the knob:
//
//   WS_P4_7B   1024 x 600 @ 170 PPI   135 -> 6 columns
//   WS_P4_5    1280 x 800 @ 294 PPI   130 -> 5 columns
//
// NOTE THE DIRECTION, because it is easy to get backwards and I did: a LARGER
// target means FEWER columns, since it is the width each card is trying to be.
// 148 was picked for the P4_5 on the reasoning that it wanted bigger cards
// than the 7B; what it actually did was drop it from 5 columns to 4. That
// board needs <= 132 for five, and 130 is the value already proven on glass.
//
// Every other board keeps the fleet default until someone has looked at it.
// This is the ConnectivityDefaults.h pattern - one shared default, overridden
// per board through the identity macro every BSP header already defines, and
// no new machinery.
#if   defined(WS_P4_5)
    #define FLEET_TARGET_CARD_W 130
#elif defined(WS_P4_7B)
    #define FLEET_TARGET_CARD_W 135
#else
    #define FLEET_TARGET_CARD_W 135
#endif

// ASPECT_PCT is a CEILING on card height as a percentage of card width, not a
// target shape - see UITokens.h. It no longer picks the row count.
UIGrid    s_grid = { .TARGET_CARD_W = FLEET_TARGET_CARD_W, .COLS_OVERRIDE = 0,
                     .ASPECT_PCT = 130, .GAP = 12, .INSET = 14,
                     .cols = 1, .rows = 1, .cellW = 0, .cellH = 0 };

int32_t s_vpW = 0, s_vpH = 0;
void (*s_onChange)() = nullptr;

// Blend two 0xRRGGBB values. Used for deriving a border from a surface, and
// exposed as UI::mix() for dimming without opacity.
uint32_t blend(uint32_t a, uint32_t b, uint8_t pct) {
    uint32_t out = 0;
    for (int sh = 16; sh >= 0; sh -= 8) {
        uint32_t ca = (a >> sh) & 0xFF, cb = (b >> sh) & 0xFF;
        out |= (uint32_t)((ca * (100 - pct) + cb * pct) / 100) << sh;
    }
    return out;
}

void recomputeGrid() {
    if (s_vpW <= 0 || s_vpH <= 0) return;

    const int32_t inset = UI::sc(s_grid.INSET);
    const int32_t gap   = UI::sc(s_grid.GAP);
    const int32_t target = UI::sc(s_grid.TARGET_CARD_W);

    const int32_t availW = s_vpW - inset * 2;
    const int32_t availH = s_vpH - inset * 2;
    if (availW <= 0 || availH <= 0) return;

    // An explicit count wins; otherwise as many whole target-width cards as fit.
    int32_t cols = s_grid.COLS_OVERRIDE
                 ? (int32_t)s_grid.COLS_OVERRIDE
                 : (availW + gap) / (target + gap);
    if (cols < 1) cols = 1;
    if (cols > UI_MAX_COLS) cols = UI_MAX_COLS;
    const int32_t cellW = (availW - gap * (cols - 1)) / cols;

    // Rows here are only an ESTIMATE for callers with no cards to count.
    //
    // CardPage overrides both the row count and the cell height, because the
    // right number of rows is a question about CONTENT and this function only
    // knows geometry. See CardPage::rowsWanted(). ASPECT_PCT is now a CAP on
    // how tall a card may get relative to its width rather than a target
    // shape, so this estimate is deliberately generous.
    const int32_t wantH = (cellW * s_grid.ASPECT_PCT) / 100;
    int32_t rows = (availH + gap) / (wantH + gap);
    if (rows < 1) rows = 1;
    const int32_t cellH = (availH - gap * (rows - 1)) / rows;

    s_grid.cols  = (uint8_t)cols;
    s_grid.rows  = (uint8_t)rows;
    s_grid.cellW = (uint16_t)cellW;
    s_grid.cellH = (uint16_t)cellH;
}

} // namespace

namespace UI {

void begin(int32_t viewportW, int32_t viewportH) {
    setViewport(viewportW, viewportH);
    // Columns and the scale only. The ROW count printed here used to be
    // taken seriously and is not the page's answer - CardPage decides rows
    // from how many cards it has, against the host it is actually given,
    // which is the screen minus the header and the deck rather than the
    // whole panel. Printing a row count here read as a decision and caused
    // real confusion ("why does it say 4x1?").
    Serial.printf("[UI] Scheme \"%s\" | %u cols of %u px | scale %.2fx (%u PPI)\n",
                  s_pal.name, (unsigned)s_grid.cols, (unsigned)s_grid.cellW,
                  (double)bspUiScale(), (unsigned)bspPixelDensity());
}

void setViewport(int32_t w, int32_t h) {
    s_vpW = w; s_vpH = h;
    recomputeGrid();
}

const UIPalette &pal()  { return s_pal; }
const UIMetrics &met()  { return s_met; }
const UIGrid    &grid() { return s_grid; }

const UIType &type() {
    // Built on first use rather than at static-init time: the font objects are
    // LVGL globals, and depending on a global's initialisation order from
    // another translation unit is the static-init-order fiasco waiting to
    // happen. One branch, once.
    //
    // The sizes come from include/UI/UITypeScale.h, which scripts/
    // gen_type_scale.py derives from THIS BOARD's pixel density. They used to
    // be four hardcoded faces shared by all eight boards, and that was the one
    // place the "derive from density" rule of tokens.md had not reached - so
    // WS_P4_5 at 294 PPI drew its smallest text at 1.04 mm while CYD_S3_3248
    // at 165 PPI drew the same token at 1.85 mm. The better panel was the
    // harder one to read. Flashed and caught by eye, not by arithmetic.
    static UIType t = {
        .VALUE   = FLEET_FONT_VALUE,
        .VALUE_SM = FLEET_FONT_VALUE_SM,
        .UNIT    = FLEET_FONT_UNIT,
        .NAME    = FLEET_FONT_NAME,
        .TAG     = FLEET_FONT_TAG,
        .ICON    = FLEET_ICONS_LG,
        .ICON_MD = FLEET_ICONS_MD,
        .ICON_SM = FLEET_ICONS_SM,
        // HERO shares VALUE's face. Referencing another size pulls a whole
        // extra font into the link for something nothing draws yet - at the
        // cost recorded in docs/design/tokens.md. Point it somewhere larger
        // when a fullscreen card actually needs one.
        .HERO    = FLEET_FONT_VALUE,
    };
    return t;
}

// --- Shared paints. #64 - see the header. -----------------------------------
static lv_style_t s_paint[(int)UIPaint::PAINT_COUNT];
static bool       s_paintReady = false;

static void refreshPaints() {
    if (!s_paintReady) return;   // nothing has asked for one yet
    auto st = [](UIPaint p) { return &s_paint[(int)p]; };
    const lv_color_t bd = border();

    // Border WIDTH as well as colour, from the scheme's metrics: Linen's 2 px
    // edge is meant for the drawer's buttons and the deck panels too (the
    // owner, 2026-09-23), and a width set locally on each widget would not
    // follow a scheme change.
    lv_style_set_bg_color    (st(UIPaint::PAINT_SURFACE),     c(s_pal.SURFACE));
    lv_style_set_border_color(st(UIPaint::PAINT_SURFACE),     bd);
    lv_style_set_border_width(st(UIPaint::PAINT_SURFACE),     s_met.BORDER_W);
    lv_style_set_bg_color    (st(UIPaint::PAINT_SURFACE_ALT), c(s_pal.SURFACE_ALT));
    lv_style_set_border_color(st(UIPaint::PAINT_SURFACE_ALT), bd);
    lv_style_set_border_width(st(UIPaint::PAINT_SURFACE_ALT), s_met.BORDER_W);
    // AND NO SHADOW. PAINT_SURFACE_ALT dresses the drawer's buttons, and
    // LVGL's default theme gives every button a grey shadow 4 px below it.
    // Each button sits in a row that clips, so the shadow showed as bottom
    // corners that "bulge a tiny bit but then something is cutting off 1 or 2
    // pixel rows", and on the dark schemes as a light "glint" at those
    // corners - both the owner's, 2026-09-23. A shared style added after the
    // theme's outranks it, so this one line removes it everywhere the paint
    // is used.
    lv_style_set_shadow_width(st(UIPaint::PAINT_SURFACE_ALT), 0);
    lv_style_set_text_color  (st(UIPaint::PAINT_TEXT),        c(s_pal.TEXT));
    lv_style_set_text_color  (st(UIPaint::PAINT_TEXT_DIM),    c(s_pal.TEXT_DIM));
    lv_style_set_text_color  (st(UIPaint::PAINT_ACCENT_TEXT), c(s_pal.ACCENT));
    lv_style_set_bg_color    (st(UIPaint::PAINT_ACCENT_BG),   c(s_pal.ACCENT));

    // NULL means "every style changed": each object re-reads what it uses and
    // invalidates itself. A scheme change is a rare, deliberate act, so the
    // whole-tree walk is the right price for never missing a widget.
    lv_obj_report_style_change(NULL);
}

lv_style_t *paint(UIPaint p) {
    if (!s_paintReady) {
        for (lv_style_t &s : s_paint) lv_style_init(&s);
        s_paintReady = true;
        // The report inside is harmless at first use: nothing is using these
        // styles yet, so there is nothing for it to repaint.
        refreshPaints();
    }
    const int i = (int)p;
    return (i >= 0 && i < (int)UIPaint::PAINT_COUNT) ? &s_paint[i]
                                                     : &s_paint[0];
}

void setScheme(const UIPalette &p, const UIMetrics &m) {
    s_pal = p;   // copies, so setAccent() can override without touching a const
    s_met = m;
    recomputeGrid();
    refreshPaints();
    if (s_onChange) s_onChange();
}

// THE SCHEME KNOB'S ORDER, in one place. Three screens each carried their own
// copy of this switch - the drawer, the card bench and the reference page -
// which is how deleting a scheme came to mean editing three files.
struct SchemeEntry { const UIPalette *pal; const UIMetrics *met; };
static const SchemeEntry SCHEMES[] = {
    { &UI_PAL_FLEET,    &UI_MET_DARK  },
    { &UI_PAL_MIDNIGHT, &UI_MET_DARK  },
    { &UI_PAL_LINEN,    &UI_MET_LIGHT },
};

static constexpr uint8_t SCHEME_N = sizeof(SCHEMES) / sizeof(SCHEMES[0]);

uint8_t schemeIndex() {
    for (uint8_t i = 0; i < SCHEME_N; i++)
        if (strcmp(SCHEMES[i].pal->name, s_pal.name) == 0) return i;
    return 0;
}

void setSchemeIndex(uint8_t i) {
    if (i >= SCHEME_N) i = 0;
    if (i == schemeIndex()) return;   // no change, no repaint
    setScheme(*SCHEMES[i].pal, *SCHEMES[i].met);
}

void cycleScheme() {
    const SchemeEntry &next = SCHEMES[(schemeIndex() + 1) % SCHEME_N];
    setScheme(*next.pal, *next.met);
}

void setAccent(uint32_t hex) {
    s_pal.ACCENT = hex;
    refreshPaints();
    if (s_onChange) s_onChange();
}

void setTargetCardWidth(uint16_t logicalPx) {
    s_grid.TARGET_CARD_W = logicalPx;
    recomputeGrid();
    if (s_onChange) s_onChange();
}

void setColumnsOverride(uint8_t cols) {
    if (cols > UI_MAX_COLS) cols = UI_MAX_COLS;
    s_grid.COLS_OVERRIDE = cols;
    recomputeGrid();
    if (s_onChange) s_onChange();
}

void setAspectPct(uint8_t pct) {
    // Clamped to a range that can still produce a usable grid. Below ~40 the
    // rows get shorter than the type scale can seat and every card goes
    // compact; above ~200 a page is one row of very tall cards.
    if (pct < 40)  pct = 40;
    if (pct > 200) pct = 200;
    s_grid.ASPECT_PCT = pct;
    recomputeGrid();
    if (s_onChange) s_onChange();
}

void onSchemeChanged(void (*cb)()) { s_onChange = cb; }

int32_t sc(int32_t logical) { return (int32_t)(logical * bspUiScale()); }

lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

uint32_t mix(uint32_t a, uint32_t b, uint8_t pct) { return blend(a, b, pct); }

// Rec. 601 luma, 0-255. Good enough to choose between two inks; not a
// colour-science claim.
static int32_t luma(uint32_t c) {
    return (int32_t)((((c >> 16) & 0xFF) * 299 + ((c >> 8) & 0xFF) * 587 +
                      (c & 0xFF) * 114) / 1000);
}

uint32_t contrastOf(uint32_t bg, uint32_t a, uint32_t b) {
    const int32_t l = luma(bg);
    const int32_t da = luma(a) - l, db = luma(b) - l;
    return (da * da >= db * db) ? a : b;
}

lv_color_t border() {
    if (s_pal.BORDER) return lv_color_hex(s_pal.BORDER);
    // Derived: pull the surface toward white on dark schemes, toward black on
    // light ones. This is the bug the bench found in its first version — a
    // border derived from a fixed white vanished entirely on a white card.
    const bool darkSurface = ((s_pal.SURFACE >> 16) & 0xFF) < 0x80;
    return lv_color_hex(blend(s_pal.SURFACE, darkSurface ? 0xFFFFFF : 0x000000,
                              s_met.BORDER_OPA_PCT));
}

void tameScroll(lv_obj_t *o) {
    lv_obj_set_scroll_dir  (o, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag      (o, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag      (o, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
}

int32_t minTouch() {
    const uint16_t ppi = bspPixelDensity();
    if (!ppi) return sc(44);          // no density declared — a sane default
    return (int32_t)((ppi * 9) / 25.4f + 0.5f);   // 9 mm
}

} // namespace UI
