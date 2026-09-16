#include "Cards/CardPage.h"
#include "Cards/CardTypes.h"
#include "Cards/CardCatalog.h"
#include "Cards/CardIcons.h"
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

void CardPage::begin(lv_obj_t *parent, CardBinder *binder, uint8_t subdivision) {
    _binder = binder;

    const UIGrid &g = UI::grid();

    const int32_t inset = UI::sc(g.INSET);
    _gap = UI::sc(g.GAP);

    _sub = subdivision ? subdivision : 1;
    if (_sub > 4) _sub = 4;     // thirds are already exotic; quarters are the
                                // most anyone has asked for

    _root = lv_obj_create(parent);
    lv_obj_set_size               (_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color     (_root, UI::c(UI::pal().GROUND), 0);
    lv_obj_set_style_border_width (_root, 0, 0);
    lv_obj_set_style_pad_all      (_root, inset, 0);
    lv_obj_set_style_pad_column   (_root, _gap, 0);
    lv_obj_set_style_pad_row      (_root, _gap, 0);

    // A DASHBOARD PAGE DOES NOT SCROLL. The owner, unprompted: "There should
    // never be any scrolling of cards on any dashboard pages, the cards should
    // effectively be locked in place. Only when swiping or navigating to a new
    // page entirely should new entities be on the screen."
    //
    // So cards that do not fit are not placed at all, and WHICH ones is now
    // CardPlacement::priority's decision rather than declaration order's.
    lv_obj_clear_flag        (_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(_root, LV_SCROLLBAR_MODE_OFF);

    // --- The unit grid ----------------------------------------------------
    //
    // Columns are FR units, not the pixel widths UI::grid() computed. That is
    // not ignoring the token - cellW is what DERIVED the column count, and the
    // count is the decision. Expressing the result as fractions is what makes
    // a spanning card come out exactly right, and it is also what makes the
    // subdivision free: `sub` FR tracks plus the gaps between them measure the
    // same as one cell did, exactly, with no rounding.
    uint16_t uc = (uint16_t)g.cols * _sub;
    if (uc < 1) uc = 1;
    if (uc > PAGE_MAX_UNIT_COLS) uc = PAGE_MAX_UNIT_COLS;
    _uCols = (uint8_t)uc;

    for (uint8_t i = 0; i < _uCols; i++) _colDsc[i] = LV_GRID_FR(1);
    _colDsc[_uCols] = LV_GRID_TEMPLATE_LAST;

    // Rows are real pixels, because a row's height is a token and not a
    // fraction of anything. One unit row is a cell split `sub` ways with the
    // gaps taken out first, so `sub` unit rows plus the gap between them come
    // back to the cell height - give or take the (sub-1) px integer division
    // loses, which commit() accounts for by telling each card the height it
    // ACTUALLY got rather than the one the token asked for.
    _unitH = ((int32_t)g.cellH - _gap * (_sub - 1)) / _sub;
    if (_unitH < 1) _unitH = 1;

    uint16_t ur = (uint16_t)g.rows * _sub;
    if (ur < 1) ur = 1;
    if (ur > PAGE_MAX_UNIT_ROWS) ur = PAGE_MAX_UNIT_ROWS;
    _uRows = (uint8_t)ur;

    for (uint8_t i = 0; i < _uRows; i++) _rowDsc[i] = (lv_coord_t)_unitH;
    _rowDsc[_uRows] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
    lv_obj_set_layout        (_root, LV_LAYOUT_GRID);

    _n = 0;
    _placed = 0;
    _committed = false;
    clearOcc();

    Serial.printf("[Cards] Page %ux%u cells / %ux%u units, cell %ux%u px, unit row %ld px, gap %ld\n",
                  (unsigned)g.cols, (unsigned)g.rows,
                  (unsigned)_uCols, (unsigned)_uRows,
                  (unsigned)g.cellW, (unsigned)g.cellH,
                  (long)_unitH, (long)_gap);
}

int32_t CardPage::heightOf(uint8_t spanY) const {
    if (spanY < 1) spanY = 1;
    return _unitH * spanY + _gap * (spanY - 1);
}

// ---------------------------------------------------------------------------
// Occupancy
// ---------------------------------------------------------------------------

void CardPage::clearOcc() {
    for (uint8_t r = 0; r < PAGE_MAX_UNIT_ROWS; r++)
        for (uint8_t b = 0; b < sizeof(_occ[0]); b++) _occ[r][b] = 0;
}

bool CardPage::blockFree(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY) const {
    for (uint8_t r = row; r < row + spanY; r++) {
        if (r >= PAGE_MAX_UNIT_ROWS) return false;
        for (uint8_t c = col; c < col + spanX; c++) {
            if (c >= PAGE_MAX_UNIT_COLS) return false;
            if (_occ[r][c >> 3] & (uint8_t)(1u << (c & 7))) return false;
        }
    }
    return true;
}

void CardPage::occupy(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY) {
    for (uint8_t r = row; r < row + spanY && r < PAGE_MAX_UNIT_ROWS; r++)
        for (uint8_t c = col; c < col + spanX && c < PAGE_MAX_UNIT_COLS; c++)
            _occ[r][c >> 3] |= (uint8_t)(1u << (c & 7));
}

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------

bool CardPage::placeOne(uint8_t i) {
    Card *c = _cards[i];
    if (!c) return true;

    const CardPlacement &p = c->placement();

    // Clamp to the page before anything else. On CYD_S3_3248 portrait the page
    // is two cells wide, so a card asking for three is not a misconfiguration
    // to reject - it is a card designed for a bigger board, and the correct
    // answer is the widest thing that fits.
    uint8_t pX = p.prefSpanX ? p.prefSpanX : 1;
    uint8_t mX = p.minSpanX  ? p.minSpanX  : 1;
    uint8_t pY = p.prefSpanY ? p.prefSpanY : 1;
    uint8_t mY = p.minSpanY  ? p.minSpanY  : 1;
    if (pX > _uCols) pX = _uCols;
    if (pY > _uRows) pY = _uRows;
    if (mX > pX)     mX = pX;
    if (mY > pY)     mY = pY;

    // --- Explicit placement, and the validator ROADMAP Q3b asked for -------
    //
    // An invalid pin is REPORTED and then flowed, rather than dropped. The
    // card is still the one the author asked for; only its coordinate was
    // wrong, and silently losing it would be a worse answer than putting it
    // somewhere visible and saying so.
    if (p.col >= 0 && p.row >= 0) {
        const uint8_t cc = (uint8_t)p.col, rr = (uint8_t)p.row;
        if (cc + pX <= _uCols && rr + pY <= _uRows && blockFree(cc, rr, pX, pY)) {
            occupy(cc, rr, pX, pY);
            _slot[i].col = cc; _slot[i].row = rr;
            _slot[i].spanX = pX; _slot[i].spanY = pY;
            _slot[i].placed = true;
            return true;
        }
        _slot[i].pinFail = true;
        DBG_CARDS("%s: pin %d,%d rejected (out of bounds or occupied) - flowing\n",
                  c->typeName(), (int)p.col, (int)p.row);
    }

    // --- Flow -------------------------------------------------------------
    //
    // First free position wins, and at that position the WIDEST span that fits
    // wins. That ordering is deliberate: a card that fits at its minimum
    // beside its neighbour beats a card sitting alone on a new row at full
    // width, which is the rule 2.4's cursor arithmetic encoded and this keeps.
    for (uint8_t r = 0; r + mY <= _uRows; r++) {
        for (uint8_t cc = 0; cc + mX <= _uCols; cc++) {
            for (uint8_t sx = pX; sx >= mX; sx--) {
                if (cc + sx > _uCols) continue;
                for (uint8_t sy = pY; sy >= mY; sy--) {
                    if (r + sy > _uRows) continue;
                    if (!blockFree(cc, r, sx, sy)) continue;
                    occupy(cc, r, sx, sy);
                    _slot[i].col = cc; _slot[i].row = r;
                    _slot[i].spanX = sx; _slot[i].spanY = sy;
                    _slot[i].placed = true;
                    return true;
                }
            }
        }
    }
    return false;
}

void CardPage::plan() {
    bool alive[CARD_PAGE_MAX];
    for (uint8_t i = 0; i < _n; i++) alive[i] = (_cards[i] != nullptr);

    for (;;) {
        clearOcc();
        for (uint8_t i = 0; i < _n; i++) {
            const bool pf = _slot[i].pinFail;
            _slot[i] = Slot();
            _slot[i].pinFail = pf;      // a bad pin stays reportable across replans
        }

        bool ok = true;
        for (uint8_t i = 0; i < _n; i++) {
            if (!alive[i]) continue;
            if (!placeOne(i)) { ok = false; break; }
        }
        if (ok) break;

        // PRIORITY DEGRADATION. Drop the lowest-priority card still alive and
        // try the whole layout again - not the card that happened to fail.
        // Those are different cards whenever a low-priority one was declared
        // early, and picking the failure would make the outcome depend on
        // declaration order, which is precisely what priority exists to stop.
        //
        // Ties go to the LATER declaration, so an author listing equals keeps
        // the reading order they wrote.
        int16_t drop = -1;
        uint8_t lowest = 255;
        for (uint8_t i = 0; i < _n; i++) {
            if (!alive[i] || !_cards[i]) continue;
            const uint8_t pr = _cards[i]->placement().priority;
            if (drop < 0 || pr <= lowest) { lowest = pr; drop = (int16_t)i; }
        }
        if (drop < 0) break;

        alive[(uint8_t)drop] = false;
        DBG_CARDS("dropped %s (priority %u) - page is full\n",
                  _cards[drop]->typeName(), (unsigned)lowest);
    }

    _placed = 0;
    for (uint8_t i = 0; i < _n; i++) if (_slot[i].placed) _placed++;
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------

Card *CardPage::add(Card *c) {
    if (!c) return nullptr;
    if (_n >= CARD_PAGE_MAX) {
        // Deleted rather than dropped on the floor. A page that silently
        // leaked its overflow would be a slow leak on a device whose whole
        // widget budget is a 128 KB static array.
        delete c;
        return nullptr;
    }
    _cards[_n] = c;
    _slot [_n] = Slot();
    _n++;
    return c;
}

void CardPage::commit() {
    if (_committed) return;
    plan();

    for (uint8_t i = 0; i < _n; i++) {
        Card *c = _cards[i];
        if (!c || !_slot[i].placed) continue;

        // THE CARD IS TOLD ITS REAL HEIGHT BEFORE IT IS BUILT.
        //
        // Card::resolveVariant() used to derive this from UI::grid() and its
        // own row span, which was right only while a row was a whole cell. It
        // is not, now that rows are units - and it was already fragile, since
        // the grid it read is global state that any other page can move. The
        // page knows the answer exactly; handing it over removes the guess.
        c->setCellHeightPx(heightOf(_slot[i].spanY));
        c->build(_root);
        lv_obj_set_grid_cell(c->root(),
                             LV_GRID_ALIGN_STRETCH, _slot[i].col, _slot[i].spanX,
                             LV_GRID_ALIGN_STRETCH, _slot[i].row, _slot[i].spanY);
        if (_binder) _binder->add(c);
    }
    _committed = true;

    if (_placed != _n) {
        Serial.printf("[Cards] %u of %u cards placed; %u dropped for space\n",
                      (unsigned)_placed, (unsigned)_n, (unsigned)(_n - _placed));
    }
}

void CardPage::relayout() {
    plan();
    for (uint8_t i = 0; i < _n; i++) {
        Card *c = _cards[i];
        if (!c || !c->root()) continue;
        if (!_slot[i].placed) { lv_obj_add_flag(c->root(), LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(c->root(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_grid_cell(c->root(),
                             LV_GRID_ALIGN_STRETCH, _slot[i].col, _slot[i].spanX,
                             LV_GRID_ALIGN_STRETCH, _slot[i].row, _slot[i].spanY);
    }
}

// ---------------------------------------------------------------------------
// From data
// ---------------------------------------------------------------------------

void CardPage::applySpec(const PageSpec &spec, EntityRegistry &reg) {
    Card::setShowAreaDefault(spec.showArea);

    for (uint8_t i = 0; i < spec.count; i++) {
        const CardSpec &cs = spec.cards[i];

        // The FIRST primary that resolves decides the card's type. A spec may
        // name several - the aggregate light case - and a room where one bulb
        // has been unpaired should still render the rest.
        const Entity *first = nullptr;
        uint8_t       firstIdx = 0;
        for (uint8_t k = 0; k < CARD_PRIMARY_MAX; k++) {
            if (!cs.primaries[k]) continue;
            const Entity *e = reg.find(cs.primaries[k]);
            if (e) { first = e; firstIdx = k; break; }
            Serial.printf("[Cards] spec %u: entity \"%s\" not in registry\n",
                          (unsigned)i, cs.primaries[k]);
        }
        if (!first) continue;

        Card *c = cardForEntity(first);
        if (!c) {
            // A real EntityKind with no card type yet - NUMBER, TEXT, CLIMATE,
            // WEATHER. Saying so beats rendering it as something it is not.
            Serial.printf("[Cards] spec %u: no card type for \"%s\"\n",
                          (unsigned)i, cs.primaries[firstIdx]);
            continue;
        }

        for (uint8_t k = (uint8_t)(firstIdx + 1); k < CARD_PRIMARY_MAX; k++) {
            if (!cs.primaries[k]) continue;
            if (const Entity *e = reg.find(cs.primaries[k])) c->bindPrimary(e);
        }
        for (uint8_t k = 0; k < CARD_SECONDARY_MAX; k++) {
            if (!cs.secondaries[k]) continue;
            if (const Entity *e = reg.find(cs.secondaries[k])) c->bindSecondary(e);
        }

        if (cs.label) c->setLabel(cs.label);
        if (cs.area) {
            c->setArea(cs.area);
            // Derived from the NAME, so two cards in one area always agree and
            // the colour survives a reboot. See cardAreaColor().
            if (spec.areaColor) c->setAreaColor(cardAreaColor(cs.area));
        }
        c->setHeaderStyle(cs.header == CARD_HDR_INHERIT
                          ? spec.headerDefault
                          : (CardHeaderStyle)cs.header);
        c->setVariant(cs.variant);
        c->setPlacement(cs.place);
        c->setTempUnit(cs.tempUnit == TempUnit::TEMP_INHERIT ? spec.tempUnit
                                                             : cs.tempUnit);
        if (cs.longStaleMs) c->setLongStaleMs(cs.longStaleMs);
        if (cs.paused)      c->setPaused(true);

        add(c);
    }

    commit();
}

// ---------------------------------------------------------------------------

void CardPage::report() const {
    SystemReport::line("  Page: %ux%u units (subdivision %u), %u of %u cards placed",
                       (unsigned)_uCols, (unsigned)_uRows, (unsigned)_sub,
                       (unsigned)_placed, (unsigned)_n);
    for (uint8_t i = 0; i < _n; i++) {
        const Card *c = _cards[i];
        if (!c) continue;
        const CardPlacement &p = c->placement();
        if (_slot[i].placed) {
            SystemReport::line("    %-14s @ %u,%u  span %ux%u (min %ux%u) pri %3u  %-7s %s%s",
                               c->typeName(),
                               (unsigned)_slot[i].col,   (unsigned)_slot[i].row,
                               (unsigned)_slot[i].spanX, (unsigned)_slot[i].spanY,
                               (unsigned)p.minSpanX, (unsigned)p.minSpanY,
                               (unsigned)p.priority,
                               cardVariantName(c->variant()),
                               cardStateName(c->state()),
                               _slot[i].pinFail ? "  [pin rejected]" : "");
        } else {
            SystemReport::line("    %-14s DROPPED - no room  span %ux%u pri %3u",
                               c->typeName(),
                               (unsigned)p.prefSpanX, (unsigned)p.prefSpanY,
                               (unsigned)p.priority);
        }
    }
}
