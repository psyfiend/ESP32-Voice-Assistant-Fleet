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
    // NOT CLICKABLE either, so a tap on the gap between two cards falls
    // through to the screen instead of stopping here.
    //
    // The cards are the interactive things; this is the grid they sit in, and
    // it was absorbing every tap that missed one. That is invisible until
    // something wants those taps - 2.6's "tap anywhere else to dismiss" is the
    // first thing that does, and without this it would work on the header
    // strip and the margins and nowhere else, which is the kind of half-broken
    // that gets blamed on the gesture code.
    //
    // Gestures are unaffected: LV_OBJ_FLAG_GESTURE_BUBBLE is on by default, so
    // a swipe still reaches the screen either way.
    lv_obj_clear_flag        (_root, LV_OBJ_FLAG_CLICKABLE);
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

    // ROWS ARE NOT DECIDED HERE ANY MORE, and that is the 2.5 fix the owner
    // found on the glass.
    //
    // They used to come straight from UI::grid().rows, which is pure geometry:
    // "how many rows of roughly this shape fit in this height". With 13 cards
    // on a 7-column board that asked for FOUR rows, and since the page then
    // divides the height by the row count, every card was sized for a row that
    // had nothing in it - which on the 7B pushed all of them under the
    // compact threshold. Hiding the deck made it WORSE, because the extra
    // height bought a fifth row rather than taller cards.
    //
    // His rule, and it is the right one: "there is no reason to create an
    // additional row if all the cards can be made to fit". So the page records
    // the height it has to spend and decides the row count in commit(), once
    // it knows what it is being asked to place.
    lv_obj_update_layout(_root);   // _root was created a moment ago; without
                                   // this its content height is still zero
    _availH = (int32_t)lv_obj_get_content_height(_root);
    _availW = (int32_t)lv_obj_get_content_width(_root);   // 0 if not laid out;
                                                          // widthOf() copes
    if (_availH <= 0) {
        // Called before the parent has been laid out. Fall back to the token's
        // own arithmetic rather than to zero.
        _availH = (int32_t)g.cellH * (g.rows ? g.rows : 1)
                + _gap * ((g.rows ? g.rows : 1) - 1);
    }

    // The most rows this height could carry before a card stops being able to
    // draw a full layout. Below this a card goes compact, which is a decision
    // the CARD makes about its own contents - the page just refuses to plan
    // more rows than could ever be useful.
    _maxRows = 1;
    for (uint8_t r = 1; r <= PAGE_MAX_UNIT_ROWS / _sub; r++) {
        const int32_t cellH = (_availH - _gap * (r - 1)) / r;
        if (cellH < Card::compactCellNeedPx()) break;
        _maxRows = r;
    }

    useRows(1);   // provisional; commit() replaces it

    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
    lv_obj_set_layout        (_root, LV_LAYOUT_GRID);

    _n = 0;
    _placed = 0;
    _committed = false;
    clearOcc();

    Serial.printf("[Cards] Page %u cols (%u units), %ld px of height, max %u rows, gap %ld\n",
                  (unsigned)g.cols, (unsigned)_uCols,
                  (long)_availH, (unsigned)_maxRows, (long)_gap);
}

// How many CELL rows the cards need, from the area they ask for.
//
// A lower bound rather than an answer: it assumes cards tile without waste,
// which a 2-wide card on an odd column count does not. commit() starts here
// and adds rows until placement succeeds.
uint8_t CardPage::rowsWanted() const {
    if (!_n) return 1;
    const uint8_t cellCols = _uCols / _sub ? _uCols / _sub : 1;

    uint16_t cells = 0;
    for (uint8_t i = 0; i < _n; i++) {
        if (!_cards[i]) continue;
        const CardPlacement &p = _cards[i]->placement();
        uint8_t sx = p.prefSpanX ? p.prefSpanX : _sub;
        uint8_t sy = p.prefSpanY ? p.prefSpanY : _sub;
        // Round each card up to whole cells; a half-cell card still occupies a
        // row, and this is a floor, not a packing.
        const uint8_t cx = (uint8_t)((sx + _sub - 1) / _sub);
        const uint8_t cy = (uint8_t)((sy + _sub - 1) / _sub);
        cells = (uint16_t)(cells + cx * cy);
    }
    uint16_t rows = (uint16_t)((cells + cellCols - 1) / cellCols);
    if (rows < 1) rows = 1;
    return (uint8_t)(rows > 255 ? 255 : rows);
}

// Spend the height on exactly this many cell rows.
//
// The rows share the whole height, so FEWER rows means TALLER cards - which is
// what makes hiding the deck grow the cards instead of shrinking them.
//
// One guard: a page with two cards on it would otherwise get one row as tall
// as the screen. UIGrid::ASPECT_PCT caps how tall a card may be relative to
// its width; past that the grid stops stretching and the slack is left at the
// bottom rather than poured into a card nobody wants that tall.
void CardPage::useRows(uint8_t cellRows) {
    if (cellRows < 1) cellRows = 1;
    if (cellRows > PAGE_MAX_UNIT_ROWS / _sub) cellRows = PAGE_MAX_UNIT_ROWS / _sub;

    int32_t cellH = (_availH - _gap * (cellRows - 1)) / cellRows;

    // The ASPECT ceiling is a guard against a two-card page making each card
    // as tall as the screen. It does NOT apply when a row count was demanded -
    // there the tall card is what was asked for, and quietly refusing to give
    // it would make the button feel broken in exactly the way the old one did.
    if (!_rowsOverride) {
        const int32_t capH = ((int32_t)UI::grid().cellW * UI::grid().ASPECT_PCT) / 100;
        if (capH > 0 && cellH > capH) cellH = capH;
    }
    if (cellH < 1) cellH = 1;

    _unitH = (cellH - _gap * (_sub - 1)) / _sub;
    if (_unitH < 1) _unitH = 1;

    uint16_t ur = (uint16_t)cellRows * _sub;
    if (ur > PAGE_MAX_UNIT_ROWS) ur = PAGE_MAX_UNIT_ROWS;
    _uRows = (uint8_t)ur;

    for (uint8_t i = 0; i < _uRows; i++) _rowDsc[i] = (lv_coord_t)_unitH;
    _rowDsc[_uRows] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(_root, _colDsc, _rowDsc);
}

