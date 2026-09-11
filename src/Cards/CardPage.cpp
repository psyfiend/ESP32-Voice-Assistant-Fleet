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

void CardPage::begin(lv_obj_t *parent, CardBinder *binder) {
    _binder = binder;

    const UIGrid    &g = UI::grid();
    const UIMetrics &m = UI::met();
    (void)m;

    _root = lv_obj_create(parent);
    lv_obj_set_size               (_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color     (_root, UI::c(UI::pal().GROUND), 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, UI::sc(g.INSET), 0);
    lv_obj_set_style_pad_gap      (_root, UI::sc(g.GAP), 0);
    UI::tameScroll(_root);

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
    uint8_t rows = g.rows;
    if (rows < 1) rows = 1;
    if (rows > 16) rows = 16;
    for (uint8_t i = 0; i < rows; i++) _rowDsc[i] = g.cellH;
    _rowDsc[rows] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
    lv_obj_set_layout        (_root, LV_LAYOUT_GRID);

    Serial.printf("[Cards] Page %ux%u, cell %ux%u px\n",
                  (unsigned)cols, (unsigned)rows,
                  (unsigned)g.cellW, (unsigned)g.cellH);
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

    _cards[_n++] = c;
    c->build(_root);
    placeCard(c);
    if (_binder) _binder->add(c);
    return c;
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

    const uint8_t spanY = p.prefSpanY ? p.prefSpanY : 1;

    lv_obj_set_grid_cell(c->root(),
                         LV_GRID_ALIGN_STRETCH, _curCol, spanX,
                         LV_GRID_ALIGN_STRETCH, _curRow, spanY);

    _curCol += spanX;
    if (_curCol >= cols) { _curCol = 0; _curRow++; }
}

void CardPage::relayout() {
    _curCol = 0;
    _curRow = 0;
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
        SystemReport::line("    %-8s span %ux%u min %ux%u pri %3u  %s",
                           c->typeName(),
                           (unsigned)p.prefSpanX, (unsigned)p.prefSpanY,
                           (unsigned)p.minSpanX,  (unsigned)p.minSpanY,
                           (unsigned)p.priority,
                           cardStateName(c->state()));
    }
}
