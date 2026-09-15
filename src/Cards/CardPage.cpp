#include "Cards/CardPage.h"
#include "Cards/CardTypes.h"
#include "UI/UITokens.h"
#include "SystemReport.h"
#include <Arduino.h>

CardPage::~CardPage() {
    for (uint8_t i = 0; i < _n; i++) {
        if (!_cards[i]) continue;
        if (_binder) _binder->remove(_cards[i]);
        delete _cards[i];           // Card's destructor deletes its widgets
    }
    if (_root) lv_obj_delete(_root);
}

void CardPage::begin(lv_obj_t *parent, CardBinder *binder, int32_t tagOverhang) {
    _binder = binder;

    const UIGrid &g = UI::grid();

    // No tag clearance any more, and no widened gaps.
    //
    // A tag now lives INSIDE its cell, above the card - see Card::build(). The
    // page went to some trouble to widen its row gap and shrink its cells so a
    // tag could hang into the space between rows, and none of it ever worked:
    // the grid container clips its children and the tags were simply cut away.
    // The owner's rule - the space between a tag and the card above it equals
    // the space between two plain cards - now falls out of an ordinary gap.
    const int32_t inset = UI::sc(g.INSET);
    const int32_t gap   = UI::sc(g.GAP);
    (void)tagOverhang;

    _root = lv_obj_create(parent);
    lv_obj_set_size               (_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color     (_root, UI::c(UI::pal().GROUND), 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, inset, 0);
    lv_obj_set_style_pad_column   (_root, gap, 0);
    lv_obj_set_style_pad_row      (_root, gap, 0);

    // A DASHBOARD PAGE DOES NOT SCROLL. The owner, unprompted: "There should
    // never be any scrolling of cards on any dashboard pages, the cards should
    // effectively be locked in place. Only when swiping or navigating to a new
    // page entirely should new entities be on the screen."
    //
    // So cards that do not fit are not placed at all - see add(). Scrolling was
    // also what made the 3248 reboot: dragging a grid whose rows ran off the
    // bottom. Which cards get dropped when a page is over-subscribed is
    // CardPlacement::priority's job, and that is milestone 2.5.
    lv_obj_clear_flag       (_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(_root, LV_SCROLLBAR_MODE_OFF);

    // Columns are FR units, not the pixel widths UI::grid() computed.
    //
    // That is not ignoring the token - cellW is what DERIVED the column count,
    // and the count is the decision. Expressing the result as fractions is
    // what makes a spanning card come out exactly right: a 2-wide card is two
    // fractions plus the gap between them, where two pixel widths plus a gap
    // would need the gap added by hand at every span and would drift by a
    // pixel per column from integer division.
    uint8_t cols = g.cols;
    if (cols < 1) cols = 1;
    if (cols > 16) cols = 16;       // _colDsc is fixed; 16 columns is already
                                    // far past the fleet's densest board
    for (uint8_t i = 0; i < cols; i++) _colDsc[i] = LV_GRID_FR(1);
    _colDsc[cols] = LV_GRID_TEMPLATE_LAST;

    // Rows are the token's real pixel height. Unlike columns, rows must NOT be
    // fractions: the page scrolls vertically, so the row count is open-ended
    // and a fraction of an unbounded height is meaningless.
    //
    // Recomputed here rather than taken from UIGrid::cellH because the tag
    // clearance above widened the gaps this page uses, and cellH was derived
    // against the standard ones. Without this the rows would still be the
    // original height, the grid would overflow by exactly the clearance, and
    // the last row would hang off the bottom.
    uint8_t rows = g.rows;
    if (rows < 1) rows = 1;
    if (rows > 16) rows = 16;

    _cellH    = g.cellH;
    _maxRows  = rows;

    // UIGrid::rows is how many rows FIT, which is a different question from
    // how many rows EXIST. Cards beyond the first screenful are placed into
    // real rows that scroll into view, and every one of them has to be in this
    // descriptor before LVGL reads it - see the header for what happens when
    // it is not.
    _rowsDefined = 0;
    ensureRows(rows);

    lv_obj_set_layout(_root, LV_LAYOUT_GRID);

    Serial.printf("[Cards] Page %ux%u, cell %ux%u px, gap %d - no scroll\n",
                  (unsigned)cols, (unsigned)_rowsDefined,
                  (unsigned)g.cellW, (unsigned)_cellH, (int)gap);
}

Card *CardPage::add(Card *c) {
    if (!c) return nullptr;
    if (_n >= CARD_PAGE_MAX) {
        // Deleted rather than dropped on the floor. A page that silently
        // leaked its overflow would be a slow leak on a device whose whole
        // widget budget is a 128 KB static array.
        delete c;
        return nullptr;
    }

    // Would this card land past the last visible row? Then it does not go on
    // the page at all. A page shows what fits and nothing else.
    if (!fits(c)) {
        DBG_CARDS("dropped %s - page is full at %u rows\n",
                  c->typeName(), (unsigned)_maxRows);
        delete c;
        return nullptr;
    }

    _cards[_n++] = c;
    c->build(_root);
    placeCard(c);
    if (_binder) _binder->add(c);
    return c;
}

// Would this card fit, without actually placing it? Runs the same cursor
// arithmetic placeCard() does, on copies, so the two cannot disagree.
bool CardPage::fits(const Card *c) const {
    const CardPlacement &p = c->placement();

    uint8_t cols = 0;
    while (_colDsc[cols] != LV_GRID_TEMPLATE_LAST && cols < 16) cols++;
    if (!cols) cols = 1;

    uint8_t col = _curCol, row = _curRow;
    uint8_t spanX = p.prefSpanX ? p.prefSpanX : 1;
    if (spanX > cols) spanX = cols;
    if (col + spanX > cols) {
        const uint8_t left = cols - col;
        if (!(left >= p.minSpanX && p.minSpanX >= 1)) { col = 0; row++; }
    }
    const uint8_t spanY = p.prefSpanY ? p.prefSpanY : 1;
    return (row + spanY) <= _maxRows;
}

void CardPage::ensureRows(uint8_t need) {
    if (need > CARD_PAGE_MAX) need = CARD_PAGE_MAX;
    if (need <= _rowsDefined) return;

    for (uint8_t i = _rowsDefined; i < need; i++) _rowDsc[i] = _cellH;
    _rowDsc[need] = LV_GRID_TEMPLATE_LAST;
    _rowsDefined  = need;

    // Re-applied because the array's LENGTH changed. LVGL keeps the pointer,
    // not a copy, so the contents are already live - but it caches the row
    // count at set time and has to be told again.
    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
}

void CardPage::placeCard(Card *c) {
    const CardPlacement &p = c->placement();

    uint8_t cols = 0;
    while (_colDsc[cols] != LV_GRID_TEMPLATE_LAST && cols < 16) cols++;
    if (!cols) cols = 1;

    // Preferred span, clamped to the page itself. On CYD_S3_3248 portrait the
    // page is two columns wide, so a card asking for three is not a
    // misconfiguration to reject - it is a card designed for a bigger board,
    // and the correct answer is the widest thing that fits.
    uint8_t spanX = p.prefSpanX ? p.prefSpanX : 1;
    if (spanX > cols) spanX = cols;

    // Not enough of this row left? Shrink toward minSpan before wrapping,
    // because a card that fits at its minimum beside its neighbour beats a
    // card that sits alone on a new row at full width. Wrapping is what
    // happens when even the minimum does not fit.
    if (_curCol + spanX > cols) {
        const uint8_t left = cols - _curCol;
        if (left >= p.minSpanX && p.minSpanX >= 1) {
            spanX = left;
        } else {
            _curCol = 0;
            _curRow++;
            if (spanX > cols) spanX = cols;
        }
    }

    uint8_t spanY = p.prefSpanY ? p.prefSpanY : 1;

    // Every row this card touches must exist before LVGL is told about it.
    if (_curRow + spanY > CARD_PAGE_MAX) spanY = 1;
    ensureRows((uint8_t)(_curRow + spanY));

    lv_obj_set_grid_cell(c->root(),
                         LV_GRID_ALIGN_STRETCH, _curCol, spanX,
                         LV_GRID_ALIGN_STRETCH, _curRow, spanY);

    _curCol += spanX;
    if (_curCol >= cols) { _curCol = 0; _curRow++; }
}

void CardPage::relayout() {
    _curCol = 0;
    _curRow = 0;
    // Not reset: _rowsDefined only ever grows, and shrinking it would mean
    // re-applying a shorter array while cards still reference the rows in it.
    for (uint8_t i = 0; i < _n; i++) if (_cards[i]) placeCard(_cards[i]);
}

void CardPage::report() const {
    SystemReport::line("  Cards: %u", (unsigned)_n);
    for (uint8_t i = 0; i < _n; i++) {
        const Card *c = _cards[i];
        if (!c) continue;
        const CardPlacement &p = c->placement();
        // priority is printed although nothing reads it yet. That is the point
        // - it makes the field visible on a real board before 2.5 has to make
        // decisions with it.
        SystemReport::line("    %-14s span %ux%u min %ux%u pri %3u  %-7s %s",
                           c->typeName(),
                           (unsigned)p.prefSpanX, (unsigned)p.prefSpanY,
                           (unsigned)p.minSpanX,  (unsigned)p.minSpanY,
                           (unsigned)p.priority,
                           cardVariantName(c->variant()),
                           cardStateName(c->state()));
    }
}