int32_t CardPage::heightOf(uint8_t spanY) const {
    if (spanY < 1) spanY = 1;
    return _unitH * spanY + _gap * (spanY - 1);
}

// The same for WIDTH. Columns are FR tracks, so a unit's width is the content
// width less the gaps, shared equally - exactly what LVGL's grid will do. Zero
// when the page was never laid out; the card then falls back to the token.
int32_t CardPage::widthOf(uint8_t spanX) const {
    if (_availW <= 0 || _uCols < 1) return 0;
    if (spanX < 1) spanX = 1;
    const int32_t unitW = (_availW - _gap * (_uCols - 1)) / _uCols;
    return unitW * spanX + _gap * (spanX - 1);
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

        // PINNED CARDS ARE PLACED FIRST, in two passes.
        //
        // The serial from WS_P4_5 showed "pin 2,2 rejected (out of bounds or
        // occupied) - flowing": the coordinate was perfectly valid and had
        // already been taken by a card that flowed into it first. A pin is a
        // STRONGER statement than flow - the author named that spot - so flow
        // must not be allowed to consume it. One pass for the pinned, then one
        // for everything else.
        bool ok = true;
        for (uint8_t pass = 0; pass < 2 && ok; pass++) {
            for (uint8_t i = 0; i < _n; i++) {
                if (!alive[i] || !_cards[i]) continue;
                const CardPlacement &pl = _cards[i]->placement();
                const bool pinned = (pl.col >= 0 && pl.row >= 0);
                if (pinned != (pass == 0)) continue;
                if (!placeOne(i)) { ok = false; break; }
            }
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

    // Spend as few rows as the cards need, and only add more when placement
    // genuinely cannot fit them. Dropping cards is the LAST resort, after the
    // page has already grown to every row it could carry.
    if (_rowsOverride) {
        // OBEYED, not negotiated. The caller asked for this many rows; cards
        // that do not fit are dropped by priority, which is the whole point of
        // asking. The only clamp is the descriptor's own size.
        uint8_t rows = _rowsOverride;
        if (rows > PAGE_MAX_UNIT_ROWS / _sub) rows = PAGE_MAX_UNIT_ROWS / _sub;
        useRows(rows);
        plan();
    } else {
        uint8_t rows = rowsWanted();
        if (rows > _maxRows) rows = _maxRows;

        for (;;) {
            useRows(rows);
            plan();
            if (_placed == _n) break;       // everything fits at this row count
            if (rows >= _maxRows) break;    // no more rows to give - plan() drops
            rows++;
        }
    }

    Serial.printf("[Cards] %u cards want %u rows; using %u of max %u -> cell %ldx%ld px\n",
                  (unsigned)_n, (unsigned)rowsWanted(), (unsigned)(_uRows / _sub),
                  (unsigned)_maxRows,
                  (long)UI::grid().cellW, (long)heightOf(_sub));


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
        c->setCellWidthPx (widthOf (_slot[i].spanX));
        c->build(_root);
        lv_obj_set_grid_cell(c->root(),
                             LV_GRID_ALIGN_STRETCH, _slot[i].col, _slot[i].spanX,
                             LV_GRID_ALIGN_STRETCH, _slot[i].row, _slot[i].spanY);
        if (_binder) _binder->add(c);
    }
    // CARDS THAT DID NOT MAKE THE PAGE ARE FREED, not kept around.
    //
    // The owner asked directly: "is the device holding the cards not on the
    // page in memory?" It was. An unplaced card was never built, so it held no
    // LVGL widgets - but the object itself, its bound entity pointers and its
    // label buffers stayed alive for the life of the page. Small, and on the
    // board where this matters there is nothing to spare.
    //
    // Deleted here rather than in the destructor because they can never come
    // back: placement has already been decided, and a card that was not placed
    // will not be placed by anything short of a rebuild - which constructs
    // fresh ones from the spec anyway.
    for (uint8_t i = 0; i < _n; i++) {
        if (!_cards[i] || _slot[i].placed) continue;
        delete _cards[i];
        _cards[i] = nullptr;
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
        c->setVariant(cs.variant == CardVariant::VAR_AUTO ? spec.variantDefault
                                                          : cs.variant);
        c->setPlacement(cs.place);
        c->setTempUnit(cs.tempUnit == TempUnit::TEMP_INHERIT ? spec.tempUnit
                                                             : cs.tempUnit);
        c->setLabelMode(cs.labelMode == CardLabel::LBL_INHERIT ? spec.labelMode
                                                               : cs.labelMode);
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
